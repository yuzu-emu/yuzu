// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <filesystem>
#include <stop_token>
#include <unordered_map>

#include <glad/glad.h>

#include "common/common_types.h"
#include "common/thread_worker.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/host_translate_info.h"
#include "shader_recompiler/object_pool.h"
#include "shader_recompiler/profile.h"
#include "video_core/renderer_opengl/gl_compute_pipeline.h"
#include "video_core/renderer_opengl/gl_graphics_pipeline.h"
#include "video_core/renderer_opengl/gl_shader_context.h"
#include "video_core/shader_cache.h"

namespace Tegra {
class MemoryManager;
}

namespace OpenGL {

class Device;
class ProgramManager;
class RasterizerOpenGL;
using ShaderWorker = Common::StatefulThreadWorker<ShaderContext::Context>;

class ShaderCache : public VideoCommon::ShaderCache {
public:
    explicit ShaderCache(RasterizerOpenGL& rasterizer_, Core::Frontend::EmuWindow& emu_window_,
                         Tegra::Engines::Maxwell3D& maxwell3d_,
                         Tegra::Engines::KeplerCompute& kepler_compute_,
                         Tegra::MemoryManager& gpu_memory_, const Device& device_,
                         TextureCache& texture_cache_, BufferCache& buffer_cache_,
                         ProgramManager& program_manager_, StateTracker& state_tracker_,
                         VideoCore::ShaderNotify& shader_notify_);
    ~ShaderCache();

    void LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                           const VideoCore::DiskResourceLoadCallback& callback);

    [[nodiscard]] GraphicsPipeline* CurrentGraphicsPipeline();

    [[nodiscard]] ComputePipeline* CurrentComputePipeline();

private:
    GraphicsPipeline* CurrentGraphicsPipelineSlowPath();

    [[nodiscard]] GraphicsPipeline* BuiltPipeline(GraphicsPipeline* pipeline) const noexcept;

    std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline();

    std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(
        ShaderContext::ShaderPools& pools, const GraphicsPipelineKey& key,
        std::span<Shader::Environment* const> envs, bool build_in_parallel);

    std::unique_ptr<ComputePipeline> CreateComputePipeline(const ComputePipelineKey& key,
                                                           const VideoCommon::ShaderInfo* shader);

    std::unique_ptr<ComputePipeline> CreateComputePipeline(ShaderContext::ShaderPools& pools,
                                                           const ComputePipelineKey& key,
                                                           Shader::Environment& env);

    std::unique_ptr<ShaderWorker> CreateWorkers() const;

    Core::Frontend::EmuWindow& emu_window;
    const Device& device;
    TextureCache& texture_cache;
    BufferCache& buffer_cache;
    ProgramManager& program_manager;
    StateTracker& state_tracker;
    VideoCore::ShaderNotify& shader_notify;
    const bool use_asynchronous_shaders;

    GraphicsPipelineKey graphics_key{};
    GraphicsPipeline* current_pipeline{};

    ShaderContext::ShaderPools main_pools;
    std::unordered_map<GraphicsPipelineKey, std::unique_ptr<GraphicsPipeline>> graphics_cache;
    std::unordered_map<ComputePipelineKey, std::unique_ptr<ComputePipeline>> compute_cache;

    Shader::Profile profile;
    Shader::HostTranslateInfo host_info;

    std::filesystem::path shader_cache_filename;
    std::unique_ptr<ShaderWorker> workers;
};

} // namespace OpenGL
