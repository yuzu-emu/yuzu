// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/service/kernel_helpers.h"

namespace Service::KernelHelpers {

ServiceContext::ServiceContext(Core::System& system_, std::string name_)
    : kernel(system_.Kernel()) {
    process = Kernel::KProcess::Create(kernel);
    ASSERT(Kernel::KProcess::Initialize(process, system_, std::move(name_),
                                        Kernel::KProcess::ProcessType::Userland)
               .IsSuccess());
}

ServiceContext::~ServiceContext() {
    process->Close();
    process = nullptr;
}

Kernel::KEvent* ServiceContext::CreateEvent(std::string&& name) {
    // Reserve a new event from the process resource limit
    Kernel::KScopedResourceReservation event_reservation(process,
                                                         Kernel::LimitableResource::Events);
    if (!event_reservation.Succeeded()) {
        LOG_CRITICAL(Service, "Resource limit reached!");
        return {};
    }

    // Create a new event.
    auto* event = Kernel::KEvent::Create(kernel);
    if (!event) {
        LOG_CRITICAL(Service, "Unable to create event!");
        return {};
    }

    // Initialize the event.
    event->Initialize(std::move(name));

    // Commit the thread reservation.
    event_reservation.Commit();

    // Register the event.
    Kernel::KEvent::Register(kernel, event);

    return event;
}

void ServiceContext::CloseEvent(Kernel::KEvent* event) {
    event->GetReadableEvent().Close();
    event->GetWritableEvent().Close();
}

} // namespace Service::KernelHelpers
