// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>
#include "common/common_types.h"
#include "core/hardware_properties.h"

namespace Common {
struct PageTable;
}

namespace Kernel {
enum class VMAPermission : u8;
}

namespace Core {
class System;
class CPUInterruptHandler;

using CPUInterrupts = std::array<CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES>;

/// Generic ARMv8 CPU interface
class ARM_Interface : NonCopyable {
public:
    explicit ARM_Interface(System& system_, CPUInterrupts& interrupt_handlers, bool uses_wall_clock)
        : system{system_}, interrupt_handlers{interrupt_handlers}, uses_wall_clock{
                                                                       uses_wall_clock} {}
    virtual ~ARM_Interface() = default;

    struct ThreadContext32 {
        std::array<u32, 16> cpu_registers{};
        std::array<u32, 64> extension_registers{};
        u32 cpsr{};
        u32 fpscr{};
        u32 fpexc{};
        u32 tpidr{};
    };
    // Internally within the kernel, it expects the AArch32 version of the
    // thread context to be 344 bytes in size.
    static_assert(sizeof(ThreadContext32) == 0x150);

    struct ThreadContext64 {
        std::array<u64, 31> cpu_registers{};
        u64 sp{};
        u64 pc{};
        u32 pstate{};
        std::array<u8, 4> padding{};
        std::array<u128, 32> vector_registers{};
        u32 fpcr{};
        u32 fpsr{};
        u64 tpidr{};
    };
    // Internally within the kernel, it expects the AArch64 version of the
    // thread context to be 800 bytes in size.
    static_assert(sizeof(ThreadContext64) == 0x320);

    /// Runs the CPU until an event happens
    virtual void Run() = 0;

    /// Step CPU by one instruction
    virtual void Step() = 0;

    /// Clear all instruction cache
    virtual void ClearInstructionCache() = 0;

    /// Notifies CPU emulation that the current page table has changed.
    ///
    /// @param new_page_table                 The new page table.
    /// @param new_address_space_size_in_bits The new usable size of the address space in bits.
    ///                                       This can be either 32, 36, or 39 on official software.
    ///
    virtual void PageTableChanged(Common::PageTable& new_page_table,
                                  std::size_t new_address_space_size_in_bits) = 0;

    /**
     * Set the Program Counter to an address
     * @param addr Address to set PC to
     */
    virtual void SetPC(u64 addr) = 0;

    /*
     * Get the current Program Counter
     * @return Returns current PC
     */
    virtual u64 GetPC() const = 0;

    /**
     * Get an ARM register
     * @param index Register index
     * @return Returns the value in the register
     */
    virtual u64 GetReg(int index) const = 0;

    /**
     * Set an ARM register
     * @param index Register index
     * @param value Value to set register to
     */
    virtual void SetReg(int index, u64 value) = 0;

    /**
     * Gets the value of a specified vector register.
     *
     * @param index The index of the vector register.
     * @return the value within the vector register.
     */
    virtual u128 GetVectorReg(int index) const = 0;

    /**
     * Sets a given value into a vector register.
     *
     * @param index The index of the vector register.
     * @param value The new value to place in the register.
     */
    virtual void SetVectorReg(int index, u128 value) = 0;

    /**
     * Get the current PSTATE register
     * @return Returns the value of the PSTATE register
     */
    virtual u32 GetPSTATE() const = 0;

    /**
     * Set the current PSTATE register
     * @param pstate Value to set PSTATE to
     */
    virtual void SetPSTATE(u32 pstate) = 0;

    virtual VAddr GetTlsAddress() const = 0;

    virtual void SetTlsAddress(VAddr address) = 0;

    /**
     * Gets the value within the TPIDR_EL0 (read/write software thread ID) register.
     *
     * @return the value within the register.
     */
    virtual u64 GetTPIDR_EL0() const = 0;

    /**
     * Sets a new value within the TPIDR_EL0 (read/write software thread ID) register.
     *
     * @param value The new value to place in the register.
     */
    virtual void SetTPIDR_EL0(u64 value) = 0;

    virtual void ChangeProcessorID(std::size_t new_core_id) = 0;

    virtual void SaveContext(ThreadContext32& ctx) = 0;
    virtual void SaveContext(ThreadContext64& ctx) = 0;
    virtual void LoadContext(const ThreadContext32& ctx) = 0;
    virtual void LoadContext(const ThreadContext64& ctx) = 0;

    /// Clears the exclusive monitor's state.
    virtual void ClearExclusiveState() = 0;

    /// Prepare core for thread reschedule (if needed to correctly handle state)
    virtual void PrepareReschedule() = 0;

    struct BacktraceEntry {
        std::string mod;
        u64 address;
        u64 original_address;
        u64 offset;
        std::string name;
    };

    static std::vector<BacktraceEntry> GetBacktraceFromContext(System& system,
                                                               const ThreadContext64& ctx);

    std::vector<BacktraceEntry> GetBacktrace() const;

    /// fp (= r29) points to the last frame record.
    /// Note that this is the frame record for the *previous* frame, not the current one.
    /// Note we need to subtract 4 from our last read to get the proper address
    /// Frame records are two words long:
    /// fp+0 : pointer to previous frame record
    /// fp+8 : value of lr for frame
    void LogBacktrace() const;

protected:
    /// System context that this ARM interface is running under.
    System& system;
    CPUInterrupts& interrupt_handlers;
    bool uses_wall_clock;
};

} // namespace Core
