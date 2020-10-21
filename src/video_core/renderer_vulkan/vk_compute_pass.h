// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <utility>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class VKDevice;
class VKScheduler;
class VKStagingBufferPool;
class VKUpdateDescriptorQueue;

class VKComputePass {
public:
    explicit VKComputePass(const VKDevice& device, VKDescriptorPool& descriptor_pool,
                           vk::Span<VkDescriptorSetLayoutBinding> bindings,
                           vk::Span<VkDescriptorUpdateTemplateEntryKHR> templates,
                           vk::Span<VkPushConstantRange> push_constants, std::size_t code_size,
                           const u8* code);
    ~VKComputePass();

protected:
    VkDescriptorSet CommitDescriptorSet(VKUpdateDescriptorQueue& update_descriptor_queue);

    vk::DescriptorUpdateTemplateKHR descriptor_template;
    vk::PipelineLayout layout;
    vk::Pipeline pipeline;

private:
    vk::DescriptorSetLayout descriptor_set_layout;
    std::optional<DescriptorAllocator> descriptor_allocator;
    vk::ShaderModule shader_module;
};

class QuadArrayPass final : public VKComputePass {
public:
    explicit QuadArrayPass(const VKDevice& device, VKScheduler& scheduler,
                           VKDescriptorPool& descriptor_pool,
                           VKStagingBufferPool& staging_buffer_pool,
                           VKUpdateDescriptorQueue& update_descriptor_queue);
    ~QuadArrayPass();

    std::pair<VkBuffer, VkDeviceSize> Assemble(u32 num_vertices, u32 first);

private:
    VKScheduler& scheduler;
    VKStagingBufferPool& staging_buffer_pool;
    VKUpdateDescriptorQueue& update_descriptor_queue;
};

class Uint8Pass final : public VKComputePass {
public:
    explicit Uint8Pass(const VKDevice& device, VKScheduler& scheduler,
                       VKDescriptorPool& descriptor_pool, VKStagingBufferPool& staging_buffer_pool,
                       VKUpdateDescriptorQueue& update_descriptor_queue);
    ~Uint8Pass();

    std::pair<VkBuffer, u64> Assemble(u32 num_vertices, VkBuffer src_buffer, u64 src_offset);

private:
    VKScheduler& scheduler;
    VKStagingBufferPool& staging_buffer_pool;
    VKUpdateDescriptorQueue& update_descriptor_queue;
};

class QuadIndexedPass final : public VKComputePass {
public:
    explicit QuadIndexedPass(const VKDevice& device, VKScheduler& scheduler,
                             VKDescriptorPool& descriptor_pool,
                             VKStagingBufferPool& staging_buffer_pool,
                             VKUpdateDescriptorQueue& update_descriptor_queue);
    ~QuadIndexedPass();

    std::pair<VkBuffer, u64> Assemble(Tegra::Engines::Maxwell3D::Regs::IndexFormat index_format,
                                      u32 num_vertices, u32 base_vertex, VkBuffer src_buffer,
                                      u64 src_offset);

private:
    VKScheduler& scheduler;
    VKStagingBufferPool& staging_buffer_pool;
    VKUpdateDescriptorQueue& update_descriptor_queue;
};

} // namespace Vulkan
