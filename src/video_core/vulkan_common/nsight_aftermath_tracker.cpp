// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef HAS_NSIGHT_AFTERMATH

#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "common/common_types.h"
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "video_core/vulkan_common/nsight_aftermath_tracker.h"

namespace Vulkan {

static constexpr char AFTERMATH_LIB_NAME[] = "GFSDK_Aftermath_Lib.x64.dll";

NsightAftermathTracker::NsightAftermathTracker() {
    if (!dl.Open(AFTERMATH_LIB_NAME)) {
        LOG_ERROR(Render_Vulkan, "Failed to load Nsight Aftermath DLL");
        return;
    }
    if (!dl.GetSymbol("GFSDK_Aftermath_DisableGpuCrashDumps",
                      &GFSDK_Aftermath_DisableGpuCrashDumps) ||
        !dl.GetSymbol("GFSDK_Aftermath_EnableGpuCrashDumps",
                      &GFSDK_Aftermath_EnableGpuCrashDumps) ||
        !dl.GetSymbol("GFSDK_Aftermath_GetShaderDebugInfoIdentifier",
                      &GFSDK_Aftermath_GetShaderDebugInfoIdentifier) ||
        !dl.GetSymbol("GFSDK_Aftermath_GetShaderHashSpirv", &GFSDK_Aftermath_GetShaderHashSpirv) ||
        !dl.GetSymbol("GFSDK_Aftermath_GpuCrashDump_CreateDecoder",
                      &GFSDK_Aftermath_GpuCrashDump_CreateDecoder) ||
        !dl.GetSymbol("GFSDK_Aftermath_GpuCrashDump_DestroyDecoder",
                      &GFSDK_Aftermath_GpuCrashDump_DestroyDecoder) ||
        !dl.GetSymbol("GFSDK_Aftermath_GpuCrashDump_GenerateJSON",
                      &GFSDK_Aftermath_GpuCrashDump_GenerateJSON) ||
        !dl.GetSymbol("GFSDK_Aftermath_GpuCrashDump_GetJSON",
                      &GFSDK_Aftermath_GpuCrashDump_GetJSON)) {
        LOG_ERROR(Render_Vulkan, "Failed to load Nsight Aftermath function pointers");
        return;
    }
    dump_dir = Common::FS::GetYuzuPath(Common::FS::YuzuPath::LogDir) / "gpucrash";

    Common::FS::RemoveDirRecursively(dump_dir);
    if (!Common::FS::CreateDir(dump_dir)) {
        LOG_ERROR(Render_Vulkan, "Failed to create Nsight Aftermath dump directory");
        return;
    }
    if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API, GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default, GpuCrashDumpCallback,
            ShaderDebugInfoCallback, CrashDumpDescriptionCallback, this))) {
        LOG_ERROR(Render_Vulkan, "GFSDK_Aftermath_EnableGpuCrashDumps failed");
        return;
    }
    LOG_INFO(Render_Vulkan, "Nsight Aftermath dump directory is \"{}\"",
             Common::FS::PathToUTF8String(dump_dir));
    initialized = true;
}

NsightAftermathTracker::~NsightAftermathTracker() {
    if (initialized) {
        (void)GFSDK_Aftermath_DisableGpuCrashDumps();
    }
}

void NsightAftermathTracker::SaveShader(std::span<const u32> spirv) const {
    if (!initialized) {
        return;
    }
    std::vector<u32> spirv_copy(spirv.begin(), spirv.end());
    GFSDK_Aftermath_SpirvCode shader;
    shader.pData = spirv_copy.data();
    shader.size = static_cast<u32>(spirv_copy.size() * 4);

    std::scoped_lock lock{mutex};

    GFSDK_Aftermath_ShaderHash hash;
    if (!GFSDK_Aftermath_SUCCEED(
            GFSDK_Aftermath_GetShaderHashSpirv(GFSDK_Aftermath_Version_API, &shader, &hash))) {
        LOG_ERROR(Render_Vulkan, "Failed to hash SPIR-V module");
        return;
    }

    const auto shader_file = dump_dir / fmt::format("source_{:016x}.spv", hash.hash);

    Common::FS::IOFile file{shader_file, Common::FS::FileAccessMode::Write,
                            Common::FS::FileType::BinaryFile};
    if (!file.IsOpen()) {
        LOG_ERROR(Render_Vulkan, "Failed to dump SPIR-V module with hash={:016x}", hash.hash);
        return;
    }
    if (file.WriteSpan(spirv) != spirv.size()) {
        LOG_ERROR(Render_Vulkan, "Failed to write SPIR-V module with hash={:016x}", hash.hash);
        return;
    }
}

void NsightAftermathTracker::OnGpuCrashDumpCallback(const void* gpu_crash_dump,
                                                    u32 gpu_crash_dump_size) {
    std::scoped_lock lock{mutex};

    LOG_CRITICAL(Render_Vulkan, "called");

    GFSDK_Aftermath_GpuCrashDump_Decoder decoder;
    if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(
            GFSDK_Aftermath_Version_API, gpu_crash_dump, gpu_crash_dump_size, &decoder))) {
        LOG_ERROR(Render_Vulkan, "Failed to create decoder");
        return;
    }
    SCOPE_EXIT({ GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder); });

    u32 json_size = 0;
    if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
            decoder, GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
            GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE, nullptr, nullptr, nullptr, nullptr,
            this, &json_size))) {
        LOG_ERROR(Render_Vulkan, "Failed to generate JSON");
        return;
    }
    std::vector<char> json(json_size);
    if (!GFSDK_Aftermath_SUCCEED(
            GFSDK_Aftermath_GpuCrashDump_GetJSON(decoder, json_size, json.data()))) {
        LOG_ERROR(Render_Vulkan, "Failed to query JSON");
        return;
    }

    std::filesystem::path base_name = [this] {
        const int id = dump_id++;
        if (id == 0) {
            return dump_dir / "crash.nv-gpudmp";
        } else {
            return dump_dir / fmt::format("crash_{}.nv-gpudmp", id);
        }
    }();

    std::string_view dump_view(static_cast<const char*>(gpu_crash_dump), gpu_crash_dump_size);
    if (Common::FS::WriteStringToFile(base_name, Common::FS::FileType::BinaryFile, dump_view) !=
        gpu_crash_dump_size) {
        LOG_ERROR(Render_Vulkan, "Failed to write dump file");
        return;
    }
    const std::string_view json_view(json.data(), json.size());
    if (Common::FS::WriteStringToFile(base_name.concat(".json"), Common::FS::FileType::TextFile,
                                      json_view) != json.size()) {
        LOG_ERROR(Render_Vulkan, "Failed to write JSON");
        return;
    }
}

void NsightAftermathTracker::OnShaderDebugInfoCallback(const void* shader_debug_info,
                                                       u32 shader_debug_info_size) {
    std::scoped_lock lock{mutex};

    GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier;
    if (!GFSDK_Aftermath_SUCCEED(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(
            GFSDK_Aftermath_Version_API, shader_debug_info, shader_debug_info_size, &identifier))) {
        LOG_ERROR(Render_Vulkan, "GFSDK_Aftermath_GetShaderDebugInfoIdentifier failed");
        return;
    }

    const auto path =
        dump_dir / fmt::format("shader_{:016x}{:016x}.nvdbg", identifier.id[0], identifier.id[1]);
    Common::FS::IOFile file{path, Common::FS::FileAccessMode::Write,
                            Common::FS::FileType::BinaryFile};
    if (!file.IsOpen()) {
        LOG_ERROR(Render_Vulkan, "Failed to create file {}", Common::FS::PathToUTF8String(path));
        return;
    }
    if (file.WriteSpan(std::span(static_cast<const u8*>(shader_debug_info),
                                 shader_debug_info_size)) != shader_debug_info_size) {
        LOG_ERROR(Render_Vulkan, "Failed to write file {}", Common::FS::PathToUTF8String(path));
        return;
    }
}

void NsightAftermathTracker::OnCrashDumpDescriptionCallback(
    PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description) {
    add_description(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "yuzu");
}

void NsightAftermathTracker::GpuCrashDumpCallback(const void* gpu_crash_dump,
                                                  u32 gpu_crash_dump_size, void* user_data) {
    static_cast<NsightAftermathTracker*>(user_data)->OnGpuCrashDumpCallback(gpu_crash_dump,
                                                                            gpu_crash_dump_size);
}

void NsightAftermathTracker::ShaderDebugInfoCallback(const void* shader_debug_info,
                                                     u32 shader_debug_info_size, void* user_data) {
    static_cast<NsightAftermathTracker*>(user_data)->OnShaderDebugInfoCallback(
        shader_debug_info, shader_debug_info_size);
}

void NsightAftermathTracker::CrashDumpDescriptionCallback(
    PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add_description, void* user_data) {
    static_cast<NsightAftermathTracker*>(user_data)->OnCrashDumpDescriptionCallback(
        add_description);
}

} // namespace Vulkan

#endif // HAS_NSIGHT_AFTERMATH
