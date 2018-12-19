// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>

#include "common/common_types.h"

union ResultCode;

namespace Kernel {

class VMManager;

/// Handles kernel capability descriptors that are provided by
/// application metadata. These descriptors provide information
/// that alters certain parameters for kernel process instance
/// that will run said application (or applet).
///
/// Capabilities are a sequence of flag descriptors, that indicate various
/// configurations and constraints for a particular process.
///
/// Flag types are indicated by a sequence of set low bits. E.g. the
/// types are indicated with the low bits as follows (where x indicates "don't care"):
///
/// - Priority and core mask   : 0bxxxxxxxxxxxx0111
/// - Allowed service call mask: 0bxxxxxxxxxxx01111
/// - Map physical memory      : 0bxxxxxxxxx0111111
/// - Map IO memory            : 0bxxxxxxxx01111111
/// - Interrupts               : 0bxxxx011111111111
/// - Application type         : 0bxx01111111111111
/// - Kernel version           : 0bx011111111111111
/// - Handle table size        : 0b0111111111111111
/// - Debugger flags           : 0b1111111111111111
///
/// These are essentially a bit offset subtracted by 1 to create a mask.
/// e.g. The first entry in the above list is simply bit 3 (value 8 -> 0b1000)
///      subtracted by one (7 -> 0b0111)
///
/// An example of a bit layout (using the map physical layout):
/// <example>
///   The MapPhysical type indicates a sequence entry pair of:
///
///   [initial, memory_flags], where:
///
///   initial:
///     bits:
///       7-24: Starting page to map memory at.
///       25  : Indicates if the memory should be mapped as read only.
///
///   memory_flags:
///     bits:
///       7-20 : Number of pages to map
///       21-25: Seems to be reserved (still checked against though)
///       26   : Whether or not the memory being mapped is IO memory, or physical memory
/// </example>
///
class ProcessCapabilities {
public:
    using InterruptCapabilities = std::bitset<1024>;
    using SyscallCapabilities = std::bitset<128>;

    ProcessCapabilities() = default;
    ProcessCapabilities(const ProcessCapabilities&) = delete;
    ProcessCapabilities(ProcessCapabilities&&) = default;

    ProcessCapabilities& operator=(const ProcessCapabilities&) = delete;
    ProcessCapabilities& operator=(ProcessCapabilities&&) = default;

    /// Initializes this process capabilities instance for a kernel process.
    ///
    /// @param capabilities     The capabilities to parse
    /// @param num_capabilities The number of capabilities to parse.
    /// @param vm_manager       The memory manager to use for handling any mapping-related
    ///                         operations (such as mapping IO memory, etc).
    ///
    /// @returns RESULT_SUCCESS if this capabilities instance was able to be initialized,
    ///          otherwise, an error code upon failure.
    ///
    ResultCode InitializeForKernelProcess(const u32* capabilities, std::size_t num_capabilities,
                                          VMManager& vm_manager);

    /// Initializes this process capabilities instance for a userland process.
    ///
    /// @param capabilities     The capabilities to parse.
    /// @param num_capabilities The total number of capabilities to parse.
    /// @param vm_manager       The memory manager to use for handling any mapping-related
    ///                         operations (such as mapping IO memory, etc).
    ///
    /// @returns RESULT_SUCCESS if this capabilities instance was able to be initialized,
    ///          otherwise, an error code upon failure.
    ///
    ResultCode InitializeForUserProcess(const u32* capabilities, std::size_t num_capabilities,
                                        VMManager& vm_manager);

    /// Initializes this process capabilities instance for a process that does not
    /// have any metadata to parse.
    ///
    /// This is necessary, as we allow running raw executables, and the internal
    /// kernel process capabilities also determine what CPU cores the process is
    /// allowed to run on, and what priorities are allowed for  threads. It also
    /// determines the max handle table size, what the program type is, whether or
    /// not the process can be debugged, or whether it's possible for a process to
    /// forcibly debug another process.
    ///
    /// Given the above, this essentially enables all capabilities across the board
    /// for the process. It allows the process to:
    ///
    /// - Run on any core
    /// - Use any thread priority
    /// - Use the maximum amount of handles a process is allowed to.
    /// - Be debuggable
    /// - Forcibly debug other processes.
    ///
    /// Note that this is not a behavior that the kernel allows a process to do via
    /// a single function like this. This is yuzu-specific behavior to handle
    /// executables with no capability descriptors whatsoever to derive behavior from.
    /// It being yuzu-specific is why this is also not the default behavior and not
    /// done by default in the constructor.
    ///
    void InitializeForMetadatalessProcess();

private:
    /// Attempts to parse a given sequence of capability descriptors.
    ///
    /// @param capabilities     The sequence of capability descriptors to parse.
    /// @param num_capabilities The number of descriptors within the given sequence.
    /// @param vm_manager       The memory manager that will perform any memory
    ///                         mapping if necessary.
    ///
    /// @return RESULT_SUCCESS if no errors occur, otherwise an error code.
    ///
    ResultCode ParseCapabilities(const u32* capabilities, std::size_t num_capabilities,
                                 VMManager& vm_manager);

    /// Attempts to parse a capability descriptor that is only represented by a
    /// single flag set.
    ///
    /// @param set_flags    Running set of flags that are used to catch
    ///                     flags being initialized more than once when they shouldn't be.
    /// @param set_svc_bits Running set of bits representing the allowed supervisor calls mask.
    /// @param flag         The flag to attempt to parse.
    /// @param vm_manager   The memory manager that will perform any memory
    ///                     mapping if necessary.
    ///
    /// @return RESULT_SUCCESS if no errors occurred, otherwise an error code.
    ///
    ResultCode ParseSingleFlagCapability(u32& set_flags, u32& set_svc_bits, u32 flag,
                                         VMManager& vm_manager);

    /// Clears the internal state of this process capability instance. Necessary,
    /// to have a sane starting point due to us allowing running executables without
    /// configuration metadata. We assume a process is not going to have metadata,
    /// and if it turns out that the process does, in fact, have metadata, then
    /// we attempt to parse it. Thus, we need this to reset data members back to
    /// a good state.
    ///
    /// DO NOT ever make this a public member function. This isn't an invariant
    /// anything external should depend upon (and if anything comes to rely on it,
    /// you should immediately be questioning the design of that thing, not this
    /// class. If the kernel itself can run without depending on behavior like that,
    /// then so can yuzu).
    ///
    void Clear();

    /// Handles flags related to the priority and core number capability flags.
    ResultCode HandlePriorityCoreNumFlags(u32 flags);

    /// Handles flags related to determining the allowable SVC mask.
    ResultCode HandleSyscallFlags(u32& set_svc_bits, u32 flags);

    /// Handles flags related to mapping physical memory pages.
    ResultCode HandleMapPhysicalFlags(u32 flags, u32 size_flags, VMManager& vm_manager);

    /// Handles flags related to mapping IO pages.
    ResultCode HandleMapIOFlags(u32 flags, VMManager& vm_manager);

    /// Handles flags related to the interrupt capability flags.
    ResultCode HandleInterruptFlags(u32 flags);

    /// Handles flags related to the program type.
    ResultCode HandleProgramTypeFlags(u32 flags);

    /// Handles flags related to the handle table size.
    ResultCode HandleHandleTableFlags(u32 flags);

    /// Handles flags related to the kernel version capability flags.
    ResultCode HandleKernelVersionFlags(u32 flags);

    /// Handles flags related to debug-specific capabilities.
    ResultCode HandleDebugFlags(u32 flags);

    SyscallCapabilities svc_capabilities;
    InterruptCapabilities interrupt_capabilities;

    u64 core_mask = 0;
    u64 priority_mask = 0;

    u32 handle_table_size = 0;
    u32 kernel_version = 0;
    u32 program_type = 0;

    bool is_debuggable = false;
    bool can_force_debug = false;
};

} // namespace Kernel
