// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/vk_compute_pass.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class DescriptorPool;
class VKScheduler;

class BufferCacheRuntime;

class Buffer : public VideoCommon::BufferBase<VideoCore::RasterizerInterface> {
public:
    explicit Buffer(BufferCacheRuntime&, VideoCommon::NullBufferParams null_params);
    explicit Buffer(BufferCacheRuntime& runtime, VideoCore::RasterizerInterface& rasterizer_,
                    VAddr cpu_addr_, u64 size_bytes_);

    [[nodiscard]] VkBufferView View(u32 offset, u32 size, VideoCore::Surface::PixelFormat format);

    [[nodiscard]] VkBuffer Handle() const noexcept {
        return *buffer;
    }

    operator VkBuffer() const noexcept {
        return *buffer;
    }

private:
    struct BufferView {
        u32 offset;
        u32 size;
        VideoCore::Surface::PixelFormat format;
        vk::BufferView handle;
    };

    const Device* device{};
    vk::Buffer buffer;
    MemoryCommit commit;
    std::vector<BufferView> views;
};

class BufferCacheRuntime {
    friend Buffer;

    using PrimitiveTopology = Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology;
    using IndexFormat = Tegra::Engines::Maxwell3D::Regs::IndexFormat;

public:
    explicit BufferCacheRuntime(const Device& device_, MemoryAllocator& memory_manager_,
                                VKScheduler& scheduler_, StagingBufferPool& staging_pool_,
                                VKUpdateDescriptorQueue& update_descriptor_queue_,
                                DescriptorPool& descriptor_pool);

    void Finish();

    [[nodiscard]] StagingBufferRef UploadStagingBuffer(size_t size);

    [[nodiscard]] StagingBufferRef DownloadStagingBuffer(size_t size);

    void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer,
                    std::span<const VideoCommon::BufferCopy> copies);

    void BindIndexBuffer(PrimitiveTopology topology, IndexFormat index_format, u32 num_indices,
                         u32 base_vertex, VkBuffer buffer, u32 offset, u32 size);

    void BindQuadArrayIndexBuffer(u32 first, u32 count);

    void BindVertexBuffer(u32 index, VkBuffer buffer, u32 offset, u32 size, u32 stride);

    void BindTransformFeedbackBuffer(u32 index, VkBuffer buffer, u32 offset, u32 size);

    std::span<u8> BindMappedUniformBuffer([[maybe_unused]] size_t stage,
                                          [[maybe_unused]] u32 binding_index, u32 size) {
        const StagingBufferRef ref = staging_pool.Request(size, MemoryUsage::Upload);
        BindBuffer(ref.buffer, static_cast<u32>(ref.offset), size);
        return ref.mapped_span;
    }

    void BindUniformBuffer(VkBuffer buffer, u32 offset, u32 size) {
        BindBuffer(buffer, offset, size);
    }

    void BindStorageBuffer(VkBuffer buffer, u32 offset, u32 size,
                           [[maybe_unused]] bool is_written) {
        BindBuffer(buffer, offset, size);
    }

    void BindTextureBuffer(Buffer& buffer, u32 offset, u32 size,
                           VideoCore::Surface::PixelFormat format) {
        update_descriptor_queue.AddTexelBuffer(buffer.View(offset, size, format));
    }

private:
    void BindBuffer(VkBuffer buffer, u32 offset, u32 size) {
        update_descriptor_queue.AddBuffer(buffer, offset, size);
    }

    void ReserveQuadArrayLUT(u32 num_indices, bool wait_for_idle);

    void ReserveNullIndexBuffer();

    const Device& device;
    MemoryAllocator& memory_allocator;
    VKScheduler& scheduler;
    StagingBufferPool& staging_pool;
    VKUpdateDescriptorQueue& update_descriptor_queue;

    vk::Buffer quad_array_lut;
    MemoryCommit quad_array_lut_commit;
    VkIndexType quad_array_lut_index_type{};
    u32 current_num_indices = 0;

    vk::Buffer null_index_buffer;
    MemoryCommit null_index_buffer_commit;

    Uint8Pass uint8_pass;
    QuadIndexedPass quad_index_pass;
};

struct BufferCacheParams {
    using Runtime = Vulkan::BufferCacheRuntime;
    using Buffer = Vulkan::Buffer;

    static constexpr bool IS_OPENGL = false;
    static constexpr bool HAS_PERSISTENT_UNIFORM_BUFFER_BINDINGS = false;
    static constexpr bool HAS_FULL_INDEX_AND_PRIMITIVE_SUPPORT = false;
    static constexpr bool NEEDS_BIND_UNIFORM_INDEX = false;
    static constexpr bool NEEDS_BIND_STORAGE_INDEX = false;
    static constexpr bool USE_MEMORY_MAPS = true;
    static constexpr bool SEPARATE_IMAGE_BUFFER_BINDINGS = false;
};

using BufferCache = VideoCommon::BufferCache<BufferCacheParams>;

} // namespace Vulkan
