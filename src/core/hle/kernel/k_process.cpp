// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <bitset>
#include <ctime>
#include <memory>
#include <random>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/device_memory.h"
#include "core/file_sys/program_metadata.h"
#include "core/hle/kernel/code_set.h"
#include "core/hle/kernel/k_memory_block_manager.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/k_slab_heap.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/lock.h"
#include "core/memory.h"

namespace Kernel {
namespace {
/**
 * Sets up the primary application thread
 *
 * @param system The system instance to create the main thread under.
 * @param owner_process The parent process for the main thread
 * @param priority The priority to give the main thread
 */
void SetupMainThread(Core::System& system, KProcess& owner_process, u32 priority, VAddr stack_top) {
    const VAddr entry_point = owner_process.PageTable().GetCodeRegionStart();
    ASSERT(owner_process.GetResourceLimit()->Reserve(LimitableResource::Threads, 1));

    KThread* thread = KThread::Create(system.Kernel());
    SCOPE_EXIT({ thread->Close(); });

    ASSERT(KThread::InitializeUserThread(system, thread, entry_point, 0, stack_top, priority,
                                         owner_process.GetIdealCoreId(), &owner_process)
               .IsSuccess());

    // Register 1 must be a handle to the main thread
    Handle thread_handle{};
    owner_process.GetHandleTable().Add(&thread_handle, thread);

    thread->SetName("main");
    thread->GetContext32().cpu_registers[0] = 0;
    thread->GetContext64().cpu_registers[0] = 0;
    thread->GetContext32().cpu_registers[1] = thread_handle;
    thread->GetContext64().cpu_registers[1] = thread_handle;

    auto& kernel = system.Kernel();
    // Threads by default are dormant, wake up the main thread so it runs when the scheduler fires
    {
        KScopedSchedulerLock lock{kernel};
        thread->SetState(ThreadState::Runnable);
    }
}
} // Anonymous namespace

// Represents a page used for thread-local storage.
//
// Each TLS page contains slots that may be used by processes and threads.
// Every process and thread is created with a slot in some arbitrary page
// (whichever page happens to have an available slot).
class TLSPage {
public:
    static constexpr std::size_t num_slot_entries =
        Core::Memory::PAGE_SIZE / Core::Memory::TLS_ENTRY_SIZE;

    explicit TLSPage(VAddr address) : base_address{address} {}

    bool HasAvailableSlots() const {
        return !is_slot_used.all();
    }

    VAddr GetBaseAddress() const {
        return base_address;
    }

    std::optional<VAddr> ReserveSlot() {
        for (std::size_t i = 0; i < is_slot_used.size(); i++) {
            if (is_slot_used[i]) {
                continue;
            }

            is_slot_used[i] = true;
            return base_address + (i * Core::Memory::TLS_ENTRY_SIZE);
        }

        return std::nullopt;
    }

    void ReleaseSlot(VAddr address) {
        // Ensure that all given addresses are consistent with how TLS pages
        // are intended to be used when releasing slots.
        ASSERT(IsWithinPage(address));
        ASSERT((address % Core::Memory::TLS_ENTRY_SIZE) == 0);

        const std::size_t index = (address - base_address) / Core::Memory::TLS_ENTRY_SIZE;
        is_slot_used[index] = false;
    }

private:
    bool IsWithinPage(VAddr address) const {
        return base_address <= address && address < base_address + Core::Memory::PAGE_SIZE;
    }

    VAddr base_address;
    std::bitset<num_slot_entries> is_slot_used;
};

ResultCode KProcess::Initialize(KProcess* process, Core::System& system, std::string process_name,
                                ProcessType type) {
    auto& kernel = system.Kernel();

    process->name = std::move(process_name);

    process->resource_limit = kernel.GetSystemResourceLimit();
    process->status = ProcessStatus::Created;
    process->program_id = 0;
    process->process_id = type == ProcessType::KernelInternal ? kernel.CreateNewKernelProcessID()
                                                              : kernel.CreateNewUserProcessID();
    process->capabilities.InitializeForMetadatalessProcess();
    process->is_initialized = true;

    std::mt19937 rng(Settings::values.rng_seed.GetValue().value_or(std::time(nullptr)));
    std::uniform_int_distribution<u64> distribution;
    std::generate(process->random_entropy.begin(), process->random_entropy.end(),
                  [&] { return distribution(rng); });

    kernel.AppendNewProcess(process);

    // Open a reference to the resource limit.
    process->resource_limit->Open();

    return ResultSuccess;
}

KResourceLimit* KProcess::GetResourceLimit() const {
    return resource_limit;
}

void KProcess::IncrementThreadCount() {
    ASSERT(num_threads >= 0);
    num_created_threads++;

    if (const auto count = ++num_threads; count > peak_num_threads) {
        peak_num_threads = count;
    }
}

void KProcess::DecrementThreadCount() {
    ASSERT(num_threads > 0);

    if (const auto count = --num_threads; count == 0) {
        LOG_WARNING(Kernel, "Process termination is not fully implemented.");
    }
}

u64 KProcess::GetTotalPhysicalMemoryAvailable() const {
    const u64 capacity{resource_limit->GetFreeValue(LimitableResource::PhysicalMemory) +
                       page_table->GetTotalHeapSize() + GetSystemResourceSize() + image_size +
                       main_thread_stack_size};
    if (const auto pool_size = kernel.MemoryManager().GetSize(KMemoryManager::Pool::Application);
        capacity != pool_size) {
        LOG_WARNING(Kernel, "capacity {} != application pool size {}", capacity, pool_size);
    }
    if (capacity < memory_usage_capacity) {
        return capacity;
    }
    return memory_usage_capacity;
}

u64 KProcess::GetTotalPhysicalMemoryAvailableWithoutSystemResource() const {
    return GetTotalPhysicalMemoryAvailable() - GetSystemResourceSize();
}

u64 KProcess::GetTotalPhysicalMemoryUsed() const {
    return image_size + main_thread_stack_size + page_table->GetTotalHeapSize() +
           GetSystemResourceSize();
}

u64 KProcess::GetTotalPhysicalMemoryUsedWithoutSystemResource() const {
    return GetTotalPhysicalMemoryUsed() - GetSystemResourceUsage();
}

bool KProcess::ReleaseUserException(KThread* thread) {
    KScopedSchedulerLock sl{kernel};

    if (exception_thread == thread) {
        exception_thread = nullptr;

        // Remove waiter thread.
        s32 num_waiters{};
        if (KThread* next = thread->RemoveWaiterByKey(
                std::addressof(num_waiters),
                reinterpret_cast<uintptr_t>(std::addressof(exception_thread)));
            next != nullptr) {
            next->SetState(ThreadState::Runnable);
        }

        KScheduler::SetSchedulerUpdateNeeded(kernel);

        return true;
    } else {
        return false;
    }
}

void KProcess::PinCurrentThread() {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    // Get the current thread.
    const s32 core_id = GetCurrentCoreId(kernel);
    KThread* cur_thread = GetCurrentThreadPointer(kernel);

    // Pin it.
    PinThread(core_id, cur_thread);
    cur_thread->Pin();

    // An update is needed.
    KScheduler::SetSchedulerUpdateNeeded(kernel);
}

void KProcess::UnpinCurrentThread() {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    // Get the current thread.
    const s32 core_id = GetCurrentCoreId(kernel);
    KThread* cur_thread = GetCurrentThreadPointer(kernel);

    // Unpin it.
    cur_thread->Unpin();
    UnpinThread(core_id, cur_thread);

    // An update is needed.
    KScheduler::SetSchedulerUpdateNeeded(kernel);
}

ResultCode KProcess::AddSharedMemory(KSharedMemory* shmem, [[maybe_unused]] VAddr address,
                                     [[maybe_unused]] size_t size) {
    // Lock ourselves, to prevent concurrent access.
    KScopedLightLock lk(state_lock);

    // TODO(bunnei): Manage KSharedMemoryInfo list here.

    // Open a reference to the shared memory.
    shmem->Open();

    return ResultSuccess;
}

void KProcess::RemoveSharedMemory(KSharedMemory* shmem, [[maybe_unused]] VAddr address,
                                  [[maybe_unused]] size_t size) {
    // Lock ourselves, to prevent concurrent access.
    KScopedLightLock lk(state_lock);

    // TODO(bunnei): Manage KSharedMemoryInfo list here.

    // Close a reference to the shared memory.
    shmem->Close();
}

void KProcess::RegisterThread(const KThread* thread) {
    thread_list.push_back(thread);
}

void KProcess::UnregisterThread(const KThread* thread) {
    thread_list.remove(thread);
}

ResultCode KProcess::Reset() {
    // Lock the process and the scheduler.
    KScopedLightLock lk(state_lock);
    KScopedSchedulerLock sl{kernel};

    // Validate that we're in a state that we can reset.
    R_UNLESS(status != ProcessStatus::Exited, ResultInvalidState);
    R_UNLESS(is_signaled, ResultInvalidState);

    // Clear signaled.
    is_signaled = false;
    return ResultSuccess;
}

ResultCode KProcess::LoadFromMetadata(const FileSys::ProgramMetadata& metadata,
                                      std::size_t code_size) {
    program_id = metadata.GetTitleID();
    ideal_core = metadata.GetMainThreadCore();
    is_64bit_process = metadata.Is64BitProgram();
    system_resource_size = metadata.GetSystemResourceSize();
    image_size = code_size;

    KScopedResourceReservation memory_reservation(resource_limit, LimitableResource::PhysicalMemory,
                                                  code_size + system_resource_size);
    if (!memory_reservation.Succeeded()) {
        LOG_ERROR(Kernel, "Could not reserve process memory requirements of size {:X} bytes",
                  code_size + system_resource_size);
        return ResultLimitReached;
    }
    // Initialize proces address space
    if (const ResultCode result{
            page_table->InitializeForProcess(metadata.GetAddressSpaceType(), false, 0x8000000,
                                             code_size, KMemoryManager::Pool::Application)};
        result.IsError()) {
        return result;
    }

    // Map process code region
    if (const ResultCode result{page_table->MapProcessCode(page_table->GetCodeRegionStart(),
                                                           code_size / PageSize, KMemoryState::Code,
                                                           KMemoryPermission::None)};
        result.IsError()) {
        return result;
    }

    // Initialize process capabilities
    const auto& caps{metadata.GetKernelCapabilities()};
    if (const ResultCode result{
            capabilities.InitializeForUserProcess(caps.data(), caps.size(), *page_table)};
        result.IsError()) {
        return result;
    }

    // Set memory usage capacity
    switch (metadata.GetAddressSpaceType()) {
    case FileSys::ProgramAddressSpaceType::Is32Bit:
    case FileSys::ProgramAddressSpaceType::Is36Bit:
    case FileSys::ProgramAddressSpaceType::Is39Bit:
        memory_usage_capacity = page_table->GetHeapRegionEnd() - page_table->GetHeapRegionStart();
        break;

    case FileSys::ProgramAddressSpaceType::Is32BitNoMap:
        memory_usage_capacity = page_table->GetHeapRegionEnd() - page_table->GetHeapRegionStart() +
                                page_table->GetAliasRegionEnd() - page_table->GetAliasRegionStart();
        break;

    default:
        UNREACHABLE();
    }

    // Create TLS region
    tls_region_address = CreateTLSRegion();
    memory_reservation.Commit();

    return handle_table.Initialize(capabilities.GetHandleTableSize());
}

void KProcess::Run(s32 main_thread_priority, u64 stack_size) {
    AllocateMainThreadStack(stack_size);
    resource_limit->Reserve(LimitableResource::Threads, 1);
    resource_limit->Reserve(LimitableResource::PhysicalMemory, main_thread_stack_size);

    const std::size_t heap_capacity{memory_usage_capacity - main_thread_stack_size - image_size};
    ASSERT(!page_table->SetHeapCapacity(heap_capacity).IsError());

    ChangeStatus(ProcessStatus::Running);

    SetupMainThread(kernel.System(), *this, main_thread_priority, main_thread_stack_top);
}

void KProcess::PrepareForTermination() {
    ChangeStatus(ProcessStatus::Exiting);

    const auto stop_threads = [this](const std::vector<KThread*>& in_thread_list) {
        for (auto& thread : in_thread_list) {
            if (thread->GetOwnerProcess() != this)
                continue;

            if (thread == kernel.CurrentScheduler()->GetCurrentThread())
                continue;

            // TODO(Subv): When are the other running/ready threads terminated?
            ASSERT_MSG(thread->GetState() == ThreadState::Waiting,
                       "Exiting processes with non-waiting threads is currently unimplemented");

            thread->Exit();
        }
    };

    stop_threads(kernel.System().GlobalSchedulerContext().GetThreadList());

    FreeTLSRegion(tls_region_address);
    tls_region_address = 0;

    if (resource_limit) {
        resource_limit->Release(LimitableResource::PhysicalMemory,
                                main_thread_stack_size + image_size);
    }

    ChangeStatus(ProcessStatus::Exited);
}

void KProcess::Finalize() {
    // Release memory to the resource limit.
    if (resource_limit != nullptr) {
        resource_limit->Close();
    }

    // Finalize the handle table and close any open handles.
    handle_table.Finalize();

    // Perform inherited finalization.
    KAutoObjectWithSlabHeapAndContainer<KProcess, KSynchronizationObject>::Finalize();
}

/**
 * Attempts to find a TLS page that contains a free slot for
 * use by a thread.
 *
 * @returns If a page with an available slot is found, then an iterator
 *          pointing to the page is returned. Otherwise the end iterator
 *          is returned instead.
 */
static auto FindTLSPageWithAvailableSlots(std::vector<TLSPage>& tls_pages) {
    return std::find_if(tls_pages.begin(), tls_pages.end(),
                        [](const auto& page) { return page.HasAvailableSlots(); });
}

VAddr KProcess::CreateTLSRegion() {
    KScopedSchedulerLock lock(kernel);
    if (auto tls_page_iter{FindTLSPageWithAvailableSlots(tls_pages)};
        tls_page_iter != tls_pages.cend()) {
        return *tls_page_iter->ReserveSlot();
    }

    Page* const tls_page_ptr{kernel.GetUserSlabHeapPages().Allocate()};
    ASSERT(tls_page_ptr);

    const VAddr start{page_table->GetKernelMapRegionStart()};
    const VAddr size{page_table->GetKernelMapRegionEnd() - start};
    const PAddr tls_map_addr{kernel.System().DeviceMemory().GetPhysicalAddr(tls_page_ptr)};
    const VAddr tls_page_addr{page_table
                                  ->AllocateAndMapMemory(1, PageSize, true, start, size / PageSize,
                                                         KMemoryState::ThreadLocal,
                                                         KMemoryPermission::ReadAndWrite,
                                                         tls_map_addr)
                                  .ValueOr(0)};

    ASSERT(tls_page_addr);

    std::memset(tls_page_ptr, 0, PageSize);
    tls_pages.emplace_back(tls_page_addr);

    const auto reserve_result{tls_pages.back().ReserveSlot()};
    ASSERT(reserve_result.has_value());

    return *reserve_result;
}

void KProcess::FreeTLSRegion(VAddr tls_address) {
    KScopedSchedulerLock lock(kernel);
    const VAddr aligned_address = Common::AlignDown(tls_address, Core::Memory::PAGE_SIZE);
    auto iter =
        std::find_if(tls_pages.begin(), tls_pages.end(), [aligned_address](const auto& page) {
            return page.GetBaseAddress() == aligned_address;
        });

    // Something has gone very wrong if we're freeing a region
    // with no actual page available.
    ASSERT(iter != tls_pages.cend());

    iter->ReleaseSlot(tls_address);
}

void KProcess::LoadModule(CodeSet code_set, VAddr base_addr) {
    std::lock_guard lock{HLE::g_hle_lock};
    const auto ReprotectSegment = [&](const CodeSet::Segment& segment,
                                      KMemoryPermission permission) {
        page_table->SetCodeMemoryPermission(segment.addr + base_addr, segment.size, permission);
    };

    kernel.System().Memory().WriteBlock(*this, base_addr, code_set.memory.data(),
                                        code_set.memory.size());

    ReprotectSegment(code_set.CodeSegment(), KMemoryPermission::ReadAndExecute);
    ReprotectSegment(code_set.RODataSegment(), KMemoryPermission::Read);
    ReprotectSegment(code_set.DataSegment(), KMemoryPermission::ReadAndWrite);
}

bool KProcess::IsSignaled() const {
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());
    return is_signaled;
}

KProcess::KProcess(KernelCore& kernel_)
    : KAutoObjectWithSlabHeapAndContainer{kernel_},
      page_table{std::make_unique<KPageTable>(kernel_.System())}, handle_table{kernel_},
      address_arbiter{kernel_.System()}, condition_var{kernel_.System()}, state_lock{kernel_} {}

KProcess::~KProcess() = default;

void KProcess::ChangeStatus(ProcessStatus new_status) {
    if (status == new_status) {
        return;
    }

    status = new_status;
    is_signaled = true;
    NotifyAvailable();
}

ResultCode KProcess::AllocateMainThreadStack(std::size_t stack_size) {
    ASSERT(stack_size);

    // The kernel always ensures that the given stack size is page aligned.
    main_thread_stack_size = Common::AlignUp(stack_size, PageSize);

    const VAddr start{page_table->GetStackRegionStart()};
    const std::size_t size{page_table->GetStackRegionEnd() - start};

    CASCADE_RESULT(main_thread_stack_top,
                   page_table->AllocateAndMapMemory(
                       main_thread_stack_size / PageSize, PageSize, false, start, size / PageSize,
                       KMemoryState::Stack, KMemoryPermission::ReadAndWrite));

    main_thread_stack_top += main_thread_stack_size;

    return ResultSuccess;
}

} // namespace Kernel
