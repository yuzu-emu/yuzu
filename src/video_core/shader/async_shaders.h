// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <memory>
#include <shared_mutex>
#include <thread>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Core::Frontend {
class EmuWindow;
class GraphicsContext;
} // namespace Core::Frontend

namespace Tegra {
class GPU;
}

namespace Vulkan {
class VKPipelineCache;
}

namespace VideoCommon::Shader {

class AsyncShaders {
public:
    enum class Backend {
        OpenGL,
        GLASM,
        Vulkan,
    };

    struct ResultPrograms {
        OpenGL::OGLProgram opengl;
        OpenGL::OGLAssemblyProgram glasm;
    };

    struct Result {
        u64 uid;
        VAddr cpu_address;
        Backend backend;
        ResultPrograms program;
        std::vector<u64> code;
        std::vector<u64> code_b;
        Tegra::Engines::ShaderType shader_type;
    };

    explicit AsyncShaders(Core::Frontend::EmuWindow& emu_window_, bool allocate_workers);
    ~AsyncShaders();

    /// Check to see if any shaders have actually been compiled
    [[nodiscard]] bool HasCompletedWork() const;

    /// Deduce if a shader can be build on another thread of MUST be built in sync. We cannot build
    /// every shader async as some shaders are only built and executed once. We try to "guess" which
    /// shader would be used only once
    [[nodiscard]] bool IsShaderAsync(const Tegra::GPU& gpu) const;

    /// Pulls completed compiled shaders
    [[nodiscard]] std::vector<Result> GetCompletedWork();

    void QueueOpenGLShader(const OpenGL::Device& device, Tegra::Engines::ShaderType shader_type,
                           u64 uid, std::vector<u64> code, std::vector<u64> code_b, u32 main_offset,
                           CompilerSettings compiler_settings, const Registry& registry,
                           VAddr cpu_addr);

    void QueueVulkanShader(Vulkan::VKPipelineCache* pp_cache, const Vulkan::Device& device,
                           Vulkan::VKScheduler& scheduler,
                           Vulkan::VKDescriptorPool& descriptor_pool,
                           Vulkan::VKUpdateDescriptorQueue& update_descriptor_queue,
                           std::vector<VkDescriptorSetLayoutBinding> bindings,
                           Vulkan::SPIRVProgram program, Vulkan::GraphicsPipelineCacheKey key,
                           u32 num_color_buffers);

private:
    void ShaderCompilerThread(std::stop_token stop_token, Core::Frontend::GraphicsContext* context);

    /// Check our worker queue to see if we have any work queued already
    [[nodiscard]] bool HasWorkQueued() const;

    struct WorkerParams {
        Backend backend;
        // For OGL
        const OpenGL::Device* device;
        Tegra::Engines::ShaderType shader_type;
        u64 uid;
        std::vector<u64> code;
        std::vector<u64> code_b;
        u32 main_offset;
        CompilerSettings compiler_settings;
        std::optional<Registry> registry;
        VAddr cpu_address;

        // For Vulkan
        Vulkan::VKPipelineCache* pp_cache;
        const Vulkan::Device* vk_device;
        Vulkan::VKScheduler* scheduler;
        Vulkan::VKDescriptorPool* descriptor_pool;
        Vulkan::VKUpdateDescriptorQueue* update_descriptor_queue;
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        Vulkan::SPIRVProgram program;
        Vulkan::GraphicsPipelineCacheKey key;
        u32 num_color_buffers;
    };

    std::condition_variable_any cv;
    mutable std::mutex queue_mutex;
    mutable std::shared_mutex completed_mutex;
    std::vector<std::unique_ptr<Core::Frontend::GraphicsContext>> context_list;
    std::vector<std::jthread> worker_threads;
    std::queue<WorkerParams> pending_queue;
    std::vector<Result> finished_work;
    Core::Frontend::EmuWindow& emu_window;
};

} // namespace VideoCommon::Shader
