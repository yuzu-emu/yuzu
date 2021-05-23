// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <unordered_map>

#include <glad/glad.h>

#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/object_pool.h"
#include "video_core/engines/shader_type.h"
#include "video_core/renderer_opengl/gl_compute_program.h"
#include "video_core/renderer_opengl/gl_graphics_program.h"
#include "video_core/shader_cache.h"

namespace Tegra {
class MemoryManager;
}

namespace Core::Frontend {
class EmuWindow;
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

    [[nodiscard]] GraphicsProgram* CurrentGraphicsProgram();

    [[nodiscard]] ComputeProgram* CurrentComputeProgram();

private:
    std::unique_ptr<GraphicsProgram> CreateGraphicsProgram();

    std::unique_ptr<GraphicsProgram> CreateGraphicsProgram(
        ShaderPools& pools, const GraphicsProgramKey& key,
        std::span<Shader::Environment* const> envs, bool build_in_parallel);

    std::unique_ptr<ComputeProgram> CreateComputeProgram(const ComputeProgramKey& key,
                                                         const VideoCommon::ShaderInfo* shader);

    std::unique_ptr<ComputeProgram> CreateComputeProgram(ShaderPools& pools,
                                                         const ComputeProgramKey& key,
                                                         Shader::Environment& env,
                                                         bool build_in_parallel);

    Core::Frontend::EmuWindow& emu_window;
    const Device& device;
    TextureCache& texture_cache;
    BufferCache& buffer_cache;
    ProgramManager& program_manager;
    StateTracker& state_tracker;

    GraphicsProgramKey graphics_key{};

    ShaderPools main_pools;
    std::unordered_map<GraphicsProgramKey, std::unique_ptr<GraphicsProgram>> graphics_cache;
    std::unordered_map<ComputeProgramKey, std::unique_ptr<ComputeProgram>> compute_cache;
};

} // namespace OpenGL
