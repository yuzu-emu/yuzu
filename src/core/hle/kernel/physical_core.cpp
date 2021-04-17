// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/spin_lock.h"
#include "core/arm/cpu_interrupt_handler.h"
#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#include "core/core.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"

namespace Kernel {

PhysicalCore::PhysicalCore(std::size_t core_index, Core::System& system,
                           Kernel::KScheduler& scheduler, Core::CPUInterrupts& interrupts)
    : core_index{core_index}, system{system}, scheduler{scheduler},
      interrupts{interrupts}, guard{std::make_unique<Common::SpinLock>()} {}

PhysicalCore::~PhysicalCore() = default;

void PhysicalCore::Initialize([[maybe_unused]] bool is_64_bit) {
#if defined(ARCHITECTURE_x86_64) || defined(ARCHITECTURE_ARM64)
    auto& kernel = system.Kernel();
    if (is_64_bit) {
        arm_interface = std::make_unique<Core::ARM_Dynarmic_64>(
            system, interrupts, kernel.IsMulticore(), kernel.GetExclusiveMonitor(), core_index);
    } else {
        arm_interface = std::make_unique<Core::ARM_Dynarmic_32>(
            system, interrupts, kernel.IsMulticore(), kernel.GetExclusiveMonitor(), core_index);
    }
#else
#error Platform not supported yet.
#endif
}

void PhysicalCore::Run() {
    arm_interface->Run();
}

void PhysicalCore::Idle() {
    interrupts[core_index].AwaitInterrupt();
}

bool PhysicalCore::IsInterrupted() const {
    return interrupts[core_index].IsInterrupted();
}

void PhysicalCore::Interrupt() {
    guard->lock();
    interrupts[core_index].SetInterrupt(true);
    guard->unlock();
}

void PhysicalCore::ClearInterrupt() {
    guard->lock();
    interrupts[core_index].SetInterrupt(false);
    guard->unlock();
}

} // namespace Kernel
