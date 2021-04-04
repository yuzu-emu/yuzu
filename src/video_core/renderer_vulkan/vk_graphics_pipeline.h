// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include "common/thread_worker.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class RenderPassCache;
class VKScheduler;
class VKUpdateDescriptorQueue;

class GraphicsPipeline {
    static constexpr size_t NUM_STAGES = Tegra::Engines::Maxwell3D::Regs::MaxShaderStage;

public:
    explicit GraphicsPipeline(Tegra::Engines::Maxwell3D& maxwell3d,
                              Tegra::MemoryManager& gpu_memory, VKScheduler& scheduler,
                              BufferCache& buffer_cache, TextureCache& texture_cache,
                              const Device& device, VKDescriptorPool& descriptor_pool,
                              VKUpdateDescriptorQueue& update_descriptor_queue,
                              Common::ThreadWorker* worker_thread,
                              RenderPassCache& render_pass_cache, const FixedPipelineState& state,
                              std::array<vk::ShaderModule, NUM_STAGES> stages,
                              const std::array<const Shader::Info*, NUM_STAGES>& infos);

    void Configure(bool is_indexed);

    GraphicsPipeline& operator=(GraphicsPipeline&&) noexcept = delete;
    GraphicsPipeline(GraphicsPipeline&&) noexcept = delete;

    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
    GraphicsPipeline(const GraphicsPipeline&) = delete;

private:
    void MakePipeline(const Device& device, VkRenderPass render_pass);

    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::MemoryManager& gpu_memory;
    TextureCache& texture_cache;
    BufferCache& buffer_cache;
    VKScheduler& scheduler;
    VKUpdateDescriptorQueue& update_descriptor_queue;
    const FixedPipelineState state;

    std::array<vk::ShaderModule, NUM_STAGES> spv_modules;
    std::array<Shader::Info, NUM_STAGES> stage_infos;
    vk::DescriptorSetLayout descriptor_set_layout;
    DescriptorAllocator descriptor_allocator;
    vk::PipelineLayout pipeline_layout;
    vk::DescriptorUpdateTemplateKHR descriptor_update_template;
    vk::Pipeline pipeline;

    std::condition_variable build_condvar;
    std::mutex build_mutex;
    std::atomic_bool is_built{false};
};

} // namespace Vulkan
