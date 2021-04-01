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

ComputePipeline::ComputePipeline(const Device& device, VKDescriptorPool& descriptor_pool,
                                 VKUpdateDescriptorQueue& update_descriptor_queue_,
                                 Common::ThreadWorker* thread_worker, const Shader::Info& info_,
                                 vk::ShaderModule spv_module_)
    : update_descriptor_queue{update_descriptor_queue_}, info{info_},
      spv_module(std::move(spv_module_)) {
    DescriptorLayoutBuilder builder{device.GetLogical()};
    builder.Add(info, VK_SHADER_STAGE_COMPUTE_BIT);

    descriptor_set_layout = builder.CreateDescriptorSetLayout();
    pipeline_layout = builder.CreatePipelineLayout(*descriptor_set_layout);
    descriptor_update_template = builder.CreateTemplate(*descriptor_set_layout, *pipeline_layout);
    descriptor_allocator = DescriptorAllocator(descriptor_pool, *descriptor_set_layout);

    auto func{[this, &device] {
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
        building_flag.test_and_set();
        building_flag.notify_all();
    }};
    if (thread_worker) {
        thread_worker->QueueWork(std::move(func));
    } else {
        func();
    }
}

void ComputePipeline::Configure(Tegra::Engines::KeplerCompute& kepler_compute,
                                Tegra::MemoryManager& gpu_memory, VKScheduler& scheduler,
                                BufferCache& buffer_cache, TextureCache& texture_cache) {
    update_descriptor_queue.Acquire();

    buffer_cache.SetEnabledComputeUniformBuffers(info.constant_buffer_mask);
    buffer_cache.UnbindComputeStorageBuffers();
    size_t ssbo_index{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        ASSERT(desc.count == 1);
        buffer_cache.BindComputeStorageBuffer(ssbo_index, desc.cbuf_index, desc.cbuf_offset, true);
        ++ssbo_index;
    }
    buffer_cache.UpdateComputeBuffers();
    buffer_cache.BindHostComputeBuffers();

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

    size_t image_index{};
    PushImageDescriptors(info, samplers.data(), image_view_ids.data(), texture_cache,
                         update_descriptor_queue, image_index);

    if (!building_flag.test()) {
        // Wait for the pipeline to be built
        scheduler.Record([this](vk::CommandBuffer) { building_flag.wait(false); });
    }
    scheduler.Record([this](vk::CommandBuffer cmdbuf) {
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    });
    if (!descriptor_set_layout) {
        return;
    }
    const VkDescriptorSet descriptor_set{descriptor_allocator.Commit()};
    update_descriptor_queue.Send(descriptor_update_template.address(), descriptor_set);
    scheduler.Record([this, descriptor_set](vk::CommandBuffer cmdbuf) {
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline_layout, 0,
                                  descriptor_set, nullptr);
    });
}

} // namespace Vulkan
