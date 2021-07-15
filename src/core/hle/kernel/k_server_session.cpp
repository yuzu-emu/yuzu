// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include <utility>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/core_timing.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_handle_table.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_server_port.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/memory.h"

namespace Kernel {

KServerSession::KServerSession(KernelCore& kernel_) : KSynchronizationObject{kernel_} {}

KServerSession::~KServerSession() {
    // Ensure that the global list tracking server sessions does not hold on to a reference.
    kernel.UnregisterServerSession(this);
}

void KServerSession::Initialize(KSession* parent_session_, std::string&& name_,
                                std::shared_ptr<SessionRequestManager> manager_) {
    // Set member variables.
    parent = parent_session_;
    name = std::move(name_);

    if (manager_) {
        manager = manager_;
    } else {
        manager = std::make_shared<SessionRequestManager>(kernel);
    }
}

void KServerSession::Destroy() {
    parent->OnServerClosed();

    parent->Close();
}

void KServerSession::OnClientClosed() {
    if (manager->HasSessionHandler()) {
        manager->SessionHandler().ClientDisconnected(this);
    }
}

bool KServerSession::IsSignaled() const {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    // If the client is closed, we're always signaled.
    if (parent->IsClientClosed()) {
        return true;
    }

    // Otherwise, we're signaled if we have a request and aren't handling one.
    return false;
}

void KServerSession::AppendDomainHandler(SessionRequestHandlerPtr handler) {
    manager->AppendDomainHandler(std::move(handler));
}

std::size_t KServerSession::NumDomainRequestHandlers() const {
    return manager->DomainHandlerCount();
}

ResultCode KServerSession::HandleDomainSyncRequest(Kernel::HLERequestContext& context) {
    if (!context.HasDomainMessageHeader()) {
        return ResultSuccess;
    }

    // Set domain handlers in HLE context, used for domain objects (IPC interfaces) as inputs
    context.SetSessionRequestManager(manager);

    // If there is a DomainMessageHeader, then this is CommandType "Request"
    const auto& domain_message_header = context.GetDomainMessageHeader();
    const u32 object_id{domain_message_header.object_id};
    switch (domain_message_header.command) {
    case IPC::DomainMessageHeader::CommandType::SendMessage:
        if (object_id > manager->DomainHandlerCount()) {
            LOG_CRITICAL(IPC,
                         "object_id {} is too big! This probably means a recent service call "
                         "to {} needed to return a new interface!",
                         object_id, name);
            UNREACHABLE();
            return ResultSuccess; // Ignore error if asserts are off
        }
        return manager->DomainHandler(object_id - 1)->HandleSyncRequest(*this, context);

    case IPC::DomainMessageHeader::CommandType::CloseVirtualHandle: {
        LOG_DEBUG(IPC, "CloseVirtualHandle, object_id=0x{:08X}", object_id);

        manager->CloseDomainHandler(object_id - 1);

        IPC::ResponseBuilder rb{context, 2};
        rb.Push(ResultSuccess);
        return ResultSuccess;
    }
    }

    LOG_CRITICAL(IPC, "Unknown domain command={}", domain_message_header.command.Value());
    ASSERT(false);
    return ResultSuccess;
}

ResultCode KServerSession::QueueSyncRequest(KThread* thread, Core::Memory::Memory& memory) {
    u32* cmd_buf{reinterpret_cast<u32*>(memory.GetPointer(thread->GetTLSAddress()))};
    auto context = std::make_shared<HLERequestContext>(kernel, memory, this, thread);

    context->PopulateFromIncomingCommandBuffer(kernel.CurrentProcess()->GetHandleTable(), cmd_buf);

    // In the event that something fails here, stub a result to prevent the game from crashing.
    // This is a work-around in the event that somehow we process a service request after the
    // session has been closed by the game. This has been observed to happen rarely in Pokemon
    // Sword/Shield and is likely a result of us using host threads/scheduling for services.
    // TODO(bunnei): Find a better solution here.
    auto error_guard = SCOPE_GUARD({ CompleteSyncRequest(*context); });

    // Ensure we have a session request handler
    if (manager->HasSessionRequestHandler(*context)) {
        if (auto strong_ptr = manager->GetServiceThread().lock()) {
            strong_ptr->QueueSyncRequest(*parent, std::move(context));

            // We succeeded.
            error_guard.Cancel();
        } else {
            ASSERT_MSG(false, "strong_ptr is nullptr!");
        }
    } else {
        ASSERT_MSG(false, "handler is invalid!");
    }

    return ResultSuccess;
}

ResultCode KServerSession::CompleteSyncRequest(HLERequestContext& context) {
    ResultCode result = ResultSuccess;

    // If the session has been converted to a domain, handle the domain request
    if (manager->HasSessionRequestHandler(context)) {
        if (IsDomain() && context.HasDomainMessageHeader()) {
            result = HandleDomainSyncRequest(context);
            // If there is no domain header, the regular session handler is used
        } else if (manager->HasSessionHandler()) {
            // If this ServerSession has an associated HLE handler, forward the request to it.
            result = manager->SessionHandler().HandleSyncRequest(*this, context);
        }
    } else {
        ASSERT_MSG(false, "Session handler is invalid, stubbing response!");
        IPC::ResponseBuilder rb(context, 2);
        rb.Push(ResultSuccess);
    }

    if (convert_to_domain) {
        ASSERT_MSG(!IsDomain(), "ServerSession is already a domain instance.");
        manager->ConvertToDomain();
        convert_to_domain = false;
    }

    // Some service requests require the thread to block
    {
        KScopedSchedulerLock lock(kernel);
        if (!context.IsThreadWaiting()) {
            context.GetThread().Wakeup();
            context.GetThread().SetSyncedObject(nullptr, result);
        }
    }

    return result;
}

ResultCode KServerSession::HandleSyncRequest(KThread* thread, Core::Memory::Memory& memory,
                                             Core::Timing::CoreTiming& core_timing) {
    return QueueSyncRequest(thread, memory);
}

} // namespace Kernel
