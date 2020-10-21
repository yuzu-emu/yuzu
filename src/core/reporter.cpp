// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <ctime>
#include <fstream>
#include <iomanip>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <nlohmann/json.hpp>

#include "common/file_util.h"
#include "common/hex_util.h"
#include "common/scm_rev.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/memory/page_table.h"
#include "core/hle/kernel/process.h"
#include "core/hle/result.h"
#include "core/hle/service/lm/manager.h"
#include "core/memory.h"
#include "core/reporter.h"
#include "core/settings.h"

namespace {

std::string GetPath(std::string_view type, u64 title_id, std::string_view timestamp) {
    return fmt::format("{}{}/{:016X}_{}.json",
                       Common::FS::GetUserPath(Common::FS::UserPath::LogDir), type, title_id,
                       timestamp);
}

std::string GetTimestamp() {
    const auto time = std::time(nullptr);
    return fmt::format("{:%FT%H-%M-%S}", *std::localtime(&time));
}

using namespace nlohmann;

void SaveToFile(json json, const std::string& filename) {
    if (!Common::FS::CreateFullPath(filename)) {
        LOG_ERROR(Core, "Failed to create path for '{}' to save report!", filename);
        return;
    }

    std::ofstream file(
        Common::FS::SanitizePath(filename, Common::FS::DirectorySeparator::PlatformDefault));
    file << std::setw(4) << json << std::endl;
}

json GetYuzuVersionData() {
    return {
        {"scm_rev", std::string(Common::g_scm_rev)},
        {"scm_branch", std::string(Common::g_scm_branch)},
        {"scm_desc", std::string(Common::g_scm_desc)},
        {"build_name", std::string(Common::g_build_name)},
        {"build_date", std::string(Common::g_build_date)},
        {"build_fullname", std::string(Common::g_build_fullname)},
        {"build_version", std::string(Common::g_build_version)},
        {"shader_cache_version", std::string(Common::g_shader_cache_version)},
    };
}

json GetReportCommonData(u64 title_id, ResultCode result, const std::string& timestamp,
                         std::optional<u128> user_id = {}) {
    auto out = json{
        {"title_id", fmt::format("{:016X}", title_id)},
        {"result_raw", fmt::format("{:08X}", result.raw)},
        {"result_module", fmt::format("{:08X}", static_cast<u32>(result.error_module.Value()))},
        {"result_description", fmt::format("{:08X}", result.description.Value())},
        {"timestamp", timestamp},
    };

    if (user_id.has_value()) {
        out["user_id"] = fmt::format("{:016X}{:016X}", (*user_id)[1], (*user_id)[0]);
    }

    return out;
}

json GetProcessorStateData(const std::string& architecture, u64 entry_point, u64 sp, u64 pc,
                           u64 pstate, std::array<u64, 31> registers,
                           std::optional<std::array<u64, 32>> backtrace = {}) {
    auto out = json{
        {"entry_point", fmt::format("{:016X}", entry_point)},
        {"sp", fmt::format("{:016X}", sp)},
        {"pc", fmt::format("{:016X}", pc)},
        {"pstate", fmt::format("{:016X}", pstate)},
        {"architecture", architecture},
    };

    auto registers_out = json::object();
    for (std::size_t i = 0; i < registers.size(); ++i) {
        registers_out[fmt::format("X{:02d}", i)] = fmt::format("{:016X}", registers[i]);
    }

    out["registers"] = std::move(registers_out);

    if (backtrace.has_value()) {
        auto backtrace_out = json::array();
        for (const auto& entry : *backtrace) {
            backtrace_out.push_back(fmt::format("{:016X}", entry));
        }
        out["backtrace"] = std::move(backtrace_out);
    }

    return out;
}

json GetProcessorStateDataAuto(Core::System& system) {
    const auto* process{system.CurrentProcess()};
    auto& arm{system.CurrentArmInterface()};

    Core::ARM_Interface::ThreadContext64 context{};
    arm.SaveContext(context);

    return GetProcessorStateData(process->Is64BitProcess() ? "AArch64" : "AArch32",
                                 process->PageTable().GetCodeRegionStart(), context.sp, context.pc,
                                 context.pstate, context.cpu_registers);
}

json GetBacktraceData(Core::System& system) {
    auto out = json::array();
    const auto& backtrace{system.CurrentArmInterface().GetBacktrace()};
    for (const auto& entry : backtrace) {
        out.push_back({
            {"module", entry.mod},
            {"address", fmt::format("{:016X}", entry.address)},
            {"original_address", fmt::format("{:016X}", entry.original_address)},
            {"offset", fmt::format("{:016X}", entry.offset)},
            {"symbol_name", entry.name},
        });
    }

    return out;
}

json GetFullDataAuto(const std::string& timestamp, u64 title_id, Core::System& system) {
    json out;

    out["yuzu_version"] = GetYuzuVersionData();
    out["report_common"] = GetReportCommonData(title_id, RESULT_SUCCESS, timestamp);
    out["processor_state"] = GetProcessorStateDataAuto(system);
    out["backtrace"] = GetBacktraceData(system);

    return out;
}

template <bool read_value, typename DescriptorType>
json GetHLEBufferDescriptorData(const std::vector<DescriptorType>& buffer,
                                Core::Memory::Memory& memory) {
    auto buffer_out = json::array();
    for (const auto& desc : buffer) {
        auto entry = json{
            {"address", fmt::format("{:016X}", desc.Address())},
            {"size", fmt::format("{:016X}", desc.Size())},
        };

        if constexpr (read_value) {
            std::vector<u8> data(desc.Size());
            memory.ReadBlock(desc.Address(), data.data(), desc.Size());
            entry["data"] = Common::HexToString(data);
        }

        buffer_out.push_back(std::move(entry));
    }

    return buffer_out;
}

json GetHLERequestContextData(Kernel::HLERequestContext& ctx, Core::Memory::Memory& memory) {
    json out;

    auto cmd_buf = json::array();
    for (std::size_t i = 0; i < IPC::COMMAND_BUFFER_LENGTH; ++i) {
        cmd_buf.push_back(fmt::format("{:08X}", ctx.CommandBuffer()[i]));
    }

    out["command_buffer"] = std::move(cmd_buf);

    out["buffer_descriptor_a"] = GetHLEBufferDescriptorData<true>(ctx.BufferDescriptorA(), memory);
    out["buffer_descriptor_b"] = GetHLEBufferDescriptorData<false>(ctx.BufferDescriptorB(), memory);
    out["buffer_descriptor_c"] = GetHLEBufferDescriptorData<false>(ctx.BufferDescriptorC(), memory);
    out["buffer_descriptor_x"] = GetHLEBufferDescriptorData<true>(ctx.BufferDescriptorX(), memory);

    return out;
}

} // Anonymous namespace

namespace Core {

Reporter::Reporter(System& system) : system(system) {}

Reporter::~Reporter() = default;

void Reporter::SaveCrashReport(u64 title_id, ResultCode result, u64 set_flags, u64 entry_point,
                               u64 sp, u64 pc, u64 pstate, u64 afsr0, u64 afsr1, u64 esr, u64 far,
                               const std::array<u64, 31>& registers,
                               const std::array<u64, 32>& backtrace, u32 backtrace_size,
                               const std::string& arch, u32 unk10) const {
    if (!IsReportingEnabled()) {
        return;
    }

    const auto timestamp = GetTimestamp();
    json out;

    out["yuzu_version"] = GetYuzuVersionData();
    out["report_common"] = GetReportCommonData(title_id, result, timestamp);

    auto proc_out = GetProcessorStateData(arch, entry_point, sp, pc, pstate, registers, backtrace);
    proc_out["set_flags"] = fmt::format("{:016X}", set_flags);
    proc_out["afsr0"] = fmt::format("{:016X}", afsr0);
    proc_out["afsr1"] = fmt::format("{:016X}", afsr1);
    proc_out["esr"] = fmt::format("{:016X}", esr);
    proc_out["far"] = fmt::format("{:016X}", far);
    proc_out["backtrace_size"] = fmt::format("{:08X}", backtrace_size);
    proc_out["unknown_10"] = fmt::format("{:08X}", unk10);

    out["processor_state"] = std::move(proc_out);

    SaveToFile(std::move(out), GetPath("crash_report", title_id, timestamp));
}

void Reporter::SaveSvcBreakReport(u32 type, bool signal_debugger, u64 info1, u64 info2,
                                  std::optional<std::vector<u8>> resolved_buffer) const {
    if (!IsReportingEnabled()) {
        return;
    }

    const auto timestamp = GetTimestamp();
    const auto title_id = system.CurrentProcess()->GetTitleID();
    auto out = GetFullDataAuto(timestamp, title_id, system);

    auto break_out = json{
        {"type", fmt::format("{:08X}", type)},
        {"signal_debugger", fmt::format("{}", signal_debugger)},
        {"info1", fmt::format("{:016X}", info1)},
        {"info2", fmt::format("{:016X}", info2)},
    };

    if (resolved_buffer.has_value()) {
        break_out["debug_buffer"] = Common::HexToString(*resolved_buffer);
    }

    out["svc_break"] = std::move(break_out);

    SaveToFile(std::move(out), GetPath("svc_break_report", title_id, timestamp));
}

void Reporter::SaveUnimplementedFunctionReport(Kernel::HLERequestContext& ctx, u32 command_id,
                                               const std::string& name,
                                               const std::string& service_name) const {
    if (!IsReportingEnabled()) {
        return;
    }

    const auto timestamp = GetTimestamp();
    const auto title_id = system.CurrentProcess()->GetTitleID();
    auto out = GetFullDataAuto(timestamp, title_id, system);

    auto function_out = GetHLERequestContextData(ctx, system.Memory());
    function_out["command_id"] = command_id;
    function_out["function_name"] = name;
    function_out["service_name"] = service_name;

    out["function"] = std::move(function_out);

    SaveToFile(std::move(out), GetPath("unimpl_func_report", title_id, timestamp));
}

void Reporter::SaveUnimplementedAppletReport(
    u32 applet_id, u32 common_args_version, u32 library_version, u32 theme_color,
    bool startup_sound, u64 system_tick, std::vector<std::vector<u8>> normal_channel,
    std::vector<std::vector<u8>> interactive_channel) const {
    if (!IsReportingEnabled()) {
        return;
    }

    const auto timestamp = GetTimestamp();
    const auto title_id = system.CurrentProcess()->GetTitleID();
    auto out = GetFullDataAuto(timestamp, title_id, system);

    out["applet_common_args"] = {
        {"applet_id", fmt::format("{:02X}", applet_id)},
        {"common_args_version", fmt::format("{:08X}", common_args_version)},
        {"library_version", fmt::format("{:08X}", library_version)},
        {"theme_color", fmt::format("{:08X}", theme_color)},
        {"startup_sound", fmt::format("{}", startup_sound)},
        {"system_tick", fmt::format("{:016X}", system_tick)},
    };

    auto normal_out = json::array();
    for (const auto& data : normal_channel) {
        normal_out.push_back(Common::HexToString(data));
    }

    auto interactive_out = json::array();
    for (const auto& data : interactive_channel) {
        interactive_out.push_back(Common::HexToString(data));
    }

    out["applet_normal_data"] = std::move(normal_out);
    out["applet_interactive_data"] = std::move(interactive_out);

    SaveToFile(std::move(out), GetPath("unimpl_applet_report", title_id, timestamp));
}

void Reporter::SavePlayReport(PlayReportType type, u64 title_id, std::vector<std::vector<u8>> data,
                              std::optional<u64> process_id, std::optional<u128> user_id) const {
    if (!IsReportingEnabled()) {
        return;
    }

    const auto timestamp = GetTimestamp();
    json out;

    out["yuzu_version"] = GetYuzuVersionData();
    out["report_common"] = GetReportCommonData(title_id, RESULT_SUCCESS, timestamp, user_id);

    auto data_out = json::array();
    for (const auto& d : data) {
        data_out.push_back(Common::HexToString(d));
    }

    if (process_id.has_value()) {
        out["play_report_process_id"] = fmt::format("{:016X}", *process_id);
    }

    out["play_report_type"] = fmt::format("{:02}", static_cast<u8>(type));
    out["play_report_data"] = std::move(data_out);

    SaveToFile(std::move(out), GetPath("play_report", title_id, timestamp));
}

void Reporter::SaveErrorReport(u64 title_id, ResultCode result,
                               std::optional<std::string> custom_text_main,
                               std::optional<std::string> custom_text_detail) const {
    if (!IsReportingEnabled()) {
        return;
    }

    const auto timestamp = GetTimestamp();
    json out;

    out["yuzu_version"] = GetYuzuVersionData();
    out["report_common"] = GetReportCommonData(title_id, result, timestamp);
    out["processor_state"] = GetProcessorStateDataAuto(system);
    out["backtrace"] = GetBacktraceData(system);

    out["error_custom_text"] = {
        {"main", *custom_text_main},
        {"detail", *custom_text_detail},
    };

    SaveToFile(std::move(out), GetPath("error_report", title_id, timestamp));
}

void Reporter::SaveLogReport(u32 destination, std::vector<Service::LM::LogMessage> messages) const {
    if (!IsReportingEnabled()) {
        return;
    }

    const auto timestamp = GetTimestamp();
    json out;

    out["yuzu_version"] = GetYuzuVersionData();
    out["report_common"] =
        GetReportCommonData(system.CurrentProcess()->GetTitleID(), RESULT_SUCCESS, timestamp);

    out["log_destination"] =
        fmt::format("{}", static_cast<Service::LM::DestinationFlag>(destination));

    auto json_messages = json::array();
    std::transform(messages.begin(), messages.end(), std::back_inserter(json_messages),
                   [](const Service::LM::LogMessage& message) {
                       json out;
                       out["is_head"] = fmt::format("{}", message.header.IsHeadLog());
                       out["is_tail"] = fmt::format("{}", message.header.IsTailLog());
                       out["pid"] = fmt::format("{:016X}", message.header.pid);
                       out["thread_context"] =
                           fmt::format("{:016X}", message.header.thread_context);
                       out["payload_size"] = fmt::format("{:016X}", message.header.payload_size);
                       out["flags"] = fmt::format("{:04X}", message.header.flags.Value());
                       out["severity"] = fmt::format("{}", message.header.severity.Value());
                       out["verbosity"] = fmt::format("{:02X}", message.header.verbosity);

                       auto fields = json::array();
                       std::transform(message.fields.begin(), message.fields.end(),
                                      std::back_inserter(fields), [](const auto& kv) {
                                          json out;
                                          out["type"] = fmt::format("{}", kv.first);
                                          out["data"] =
                                              Service::LM::FormatField(kv.first, kv.second);
                                          return out;
                                      });

                       out["fields"] = std::move(fields);
                       return out;
                   });

    out["log_messages"] = std::move(json_messages);

    SaveToFile(std::move(out),
               GetPath("log_report", system.CurrentProcess()->GetTitleID(), timestamp));
}

void Reporter::SaveFilesystemAccessReport(Service::FileSystem::LogMode log_mode,
                                          std::string log_message) const {
    if (!IsReportingEnabled())
        return;

    const auto timestamp = GetTimestamp();
    const auto title_id = system.CurrentProcess()->GetTitleID();
    json out;

    out["yuzu_version"] = GetYuzuVersionData();
    out["report_common"] = GetReportCommonData(title_id, RESULT_SUCCESS, timestamp);

    out["log_mode"] = fmt::format("{:08X}", static_cast<u32>(log_mode));
    out["log_message"] = std::move(log_message);

    SaveToFile(std::move(out), GetPath("filesystem_access_report", title_id, timestamp));
}

void Reporter::SaveUserReport() const {
    if (!IsReportingEnabled()) {
        return;
    }

    const auto timestamp = GetTimestamp();
    const auto title_id = system.CurrentProcess()->GetTitleID();

    SaveToFile(GetFullDataAuto(timestamp, title_id, system),
               GetPath("user_report", title_id, timestamp));
}

bool Reporter::IsReportingEnabled() const {
    return Settings::values.reporting_services;
}

} // namespace Core
