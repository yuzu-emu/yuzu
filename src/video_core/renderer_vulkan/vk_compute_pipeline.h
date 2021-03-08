// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_pipeline.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;

class ComputePipeline : public Pipeline {
public:
    explicit ComputePipeline() = default;
    explicit ComputePipeline(const Device& device, VKDescriptorPool& descriptor_pool,
                             VKUpdateDescriptorQueue& update_descriptor_queue,
                             const Shader::Info& info, vk::ShaderModule spv_module);

    ComputePipeline& operator=(ComputePipeline&&) noexcept = default;
    ComputePipeline(ComputePipeline&&) noexcept = default;

    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(const ComputePipeline&) = delete;

    void ConfigureBufferCache(BufferCache& buffer_cache);
    void ConfigureTextureCache(Tegra::Engines::KeplerCompute& kepler_compute,
                               Tegra::MemoryManager& gpu_memory, TextureCache& texture_cache);

    [[nodiscard]] VkDescriptorSet UpdateDescriptorSet();

    [[nodiscard]] VkPipeline Handle() const noexcept {
        return *pipeline;
    }

    [[nodiscard]] VkPipelineLayout PipelineLayout() const noexcept {
        return *pipeline_layout;
    }

private:
    VKUpdateDescriptorQueue* update_descriptor_queue;
    Shader::Info info;

    vk::ShaderModule spv_module;
    vk::DescriptorSetLayout descriptor_set_layout;
    DescriptorAllocator descriptor_allocator;
    vk::PipelineLayout pipeline_layout;
    vk::DescriptorUpdateTemplateKHR descriptor_update_template;
    vk::Pipeline pipeline;
};

} // namespace Vulkan
