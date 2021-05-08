// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "shader_recompiler/backend/glasm/emit_glasm.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/profile.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_environment.h"
#include "video_core/shader_notify.h"

namespace OpenGL {
namespace {
// FIXME: Move this somewhere else
const Shader::Profile profile{
    .supported_spirv = 0x00010000,

    .unified_descriptor_binding = false,
    .support_descriptor_aliasing = false,
    .support_int8 = false,
    .support_int16 = false,
    .support_vertex_instance_id = true,
    .support_float_controls = false,
    .support_separate_denorm_behavior = false,
    .support_separate_rounding_mode = false,
    .support_fp16_denorm_preserve = false,
    .support_fp32_denorm_preserve = false,
    .support_fp16_denorm_flush = false,
    .support_fp32_denorm_flush = false,
    .support_fp16_signed_zero_nan_preserve = false,
    .support_fp32_signed_zero_nan_preserve = false,
    .support_fp64_signed_zero_nan_preserve = false,
    .support_explicit_workgroup_layout = false,
    .support_vote = true,
    .support_viewport_index_layer_non_geometry = true,
    .support_viewport_mask = true,
    .support_typeless_image_loads = true,
    .support_demote_to_helper_invocation = false,
    .warp_size_potentially_larger_than_guest = true,
    .support_int64_atomics = false,
    .lower_left_origin_mode = true,

    .has_broken_spirv_clamp = true,
    .has_broken_unsigned_image_offsets = true,
    .has_broken_signed_operations = true,
    .ignore_nan_fp_comparisons = true,

    .generic_input_types = {},
    .convert_depth_mode = false,
    .force_early_z = false,

    .tess_primitive = {},
    .tess_spacing = {},
    .tess_clockwise = false,

    .input_topology = Shader::InputTopology::Triangles,

    .fixed_state_point_size = std::nullopt,

    .alpha_test_func = Shader::CompareFunction::Always,
    .alpha_test_reference = 0.0f,

    .y_negate = false,

    .xfb_varyings = {},
};

using Shader::Backend::GLASM::EmitGLASM;
using Shader::Backend::SPIRV::EmitSPIRV;
using Shader::Maxwell::TranslateProgram;
using VideoCommon::ComputeEnvironment;
using VideoCommon::GraphicsEnvironment;

template <typename Container>
auto MakeSpan(Container& container) {
    return std::span(container.data(), container.size());
}

void AddShader(GLenum stage, GLuint program, std::span<const u32> code) {
    OGLShader shader;
    shader.handle = glCreateShader(stage);

    glShaderBinary(1, &shader.handle, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, code.data(),
                   static_cast<GLsizei>(code.size_bytes()));
    glSpecializeShader(shader.handle, "main", 0, nullptr, nullptr);
    glAttachShader(program, shader.handle);
    if (!Settings::values.renderer_debug) {
        return;
    }
    GLint shader_status{};
    glGetShaderiv(shader.handle, GL_COMPILE_STATUS, &shader_status);
    if (shader_status == GL_FALSE) {
        LOG_ERROR(Render_OpenGL, "Failed to build shader");
    }
    GLint log_length{};
    glGetShaderiv(shader.handle, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length == 0) {
        return;
    }
    std::string log(log_length, 0);
    glGetShaderInfoLog(shader.handle, log_length, nullptr, log.data());
    if (shader_status == GL_FALSE) {
        LOG_ERROR(Render_OpenGL, "{}", log);
    } else {
        LOG_WARNING(Render_OpenGL, "{}", log);
    }
}

void LinkProgram(GLuint program) {
    glLinkProgram(program);
    if (!Settings::values.renderer_debug) {
        return;
    }
    GLint link_status{};
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);

    GLint log_length{};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length == 0) {
        return;
    }
    std::string log(log_length, 0);
    glGetProgramInfoLog(program, log_length, nullptr, log.data());
    if (link_status == GL_FALSE) {
        LOG_ERROR(Render_OpenGL, "{}", log);
    } else {
        LOG_WARNING(Render_OpenGL, "{}", log);
    }
}

OGLAssemblyProgram CompileProgram(std::string_view code, GLenum target) {
    OGLAssemblyProgram program;
    glGenProgramsARB(1, &program.handle);
    glNamedProgramStringEXT(program.handle, target, GL_PROGRAM_FORMAT_ASCII_ARB,
                            static_cast<GLsizei>(code.size()), code.data());
    if (!Settings::values.renderer_debug) {
        return program;
    }
    const auto err = reinterpret_cast<const char*>(glGetString(GL_PROGRAM_ERROR_STRING_NV));
    if (err && *err) {
        LOG_CRITICAL(Render_OpenGL, "{}", err);
        LOG_INFO(Render_OpenGL, "{}", code);
    }
    return program;
}

GLenum Stage(size_t stage_index) {
    switch (stage_index) {
    case 0:
        return GL_VERTEX_SHADER;
    case 1:
        return GL_TESS_CONTROL_SHADER;
    case 2:
        return GL_TESS_EVALUATION_SHADER;
    case 3:
        return GL_GEOMETRY_SHADER;
    case 4:
        return GL_FRAGMENT_SHADER;
    }
    UNREACHABLE_MSG("{}", stage_index);
    return GL_NONE;
}
} // Anonymous namespace

ShaderCache::ShaderCache(RasterizerOpenGL& rasterizer_, Core::Frontend::EmuWindow& emu_window_,
                         Tegra::Engines::Maxwell3D& maxwell3d_,
                         Tegra::Engines::KeplerCompute& kepler_compute_,
                         Tegra::MemoryManager& gpu_memory_, const Device& device_,
                         TextureCache& texture_cache_, BufferCache& buffer_cache_,
                         ProgramManager& program_manager_, StateTracker& state_tracker_)
    : VideoCommon::ShaderCache{rasterizer_, gpu_memory_, maxwell3d_, kepler_compute_},
      emu_window{emu_window_}, device{device_}, texture_cache{texture_cache_},
      buffer_cache{buffer_cache_}, program_manager{program_manager_}, state_tracker{
                                                                          state_tracker_} {}

ShaderCache::~ShaderCache() = default;

GraphicsProgram* ShaderCache::CurrentGraphicsProgram() {
    if (!RefreshStages(graphics_key.unique_hashes)) {
        return nullptr;
    }
    const auto& regs{maxwell3d.regs};
    graphics_key.raw = 0;
    graphics_key.early_z.Assign(regs.force_early_fragment_tests != 0 ? 1 : 0);
    graphics_key.gs_input_topology.Assign(graphics_key.unique_hashes[4] != 0
                                              ? regs.draw.topology.Value()
                                              : Maxwell::PrimitiveTopology{});
    graphics_key.tessellation_primitive.Assign(regs.tess_mode.prim.Value());
    graphics_key.tessellation_spacing.Assign(regs.tess_mode.spacing.Value());
    graphics_key.tessellation_clockwise.Assign(regs.tess_mode.cw.Value());

    const auto [pair, is_new]{graphics_cache.try_emplace(graphics_key)};
    auto& program{pair->second};
    if (is_new) {
        program = CreateGraphicsProgram();
    }
    return program.get();
}

ComputeProgram* ShaderCache::CurrentComputeProgram() {
    const VideoCommon::ShaderInfo* const shader{ComputeShader()};
    if (!shader) {
        return nullptr;
    }
    const auto& qmd{kepler_compute.launch_description};
    const ComputeProgramKey key{
        .unique_hash = shader->unique_hash,
        .shared_memory_size = qmd.shared_alloc,
        .workgroup_size{qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z},
    };
    const auto [pair, is_new]{compute_cache.try_emplace(key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return pipeline.get();
    }
    pipeline = CreateComputeProgram(key, shader);
    return pipeline.get();
}

std::unique_ptr<GraphicsProgram> ShaderCache::CreateGraphicsProgram() {
    GraphicsEnvironments environments;
    GetGraphicsEnvironments(environments, graphics_key.unique_hashes);

    main_pools.ReleaseContents();
    return CreateGraphicsProgram(main_pools, graphics_key, environments.Span(), true);
}

std::unique_ptr<GraphicsProgram> ShaderCache::CreateGraphicsProgram(
    ShaderPools& pools, const GraphicsProgramKey& key, std::span<Shader::Environment* const> envs,
    bool build_in_parallel) {
    LOG_INFO(Render_OpenGL, "0x{:016x}", key.Hash());
    size_t env_index{0};
    std::array<Shader::IR::Program, Maxwell::MaxShaderProgram> programs;
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == 0) {
            continue;
        }
        Shader::Environment& env{*envs[env_index]};
        ++env_index;

        const u32 cfg_offset{static_cast<u32>(env.StartAddress() + sizeof(Shader::ProgramHeader))};
        Shader::Maxwell::Flow::CFG cfg(env, pools.flow_block, cfg_offset);
        programs[index] = TranslateProgram(pools.inst, pools.block, env, cfg);
    }
    std::array<const Shader::Info*, Maxwell::MaxShaderStage> infos{};

    OGLProgram gl_program;
    gl_program.handle = glCreateProgram();

    Shader::Backend::Bindings binding;
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == 0) {
            continue;
        }
        UNIMPLEMENTED_IF(index == 0);

        Shader::IR::Program& program{programs[index]};
        const size_t stage_index{index - 1};
        infos[stage_index] = &program.info;

        const std::vector<u32> code{EmitSPIRV(profile, program, binding)};
        AddShader(Stage(stage_index), gl_program.handle, code);
    }
    LinkProgram(gl_program.handle);

    return std::make_unique<GraphicsProgram>(texture_cache, buffer_cache, gpu_memory, maxwell3d,
                                             program_manager, state_tracker, std::move(gl_program),
                                             infos);
}

std::unique_ptr<ComputeProgram> ShaderCache::CreateComputeProgram(
    const ComputeProgramKey& key, const VideoCommon::ShaderInfo* shader) {
    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    ComputeEnvironment env{kepler_compute, gpu_memory, program_base, qmd.program_start};
    env.SetCachedSize(shader->size_bytes);

    main_pools.ReleaseContents();
    return CreateComputeProgram(main_pools, key, env, true);
}

std::unique_ptr<ComputeProgram> ShaderCache::CreateComputeProgram(ShaderPools& pools,
                                                                  const ComputeProgramKey& key,
                                                                  Shader::Environment& env,
                                                                  bool build_in_parallel) {
    LOG_INFO(Render_OpenGL, "0x{:016x}", key.Hash());

    Shader::Maxwell::Flow::CFG cfg{env, pools.flow_block, env.StartAddress()};
    Shader::IR::Program program{TranslateProgram(pools.inst, pools.block, env, cfg)};
    OGLAssemblyProgram asm_program;
    OGLProgram source_program;
    if (device.UseAssemblyShaders()) {
        const std::string code{EmitGLASM(profile, program)};
        asm_program = CompileProgram(code, GL_COMPUTE_PROGRAM_NV);
    } else {
        const std::vector<u32> code{EmitSPIRV(profile, program)};
        source_program.handle = glCreateProgram();
        AddShader(GL_COMPUTE_SHADER, source_program.handle, code);
        LinkProgram(source_program.handle);
    }
    return std::make_unique<ComputeProgram>(texture_cache, buffer_cache, gpu_memory, kepler_compute,
                                            program_manager, program.info,
                                            std::move(source_program), std::move(asm_program));
}

} // namespace OpenGL
