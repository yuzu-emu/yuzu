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
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/object_pool.h"
#include "video_core/engines/shader_type.h"
#include "video_core/renderer_opengl/gl_compute_pipeline.h"
#include "video_core/renderer_opengl/gl_graphics_pipeline.h"
#include "video_core/shader_cache.h"

namespace Tegra {
class MemoryManager;
}

namespace OpenGL {

class Device;
class ProgramManager;
class RasterizerOpenGL;

struct ShaderPools {
    void ReleaseContents() {
        flow_block.ReleaseContents();
        block.ReleaseContents();
        inst.ReleaseContents();
    }

    Shader::ObjectPool<Shader::IR::Inst> inst;
    Shader::ObjectPool<Shader::IR::Block> block;
    Shader::ObjectPool<Shader::Maxwell::Flow::Block> flow_block;
};

class ShaderCache : public VideoCommon::ShaderCache {
public:
    explicit ShaderCache(RasterizerOpenGL& rasterizer_, Core::Frontend::EmuWindow& emu_window_,
                         Tegra::Engines::Maxwell3D& maxwell3d_,
                         Tegra::Engines::KeplerCompute& kepler_compute_,
                         Tegra::MemoryManager& gpu_memory_, const Device& device_,
                         TextureCache& texture_cache_, BufferCache& buffer_cache_,
                         ProgramManager& program_manager_, StateTracker& state_tracker_);
    ~ShaderCache();

    void LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                           const VideoCore::DiskResourceLoadCallback& callback);

    [[nodiscard]] GraphicsPipeline* CurrentGraphicsPipeline();

    [[nodiscard]] ComputePipeline* CurrentComputePipeline();

private:
    std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline();

    std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(
        ShaderPools& pools, const GraphicsPipelineKey& key,
        std::span<Shader::Environment* const> envs, bool build_in_parallel);

    std::unique_ptr<ComputePipeline> CreateComputePipeline(const ComputePipelineKey& key,
                                                           const VideoCommon::ShaderInfo* shader);

    std::unique_ptr<ComputePipeline> CreateComputePipeline(ShaderPools& pools,
                                                           const ComputePipelineKey& key,
                                                           Shader::Environment& env,
                                                           bool build_in_parallel);

    Core::Frontend::EmuWindow& emu_window;
    const Device& device;
    TextureCache& texture_cache;
    BufferCache& buffer_cache;
    ProgramManager& program_manager;
    StateTracker& state_tracker;

    GraphicsPipelineKey graphics_key{};

    ShaderPools main_pools;
    std::unordered_map<GraphicsPipelineKey, std::unique_ptr<GraphicsPipeline>> graphics_cache;
    std::unordered_map<ComputePipelineKey, std::unique_ptr<ComputePipeline>> compute_cache;

    Shader::Profile profile;
    std::filesystem::path shader_cache_filename;
};

} // namespace OpenGL
