// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include <boost/container/small_vector.hpp>

#include "video_core/renderer_vulkan/pipeline_helper.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
DescriptorLayoutTuple CreateLayout(const Device& device, const Shader::Info& info) {
    DescriptorLayoutBuilder builder;
    builder.Add(info, VK_SHADER_STAGE_COMPUTE_BIT);
    return builder.Create(device.GetLogical());
}
} // Anonymous namespace

ComputePipeline::ComputePipeline(const Device& device, VKDescriptorPool& descriptor_pool,
                                 VKUpdateDescriptorQueue& update_descriptor_queue_,
                                 const Shader::Info& info_, vk::ShaderModule spv_module_)
    : update_descriptor_queue{&update_descriptor_queue_}, info{info_},
      spv_module(std::move(spv_module_)) {
    DescriptorLayoutTuple tuple{CreateLayout(device, info)};
    descriptor_set_layout = std::move(tuple.descriptor_set_layout);
    pipeline_layout = std::move(tuple.pipeline_layout);
    descriptor_update_template = std::move(tuple.descriptor_update_template);
    descriptor_allocator = DescriptorAllocator(descriptor_pool, *descriptor_set_layout);

    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroup_size_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT,
        .pNext = nullptr,
        .requiredSubgroupSize = GuestWarpSize,
    };
    pipeline = device.GetLogical().CreateComputePipeline({
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = device.IsExtSubgroupSizeControlSupported() ? &subgroup_size_ci : nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = *spv_module,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
        .layout = *pipeline_layout,
        .basePipelineHandle = 0,
        .basePipelineIndex = 0,
    });
}

void ComputePipeline::ConfigureBufferCache(BufferCache& buffer_cache) {
    buffer_cache.SetEnabledComputeUniformBuffers(info.constant_buffer_mask);
    buffer_cache.UnbindComputeStorageBuffers();
    size_t index{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        ASSERT(desc.count == 1);
        buffer_cache.BindComputeStorageBuffer(index, desc.cbuf_index, desc.cbuf_offset, true);
        ++index;
    }
    buffer_cache.UpdateComputeBuffers();
    buffer_cache.BindHostComputeBuffers();
}

void ComputePipeline::ConfigureTextureCache(Tegra::Engines::KeplerCompute& kepler_compute,
                                            Tegra::MemoryManager& gpu_memory,
                                            TextureCache& texture_cache) {
    texture_cache.SynchronizeComputeDescriptors();

    static constexpr size_t max_elements = 64;
    std::array<ImageId, max_elements> image_view_ids;
    boost::container::static_vector<u32, max_elements> image_view_indices;
    boost::container::static_vector<VkSampler, max_elements> samplers;

    const auto& launch_desc{kepler_compute.launch_description};
    const auto& cbufs{launch_desc.const_buffer_config};
    const bool via_header_index{launch_desc.linked_tsc};
    for (const auto& desc : info.texture_descriptors) {
        const u32 cbuf_index{desc.cbuf_index};
        const u32 cbuf_offset{desc.cbuf_offset};
        ASSERT(((launch_desc.const_buffer_enable_mask >> cbuf_index) & 1) != 0);

        const GPUVAddr addr{cbufs[cbuf_index].Address() + cbuf_offset};
        const u32 raw_handle{gpu_memory.Read<u32>(addr)};

        const TextureHandle handle(raw_handle, via_header_index);
        image_view_indices.push_back(handle.image);

        Sampler* const sampler = texture_cache.GetComputeSampler(handle.sampler);
        samplers.push_back(sampler->Handle());
    }
    const std::span indices_span(image_view_indices.data(), image_view_indices.size());
    texture_cache.FillComputeImageViews(indices_span, image_view_ids);

    size_t index{};
    PushImageDescriptors(info, samplers.data(), image_view_ids.data(), texture_cache,
                         *update_descriptor_queue, index);
}

VkDescriptorSet ComputePipeline::UpdateDescriptorSet() {
    const VkDescriptorSet descriptor_set{descriptor_allocator.Commit()};
    update_descriptor_queue->Send(*descriptor_update_template, descriptor_set);
    return descriptor_set;
}

} // namespace Vulkan
