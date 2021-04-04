// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <condition_variable>
#include <atomic>

#include "common/common_types.h"
#include "common/thread_worker.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class VKScheduler;

class ComputePipeline {
public:
    explicit ComputePipeline(const Device& device, VKDescriptorPool& descriptor_pool,
                             VKUpdateDescriptorQueue& update_descriptor_queue,
                             Common::ThreadWorker* thread_worker, const Shader::Info& info,
                             vk::ShaderModule spv_module);

    ComputePipeline& operator=(ComputePipeline&&) noexcept = delete;
    ComputePipeline(ComputePipeline&&) noexcept = delete;

    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(const ComputePipeline&) = delete;

    void Configure(Tegra::Engines::KeplerCompute& kepler_compute, Tegra::MemoryManager& gpu_memory,
                   VKScheduler& scheduler, BufferCache& buffer_cache, TextureCache& texture_cache);

private:
    VKUpdateDescriptorQueue& update_descriptor_queue;
    Shader::Info info;

    vk::ShaderModule spv_module;
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
