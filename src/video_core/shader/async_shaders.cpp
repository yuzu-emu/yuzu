// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/shader/async_shaders.h"

namespace VideoCommon::Shader {

AsyncShaders::AsyncShaders(Core::Frontend::EmuWindow& emu_window_, bool allocate_workers)
    : emu_window(emu_window_) {
    if (!allocate_workers) {
        // Don't allocate workers unless it's requested
        return;
    }
    // Use at least one thread
    u32 num_workers = 1;
    // Deduce how many more threads we can use
    const u32 thread_count = std::thread::hardware_concurrency();
    if (thread_count >= 8) {
        // Increase async workers by 1 for every 2 threads >= 8
        num_workers += 1 + (thread_count - 8) / 2;
    }
    // Create workers
    context_list.reserve(num_workers);
    for (size_t i = 0; i < num_workers; i++) {
        const auto context = context_list.emplace_back(emu_window.CreateSharedContext()).get();
        worker_threads.emplace_back([this, context](std::stop_token stop_token) {
            ShaderCompilerThread(stop_token, context);
        });
    }
}

AsyncShaders::~AsyncShaders() = default;

bool AsyncShaders::HasWorkQueued() const {
    return !pending_queue.empty();
}

bool AsyncShaders::HasCompletedWork() const {
    std::shared_lock lock{completed_mutex};
    return !finished_work.empty();
}

bool AsyncShaders::IsShaderAsync(const Tegra::GPU& gpu) const {
    const auto& regs = gpu.Maxwell3D().regs;

    // If something is using depth, we can assume that games are not rendering anything which will
    // be used one time.
    if (regs.zeta_enable) {
        return true;
    }

    // If games are using a small index count, we can assume these are full screen quads. Usually
    // these shaders are only used once for building textures so we can assume they can't be built
    // async
    if (regs.index_array.count <= 6 || regs.vertex_buffer.count <= 6) {
        return false;
    }

    return true;
}

std::vector<AsyncShaders::Result> AsyncShaders::GetCompletedWork() {
    std::vector<Result> results;
    {
        std::unique_lock lock{completed_mutex};
        results = std::move(finished_work);
        finished_work.clear();
    }
    return results;
}

void AsyncShaders::QueueOpenGLShader(const OpenGL::Device& device,
                                     Tegra::Engines::ShaderType shader_type, u64 uid,
                                     std::vector<u64> code, std::vector<u64> code_b,
                                     u32 main_offset, CompilerSettings compiler_settings,
                                     const Registry& registry, VAddr cpu_addr) {
    std::unique_lock lock(queue_mutex);
    pending_queue.push({
        .backend = device.UseAssemblyShaders() ? Backend::GLASM : Backend::OpenGL,
        .device = &device,
        .shader_type = shader_type,
        .uid = uid,
        .code = std::move(code),
        .code_b = std::move(code_b),
        .main_offset = main_offset,
        .compiler_settings = compiler_settings,
        .registry = registry,
        .cpu_address = cpu_addr,
        .pp_cache = nullptr,
        .vk_device = nullptr,
        .scheduler = nullptr,
        .descriptor_pool = nullptr,
        .update_descriptor_queue = nullptr,
        .bindings{},
        .program{},
        .key{},
        .num_color_buffers = 0,
    });
    cv.notify_one();
}

void AsyncShaders::QueueVulkanShader(Vulkan::VKPipelineCache* pp_cache,
                                     const Vulkan::Device& device, Vulkan::VKScheduler& scheduler,
                                     Vulkan::VKDescriptorPool& descriptor_pool,
                                     Vulkan::VKUpdateDescriptorQueue& update_descriptor_queue,
                                     std::vector<VkDescriptorSetLayoutBinding> bindings,
                                     Vulkan::SPIRVProgram program,
                                     Vulkan::GraphicsPipelineCacheKey key, u32 num_color_buffers) {
    std::unique_lock lock(queue_mutex);
    pending_queue.push({
        .backend = Backend::Vulkan,
        .device = nullptr,
        .shader_type{},
        .uid = 0,
        .code{},
        .code_b{},
        .main_offset = 0,
        .compiler_settings{},
        .registry{},
        .cpu_address = 0,
        .pp_cache = pp_cache,
        .vk_device = &device,
        .scheduler = &scheduler,
        .descriptor_pool = &descriptor_pool,
        .update_descriptor_queue = &update_descriptor_queue,
        .bindings = std::move(bindings),
        .program = std::move(program),
        .key = key,
        .num_color_buffers = num_color_buffers,
    });
    cv.notify_one();
}

void AsyncShaders::ShaderCompilerThread(std::stop_token stop_token,
                                        Core::Frontend::GraphicsContext* context) {
    while (!stop_token.stop_requested()) {
        std::unique_lock lock{queue_mutex};
        cv.wait(lock, stop_token, [this] { return HasWorkQueued(); });
        if (stop_token.stop_requested()) {
            return;
        }

        // Partial lock to allow all threads to read at the same time
        if (!HasWorkQueued()) {
            continue;
        }
        // Another thread beat us, just unlock and wait for the next load
        if (pending_queue.empty()) {
            continue;
        }

        // Pull work from queue
        WorkerParams work = std::move(pending_queue.front());
        pending_queue.pop();
        lock.unlock();

        if (work.backend == Backend::OpenGL || work.backend == Backend::GLASM) {
            const ShaderIR ir(work.code, work.main_offset, work.compiler_settings, *work.registry);
            const auto scope = context->Acquire();
            auto program =
                OpenGL::BuildShader(*work.device, work.shader_type, work.uid, ir, *work.registry);
            Result result{};
            result.backend = work.backend;
            result.cpu_address = work.cpu_address;
            result.uid = work.uid;
            result.code = std::move(work.code);
            result.code_b = std::move(work.code_b);
            result.shader_type = work.shader_type;

            if (work.backend == Backend::OpenGL) {
                result.program.opengl = std::move(program->source_program);
            } else if (work.backend == Backend::GLASM) {
                result.program.glasm = std::move(program->assembly_program);
            }

            {
                std::unique_lock complete_lock(completed_mutex);
                finished_work.push_back(std::move(result));
            }
        } else if (work.backend == Backend::Vulkan) {
            auto pipeline = std::make_unique<Vulkan::VKGraphicsPipeline>(
                *work.vk_device, *work.scheduler, *work.descriptor_pool,
                *work.update_descriptor_queue, work.key, work.bindings, work.program,
                work.num_color_buffers);

            work.pp_cache->EmplacePipeline(std::move(pipeline));
        }
    }
}

} // namespace VideoCommon::Shader
