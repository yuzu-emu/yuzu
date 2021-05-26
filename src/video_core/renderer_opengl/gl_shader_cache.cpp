// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/thread_worker.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "shader_recompiler/backend/glasm/emit_glasm.h"
#include "shader_recompiler/backend/glsl/emit_glsl.h"
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
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_environment.h"
#include "video_core/shader_notify.h"

namespace OpenGL {
namespace {
using Shader::Backend::GLASM::EmitGLASM;
using Shader::Backend::GLSL::EmitGLSL;
using Shader::Backend::SPIRV::EmitSPIRV;
using Shader::Maxwell::MergeDualVertexPrograms;
using Shader::Maxwell::TranslateProgram;
using VideoCommon::ComputeEnvironment;
using VideoCommon::FileEnvironment;
using VideoCommon::GenericEnvironment;
using VideoCommon::GraphicsEnvironment;
using VideoCommon::SerializePipeline;

template <typename Container>
auto MakeSpan(Container& container) {
    return std::span(container.data(), container.size());
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

GLenum AssemblyStage(size_t stage_index) {
    switch (stage_index) {
    case 0:
        return GL_VERTEX_PROGRAM_NV;
    case 1:
        return GL_TESS_CONTROL_PROGRAM_NV;
    case 2:
        return GL_TESS_EVALUATION_PROGRAM_NV;
    case 3:
        return GL_GEOMETRY_PROGRAM_NV;
    case 4:
        return GL_FRAGMENT_PROGRAM_NV;
    }
    UNREACHABLE_MSG("{}", stage_index);
    return GL_NONE;
}

Shader::RuntimeInfo MakeRuntimeInfo(const GraphicsPipelineKey& key,
                                    const Shader::IR::Program& program,
                                    bool glasm_use_storage_buffers) {
    Shader::RuntimeInfo info;
    switch (program.stage) {
    case Shader::Stage::TessellationEval:
        info.tess_clockwise = key.tessellation_clockwise != 0;
        info.tess_primitive = [&key] {
            switch (key.tessellation_primitive) {
            case Maxwell::TessellationPrimitive::Isolines:
                return Shader::TessPrimitive::Isolines;
            case Maxwell::TessellationPrimitive::Triangles:
                return Shader::TessPrimitive::Triangles;
            case Maxwell::TessellationPrimitive::Quads:
                return Shader::TessPrimitive::Quads;
            }
            UNREACHABLE();
            return Shader::TessPrimitive::Triangles;
        }();
        info.tess_spacing = [&] {
            switch (key.tessellation_spacing) {
            case Maxwell::TessellationSpacing::Equal:
                return Shader::TessSpacing::Equal;
            case Maxwell::TessellationSpacing::FractionalOdd:
                return Shader::TessSpacing::FractionalOdd;
            case Maxwell::TessellationSpacing::FractionalEven:
                return Shader::TessSpacing::FractionalEven;
            }
            UNREACHABLE();
            return Shader::TessSpacing::Equal;
        }();
        break;
    case Shader::Stage::Fragment:
        info.force_early_z = key.early_z != 0;
        break;
    default:
        break;
    }
    switch (key.gs_input_topology) {
    case Maxwell::PrimitiveTopology::Points:
        info.input_topology = Shader::InputTopology::Points;
        break;
    case Maxwell::PrimitiveTopology::Lines:
    case Maxwell::PrimitiveTopology::LineLoop:
    case Maxwell::PrimitiveTopology::LineStrip:
        info.input_topology = Shader::InputTopology::Lines;
        break;
    case Maxwell::PrimitiveTopology::Triangles:
    case Maxwell::PrimitiveTopology::TriangleStrip:
    case Maxwell::PrimitiveTopology::TriangleFan:
    case Maxwell::PrimitiveTopology::Quads:
    case Maxwell::PrimitiveTopology::QuadStrip:
    case Maxwell::PrimitiveTopology::Polygon:
    case Maxwell::PrimitiveTopology::Patches:
        info.input_topology = Shader::InputTopology::Triangles;
        break;
    case Maxwell::PrimitiveTopology::LinesAdjacency:
    case Maxwell::PrimitiveTopology::LineStripAdjacency:
        info.input_topology = Shader::InputTopology::LinesAdjacency;
        break;
    case Maxwell::PrimitiveTopology::TrianglesAdjacency:
    case Maxwell::PrimitiveTopology::TriangleStripAdjacency:
        info.input_topology = Shader::InputTopology::TrianglesAdjacency;
        break;
    }
    info.glasm_use_storage_buffers = glasm_use_storage_buffers;
    return info;
}

void SetXfbState(VideoCommon::TransformFeedbackState& state, const Maxwell& regs) {
    std::ranges::transform(regs.tfb_layouts, state.layouts.begin(), [](const auto& layout) {
        return VideoCommon::TransformFeedbackState::Layout{
            .stream = layout.stream,
            .varying_count = layout.varying_count,
            .stride = layout.stride,
        };
    });
    state.varyings = regs.tfb_varying_locs;
}
} // Anonymous namespace

struct ShaderCache::Context {
    explicit Context(Core::Frontend::EmuWindow& emu_window)
        : gl_context{emu_window.CreateSharedContext()}, scoped{*gl_context} {}

    std::unique_ptr<Core::Frontend::GraphicsContext> gl_context;
    Core::Frontend::GraphicsContext::Scoped scoped;
    ShaderPools pools;
};

ShaderCache::ShaderCache(RasterizerOpenGL& rasterizer_, Core::Frontend::EmuWindow& emu_window_,
                         Tegra::Engines::Maxwell3D& maxwell3d_,
                         Tegra::Engines::KeplerCompute& kepler_compute_,
                         Tegra::MemoryManager& gpu_memory_, const Device& device_,
                         TextureCache& texture_cache_, BufferCache& buffer_cache_,
                         ProgramManager& program_manager_, StateTracker& state_tracker_)
    : VideoCommon::ShaderCache{rasterizer_, gpu_memory_, maxwell3d_, kepler_compute_},
      emu_window{emu_window_}, device{device_}, texture_cache{texture_cache_},
      buffer_cache{buffer_cache_}, program_manager{program_manager_}, state_tracker{state_tracker_},
      use_asynchronous_shaders{device.UseAsynchronousShaders()},
      profile{
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
          .support_viewport_index_layer_non_geometry =
              device.HasNvViewportArray2() || device.HasVertexViewportLayer(),
          .support_viewport_mask = device.HasNvViewportArray2(),
          .support_typeless_image_loads = device.HasImageLoadFormatted(),
          .support_demote_to_helper_invocation = false,
          .support_int64_atomics = false,
          .support_derivative_control = device.HasDerivativeControl(),
          .support_gl_nv_gpu_shader_5 = device.HasNvGpuShader5(),
          .support_gl_amd_gpu_shader_half_float = device.HasAmdShaderHalfFloat(),

          .warp_size_potentially_larger_than_guest = true,

          .lower_left_origin_mode = true,
          .need_declared_frag_colors = true,

          .has_broken_spirv_clamp = true,
          .has_broken_unsigned_image_offsets = true,
          .has_broken_signed_operations = true,
          .ignore_nan_fp_comparisons = true,
      } {
    if (use_asynchronous_shaders) {
        workers = CreateWorkers();
    }
}

ShaderCache::~ShaderCache() = default;

void ShaderCache::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                    const VideoCore::DiskResourceLoadCallback& callback) {
    if (title_id == 0) {
        return;
    }
    const auto shader_dir{Common::FS::GetYuzuPath(Common::FS::YuzuPath::ShaderDir)};
    const auto base_dir{shader_dir / fmt::format("{:016x}", title_id)};
    if (!Common::FS::CreateDir(shader_dir) || !Common::FS::CreateDir(base_dir)) {
        LOG_ERROR(Common_Filesystem, "Failed to create shader cache directories");
        return;
    }
    shader_cache_filename = base_dir / "opengl.bin";

    if (!workers) {
        workers = CreateWorkers();
    }
    struct {
        std::mutex mutex;
        size_t total{};
        size_t built{};
        bool has_loaded{};
    } state;

    const auto load_compute{[&](std::ifstream& file, FileEnvironment env) {
        ComputePipelineKey key;
        file.read(reinterpret_cast<char*>(&key), sizeof(key));
        workers->QueueWork(
            [this, key, env = std::move(env), &state, &callback](Context* ctx) mutable {
                ctx->pools.ReleaseContents();
                auto pipeline{CreateComputePipeline(ctx->pools, key, env)};
                std::lock_guard lock{state.mutex};
                if (pipeline) {
                    compute_cache.emplace(key, std::move(pipeline));
                }
                ++state.built;
                if (state.has_loaded) {
                    callback(VideoCore::LoadCallbackStage::Build, state.built, state.total);
                }
            });
        ++state.total;
    }};
    const auto load_graphics{[&](std::ifstream& file, std::vector<FileEnvironment> envs) {
        GraphicsPipelineKey key;
        file.read(reinterpret_cast<char*>(&key), sizeof(key));
        workers->QueueWork(
            [this, key, envs = std::move(envs), &state, &callback](Context* ctx) mutable {
                boost::container::static_vector<Shader::Environment*, 5> env_ptrs;
                for (auto& env : envs) {
                    env_ptrs.push_back(&env);
                }
                ctx->pools.ReleaseContents();
                auto pipeline{CreateGraphicsPipeline(ctx->pools, key, MakeSpan(env_ptrs))};
                std::lock_guard lock{state.mutex};
                if (pipeline) {
                    graphics_cache.emplace(key, std::move(pipeline));
                }
                ++state.built;
                if (state.has_loaded) {
                    callback(VideoCore::LoadCallbackStage::Build, state.built, state.total);
                }
            });
        ++state.total;
    }};
    VideoCommon::LoadPipelines(stop_loading, shader_cache_filename, load_compute, load_graphics);

    std::unique_lock lock{state.mutex};
    callback(VideoCore::LoadCallbackStage::Build, 0, state.total);
    state.has_loaded = true;
    lock.unlock();

    workers->WaitForRequests();
    if (!use_asynchronous_shaders) {
        workers.reset();
    }
}

GraphicsPipeline* ShaderCache::CurrentGraphicsPipeline() {
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
    graphics_key.xfb_enabled.Assign(regs.tfb_enabled != 0 ? 1 : 0);
    if (graphics_key.xfb_enabled) {
        SetXfbState(graphics_key.xfb_state, regs);
    }
    const auto [pair, is_new]{graphics_cache.try_emplace(graphics_key)};
    auto& program{pair->second};
    if (is_new) {
        program = CreateGraphicsPipeline();
    }
    return program.get();
}

ComputePipeline* ShaderCache::CurrentComputePipeline() {
    const VideoCommon::ShaderInfo* const shader{ComputeShader()};
    if (!shader) {
        return nullptr;
    }
    const auto& qmd{kepler_compute.launch_description};
    const ComputePipelineKey key{
        .unique_hash = shader->unique_hash,
        .shared_memory_size = qmd.shared_alloc,
        .workgroup_size{qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z},
    };
    const auto [pair, is_new]{compute_cache.try_emplace(key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return pipeline.get();
    }
    pipeline = CreateComputePipeline(key, shader);
    return pipeline.get();
}

std::unique_ptr<GraphicsPipeline> ShaderCache::CreateGraphicsPipeline() {
    GraphicsEnvironments environments;
    GetGraphicsEnvironments(environments, graphics_key.unique_hashes);

    main_pools.ReleaseContents();
    auto pipeline{CreateGraphicsPipeline(main_pools, graphics_key, environments.Span())};
    if (!pipeline || shader_cache_filename.empty()) {
        return pipeline;
    }
    boost::container::static_vector<const GenericEnvironment*, Maxwell::MaxShaderProgram> env_ptrs;
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (graphics_key.unique_hashes[index] != 0) {
            env_ptrs.push_back(&environments.envs[index]);
        }
    }
    SerializePipeline(graphics_key, env_ptrs, shader_cache_filename);
    return pipeline;
}

std::unique_ptr<GraphicsPipeline> ShaderCache::CreateGraphicsPipeline(
    ShaderPools& pools, const GraphicsPipelineKey& key,
    std::span<Shader::Environment* const> envs) try {
    LOG_INFO(Render_OpenGL, "0x{:016x}", key.Hash());
    size_t env_index{};
    u32 total_storage_buffers{};
    std::array<Shader::IR::Program, Maxwell::MaxShaderProgram> programs;
    const bool uses_vertex_a{key.unique_hashes[0] != 0};
    const bool uses_vertex_b{key.unique_hashes[1] != 0};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == 0) {
            continue;
        }
        Shader::Environment& env{*envs[env_index]};
        ++env_index;

        const u32 cfg_offset{static_cast<u32>(env.StartAddress() + sizeof(Shader::ProgramHeader))};
        Shader::Maxwell::Flow::CFG cfg(env, pools.flow_block, cfg_offset, index == 0);
        if (!uses_vertex_a || index != 1) {
            // Normal path
            programs[index] = TranslateProgram(pools.inst, pools.block, env, cfg);

            for (const auto& desc : programs[index].info.storage_buffers_descriptors) {
                total_storage_buffers += desc.count;
            }
        } else {
            // VertexB path when VertexA is present.
            Shader::IR::Program& program_va{programs[0]};
            Shader::IR::Program program_vb{TranslateProgram(pools.inst, pools.block, env, cfg)};
            for (const auto& desc : program_vb.info.storage_buffers_descriptors) {
                total_storage_buffers += desc.count;
            }
            programs[index] = MergeDualVertexPrograms(program_va, program_vb, env);
        }
    }
    const u32 glasm_storage_buffer_limit{device.GetMaxGLASMStorageBufferBlocks()};
    const bool glasm_use_storage_buffers{total_storage_buffers <= glasm_storage_buffer_limit};

    std::array<const Shader::Info*, Maxwell::MaxShaderStage> infos{};

    OGLProgram source_program;
    std::array<OGLAssemblyProgram, 5> assembly_programs;
    Shader::Backend::Bindings binding;
    if (!device.UseAssemblyShaders()) {
        source_program.handle = glCreateProgram();
    }
    const size_t first_index = uses_vertex_a && uses_vertex_b ? 1 : 0;
    for (size_t index = first_index; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == 0) {
            continue;
        }
        UNIMPLEMENTED_IF(index == 0);

        Shader::IR::Program& program{programs[index]};
        const size_t stage_index{index - 1};
        infos[stage_index] = &program.info;

        const auto runtime_info{MakeRuntimeInfo(key, program, glasm_use_storage_buffers)};
        if (device.UseAssemblyShaders()) {
            const std::string code{EmitGLASM(profile, runtime_info, program, binding)};
            assembly_programs[stage_index] = CompileProgram(code, AssemblyStage(stage_index));
        } else {
            const auto code{EmitGLSL(profile, runtime_info, program, binding)};
            AttachShader(Stage(stage_index), source_program.handle, code);
        }
    }
    if (!device.UseAssemblyShaders()) {
        LinkProgram(source_program.handle);
    }
    return std::make_unique<GraphicsPipeline>(
        device, texture_cache, buffer_cache, gpu_memory, maxwell3d, program_manager, state_tracker,
        std::move(source_program), std::move(assembly_programs), infos,
        key.xfb_enabled != 0 ? &key.xfb_state : nullptr);

} catch (Shader::Exception& exception) {
    LOG_ERROR(Render_OpenGL, "{}", exception.what());
    return nullptr;
}

std::unique_ptr<ComputePipeline> ShaderCache::CreateComputePipeline(
    const ComputePipelineKey& key, const VideoCommon::ShaderInfo* shader) {
    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    ComputeEnvironment env{kepler_compute, gpu_memory, program_base, qmd.program_start};
    env.SetCachedSize(shader->size_bytes);

    main_pools.ReleaseContents();
    auto pipeline{CreateComputePipeline(main_pools, key, env)};
    if (!pipeline || shader_cache_filename.empty()) {
        return pipeline;
    }
    SerializePipeline(key, std::array<const GenericEnvironment*, 1>{&env}, shader_cache_filename);
    return pipeline;
}

std::unique_ptr<ComputePipeline> ShaderCache::CreateComputePipeline(ShaderPools& pools,
                                                                    const ComputePipelineKey& key,
                                                                    Shader::Environment& env) try {
    LOG_INFO(Render_OpenGL, "0x{:016x}", key.Hash());

    Shader::Maxwell::Flow::CFG cfg{env, pools.flow_block, env.StartAddress()};
    Shader::IR::Program program{TranslateProgram(pools.inst, pools.block, env, cfg)};

    u32 num_storage_buffers{};
    for (const auto& desc : program.info.storage_buffers_descriptors) {
        num_storage_buffers += desc.count;
    }
    Shader::RuntimeInfo info;
    info.glasm_use_storage_buffers = num_storage_buffers <= device.GetMaxGLASMStorageBufferBlocks();

    OGLAssemblyProgram asm_program;
    OGLProgram source_program;
    if (device.UseAssemblyShaders()) {
        const std::string code{EmitGLASM(profile, info, program)};
        asm_program = CompileProgram(code, GL_COMPUTE_PROGRAM_NV);
    } else {
        const auto code{EmitGLSL(profile, program)};
        OGLShader shader;
        source_program.handle = glCreateProgram();
        AttachShader(GL_COMPUTE_SHADER, source_program.handle, code);
        LinkProgram(source_program.handle);
    }
    return std::make_unique<ComputePipeline>(device, texture_cache, buffer_cache, gpu_memory,
                                             kepler_compute, program_manager, program.info,
                                             std::move(source_program), std::move(asm_program));
} catch (Shader::Exception& exception) {
    LOG_ERROR(Render_OpenGL, "{}", exception.what());
    return nullptr;
}

std::unique_ptr<Common::StatefulThreadWorker<ShaderCache::Context>> ShaderCache::CreateWorkers()
    const {
    return std::make_unique<Common::StatefulThreadWorker<Context>>(
        std::max(std::thread::hardware_concurrency(), 2U) - 1, "yuzu:ShaderBuilder",
        [this] { return Context{emu_window}; });
}

} // namespace OpenGL
