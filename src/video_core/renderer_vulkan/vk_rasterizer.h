// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <bitset>
#include <memory>
#include <utility>
#include <vector>

#include <boost/container/static_vector.hpp>

#include "common/common_types.h"
#include "video_core/rasterizer_accelerated.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_vulkan/blit_image.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_fence_manager.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/shader/async_shaders.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Core {
class System;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra::Engines {
class Maxwell3D;
}

namespace Vulkan {

struct VKScreenInfo;

class StateTracker;

class RasterizerVulkan final : public VideoCore::RasterizerAccelerated {
public:
    explicit RasterizerVulkan(Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                              Tegra::MemoryManager& gpu_memory_, Core::Memory::Memory& cpu_memory_,
                              VKScreenInfo& screen_info_, const Device& device_,
                              MemoryAllocator& memory_allocator_, StateTracker& state_tracker_,
                              VKScheduler& scheduler_);
    ~RasterizerVulkan() override;

    void Draw(bool is_indexed, bool is_instanced) override;
    void Clear() override;
    void DispatchCompute(GPUVAddr code_addr) override;
    void ResetCounter(VideoCore::QueryType type) override;
    void Query(GPUVAddr gpu_addr, VideoCore::QueryType type, std::optional<u64> timestamp) override;
    void BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr, u32 size) override;
    void DisableGraphicsUniformBuffer(size_t stage, u32 index) override;
    void FlushAll() override;
    void FlushRegion(VAddr addr, u64 size) override;
    void InvalidateExceptTextureCache(VAddr addr, u64 size) override;
    void InvalidateTextureCache(VAddr addr, u64 size) override;
    bool MustFlushRegion(VAddr addr, u64 size) override;
    void InvalidateRegion(VAddr addr, u64 size) override;
    void OnCPUWrite(VAddr addr, u64 size) override;
    void SyncGuestHost() override;
    void UnmapMemory(VAddr addr, u64 size) override;
    void ModifyGPUMemory(GPUVAddr addr, u64 size) override;
    void SignalSemaphore(GPUVAddr addr, u32 value) override;
    void SignalSyncPoint(u32 value) override;
    void ReleaseFences() override;
    void FlushAndInvalidateRegion(VAddr addr, u64 size) override;
    void WaitForIdle() override;
    void FragmentBarrier() override;
    void TiledCacheBarrier() override;
    void FlushCommands() override;
    void TickFrame() override;
    bool AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Surface& src,
                               const Tegra::Engines::Fermi2D::Surface& dst,
                               const Tegra::Engines::Fermi2D::Config& copy_config) override;
    bool AccelerateDisplay(const Tegra::FramebufferConfig& config, VAddr framebuffer_addr,
                           u32 pixel_stride) override;

    VideoCommon::Shader::AsyncShaders& GetAsyncShaders() {
        return async_shaders;
    }

    const VideoCommon::Shader::AsyncShaders& GetAsyncShaders() const {
        return async_shaders;
    }

    /// Maximum supported size that a constbuffer can have in bytes.
    static constexpr size_t MaxConstbufferSize = 0x10000;
    static_assert(MaxConstbufferSize % (4 * sizeof(float)) == 0,
                  "The maximum size of a constbuffer must be a multiple of the size of GLvec4");

private:
    static constexpr size_t MAX_TEXTURES = 192;
    static constexpr size_t MAX_IMAGES = 48;
    static constexpr size_t MAX_IMAGE_VIEWS = MAX_TEXTURES + MAX_IMAGES;

    static constexpr VkDeviceSize DEFAULT_BUFFER_SIZE = 4 * sizeof(float);

    void FlushWork();

    /// Setup descriptors in the graphics pipeline.
    void SetupShaderDescriptors(const std::array<Shader*, Maxwell::MaxShaderProgram>& shaders,
                                bool is_indexed);

    void UpdateDynamicStates();

    void BeginTransformFeedback();

    void EndTransformFeedback();

    /// Setup uniform texels in the graphics pipeline.
    void SetupGraphicsUniformTexels(const ShaderEntries& entries, std::size_t stage);

    /// Setup textures in the graphics pipeline.
    void SetupGraphicsTextures(const ShaderEntries& entries, std::size_t stage);

    /// Setup storage texels in the graphics pipeline.
    void SetupGraphicsStorageTexels(const ShaderEntries& entries, std::size_t stage);

    /// Setup images in the graphics pipeline.
    void SetupGraphicsImages(const ShaderEntries& entries, std::size_t stage);

    /// Setup texel buffers in the compute pipeline.
    void SetupComputeUniformTexels(const ShaderEntries& entries);

    /// Setup textures in the compute pipeline.
    void SetupComputeTextures(const ShaderEntries& entries);

    /// Setup storage texels in the compute pipeline.
    void SetupComputeStorageTexels(const ShaderEntries& entries);

    /// Setup images in the compute pipeline.
    void SetupComputeImages(const ShaderEntries& entries);

    void UpdateViewportsState(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateScissorsState(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateDepthBias(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateBlendConstants(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateDepthBounds(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateStencilFaces(Tegra::Engines::Maxwell3D::Regs& regs);

    void UpdateCullMode(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateDepthBoundsTestEnable(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateDepthTestEnable(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateDepthWriteEnable(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateDepthCompareOp(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateFrontFace(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateStencilOp(Tegra::Engines::Maxwell3D::Regs& regs);
    void UpdateStencilTestEnable(Tegra::Engines::Maxwell3D::Regs& regs);

    Tegra::GPU& gpu;
    Tegra::MemoryManager& gpu_memory;
    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::Engines::KeplerCompute& kepler_compute;

    VKScreenInfo& screen_info;
    const Device& device;
    MemoryAllocator& memory_allocator;
    StateTracker& state_tracker;
    VKScheduler& scheduler;

    StagingBufferPool staging_pool;
    VKDescriptorPool descriptor_pool;
    VKUpdateDescriptorQueue update_descriptor_queue;
    BlitImageHelper blit_image;
    ASTCDecoderPass astc_decoder_pass;

    GraphicsPipelineCacheKey graphics_key;

    TextureCacheRuntime texture_cache_runtime;
    TextureCache texture_cache;
    BufferCacheRuntime buffer_cache_runtime;
    BufferCache buffer_cache;
    VKPipelineCache pipeline_cache;
    VKQueryCache query_cache;
    VKFenceManager fence_manager;

    vk::Event wfi_event;
    VideoCommon::Shader::AsyncShaders async_shaders;

    boost::container::static_vector<u32, MAX_IMAGE_VIEWS> image_view_indices;
    std::array<VideoCommon::ImageViewId, MAX_IMAGE_VIEWS> image_view_ids;
    boost::container::static_vector<VkSampler, MAX_TEXTURES> sampler_handles;

    u32 draw_counter = 0;
};

} // namespace Vulkan
