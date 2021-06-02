// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <vector>

#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
VkBufferCopy MakeBufferCopy(const VideoCommon::BufferCopy& copy) {
    return VkBufferCopy{
        .srcOffset = copy.src_offset,
        .dstOffset = copy.dst_offset,
        .size = copy.size,
    };
}

VkIndexType IndexTypeFromNumElements(const Device& device, u32 num_elements) {
    if (num_elements <= 0xff && device.IsExtIndexTypeUint8Supported()) {
        return VK_INDEX_TYPE_UINT8_EXT;
    }
    if (num_elements <= 0xffff) {
        return VK_INDEX_TYPE_UINT16;
    }
    return VK_INDEX_TYPE_UINT32;
}

size_t BytesPerIndex(VkIndexType index_type) {
    switch (index_type) {
    case VK_INDEX_TYPE_UINT8_EXT:
        return 1;
    case VK_INDEX_TYPE_UINT16:
        return 2;
    case VK_INDEX_TYPE_UINT32:
        return 4;
    default:
        UNREACHABLE_MSG("Invalid index type={}", index_type);
        return 1;
    }
}

template <typename T>
std::array<T, 6> MakeQuadIndices(u32 quad, u32 first) {
    std::array<T, 6> indices{0, 1, 2, 0, 2, 3};
    for (T& index : indices) {
        index = static_cast<T>(first + index + quad * 4);
    }
    return indices;
}

vk::Buffer CreateBuffer(const Device& device, u64 size) {
    VkBufferUsageFlags flags =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (device.IsExtTransformFeedbackSupported()) {
        flags |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
    }
    return device.GetLogical().CreateBuffer({
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    });
}
} // Anonymous namespace

Buffer::Buffer(BufferCacheRuntime&, VideoCommon::NullBufferParams null_params)
    : VideoCommon::BufferBase<VideoCore::RasterizerInterface>(null_params) {}

Buffer::Buffer(BufferCacheRuntime& runtime, VideoCore::RasterizerInterface& rasterizer_,
               VAddr cpu_addr_, u64 size_bytes_)
    : VideoCommon::BufferBase<VideoCore::RasterizerInterface>(rasterizer_, cpu_addr_, size_bytes_),
      device{&runtime.device}, buffer{CreateBuffer(*device, SizeBytes())},
      commit{runtime.memory_allocator.Commit(buffer, MemoryUsage::DeviceLocal)} {
    if (runtime.device.HasDebuggingToolAttached()) {
        buffer.SetObjectNameEXT(fmt::format("Buffer 0x{:x}", CpuAddr()).c_str());
    }
}

VkBufferView Buffer::View(u32 offset, u32 size, VideoCore::Surface::PixelFormat format) {
    if (!device) {
        // Null buffer, return a null descriptor
        return VK_NULL_HANDLE;
    }
    const auto it{std::ranges::find_if(views, [offset, size, format](const BufferView& view) {
        return offset == view.offset && size == view.size && format == view.format;
    })};
    if (it != views.end()) {
        return *it->handle;
    }
    views.push_back({
        .offset = offset,
        .size = size,
        .format = format,
        .handle = device->GetLogical().CreateBufferView({
            .sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .buffer = *buffer,
            .format = MaxwellToVK::SurfaceFormat(*device, FormatType::Buffer, false, format).format,
            .offset = offset,
            .range = size,
        }),
    });
    return *views.back().handle;
}

BufferCacheRuntime::BufferCacheRuntime(const Device& device_, MemoryAllocator& memory_allocator_,
                                       VKScheduler& scheduler_, StagingBufferPool& staging_pool_,
                                       VKUpdateDescriptorQueue& update_descriptor_queue_,
                                       DescriptorPool& descriptor_pool)
    : device{device_}, memory_allocator{memory_allocator_}, scheduler{scheduler_},
      staging_pool{staging_pool_}, update_descriptor_queue{update_descriptor_queue_},
      uint8_pass(device, scheduler, descriptor_pool, staging_pool, update_descriptor_queue),
      quad_index_pass(device, scheduler, descriptor_pool, staging_pool, update_descriptor_queue) {}

StagingBufferRef BufferCacheRuntime::UploadStagingBuffer(size_t size) {
    return staging_pool.Request(size, MemoryUsage::Upload);
}

StagingBufferRef BufferCacheRuntime::DownloadStagingBuffer(size_t size) {
    return staging_pool.Request(size, MemoryUsage::Download);
}

void BufferCacheRuntime::Finish() {
    scheduler.Finish();
}

void BufferCacheRuntime::CopyBuffer(VkBuffer dst_buffer, VkBuffer src_buffer,
                                    std::span<const VideoCommon::BufferCopy> copies) {
    static constexpr VkMemoryBarrier READ_BARRIER{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
    };
    static constexpr VkMemoryBarrier WRITE_BARRIER{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
    };
    // Measuring a popular game, this number never exceeds the specified size once data is warmed up
    boost::container::small_vector<VkBufferCopy, 3> vk_copies(copies.size());
    std::ranges::transform(copies, vk_copies.begin(), MakeBufferCopy);
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([src_buffer, dst_buffer, vk_copies](vk::CommandBuffer cmdbuf) {
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               0, READ_BARRIER);
        cmdbuf.CopyBuffer(src_buffer, dst_buffer, vk_copies);
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               0, WRITE_BARRIER);
    });
}

void BufferCacheRuntime::BindIndexBuffer(PrimitiveTopology topology, IndexFormat index_format,
                                         u32 base_vertex, u32 num_indices, VkBuffer buffer,
                                         u32 offset, [[maybe_unused]] u32 size) {
    VkIndexType vk_index_type = MaxwellToVK::IndexFormat(index_format);
    VkDeviceSize vk_offset = offset;
    VkBuffer vk_buffer = buffer;
    if (topology == PrimitiveTopology::Quads) {
        vk_index_type = VK_INDEX_TYPE_UINT32;
        std::tie(vk_buffer, vk_offset) =
            quad_index_pass.Assemble(index_format, num_indices, base_vertex, buffer, offset);
    } else if (vk_index_type == VK_INDEX_TYPE_UINT8_EXT && !device.IsExtIndexTypeUint8Supported()) {
        vk_index_type = VK_INDEX_TYPE_UINT16;
        std::tie(vk_buffer, vk_offset) = uint8_pass.Assemble(num_indices, buffer, offset);
    }
    if (vk_buffer == VK_NULL_HANDLE) {
        // Vulkan doesn't support null index buffers. Replace it with our own null buffer.
        ReserveNullIndexBuffer();
        vk_buffer = *null_index_buffer;
    }
    scheduler.Record([vk_buffer, vk_offset, vk_index_type](vk::CommandBuffer cmdbuf) {
        cmdbuf.BindIndexBuffer(vk_buffer, vk_offset, vk_index_type);
    });
}

void BufferCacheRuntime::BindQuadArrayIndexBuffer(u32 first, u32 count) {
    ReserveQuadArrayLUT(first + count, true);

    // The LUT has the indices 0, 1, 2, and 3 copied as an array
    // To apply these 'first' offsets we can apply an offset based on the modulus.
    const VkIndexType index_type = quad_array_lut_index_type;
    const size_t sub_first_offset = static_cast<size_t>(first % 4) * (current_num_indices / 4);
    const size_t offset = (sub_first_offset + first / 4) * 6ULL * BytesPerIndex(index_type);
    scheduler.Record([buffer = *quad_array_lut, index_type, offset](vk::CommandBuffer cmdbuf) {
        cmdbuf.BindIndexBuffer(buffer, offset, index_type);
    });
}

void BufferCacheRuntime::BindVertexBuffer(u32 index, VkBuffer buffer, u32 offset, u32 size,
                                          u32 stride) {
    if (device.IsExtExtendedDynamicStateSupported()) {
        scheduler.Record([index, buffer, offset, size, stride](vk::CommandBuffer cmdbuf) {
            const VkDeviceSize vk_offset = buffer != VK_NULL_HANDLE ? offset : 0;
            const VkDeviceSize vk_size = buffer != VK_NULL_HANDLE ? size : VK_WHOLE_SIZE;
            const VkDeviceSize vk_stride = stride;
            cmdbuf.BindVertexBuffers2EXT(index, 1, &buffer, &vk_offset, &vk_size, &vk_stride);
        });
    } else {
        scheduler.Record([index, buffer, offset](vk::CommandBuffer cmdbuf) {
            cmdbuf.BindVertexBuffer(index, buffer, offset);
        });
    }
}

void BufferCacheRuntime::BindTransformFeedbackBuffer(u32 index, VkBuffer buffer, u32 offset,
                                                     u32 size) {
    if (!device.IsExtTransformFeedbackSupported()) {
        // Already logged in the rasterizer
        return;
    }
    scheduler.Record([index, buffer, offset, size](vk::CommandBuffer cmdbuf) {
        const VkDeviceSize vk_offset = offset;
        const VkDeviceSize vk_size = size;
        cmdbuf.BindTransformFeedbackBuffersEXT(index, 1, &buffer, &vk_offset, &vk_size);
    });
}

void BufferCacheRuntime::ReserveQuadArrayLUT(u32 num_indices, bool wait_for_idle) {
    if (num_indices <= current_num_indices) {
        return;
    }
    if (wait_for_idle) {
        scheduler.Finish();
    }
    current_num_indices = num_indices;
    quad_array_lut_index_type = IndexTypeFromNumElements(device, num_indices);

    const u32 num_quads = num_indices / 4;
    const u32 num_triangle_indices = num_quads * 6;
    const u32 num_first_offset_copies = 4;
    const size_t bytes_per_index = BytesPerIndex(quad_array_lut_index_type);
    const size_t size_bytes = num_triangle_indices * bytes_per_index * num_first_offset_copies;
    quad_array_lut = device.GetLogical().CreateBuffer(VkBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size_bytes,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    });
    if (device.HasDebuggingToolAttached()) {
        quad_array_lut.SetObjectNameEXT("Quad LUT");
    }
    quad_array_lut_commit = memory_allocator.Commit(quad_array_lut, MemoryUsage::DeviceLocal);

    const StagingBufferRef staging = staging_pool.Request(size_bytes, MemoryUsage::Upload);
    u8* staging_data = staging.mapped_span.data();
    const size_t quad_size = bytes_per_index * 6;
    for (u32 first = 0; first < num_first_offset_copies; ++first) {
        for (u32 quad = 0; quad < num_quads; ++quad) {
            switch (quad_array_lut_index_type) {
            case VK_INDEX_TYPE_UINT8_EXT:
                std::memcpy(staging_data, MakeQuadIndices<u8>(quad, first).data(), quad_size);
                break;
            case VK_INDEX_TYPE_UINT16:
                std::memcpy(staging_data, MakeQuadIndices<u16>(quad, first).data(), quad_size);
                break;
            case VK_INDEX_TYPE_UINT32:
                std::memcpy(staging_data, MakeQuadIndices<u32>(quad, first).data(), quad_size);
                break;
            default:
                UNREACHABLE();
                break;
            }
            staging_data += quad_size;
        }
    }
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([src_buffer = staging.buffer, src_offset = staging.offset,
                      dst_buffer = *quad_array_lut, size_bytes](vk::CommandBuffer cmdbuf) {
        const VkBufferCopy copy{
            .srcOffset = src_offset,
            .dstOffset = 0,
            .size = size_bytes,
        };
        const VkBufferMemoryBarrier write_barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_INDEX_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = dst_buffer,
            .offset = 0,
            .size = size_bytes,
        };
        cmdbuf.CopyBuffer(src_buffer, dst_buffer, copy);
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                               0, write_barrier);
    });
}

void BufferCacheRuntime::ReserveNullIndexBuffer() {
    if (null_index_buffer) {
        return;
    }
    null_index_buffer = device.GetLogical().CreateBuffer(VkBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = 4,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    });
    if (device.HasDebuggingToolAttached()) {
        null_index_buffer.SetObjectNameEXT("Null index buffer");
    }
    null_index_buffer_commit = memory_allocator.Commit(null_index_buffer, MemoryUsage::DeviceLocal);

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([buffer = *null_index_buffer](vk::CommandBuffer cmdbuf) {
        cmdbuf.FillBuffer(buffer, 0, VK_WHOLE_SIZE, 0);
    });
}

} // namespace Vulkan
