// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <fmt/format.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_server_port.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/acc/acc.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/aoc/aoc_u.h"
#include "core/hle/service/apm/apm.h"
#include "core/hle/service/audio/audio.h"
#include "core/hle/service/bcat/bcat_module.h"
#include "core/hle/service/bpc/bpc.h"
#include "core/hle/service/btdrv/btdrv.h"
#include "core/hle/service/btm/btm.h"
#include "core/hle/service/caps/caps.h"
#include "core/hle/service/erpt/erpt.h"
#include "core/hle/service/es/es.h"
#include "core/hle/service/eupld/eupld.h"
#include "core/hle/service/fatal/fatal.h"
#include "core/hle/service/fgm/fgm.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/friend/friend.h"
#include "core/hle/service/glue/glue.h"
#include "core/hle/service/grc/grc.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/lbl/lbl.h"
#include "core/hle/service/ldn/ldn.h"
#include "core/hle/service/ldr/ldr.h"
#include "core/hle/service/lm/lm.h"
#include "core/hle/service/mig/mig.h"
#include "core/hle/service/mii/mii.h"
#include "core/hle/service/mm/mm_u.h"
#include "core/hle/service/ncm/ncm.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfp/nfp.h"
#include "core/hle/service/nifm/nifm.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/npns/npns.h"
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvflinger/nvflinger.h"
#include "core/hle/service/olsc/olsc.h"
#include "core/hle/service/pcie/pcie.h"
#include "core/hle/service/pctl/pctl_module.h"
#include "core/hle/service/pcv/pcv.h"
#include "core/hle/service/pm/pm.h"
#include "core/hle/service/prepo/prepo.h"
#include "core/hle/service/psc/psc.h"
#include "core/hle/service/ptm/psm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/set/settings.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sockets/sockets.h"
#include "core/hle/service/spl/spl_module.h"
#include "core/hle/service/ssl/ssl.h"
#include "core/hle/service/time/time.h"
#include "core/hle/service/usb/usb.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/wlan/wlan.h"
#include "core/reporter.h"

namespace Service {

/**
 * Creates a function string for logging, complete with the name (or header code, depending
 * on what's passed in) the port name, and all the cmd_buff arguments.
 */
[[maybe_unused]] static std::string MakeFunctionString(std::string_view name,
                                                       std::string_view port_name,
                                                       const u32* cmd_buff) {
    // Number of params == bits 0-5 + bits 6-11
    int num_params = (cmd_buff[0] & 0x3F) + ((cmd_buff[0] >> 6) & 0x3F);

    std::string function_string = fmt::format("function '{}': port={}", name, port_name);
    for (int i = 1; i <= num_params; ++i) {
        function_string += fmt::format(", cmd_buff[{}]=0x{:X}", i, cmd_buff[i]);
    }
    return function_string;
}

ServiceFrameworkBase::ServiceFrameworkBase(Core::System& system_, const char* service_name_,
                                           u32 max_sessions_, InvokerFn* handler_invoker_)
    : SessionRequestHandler(system_.Kernel(), service_name_), system{system_},
      service_name{service_name_}, max_sessions{max_sessions_}, handler_invoker{handler_invoker_} {}

ServiceFrameworkBase::~ServiceFrameworkBase() {
    // Wait for other threads to release access before destroying
    const auto guard = LockService();
}

void ServiceFrameworkBase::InstallAsService(SM::ServiceManager& service_manager) {
    const auto guard = LockService();

    ASSERT(!service_registered);

    service_manager.RegisterService(service_name, max_sessions, shared_from_this());
    service_registered = true;
}

Kernel::KClientPort& ServiceFrameworkBase::CreatePort() {
    const auto guard = LockService();

    ASSERT(!service_registered);

    auto* port = Kernel::KPort::Create(kernel);
    port->Initialize(max_sessions, false, service_name);
    port->GetServerPort().SetSessionHandler(shared_from_this());

    service_registered = true;

    return port->GetClientPort();
}

void ServiceFrameworkBase::RegisterHandlersBase(const FunctionInfoBase* functions, std::size_t n) {
    handlers.reserve(handlers.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to insert at the end
        handlers.emplace_hint(handlers.cend(), functions[i].expected_header, functions[i]);
    }
}

void ServiceFrameworkBase::RegisterHandlersBaseTipc(const FunctionInfoBase* functions,
                                                    std::size_t n) {
    handlers_tipc.reserve(handlers_tipc.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        // Usually this array is sorted by id already, so hint to insert at the end
        handlers_tipc.emplace_hint(handlers_tipc.cend(), functions[i].expected_header,
                                   functions[i]);
    }
}

void ServiceFrameworkBase::ReportUnimplementedFunction(Kernel::HLERequestContext& ctx,
                                                       const FunctionInfoBase* info) {
    auto cmd_buf = ctx.CommandBuffer();
    std::string function_name = info == nullptr ? fmt::format("{}", ctx.GetCommand()) : info->name;

    fmt::memory_buffer buf;
    fmt::format_to(std::back_inserter(buf), "function '{}': port='{}' cmd_buf={{[0]=0x{:X}",
                   function_name, service_name, cmd_buf[0]);
    for (int i = 1; i <= 8; ++i) {
        fmt::format_to(std::back_inserter(buf), ", [{}]=0x{:X}", i, cmd_buf[i]);
    }
    buf.push_back('}');

    system.GetReporter().SaveUnimplementedFunctionReport(ctx, ctx.GetCommand(), function_name,
                                                         service_name);
    UNIMPLEMENTED_MSG("Unknown / unimplemented {}", fmt::to_string(buf));
    if (Settings::values.use_auto_stub) {
        LOG_WARNING(Service, "Using auto stub fallback!");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }
}

void ServiceFrameworkBase::InvokeRequest(Kernel::HLERequestContext& ctx) {
    auto itr = handlers.find(ctx.GetCommand());
    const FunctionInfoBase* info = itr == handlers.end() ? nullptr : &itr->second;
    if (info == nullptr || info->handler_callback == nullptr) {
        return ReportUnimplementedFunction(ctx, info);
    }

    LOG_TRACE(Service, "{}", MakeFunctionString(info->name, GetServiceName(), ctx.CommandBuffer()));
    handler_invoker(this, info->handler_callback, ctx);
}

void ServiceFrameworkBase::InvokeRequestTipc(Kernel::HLERequestContext& ctx) {
    boost::container::flat_map<u32, FunctionInfoBase>::iterator itr;

    itr = handlers_tipc.find(ctx.GetCommand());

    const FunctionInfoBase* info = itr == handlers_tipc.end() ? nullptr : &itr->second;
    if (info == nullptr || info->handler_callback == nullptr) {
        return ReportUnimplementedFunction(ctx, info);
    }

    LOG_TRACE(Service, "{}", MakeFunctionString(info->name, GetServiceName(), ctx.CommandBuffer()));
    handler_invoker(this, info->handler_callback, ctx);
}

ResultCode ServiceFrameworkBase::HandleSyncRequest(Kernel::KServerSession& session,
                                                   Kernel::HLERequestContext& ctx) {
    const auto guard = LockService();

    switch (ctx.GetCommandType()) {
    case IPC::CommandType::Close:
    case IPC::CommandType::TIPC_Close: {
        session.Close();
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
        return IPC::ERR_REMOTE_PROCESS_DEAD;
    }
    case IPC::CommandType::ControlWithContext:
    case IPC::CommandType::Control: {
        system.ServiceManager().InvokeControlRequest(ctx);
        break;
    }
    case IPC::CommandType::RequestWithContext:
    case IPC::CommandType::Request: {
        InvokeRequest(ctx);
        break;
    }
    default:
        if (ctx.IsTipc()) {
            InvokeRequestTipc(ctx);
            break;
        }

        UNIMPLEMENTED_MSG("command_type={}", ctx.GetCommandType());
    }

    // If emulation was shutdown, we are closing service threads, do not write the response back to
    // memory that may be shutting down as well.
    if (system.IsPoweredOn()) {
        ctx.WriteToOutgoingCommandBuffer(ctx.GetThread());
    }

    return ResultSuccess;
}

/// Initialize Services
Services::Services(std::shared_ptr<SM::ServiceManager>& sm, Core::System& system)
    : nv_flinger{std::make_unique<NVFlinger::NVFlinger>(system)} {

    // NVFlinger needs to be accessed by several services like Vi and AppletOE so we instantiate it
    // here and pass it into the respective InstallInterfaces functions.

    system.GetFileSystemController().CreateFactories(*system.GetFilesystem(), false);

    system.Kernel().RegisterNamedService("sm:", SM::ServiceManager::InterfaceFactory);

    Account::InstallInterfaces(system);
    AM::InstallInterfaces(*sm, *nv_flinger, system);
    AOC::InstallInterfaces(*sm, system);
    APM::InstallInterfaces(system);
    Audio::InstallInterfaces(*sm, system);
    BCAT::InstallInterfaces(system);
    BPC::InstallInterfaces(*sm, system);
    BtDrv::InstallInterfaces(*sm, system);
    BTM::InstallInterfaces(*sm, system);
    Capture::InstallInterfaces(*sm, system);
    ERPT::InstallInterfaces(*sm, system);
    ES::InstallInterfaces(*sm, system);
    EUPLD::InstallInterfaces(*sm, system);
    Fatal::InstallInterfaces(*sm, system);
    FGM::InstallInterfaces(*sm, system);
    FileSystem::InstallInterfaces(system);
    Friend::InstallInterfaces(*sm, system);
    Glue::InstallInterfaces(system);
    GRC::InstallInterfaces(*sm, system);
    HID::InstallInterfaces(*sm, system);
    LBL::InstallInterfaces(*sm, system);
    LDN::InstallInterfaces(*sm, system);
    LDR::InstallInterfaces(*sm, system);
    LM::InstallInterfaces(system);
    Migration::InstallInterfaces(*sm, system);
    Mii::InstallInterfaces(*sm, system);
    MM::InstallInterfaces(*sm, system);
    NCM::InstallInterfaces(*sm, system);
    NFC::InstallInterfaces(*sm, system);
    NFP::InstallInterfaces(*sm, system);
    NIFM::InstallInterfaces(*sm, system);
    NIM::InstallInterfaces(*sm, system);
    NPNS::InstallInterfaces(*sm, system);
    NS::InstallInterfaces(*sm, system);
    Nvidia::InstallInterfaces(*sm, *nv_flinger, system);
    OLSC::InstallInterfaces(*sm, system);
    PCIe::InstallInterfaces(*sm, system);
    PCTL::InstallInterfaces(*sm, system);
    PCV::InstallInterfaces(*sm, system);
    PlayReport::InstallInterfaces(*sm, system);
    PM::InstallInterfaces(system);
    PSC::InstallInterfaces(*sm, system);
    PSM::InstallInterfaces(*sm, system);
    Set::InstallInterfaces(*sm, system);
    Sockets::InstallInterfaces(*sm, system);
    SPL::InstallInterfaces(*sm, system);
    SSL::InstallInterfaces(*sm, system);
    Time::InstallInterfaces(system);
    USB::InstallInterfaces(*sm, system);
    VI::InstallInterfaces(*sm, system, *nv_flinger);
    WLAN::InstallInterfaces(*sm, system);
}

Services::~Services() = default;

} // namespace Service
