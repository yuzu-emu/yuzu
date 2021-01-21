// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>

#include "common/assert.h"
#include "common/microprofile.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/frontend/emu_window.h"
#include "core/hardware_interrupt_manager.h"
#include "core/memory.h"
#include "core/settings.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/kepler_memory.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/maxwell_dma.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_base.h"
#include "video_core/shader_notify.h"
#include "video_core/video_core.h"

namespace Tegra {

MICROPROFILE_DEFINE(GPU_wait, "GPU", "Wait for the GPU", MP_RGB(128, 128, 192));

GPU::GPU(Core::System& system_, bool is_async_, bool use_nvdec_)
    : system{system_}, memory_manager{std::make_unique<Tegra::MemoryManager>(system)},
      dma_pusher{std::make_unique<Tegra::DmaPusher>(system, *this)},
      cdma_pusher{std::make_unique<Tegra::CDmaPusher>(*this)}, use_nvdec{use_nvdec_},
      maxwell_3d{std::make_unique<Engines::Maxwell3D>(system, *memory_manager)},
      fermi_2d{std::make_unique<Engines::Fermi2D>()},
      kepler_compute{std::make_unique<Engines::KeplerCompute>(system, *memory_manager)},
      maxwell_dma{std::make_unique<Engines::MaxwellDMA>(system, *memory_manager)},
      kepler_memory{std::make_unique<Engines::KeplerMemory>(system, *memory_manager)},
      shader_notify{std::make_unique<VideoCore::ShaderNotify>()}, is_async{is_async_},
      gpu_thread{system_, is_async_} {}

GPU::~GPU() = default;

void GPU::BindRenderer(std::unique_ptr<VideoCore::RendererBase> renderer_) {
    renderer = std::move(renderer_);
    rasterizer = renderer->ReadRasterizer();

    memory_manager->BindRasterizer(rasterizer);
    maxwell_3d->BindRasterizer(rasterizer);
    fermi_2d->BindRasterizer(rasterizer);
    kepler_compute->BindRasterizer(rasterizer);
}

Engines::Maxwell3D& GPU::Maxwell3D() {
    return *maxwell_3d;
}

const Engines::Maxwell3D& GPU::Maxwell3D() const {
    return *maxwell_3d;
}

Engines::KeplerCompute& GPU::KeplerCompute() {
    return *kepler_compute;
}

const Engines::KeplerCompute& GPU::KeplerCompute() const {
    return *kepler_compute;
}

MemoryManager& GPU::MemoryManager() {
    return *memory_manager;
}

const MemoryManager& GPU::MemoryManager() const {
    return *memory_manager;
}

DmaPusher& GPU::DmaPusher() {
    return *dma_pusher;
}

Tegra::CDmaPusher& GPU::CDmaPusher() {
    return *cdma_pusher;
}

const DmaPusher& GPU::DmaPusher() const {
    return *dma_pusher;
}

const Tegra::CDmaPusher& GPU::CDmaPusher() const {
    return *cdma_pusher;
}

void GPU::WaitFence(u32 syncpoint_id, u32 value) {
    // Synced GPU, is always in sync
    if (!is_async) {
        return;
    }
    if (syncpoint_id == UINT32_MAX) {
        // TODO: Research what this does.
        LOG_ERROR(HW_GPU, "Waiting for syncpoint -1 not implemented");
        return;
    }
    MICROPROFILE_SCOPE(GPU_wait);
    std::unique_lock lock{sync_mutex};
    sync_cv.wait(lock, [=, this] { return syncpoints.at(syncpoint_id).load() >= value; });
}

void GPU::IncrementSyncPoint(const u32 syncpoint_id) {
    auto& syncpoint = syncpoints.at(syncpoint_id);
    syncpoint++;
    std::lock_guard lock{sync_mutex};
    sync_cv.notify_all();
    auto& interrupt = syncpt_interrupts.at(syncpoint_id);
    if (!interrupt.empty()) {
        u32 value = syncpoint.load();
        auto it = interrupt.begin();
        while (it != interrupt.end()) {
            if (value >= *it) {
                TriggerCpuInterrupt(syncpoint_id, *it);
                it = interrupt.erase(it);
                continue;
            }
            it++;
        }
    }
}

u32 GPU::GetSyncpointValue(const u32 syncpoint_id) const {
    return syncpoints.at(syncpoint_id).load();
}

void GPU::RegisterSyncptInterrupt(const u32 syncpoint_id, const u32 value) {
    auto& interrupt = syncpt_interrupts.at(syncpoint_id);
    bool contains = std::any_of(interrupt.begin(), interrupt.end(),
                                [value](u32 in_value) { return in_value == value; });
    if (contains) {
        return;
    }
    interrupt.emplace_back(value);
}

bool GPU::CancelSyncptInterrupt(const u32 syncpoint_id, const u32 value) {
    std::lock_guard lock{sync_mutex};
    auto& interrupt = syncpt_interrupts.at(syncpoint_id);
    const auto iter =
        std::find_if(interrupt.begin(), interrupt.end(),
                     [value](u32 interrupt_value) { return value == interrupt_value; });

    if (iter == interrupt.end()) {
        return false;
    }
    interrupt.erase(iter);
    return true;
}

u64 GPU::RequestFlush(VAddr addr, std::size_t size) {
    std::unique_lock lck{flush_request_mutex};
    const u64 fence = ++last_flush_fence;
    flush_requests.emplace_back(fence, addr, size);
    return fence;
}

void GPU::TickWork() {
    std::unique_lock lck{flush_request_mutex};
    while (!flush_requests.empty()) {
        auto& request = flush_requests.front();
        const u64 fence = request.fence;
        const VAddr addr = request.addr;
        const std::size_t size = request.size;
        flush_requests.pop_front();
        flush_request_mutex.unlock();
        rasterizer->FlushRegion(addr, size);
        current_flush_fence.store(fence);
        flush_request_mutex.lock();
    }
}

u64 GPU::GetTicks() const {
    // This values were reversed engineered by fincs from NVN
    // The gpu clock is reported in units of 385/625 nanoseconds
    constexpr u64 gpu_ticks_num = 384;
    constexpr u64 gpu_ticks_den = 625;

    u64 nanoseconds = system.CoreTiming().GetGlobalTimeNs().count();
    if (Settings::values.use_fast_gpu_time.GetValue()) {
        nanoseconds /= 256;
    }
    const u64 nanoseconds_num = nanoseconds / gpu_ticks_den;
    const u64 nanoseconds_rem = nanoseconds % gpu_ticks_den;
    return nanoseconds_num * gpu_ticks_num + (nanoseconds_rem * gpu_ticks_num) / gpu_ticks_den;
}

void GPU::FlushCommands() {
    rasterizer->FlushCommands();
}

void GPU::SyncGuestHost() {
    rasterizer->SyncGuestHost();
}

enum class GpuSemaphoreOperation {
    AcquireEqual = 0x1,
    WriteLong = 0x2,
    AcquireGequal = 0x4,
    AcquireMask = 0x8,
};

void GPU::CallMethod(const MethodCall& method_call) {
    LOG_TRACE(HW_GPU, "Processing method {:08X} on subchannel {}", method_call.method,
              method_call.subchannel);

    ASSERT(method_call.subchannel < bound_engines.size());

    if (ExecuteMethodOnEngine(method_call.method)) {
        CallEngineMethod(method_call);
    } else {
        CallPullerMethod(method_call);
    }
}

void GPU::CallMultiMethod(u32 method, u32 subchannel, const u32* base_start, u32 amount,
                          u32 methods_pending) {
    LOG_TRACE(HW_GPU, "Processing method {:08X} on subchannel {}", method, subchannel);

    ASSERT(subchannel < bound_engines.size());

    if (ExecuteMethodOnEngine(method)) {
        CallEngineMultiMethod(method, subchannel, base_start, amount, methods_pending);
    } else {
        for (std::size_t i = 0; i < amount; i++) {
            CallPullerMethod(MethodCall{
                method,
                base_start[i],
                subchannel,
                methods_pending - static_cast<u32>(i),
            });
        }
    }
}

bool GPU::ExecuteMethodOnEngine(u32 method) {
    const auto buffer_method = static_cast<BufferMethods>(method);
    return buffer_method >= BufferMethods::NonPullerMethods;
}

void GPU::CallPullerMethod(const MethodCall& method_call) {
    regs.reg_array[method_call.method] = method_call.argument;
    const auto method = static_cast<BufferMethods>(method_call.method);

    switch (method) {
    case BufferMethods::BindObject: {
        ProcessBindMethod(method_call);
        break;
    }
    case BufferMethods::Nop:
    case BufferMethods::SemaphoreAddressHigh:
    case BufferMethods::SemaphoreAddressLow:
    case BufferMethods::SemaphoreSequence:
    case BufferMethods::RefCnt:
    case BufferMethods::UnkCacheFlush:
    case BufferMethods::WrcacheFlush:
    case BufferMethods::FenceValue:
        break;
    case BufferMethods::FenceAction:
        ProcessFenceActionMethod();
        break;
    case BufferMethods::WaitForInterrupt:
        ProcessWaitForInterruptMethod();
        break;
    case BufferMethods::SemaphoreTrigger: {
        ProcessSemaphoreTriggerMethod();
        break;
    }
    case BufferMethods::NotifyIntr: {
        // TODO(Kmather73): Research and implement this method.
        LOG_ERROR(HW_GPU, "Special puller engine method NotifyIntr not implemented");
        break;
    }
    case BufferMethods::Unk28: {
        // TODO(Kmather73): Research and implement this method.
        LOG_ERROR(HW_GPU, "Special puller engine method Unk28 not implemented");
        break;
    }
    case BufferMethods::SemaphoreAcquire: {
        ProcessSemaphoreAcquire();
        break;
    }
    case BufferMethods::SemaphoreRelease: {
        ProcessSemaphoreRelease();
        break;
    }
    case BufferMethods::Yield: {
        // TODO(Kmather73): Research and implement this method.
        LOG_ERROR(HW_GPU, "Special puller engine method Yield not implemented");
        break;
    }
    default:
        LOG_ERROR(HW_GPU, "Special puller engine method {:X} not implemented", method);
        break;
    }
}

void GPU::CallEngineMethod(const MethodCall& method_call) {
    const EngineID engine = bound_engines[method_call.subchannel];

    switch (engine) {
    case EngineID::FERMI_TWOD_A:
        fermi_2d->CallMethod(method_call.method, method_call.argument, method_call.IsLastCall());
        break;
    case EngineID::MAXWELL_B:
        maxwell_3d->CallMethod(method_call.method, method_call.argument, method_call.IsLastCall());
        break;
    case EngineID::KEPLER_COMPUTE_B:
        kepler_compute->CallMethod(method_call.method, method_call.argument,
                                   method_call.IsLastCall());
        break;
    case EngineID::MAXWELL_DMA_COPY_A:
        maxwell_dma->CallMethod(method_call.method, method_call.argument, method_call.IsLastCall());
        break;
    case EngineID::KEPLER_INLINE_TO_MEMORY_B:
        kepler_memory->CallMethod(method_call.method, method_call.argument,
                                  method_call.IsLastCall());
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented engine");
    }
}

void GPU::CallEngineMultiMethod(u32 method, u32 subchannel, const u32* base_start, u32 amount,
                                u32 methods_pending) {
    const EngineID engine = bound_engines[subchannel];

    switch (engine) {
    case EngineID::FERMI_TWOD_A:
        fermi_2d->CallMultiMethod(method, base_start, amount, methods_pending);
        break;
    case EngineID::MAXWELL_B:
        maxwell_3d->CallMultiMethod(method, base_start, amount, methods_pending);
        break;
    case EngineID::KEPLER_COMPUTE_B:
        kepler_compute->CallMultiMethod(method, base_start, amount, methods_pending);
        break;
    case EngineID::MAXWELL_DMA_COPY_A:
        maxwell_dma->CallMultiMethod(method, base_start, amount, methods_pending);
        break;
    case EngineID::KEPLER_INLINE_TO_MEMORY_B:
        kepler_memory->CallMultiMethod(method, base_start, amount, methods_pending);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented engine");
    }
}

void GPU::ProcessBindMethod(const MethodCall& method_call) {
    // Bind the current subchannel to the desired engine id.
    LOG_DEBUG(HW_GPU, "Binding subchannel {} to engine {}", method_call.subchannel,
              method_call.argument);
    const auto engine_id = static_cast<EngineID>(method_call.argument);
    bound_engines[method_call.subchannel] = static_cast<EngineID>(engine_id);
    switch (engine_id) {
    case EngineID::FERMI_TWOD_A:
        dma_pusher->BindSubchannel(fermi_2d.get(), method_call.subchannel);
        break;
    case EngineID::MAXWELL_B:
        dma_pusher->BindSubchannel(maxwell_3d.get(), method_call.subchannel);
        break;
    case EngineID::KEPLER_COMPUTE_B:
        dma_pusher->BindSubchannel(kepler_compute.get(), method_call.subchannel);
        break;
    case EngineID::MAXWELL_DMA_COPY_A:
        dma_pusher->BindSubchannel(maxwell_dma.get(), method_call.subchannel);
        break;
    case EngineID::KEPLER_INLINE_TO_MEMORY_B:
        dma_pusher->BindSubchannel(kepler_memory.get(), method_call.subchannel);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented engine {:04X}", engine_id);
    }
}

void GPU::ProcessFenceActionMethod() {
    switch (regs.fence_action.op) {
    case FenceOperation::Acquire:
        WaitFence(regs.fence_action.syncpoint_id, regs.fence_value);
        break;
    case FenceOperation::Increment:
        IncrementSyncPoint(regs.fence_action.syncpoint_id);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented operation {}", regs.fence_action.op.Value());
    }
}

void GPU::ProcessWaitForInterruptMethod() {
    // TODO(bunnei) ImplementMe
    LOG_WARNING(HW_GPU, "(STUBBED) called");
}

void GPU::ProcessSemaphoreTriggerMethod() {
    const auto semaphoreOperationMask = 0xF;
    const auto op =
        static_cast<GpuSemaphoreOperation>(regs.semaphore_trigger & semaphoreOperationMask);
    if (op == GpuSemaphoreOperation::WriteLong) {
        struct Block {
            u32 sequence;
            u32 zeros = 0;
            u64 timestamp;
        };

        Block block{};
        block.sequence = regs.semaphore_sequence;
        // TODO(Kmather73): Generate a real GPU timestamp and write it here instead of
        // CoreTiming
        block.timestamp = GetTicks();
        memory_manager->WriteBlock(regs.semaphore_address.SemaphoreAddress(), &block,
                                   sizeof(block));
    } else {
        const u32 word{memory_manager->Read<u32>(regs.semaphore_address.SemaphoreAddress())};
        if ((op == GpuSemaphoreOperation::AcquireEqual && word == regs.semaphore_sequence) ||
            (op == GpuSemaphoreOperation::AcquireGequal &&
             static_cast<s32>(word - regs.semaphore_sequence) > 0) ||
            (op == GpuSemaphoreOperation::AcquireMask && (word & regs.semaphore_sequence))) {
            // Nothing to do in this case
        } else {
            regs.acquire_source = true;
            regs.acquire_value = regs.semaphore_sequence;
            if (op == GpuSemaphoreOperation::AcquireEqual) {
                regs.acquire_active = true;
                regs.acquire_mode = false;
            } else if (op == GpuSemaphoreOperation::AcquireGequal) {
                regs.acquire_active = true;
                regs.acquire_mode = true;
            } else if (op == GpuSemaphoreOperation::AcquireMask) {
                // TODO(kemathe) The acquire mask operation waits for a value that, ANDed with
                // semaphore_sequence, gives a non-0 result
                LOG_ERROR(HW_GPU, "Invalid semaphore operation AcquireMask not implemented");
            } else {
                LOG_ERROR(HW_GPU, "Invalid semaphore operation");
            }
        }
    }
}

void GPU::ProcessSemaphoreRelease() {
    memory_manager->Write<u32>(regs.semaphore_address.SemaphoreAddress(), regs.semaphore_release);
}

void GPU::ProcessSemaphoreAcquire() {
    const u32 word = memory_manager->Read<u32>(regs.semaphore_address.SemaphoreAddress());
    const auto value = regs.semaphore_acquire;
    if (word != value) {
        regs.acquire_active = true;
        regs.acquire_value = value;
        // TODO(kemathe73) figure out how to do the acquire_timeout
        regs.acquire_mode = false;
        regs.acquire_source = false;
    }
}

void GPU::Start() {
    gpu_thread.StartThread(*renderer, renderer->Context(), *dma_pusher, *cdma_pusher);
    cpu_context = renderer->GetRenderWindow().CreateSharedContext();
    cpu_context->MakeCurrent();
}

void GPU::ObtainContext() {
    cpu_context->MakeCurrent();
}

void GPU::ReleaseContext() {
    cpu_context->DoneCurrent();
}

void GPU::PushGPUEntries(Tegra::CommandList&& entries) {
    gpu_thread.SubmitList(std::move(entries));
}

void GPU::PushCommandBuffer(Tegra::ChCommandHeaderList& entries) {
    if (!use_nvdec) {
        return;
    }
    // This condition fires when a video stream ends, clear all intermediary data
    if (entries[0].raw == 0xDEADB33F) {
        cdma_pusher.reset();
        return;
    }
    if (!cdma_pusher) {
        cdma_pusher = std::make_unique<Tegra::CDmaPusher>(*this);
    }

    // SubmitCommandBuffer would make the nvdec operations async, this is not currently working
    // TODO(ameerj): RE proper async nvdec operation
    // gpu_thread.SubmitCommandBuffer(std::move(entries));

    cdma_pusher->Push(std::move(entries));
    cdma_pusher->DispatchCalls();
}

void GPU::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    gpu_thread.SwapBuffers(framebuffer);
}

void GPU::FlushRegion(VAddr addr, u64 size) {
    gpu_thread.FlushRegion(addr, size);
}

void GPU::InvalidateRegion(VAddr addr, u64 size) {
    gpu_thread.InvalidateRegion(addr, size);
}

void GPU::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    gpu_thread.FlushAndInvalidateRegion(addr, size);
}

void GPU::TriggerCpuInterrupt(const u32 syncpoint_id, const u32 value) const {
    auto& interrupt_manager = system.InterruptManager();
    interrupt_manager.GPUInterruptSyncpt(syncpoint_id, value);
}

void GPU::OnCommandListEnd() {
    if (is_async) {
        // This command only applies to asynchronous GPU mode
        gpu_thread.OnCommandListEnd();
    }
}

} // namespace Tegra
