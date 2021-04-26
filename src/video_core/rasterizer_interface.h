// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <optional>
#include <span>
#include <stop_token>
#include "common/common_types.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/gpu.h"
#include "video_core/guest_driver.h"

namespace Tegra {
class MemoryManager;
}

namespace VideoCore {

enum class QueryType {
    SamplesPassed,
};
constexpr std::size_t NumQueryTypes = 1;

enum class LoadCallbackStage {
    Prepare,
    Build,
    Complete,
};
using DiskResourceLoadCallback = std::function<void(LoadCallbackStage, std::size_t, std::size_t)>;

class RasterizerInterface {
public:
    virtual ~RasterizerInterface() = default;

    /// Dispatches a draw invocation
    virtual void Draw(bool is_indexed, bool is_instanced) = 0;

    /// Clear the current framebuffer
    virtual void Clear() = 0;

    /// Dispatches a compute shader invocation
    virtual void DispatchCompute(GPUVAddr code_addr) = 0;

    /// Resets the counter of a query
    virtual void ResetCounter(QueryType type) = 0;

    /// Records a GPU query and caches it
    virtual void Query(GPUVAddr gpu_addr, QueryType type, std::optional<u64> timestamp) = 0;

    /// Signal an uniform buffer binding
    virtual void BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr,
                                           u32 size) = 0;

    /// Signal disabling of a uniform buffer
    virtual void DisableGraphicsUniformBuffer(size_t stage, u32 index) = 0;

    /// Signal a GPU based semaphore as a fence
    virtual void SignalSemaphore(GPUVAddr addr, u32 value) = 0;

    /// Signal a GPU based syncpoint as a fence
    virtual void SignalSyncPoint(u32 value) = 0;

    /// Release all pending fences.
    virtual void ReleaseFences() = 0;

    /// Notify rasterizer that all caches should be flushed to Switch memory
    virtual void FlushAll() = 0;

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    virtual void FlushRegion(VAddr addr, u64 size) = 0;

    /// Notify rasterizer to flush the texture cache to Switch memory
    virtual void InvalidateExceptTextureCache(VAddr addr, u64 size) = 0;

    /// Notify rasterizer to invalidate the texture cache
    virtual void InvalidateTextureCache(VAddr addr, u64 size) = 0;

    /// Check if the the specified memory area requires flushing to CPU Memory.
    virtual bool MustFlushRegion(VAddr addr, u64 size) = 0;

    /// Notify rasterizer that any caches of the specified region should be invalidated
    virtual void InvalidateRegion(VAddr addr, u64 size) = 0;

    /// Notify rasterizer that any caches of the specified region are desync with guest
    virtual void OnCPUWrite(VAddr addr, u64 size) = 0;

    /// Sync memory between guest and host.
    virtual void SyncGuestHost() = 0;

    /// Unmap memory range
    virtual void UnmapMemory(VAddr addr, u64 size) = 0;

    /// Remap GPU memory range. This means underneath backing memory changed
    virtual void ModifyGPUMemory(GPUVAddr addr, u64 size) = 0;

    /// Notify rasterizer that any caches of the specified region should be flushed to Switch memory
    /// and invalidated
    virtual void FlushAndInvalidateRegion(VAddr addr, u64 size) = 0;

    /// Notify the host renderer to wait for previous primitive and compute operations.
    virtual void WaitForIdle() = 0;

    /// Notify the host renderer to wait for reads and writes to render targets and flush caches.
    virtual void FragmentBarrier() = 0;

    /// Notify the host renderer to make available previous render target writes.
    virtual void TiledCacheBarrier() = 0;

    /// Notify the rasterizer to send all written commands to the host GPU.
    virtual void FlushCommands() = 0;

    /// Notify rasterizer that a frame is about to finish
    virtual void TickFrame() = 0;

    /// Attempt to use a faster method to perform a surface copy
    [[nodiscard]] virtual bool AccelerateSurfaceCopy(
        const Tegra::Engines::Fermi2D::Surface& src, const Tegra::Engines::Fermi2D::Surface& dst,
        const Tegra::Engines::Fermi2D::Config& copy_config) {
        return false;
    }

    /// Attempt to use a faster method to display the framebuffer to screen
    [[nodiscard]] virtual bool AccelerateDisplay(const Tegra::FramebufferConfig& config,
                                                 VAddr framebuffer_addr, u32 pixel_stride) {
        return false;
    }

    /// Increase/decrease the number of object in pages touching the specified region
    virtual void UpdatePagesCachedCount(VAddr addr, u64 size, int delta) {}

    /// Initialize disk cached resources for the game being emulated
    virtual void LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                   const DiskResourceLoadCallback& callback) {}

    /// Grant access to the Guest Driver Profile for recording/obtaining info on the guest driver.
    [[nodiscard]] GuestDriverProfile& AccessGuestDriverProfile() {
        return guest_driver_profile;
    }

    /// Grant access to the Guest Driver Profile for recording/obtaining info on the guest driver.
    [[nodiscard]] const GuestDriverProfile& AccessGuestDriverProfile() const {
        return guest_driver_profile;
    }

private:
    GuestDriverProfile guest_driver_profile{};
};
} // namespace VideoCore
