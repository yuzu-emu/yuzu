// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "core/arm/cpu_interrupt_handler.h"
#include "core/hardware_properties.h"
#include "core/hle/kernel/k_auto_object.h"
#include "core/hle/kernel/k_slab_heap.h"
#include "core/hle/kernel/memory_types.h"
#include "core/hle/kernel/svc_common.h"

namespace Core {
class CPUInterruptHandler;
class ExclusiveMonitor;
class System;
} // namespace Core

namespace Core::Timing {
class CoreTiming;
struct EventType;
} // namespace Core::Timing

namespace Service::SM {
class ServiceManager;
}

namespace Kernel {

class KClientPort;
class GlobalSchedulerContext;
class KAutoObjectWithListContainer;
class KClientSession;
class KEvent;
class KHandleTable;
class KLinkedListNode;
class KMemoryManager;
class KPort;
class KProcess;
class KResourceLimit;
class KScheduler;
class KServerSession;
class KSession;
class KSharedMemory;
class KThread;
class KTransferMemory;
class KWritableEvent;
class PhysicalCore;
class ServiceThread;
class Synchronization;
class TimeManager;

using ServiceInterfaceFactory =
    std::function<KClientPort&(Service::SM::ServiceManager&, Core::System&)>;

namespace Init {
struct KSlabResourceCounts;
}

template <typename T>
class KSlabHeap;

using EmuThreadHandle = uintptr_t;
constexpr EmuThreadHandle EmuThreadHandleInvalid{};
constexpr EmuThreadHandle EmuThreadHandleReserved{1ULL << 63};

/// Represents a single instance of the kernel.
class KernelCore {
private:
    using NamedPortTable = std::unordered_map<std::string, KClientPort*>;

public:
    /// Constructs an instance of the kernel using the given System
    /// instance as a context for any necessary system-related state,
    /// such as threads, CPU core state, etc.
    ///
    /// @post After execution of the constructor, the provided System
    ///       object *must* outlive the kernel instance itself.
    ///
    explicit KernelCore(Core::System& system);
    ~KernelCore();

    KernelCore(const KernelCore&) = delete;
    KernelCore& operator=(const KernelCore&) = delete;

    KernelCore(KernelCore&&) = delete;
    KernelCore& operator=(KernelCore&&) = delete;

    /// Sets if emulation is multicore or single core, must be set before Initialize
    void SetMulticore(bool is_multicore);

    /// Resets the kernel to a clean slate for use.
    void Initialize();

    /// Initializes the CPU cores.
    void InitializeCores();

    /// Clears all resources in use by the kernel instance.
    void Shutdown();

    /// Retrieves a shared pointer to the system resource limit instance.
    const KResourceLimit* GetSystemResourceLimit() const;

    /// Retrieves a shared pointer to the system resource limit instance.
    KResourceLimit* GetSystemResourceLimit();

    /// Retrieves a shared pointer to a Thread instance within the thread wakeup handle table.
    KScopedAutoObject<KThread> RetrieveThreadFromGlobalHandleTable(Handle handle) const;

    /// Adds the given shared pointer to an internal list of active processes.
    void AppendNewProcess(KProcess* process);

    /// Makes the given process the new current process.
    void MakeCurrentProcess(KProcess* process);

    /// Retrieves a pointer to the current process.
    KProcess* CurrentProcess();

    /// Retrieves a const pointer to the current process.
    const KProcess* CurrentProcess() const;

    /// Retrieves the list of processes.
    const std::vector<KProcess*>& GetProcessList() const;

    /// Gets the sole instance of the global scheduler
    Kernel::GlobalSchedulerContext& GlobalSchedulerContext();

    /// Gets the sole instance of the global scheduler
    const Kernel::GlobalSchedulerContext& GlobalSchedulerContext() const;

    /// Gets the sole instance of the Scheduler assoviated with cpu core 'id'
    Kernel::KScheduler& Scheduler(std::size_t id);

    /// Gets the sole instance of the Scheduler assoviated with cpu core 'id'
    const Kernel::KScheduler& Scheduler(std::size_t id) const;

    /// Gets the an instance of the respective physical CPU core.
    Kernel::PhysicalCore& PhysicalCore(std::size_t id);

    /// Gets the an instance of the respective physical CPU core.
    const Kernel::PhysicalCore& PhysicalCore(std::size_t id) const;

    /// Gets the sole instance of the Scheduler at the current running core.
    Kernel::KScheduler* CurrentScheduler();

    /// Gets the an instance of the current physical CPU core.
    Kernel::PhysicalCore& CurrentPhysicalCore();

    /// Gets the an instance of the current physical CPU core.
    const Kernel::PhysicalCore& CurrentPhysicalCore() const;

    /// Gets the an instance of the TimeManager Interface.
    Kernel::TimeManager& TimeManager();

    /// Gets the an instance of the TimeManager Interface.
    const Kernel::TimeManager& TimeManager() const;

    /// Stops execution of 'id' core, in order to reschedule a new thread.
    void PrepareReschedule(std::size_t id);

    Core::ExclusiveMonitor& GetExclusiveMonitor();

    const Core::ExclusiveMonitor& GetExclusiveMonitor() const;

    KAutoObjectWithListContainer& ObjectListContainer();

    const KAutoObjectWithListContainer& ObjectListContainer() const;

    std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES>& Interrupts();

    const std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES>& Interrupts() const;

    void InvalidateAllInstructionCaches();

    void InvalidateCpuInstructionCacheRange(VAddr addr, std::size_t size);

    /// Registers a named HLE service, passing a factory used to open a port to that service.
    void RegisterNamedService(std::string name, ServiceInterfaceFactory&& factory);

    /// Opens a port to a service previously registered with RegisterNamedService.
    KClientPort* CreateNamedServicePort(std::string name);

    /// Registers a server session with the gobal emulation state, to be freed on shutdown. This is
    /// necessary because we do not emulate processes for HLE sessions.
    void RegisterServerSession(KServerSession* server_session);

    /// Unregisters a server session previously registered with RegisterServerSession when it was
    /// destroyed during the current emulation session.
    void UnregisterServerSession(KServerSession* server_session);

    /// Registers all kernel objects with the global emulation state, this is purely for tracking
    /// leaks after emulation has been shutdown.
    void RegisterKernelObject(KAutoObject* object);

    /// Unregisters a kernel object previously registered with RegisterKernelObject when it was
    /// destroyed during the current emulation session.
    void UnregisterKernelObject(KAutoObject* object);

    /// Determines whether or not the given port is a valid named port.
    bool IsValidNamedPort(NamedPortTable::const_iterator port) const;

    /// Gets the current host_thread/guest_thread pointer.
    KThread* GetCurrentEmuThread() const;

    /// Gets the current host_thread handle.
    u32 GetCurrentHostThreadID() const;

    /// Register the current thread as a CPU Core Thread.
    void RegisterCoreThread(std::size_t core_id);

    /// Register the current thread as a non CPU core thread.
    void RegisterHostThread();

    /// Gets the virtual memory manager for the kernel.
    KMemoryManager& MemoryManager();

    /// Gets the virtual memory manager for the kernel.
    const KMemoryManager& MemoryManager() const;

    /// Gets the slab heap allocated for user space pages.
    KSlabHeap<Page>& GetUserSlabHeapPages();

    /// Gets the slab heap allocated for user space pages.
    const KSlabHeap<Page>& GetUserSlabHeapPages() const;

    /// Gets the shared memory object for HID services.
    Kernel::KSharedMemory& GetHidSharedMem();

    /// Gets the shared memory object for HID services.
    const Kernel::KSharedMemory& GetHidSharedMem() const;

    /// Gets the shared memory object for font services.
    Kernel::KSharedMemory& GetFontSharedMem();

    /// Gets the shared memory object for font services.
    const Kernel::KSharedMemory& GetFontSharedMem() const;

    /// Gets the shared memory object for IRS services.
    Kernel::KSharedMemory& GetIrsSharedMem();

    /// Gets the shared memory object for IRS services.
    const Kernel::KSharedMemory& GetIrsSharedMem() const;

    /// Gets the shared memory object for Time services.
    Kernel::KSharedMemory& GetTimeSharedMem();

    /// Gets the shared memory object for Time services.
    const Kernel::KSharedMemory& GetTimeSharedMem() const;

    /// Suspend/unsuspend the OS.
    void Suspend(bool in_suspention);

    /// Exceptional exit the OS.
    void ExceptionalExit();

    bool IsMulticore() const;

    void EnterSVCProfile();

    void ExitSVCProfile();

    /**
     * Creates an HLE service thread, which are used to execute service routines asynchronously.
     * While these are allocated per ServerSession, these need to be owned and managed outside
     * of ServerSession to avoid a circular dependency.
     * @param name String name for the ServerSession creating this thread, used for debug
     * purposes.
     * @returns The a weak pointer newly created service thread.
     */
    std::weak_ptr<Kernel::ServiceThread> CreateServiceThread(const std::string& name);

    /**
     * Releases a HLE service thread, instructing KernelCore to free it. This should be called when
     * the ServerSession associated with the thread is destroyed.
     * @param service_thread Service thread to release.
     */
    void ReleaseServiceThread(std::weak_ptr<Kernel::ServiceThread> service_thread);

    /// Workaround for single-core mode when preempting threads while idle.
    bool IsPhantomModeForSingleCore() const;
    void SetIsPhantomModeForSingleCore(bool value);

    Core::System& System();
    const Core::System& System() const;

    /// Gets the slab heap for the specified kernel object type.
    template <typename T>
    KSlabHeap<T>& SlabHeap() {
        if constexpr (std::is_same_v<T, KClientSession>) {
            return slab_heap_container->client_session;
        } else if constexpr (std::is_same_v<T, KEvent>) {
            return slab_heap_container->event;
        } else if constexpr (std::is_same_v<T, KLinkedListNode>) {
            return slab_heap_container->linked_list_node;
        } else if constexpr (std::is_same_v<T, KPort>) {
            return slab_heap_container->port;
        } else if constexpr (std::is_same_v<T, KProcess>) {
            return slab_heap_container->process;
        } else if constexpr (std::is_same_v<T, KResourceLimit>) {
            return slab_heap_container->resource_limit;
        } else if constexpr (std::is_same_v<T, KSession>) {
            return slab_heap_container->session;
        } else if constexpr (std::is_same_v<T, KSharedMemory>) {
            return slab_heap_container->shared_memory;
        } else if constexpr (std::is_same_v<T, KThread>) {
            return slab_heap_container->thread;
        } else if constexpr (std::is_same_v<T, KTransferMemory>) {
            return slab_heap_container->transfer_memory;
        } else if constexpr (std::is_same_v<T, KWritableEvent>) {
            return slab_heap_container->writeable_event;
        }
    }

    /// Gets the current slab resource counts.
    Init::KSlabResourceCounts& SlabResourceCounts();

    /// Gets the current slab resource counts.
    const Init::KSlabResourceCounts& SlabResourceCounts() const;

private:
    friend class KProcess;
    friend class KThread;

    /// Creates a new object ID, incrementing the internal object ID counter.
    u32 CreateNewObjectID();

    /// Creates a new process ID, incrementing the internal process ID counter;
    u64 CreateNewKernelProcessID();

    /// Creates a new process ID, incrementing the internal process ID counter;
    u64 CreateNewUserProcessID();

    /// Creates a new thread ID, incrementing the internal thread ID counter.
    u64 CreateNewThreadID();

    /// Provides a reference to the global handle table.
    KHandleTable& GlobalHandleTable();

    /// Provides a const reference to the global handle table.
    const KHandleTable& GlobalHandleTable() const;

    struct Impl;
    std::unique_ptr<Impl> impl;

    bool exception_exited{};

private:
    /// Helper to encapsulate all slab heaps in a single heap allocated container
    struct SlabHeapContainer {
        KSlabHeap<KClientSession> client_session;
        KSlabHeap<KEvent> event;
        KSlabHeap<KLinkedListNode> linked_list_node;
        KSlabHeap<KPort> port;
        KSlabHeap<KProcess> process;
        KSlabHeap<KResourceLimit> resource_limit;
        KSlabHeap<KSession> session;
        KSlabHeap<KSharedMemory> shared_memory;
        KSlabHeap<KThread> thread;
        KSlabHeap<KTransferMemory> transfer_memory;
        KSlabHeap<KWritableEvent> writeable_event;
    };

    std::unique_ptr<SlabHeapContainer> slab_heap_container;
};

} // namespace Kernel
