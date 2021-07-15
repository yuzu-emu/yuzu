// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/assert.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_client_session.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_server_port.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/result.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sm/sm_controller.h"

namespace Service::SM {

constexpr ResultCode ERR_NOT_INITIALIZED(ErrorModule::SM, 2);
constexpr ResultCode ERR_ALREADY_REGISTERED(ErrorModule::SM, 4);
constexpr ResultCode ERR_INVALID_NAME(ErrorModule::SM, 6);
constexpr ResultCode ERR_SERVICE_NOT_REGISTERED(ErrorModule::SM, 7);

ServiceManager::ServiceManager(Kernel::KernelCore& kernel_) : kernel{kernel_} {}
ServiceManager::~ServiceManager() = default;

void ServiceManager::InvokeControlRequest(Kernel::HLERequestContext& context) {
    controller_interface->InvokeRequest(context);
}

static ResultCode ValidateServiceName(const std::string& name) {
    if (name.empty() || name.size() > 8) {
        LOG_ERROR(Service_SM, "Invalid service name! service={}", name);
        return ERR_INVALID_NAME;
    }
    return ResultSuccess;
}

Kernel::KClientPort& ServiceManager::InterfaceFactory(ServiceManager& self, Core::System& system) {
    self.sm_interface = std::make_shared<SM>(self, system);
    self.controller_interface = std::make_unique<Controller>(system);
    return self.sm_interface->CreatePort();
}

ResultCode ServiceManager::RegisterService(std::string name, u32 max_sessions,
                                           Kernel::SessionRequestHandlerPtr handler) {

    CASCADE_CODE(ValidateServiceName(name));

    if (registered_services.find(name) != registered_services.end()) {
        LOG_ERROR(Service_SM, "Service is already registered! service={}", name);
        return ERR_ALREADY_REGISTERED;
    }

    registered_services.emplace(std::move(name), handler);

    return ResultSuccess;
}

ResultCode ServiceManager::UnregisterService(const std::string& name) {
    CASCADE_CODE(ValidateServiceName(name));

    const auto iter = registered_services.find(name);
    if (iter == registered_services.end()) {
        LOG_ERROR(Service_SM, "Server is not registered! service={}", name);
        return ERR_SERVICE_NOT_REGISTERED;
    }

    registered_services.erase(iter);
    return ResultSuccess;
}

ResultVal<Kernel::KPort*> ServiceManager::GetServicePort(const std::string& name) {
    CASCADE_CODE(ValidateServiceName(name));
    auto it = registered_services.find(name);
    if (it == registered_services.end()) {
        LOG_ERROR(Service_SM, "Server is not registered! service={}", name);
        return ERR_SERVICE_NOT_REGISTERED;
    }

    auto* port = Kernel::KPort::Create(kernel);
    port->Initialize(ServerSessionCountMax, false, name);
    auto handler = it->second;
    port->GetServerPort().SetSessionHandler(std::move(handler));

    return MakeResult(port);
}

/**
 * SM::Initialize service function
 *  Inputs:
 *      0: 0x00000000
 *  Outputs:
 *      0: ResultCode
 */
void SM::Initialize(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SM, "called");

    is_initialized = true;

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SM::GetService(Kernel::HLERequestContext& ctx) {
    auto result = GetServiceImpl(ctx);
    if (result.Succeeded()) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
        rb.Push(result.Code());
        rb.PushMoveObjects(result.Unwrap());
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result.Code());
    }
}

void SM::GetServiceTipc(Kernel::HLERequestContext& ctx) {
    auto result = GetServiceImpl(ctx);
    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(result.Code());
    rb.PushMoveObjects(result.Succeeded() ? result.Unwrap() : nullptr);
}

static std::string PopServiceName(IPC::RequestParser& rp) {
    auto name_buf = rp.PopRaw<std::array<char, 8>>();
    std::string result;
    for (const auto& c : name_buf) {
        if (c >= ' ' && c <= '~') {
            result.push_back(c);
        }
    }
    return result;
}

ResultVal<Kernel::KClientSession*> SM::GetServiceImpl(Kernel::HLERequestContext& ctx) {
    if (!is_initialized) {
        return ERR_NOT_INITIALIZED;
    }

    IPC::RequestParser rp{ctx};
    std::string name(PopServiceName(rp));

    // Find the named port.
    auto port_result = service_manager.GetServicePort(name);
    if (port_result.Failed()) {
        LOG_ERROR(Service_SM, "called service={} -> error 0x{:08X}", name, port_result.Code().raw);
        return port_result.Code();
    }
    auto& port = port_result.Unwrap();
    SCOPE_EXIT({ port->GetClientPort().Close(); });

    server_ports.emplace_back(&port->GetServerPort());

    // Create a new session.
    Kernel::KClientSession* session{};
    if (const auto result = port->GetClientPort().CreateSession(std::addressof(session));
        result.IsError()) {
        LOG_ERROR(Service_SM, "called service={} -> error 0x{:08X}", name, result.raw);
        return result;
    }

    LOG_DEBUG(Service_SM, "called service={} -> session={}", name, session->GetId());

    return MakeResult(session);
}

void SM::RegisterService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    std::string name(PopServiceName(rp));

    const auto is_light = static_cast<bool>(rp.PopRaw<u32>());
    const auto max_session_count = rp.PopRaw<u32>();

    LOG_DEBUG(Service_SM, "called with name={}, max_session_count={}, is_light={}", name,
              max_session_count, is_light);

    if (const auto result = service_manager.RegisterService(name, max_session_count, nullptr);
        result.IsError()) {
        LOG_ERROR(Service_SM, "failed to register service with error_code={:08X}", result.raw);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    auto* port = Kernel::KPort::Create(kernel);
    port->Initialize(ServerSessionCountMax, is_light, name);
    SCOPE_EXIT({ port->GetClientPort().Close(); });

    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(ResultSuccess);
    rb.PushMoveObjects(port->GetServerPort());
}

void SM::UnregisterService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    std::string name(PopServiceName(rp));

    LOG_DEBUG(Service_SM, "called with name={}", name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(service_manager.UnregisterService(name));
}

SM::SM(ServiceManager& service_manager_, Core::System& system_)
    : ServiceFramework{system_, "sm:", 4},
      service_manager{service_manager_}, kernel{system_.Kernel()} {
    RegisterHandlers({
        {0, &SM::Initialize, "Initialize"},
        {1, &SM::GetService, "GetService"},
        {2, &SM::RegisterService, "RegisterService"},
        {3, &SM::UnregisterService, "UnregisterService"},
        {4, nullptr, "DetachClient"},
    });
    RegisterHandlersTipc({
        {0, &SM::Initialize, "Initialize"},
        {1, &SM::GetServiceTipc, "GetService"},
        {2, &SM::RegisterService, "RegisterService"},
        {3, &SM::UnregisterService, "UnregisterService"},
        {4, nullptr, "DetachClient"},
    });
}

SM::~SM() {
    for (auto& server_port : server_ports) {
        server_port->Close();
    }
}

} // namespace Service::SM
