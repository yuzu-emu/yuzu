// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include <boost/container/small_vector.hpp>

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
vk::DescriptorSetLayout CreateDescriptorSetLayout(const Device& device, const Shader::Info& info) {
    boost::container::small_vector<VkDescriptorSetLayoutBinding, 24> bindings;
    u32 binding{};
    for ([[maybe_unused]] const auto& desc : info.constant_buffer_descriptors) {
        bindings.push_back({
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        });
        ++binding;
    }
    for ([[maybe_unused]] const auto& desc : info.storage_buffers_descriptors) {
        bindings.push_back({
            .binding = binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        });
        ++binding;
    }
    return device.GetLogical().CreateDescriptorSetLayout({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    });
}

vk::DescriptorUpdateTemplateKHR CreateDescriptorUpdateTemplate(
    const Device& device, const Shader::Info& info, VkDescriptorSetLayout descriptor_set_layout,
    VkPipelineLayout pipeline_layout) {
    boost::container::small_vector<VkDescriptorUpdateTemplateEntry, 24> entries;
    size_t offset{};
    u32 binding{};
    for ([[maybe_unused]] const auto& desc : info.constant_buffer_descriptors) {
        entries.push_back({
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .offset = offset,
            .stride = sizeof(DescriptorUpdateEntry),
        });
        ++binding;
        offset += sizeof(DescriptorUpdateEntry);
    }
    for ([[maybe_unused]] const auto& desc : info.storage_buffers_descriptors) {
        entries.push_back({
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = offset,
            .stride = sizeof(DescriptorUpdateEntry),
        });
        ++binding;
        offset += sizeof(DescriptorUpdateEntry);
    }
    return device.GetLogical().CreateDescriptorUpdateTemplateKHR({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .descriptorUpdateEntryCount = static_cast<u32>(entries.size()),
        .pDescriptorUpdateEntries = entries.data(),
        .templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET,
        .descriptorSetLayout = descriptor_set_layout,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE,
        .pipelineLayout = pipeline_layout,
        .set = 0,
    });
}
} // Anonymous namespace

ComputePipeline::ComputePipeline(const Device& device, VKDescriptorPool& descriptor_pool,
                                 VKUpdateDescriptorQueue& update_descriptor_queue_,
                                 const Shader::Info& info_, vk::ShaderModule spv_module_)
    : update_descriptor_queue{&update_descriptor_queue_}, info{info_},
      spv_module(std::move(spv_module_)),
      descriptor_set_layout(CreateDescriptorSetLayout(device, info)),
      descriptor_allocator(descriptor_pool, *descriptor_set_layout),
      pipeline_layout{device.GetLogical().CreatePipelineLayout({
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .setLayoutCount = 1,
          .pSetLayouts = descriptor_set_layout.address(),
          .pushConstantRangeCount = 0,
          .pPushConstantRanges = nullptr,
      })},
      descriptor_update_template{
          CreateDescriptorUpdateTemplate(device, info, *descriptor_set_layout, *pipeline_layout)},
      pipeline{device.GetLogical().CreateComputePipeline({
          .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .stage{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = *spv_module,
              .pName = "main",
              .pSpecializationInfo = nullptr,
          },
          .layout = *pipeline_layout,
          .basePipelineHandle = 0,
          .basePipelineIndex = 0,
      })} {}

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

VkDescriptorSet ComputePipeline::UpdateDescriptorSet() {
    const VkDescriptorSet descriptor_set{descriptor_allocator.Commit()};
    update_descriptor_queue->Send(*descriptor_update_template, descriptor_set);
    return descriptor_set;
}

} // namespace Vulkan
