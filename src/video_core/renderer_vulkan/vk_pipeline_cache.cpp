// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include "common/bit_cast.h"
#include "common/cityhash.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/microprofile.h"
#include "common/thread_worker.h"
#include "core/core.h"
#include "core/memory.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/program_header.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/pipeline_helper.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_environment.h"
#include "video_core/shader_notify.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
MICROPROFILE_DECLARE(Vulkan_PipelineCache);

namespace {
using Shader::Backend::SPIRV::EmitSPIRV;
using Shader::Maxwell::MergeDualVertexPrograms;
using Shader::Maxwell::TranslateProgram;
using VideoCommon::ComputeEnvironment;
using VideoCommon::FileEnvironment;
using VideoCommon::GenericEnvironment;
using VideoCommon::GraphicsEnvironment;

template <typename Container>
auto MakeSpan(Container& container) {
    return std::span(container.data(), container.size());
}

Shader::CompareFunction MaxwellToCompareFunction(Maxwell::ComparisonOp comparison) {
    switch (comparison) {
    case Maxwell::ComparisonOp::Never:
    case Maxwell::ComparisonOp::NeverOld:
        return Shader::CompareFunction::Never;
    case Maxwell::ComparisonOp::Less:
    case Maxwell::ComparisonOp::LessOld:
        return Shader::CompareFunction::Less;
    case Maxwell::ComparisonOp::Equal:
    case Maxwell::ComparisonOp::EqualOld:
        return Shader::CompareFunction::Equal;
    case Maxwell::ComparisonOp::LessEqual:
    case Maxwell::ComparisonOp::LessEqualOld:
        return Shader::CompareFunction::LessThanEqual;
    case Maxwell::ComparisonOp::Greater:
    case Maxwell::ComparisonOp::GreaterOld:
        return Shader::CompareFunction::Greater;
    case Maxwell::ComparisonOp::NotEqual:
    case Maxwell::ComparisonOp::NotEqualOld:
        return Shader::CompareFunction::NotEqual;
    case Maxwell::ComparisonOp::GreaterEqual:
    case Maxwell::ComparisonOp::GreaterEqualOld:
        return Shader::CompareFunction::GreaterThanEqual;
    case Maxwell::ComparisonOp::Always:
    case Maxwell::ComparisonOp::AlwaysOld:
        return Shader::CompareFunction::Always;
    }
    UNIMPLEMENTED_MSG("Unimplemented comparison op={}", comparison);
    return {};
}
} // Anonymous namespace

size_t ComputePipelineCacheKey::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this);
    return static_cast<size_t>(hash);
}

bool ComputePipelineCacheKey::operator==(const ComputePipelineCacheKey& rhs) const noexcept {
    return std::memcmp(&rhs, this, sizeof *this) == 0;
}

size_t GraphicsPipelineCacheKey::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), Size());
    return static_cast<size_t>(hash);
}

bool GraphicsPipelineCacheKey::operator==(const GraphicsPipelineCacheKey& rhs) const noexcept {
    return std::memcmp(&rhs, this, Size()) == 0;
}

PipelineCache::PipelineCache(RasterizerVulkan& rasterizer_, Tegra::Engines::Maxwell3D& maxwell3d_,
                             Tegra::Engines::KeplerCompute& kepler_compute_,
                             Tegra::MemoryManager& gpu_memory_, const Device& device_,
                             VKScheduler& scheduler_, DescriptorPool& descriptor_pool_,
                             VKUpdateDescriptorQueue& update_descriptor_queue_,
                             RenderPassCache& render_pass_cache_, BufferCache& buffer_cache_,
                             TextureCache& texture_cache_)
    : VideoCommon::ShaderCache{rasterizer_, gpu_memory_, maxwell3d_, kepler_compute_},
      device{device_}, scheduler{scheduler_}, descriptor_pool{descriptor_pool_},
      update_descriptor_queue{update_descriptor_queue_}, render_pass_cache{render_pass_cache_},
      buffer_cache{buffer_cache_}, texture_cache{texture_cache_},
      workers(std::max(std::thread::hardware_concurrency(), 2U) - 1, "yuzu:PipelineBuilder"),
      serialization_thread(1, "yuzu:PipelineSerialization") {
    const auto& float_control{device.FloatControlProperties()};
    const VkDriverIdKHR driver_id{device.GetDriverID()};
    base_profile = Shader::Profile{
        .supported_spirv = device.IsKhrSpirv1_4Supported() ? 0x00010400U : 0x00010000U,
        .unified_descriptor_binding = true,
        .support_vertex_instance_id = false,
        .support_float_controls = true,
        .support_separate_denorm_behavior = float_control.denormBehaviorIndependence ==
                                            VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR,
        .support_separate_rounding_mode =
            float_control.roundingModeIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR,
        .support_fp16_denorm_preserve = float_control.shaderDenormPreserveFloat16 != VK_FALSE,
        .support_fp32_denorm_preserve = float_control.shaderDenormPreserveFloat32 != VK_FALSE,
        .support_fp16_denorm_flush = float_control.shaderDenormFlushToZeroFloat16 != VK_FALSE,
        .support_fp32_denorm_flush = float_control.shaderDenormFlushToZeroFloat32 != VK_FALSE,
        .support_fp16_signed_zero_nan_preserve =
            float_control.shaderSignedZeroInfNanPreserveFloat16 != VK_FALSE,
        .support_fp32_signed_zero_nan_preserve =
            float_control.shaderSignedZeroInfNanPreserveFloat32 != VK_FALSE,
        .support_fp64_signed_zero_nan_preserve =
            float_control.shaderSignedZeroInfNanPreserveFloat64 != VK_FALSE,
        .support_explicit_workgroup_layout = device.IsKhrWorkgroupMemoryExplicitLayoutSupported(),
        .support_vote = true,
        .support_viewport_index_layer_non_geometry =
            device.IsExtShaderViewportIndexLayerSupported(),
        .support_viewport_mask = device.IsNvViewportArray2Supported(),
        .support_typeless_image_loads = device.IsFormatlessImageLoadSupported(),
        .warp_size_potentially_larger_than_guest = device.IsWarpSizePotentiallyBiggerThanGuest(),
        .support_int64_atomics = device.IsExtShaderAtomicInt64Supported(),
        .has_broken_spirv_clamp = driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS_KHR,
        .generic_input_types{},
        .fixed_state_point_size{},
        .alpha_test_func{},
        .xfb_varyings{},
    };
}

PipelineCache::~PipelineCache() = default;

GraphicsPipeline* PipelineCache::CurrentGraphicsPipeline() {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    if (!RefreshStages(graphics_key.unique_hashes)) {
        current_pipeline = nullptr;
        return nullptr;
    }
    graphics_key.state.Refresh(maxwell3d, device.IsExtExtendedDynamicStateSupported());

    if (current_pipeline) {
        GraphicsPipeline* const next{current_pipeline->Next(graphics_key)};
        if (next) {
            current_pipeline = next;
            return current_pipeline;
        }
    }
    const auto [pair, is_new]{graphics_cache.try_emplace(graphics_key)};
    auto& pipeline{pair->second};
    if (is_new) {
        pipeline = CreateGraphicsPipeline();
    }
    if (current_pipeline) {
        current_pipeline->AddTransition(pipeline.get());
    }
    current_pipeline = pipeline.get();
    return current_pipeline;
}

ComputePipeline* PipelineCache::CurrentComputePipeline() {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    const ShaderInfo* const shader{ComputeShader()};
    if (!shader) {
        return nullptr;
    }
    const auto& qmd{kepler_compute.launch_description};
    const ComputePipelineCacheKey key{
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

void PipelineCache::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                      const VideoCore::DiskResourceLoadCallback& callback) {
    if (title_id == 0) {
        return;
    }
    auto shader_dir{Common::FS::GetYuzuPath(Common::FS::YuzuPath::ShaderDir)};
    auto base_dir{shader_dir / "vulkan"};
    auto transferable_dir{base_dir / "transferable"};
    auto precompiled_dir{base_dir / "precompiled"};
    if (!Common::FS::CreateDir(shader_dir) || !Common::FS::CreateDir(base_dir) ||
        !Common::FS::CreateDir(transferable_dir) || !Common::FS::CreateDir(precompiled_dir)) {
        LOG_ERROR(Common_Filesystem, "Failed to create pipeline cache directories");
        return;
    }
    pipeline_cache_filename = transferable_dir / fmt::format("{:016x}.bin", title_id);

    struct {
        std::mutex mutex;
        size_t total{0};
        size_t built{0};
        bool has_loaded{false};
    } state;

    const auto load_compute{[&](std::ifstream& file, FileEnvironment env) {
        ComputePipelineCacheKey key;
        file.read(reinterpret_cast<char*>(&key), sizeof(key));

        workers.QueueWork([this, key, env = std::move(env), &state, &callback]() mutable {
            ShaderPools pools;
            auto pipeline{CreateComputePipeline(pools, key, env, false)};

            std::lock_guard lock{state.mutex};
            compute_cache.emplace(key, std::move(pipeline));
            ++state.built;
            if (state.has_loaded) {
                callback(VideoCore::LoadCallbackStage::Build, state.built, state.total);
            }
        });
        ++state.total;
    }};
    const auto load_graphics{[&](std::ifstream& file, std::vector<FileEnvironment> envs) {
        GraphicsPipelineCacheKey key;
        file.read(reinterpret_cast<char*>(&key), sizeof(key));

        workers.QueueWork([this, key, envs = std::move(envs), &state, &callback]() mutable {
            ShaderPools pools;
            boost::container::static_vector<Shader::Environment*, 5> env_ptrs;
            for (auto& env : envs) {
                env_ptrs.push_back(&env);
            }
            auto pipeline{CreateGraphicsPipeline(pools, key, MakeSpan(env_ptrs), false)};

            std::lock_guard lock{state.mutex};
            graphics_cache.emplace(key, std::move(pipeline));
            ++state.built;
            if (state.has_loaded) {
                callback(VideoCore::LoadCallbackStage::Build, state.built, state.total);
            }
        });
        ++state.total;
    }};
    VideoCommon::LoadPipelines(stop_loading, pipeline_cache_filename, load_compute, load_graphics);

    std::unique_lock lock{state.mutex};
    callback(VideoCore::LoadCallbackStage::Build, 0, state.total);
    state.has_loaded = true;
    lock.unlock();

    workers.WaitForRequests();
}

std::unique_ptr<GraphicsPipeline> PipelineCache::CreateGraphicsPipeline(
    ShaderPools& pools, const GraphicsPipelineCacheKey& key,
    std::span<Shader::Environment* const> envs, bool build_in_parallel) {
    LOG_INFO(Render_Vulkan, "0x{:016x}", key.Hash());
    size_t env_index{0};
    std::array<Shader::IR::Program, Maxwell::MaxShaderProgram> programs;
    bool uses_vertex_a{};
    std::size_t start_value_processing{};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == 0) {
            continue;
        }
        uses_vertex_a |= index == 0;
        Shader::Environment& env{*envs[env_index]};
        ++env_index;

        const u32 cfg_offset{static_cast<u32>(env.StartAddress() + sizeof(Shader::ProgramHeader))};
        Shader::Maxwell::Flow::CFG cfg(env, pools.flow_block, cfg_offset, index == 0);
        if (!uses_vertex_a || index != 1) {
            programs[index] = TranslateProgram(pools.inst, pools.block, env, cfg);
            continue;
        }
        Shader::IR::Program& program_va{programs[0]};
        Shader::IR::Program program_vb{TranslateProgram(pools.inst, pools.block, env, cfg)};
        programs[index] = MergeDualVertexPrograms(program_va, program_vb, env);
        start_value_processing = 1;
    }
    std::array<const Shader::Info*, Maxwell::MaxShaderStage> infos{};
    std::array<vk::ShaderModule, Maxwell::MaxShaderStage> modules;

    u32 binding{0};
    for (size_t index = start_value_processing; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == 0) {
            continue;
        }
        UNIMPLEMENTED_IF(index == 0);

        Shader::IR::Program& program{programs[index]};
        const size_t stage_index{index - 1};
        infos[stage_index] = &program.info;

        const Shader::Profile profile{MakeProfile(key, program)};
        const std::vector<u32> code{EmitSPIRV(profile, program, binding)};
        device.SaveShader(code);
        modules[stage_index] = BuildShader(device, code);
        if (device.HasDebuggingToolAttached()) {
            const std::string name{fmt::format("{:016x}", key.unique_hashes[index])};
            modules[stage_index].SetObjectNameEXT(name.c_str());
        }
    }
    Common::ThreadWorker* const thread_worker{build_in_parallel ? &workers : nullptr};
    return std::make_unique<GraphicsPipeline>(
        maxwell3d, gpu_memory, scheduler, buffer_cache, texture_cache, device, descriptor_pool,
        update_descriptor_queue, thread_worker, render_pass_cache, key, std::move(modules), infos);
}

std::unique_ptr<GraphicsPipeline> PipelineCache::CreateGraphicsPipeline() {
    main_pools.ReleaseContents();

    std::array<GraphicsEnvironment, Maxwell::MaxShaderProgram> graphics_envs;
    boost::container::static_vector<Shader::Environment*, Maxwell::MaxShaderProgram> envs;

    const GPUVAddr base_addr{maxwell3d.regs.code_address.CodeAddress()};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (graphics_key.unique_hashes[index] == 0) {
            continue;
        }
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};
        auto& env{graphics_envs[index]};
        const u32 start_address{maxwell3d.regs.shader_config[index].offset};
        env = GraphicsEnvironment{maxwell3d, gpu_memory, program, base_addr, start_address};
        env.SetCachedSize(shader_infos[index]->size_bytes);
        envs.push_back(&env);
    }
    auto pipeline{CreateGraphicsPipeline(main_pools, graphics_key, MakeSpan(envs), true)};
    if (pipeline_cache_filename.empty()) {
        return pipeline;
    }
    serialization_thread.QueueWork([this, key = graphics_key, envs = std::move(graphics_envs)] {
        boost::container::static_vector<const GenericEnvironment*, Maxwell::MaxShaderProgram>
            env_ptrs;
        for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
            if (key.unique_hashes[index] != 0) {
                env_ptrs.push_back(&envs[index]);
            }
        }
        VideoCommon::SerializePipeline(key, env_ptrs, pipeline_cache_filename);
    });
    return pipeline;
}

std::unique_ptr<ComputePipeline> PipelineCache::CreateComputePipeline(
    const ComputePipelineCacheKey& key, const ShaderInfo* shader) {
    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    ComputeEnvironment env{kepler_compute, gpu_memory, program_base, qmd.program_start};
    env.SetCachedSize(shader->size_bytes);

    main_pools.ReleaseContents();
    auto pipeline{CreateComputePipeline(main_pools, key, env, true)};
    if (!pipeline_cache_filename.empty()) {
        serialization_thread.QueueWork([this, key, env = std::move(env)] {
            VideoCommon::SerializePipeline(key, std::array<const GenericEnvironment*, 1>{&env},
                                           pipeline_cache_filename);
        });
    }
    return pipeline;
}

std::unique_ptr<ComputePipeline> PipelineCache::CreateComputePipeline(
    ShaderPools& pools, const ComputePipelineCacheKey& key, Shader::Environment& env,
    bool build_in_parallel) {
    LOG_INFO(Render_Vulkan, "0x{:016x}", key.Hash());

    Shader::Maxwell::Flow::CFG cfg{env, pools.flow_block, env.StartAddress()};
    Shader::IR::Program program{TranslateProgram(pools.inst, pools.block, env, cfg)};
    u32 binding{0};
    const std::vector<u32> code{EmitSPIRV(base_profile, program, binding)};
    device.SaveShader(code);
    vk::ShaderModule spv_module{BuildShader(device, code)};
    if (device.HasDebuggingToolAttached()) {
        const auto name{fmt::format("{:016x}", key.unique_hash)};
        spv_module.SetObjectNameEXT(name.c_str());
    }
    Common::ThreadWorker* const thread_worker{build_in_parallel ? &workers : nullptr};
    return std::make_unique<ComputePipeline>(device, descriptor_pool, update_descriptor_queue,
                                             thread_worker, program.info, std::move(spv_module));
}

static Shader::AttributeType CastAttributeType(const FixedPipelineState::VertexAttribute& attr) {
    if (attr.enabled == 0) {
        return Shader::AttributeType::Disabled;
    }
    switch (attr.Type()) {
    case Maxwell::VertexAttribute::Type::SignedNorm:
    case Maxwell::VertexAttribute::Type::UnsignedNorm:
    case Maxwell::VertexAttribute::Type::UnsignedScaled:
    case Maxwell::VertexAttribute::Type::SignedScaled:
    case Maxwell::VertexAttribute::Type::Float:
        return Shader::AttributeType::Float;
    case Maxwell::VertexAttribute::Type::SignedInt:
        return Shader::AttributeType::SignedInt;
    case Maxwell::VertexAttribute::Type::UnsignedInt:
        return Shader::AttributeType::UnsignedInt;
    }
    return Shader::AttributeType::Float;
}

static std::vector<Shader::TransformFeedbackVarying> MakeTransformFeedbackVaryings(
    const GraphicsPipelineCacheKey& key) {
    static constexpr std::array VECTORS{
        28,  // gl_Position
        32,  // Generic 0
        36,  // Generic 1
        40,  // Generic 2
        44,  // Generic 3
        48,  // Generic 4
        52,  // Generic 5
        56,  // Generic 6
        60,  // Generic 7
        64,  // Generic 8
        68,  // Generic 9
        72,  // Generic 10
        76,  // Generic 11
        80,  // Generic 12
        84,  // Generic 13
        88,  // Generic 14
        92,  // Generic 15
        96,  // Generic 16
        100, // Generic 17
        104, // Generic 18
        108, // Generic 19
        112, // Generic 20
        116, // Generic 21
        120, // Generic 22
        124, // Generic 23
        128, // Generic 24
        132, // Generic 25
        136, // Generic 26
        140, // Generic 27
        144, // Generic 28
        148, // Generic 29
        152, // Generic 30
        156, // Generic 31
        160, // gl_FrontColor
        164, // gl_FrontSecondaryColor
        160, // gl_BackColor
        164, // gl_BackSecondaryColor
        192, // gl_TexCoord[0]
        196, // gl_TexCoord[1]
        200, // gl_TexCoord[2]
        204, // gl_TexCoord[3]
        208, // gl_TexCoord[4]
        212, // gl_TexCoord[5]
        216, // gl_TexCoord[6]
        220, // gl_TexCoord[7]
    };
    std::vector<Shader::TransformFeedbackVarying> xfb(256);
    for (size_t buffer = 0; buffer < Maxwell::NumTransformFeedbackBuffers; ++buffer) {
        const auto& locations = key.state.xfb_state.varyings[buffer];
        const auto& layout = key.state.xfb_state.layouts[buffer];
        const u32 varying_count = layout.varying_count;
        u32 highest = 0;
        for (u32 offset = 0; offset < varying_count; ++offset) {
            const u32 base_offset = offset;
            const u8 location = locations[offset];

            Shader::TransformFeedbackVarying varying;
            varying.buffer = layout.stream;
            varying.stride = layout.stride;
            varying.offset = offset * 4;
            varying.components = 1;

            if (std::ranges::find(VECTORS, Common::AlignDown(location, 4)) != VECTORS.end()) {
                UNIMPLEMENTED_IF_MSG(location % 4 != 0, "Unaligned TFB");

                const u8 base_index = location / 4;
                while (offset + 1 < varying_count && base_index == locations[offset + 1] / 4) {
                    ++offset;
                    ++varying.components;
                }
            }
            xfb[location] = varying;
            highest = std::max(highest, (base_offset + varying.components) * 4);
        }
        UNIMPLEMENTED_IF(highest != layout.stride);
    }
    return xfb;
}

Shader::Profile PipelineCache::MakeProfile(const GraphicsPipelineCacheKey& key,
                                           const Shader::IR::Program& program) {
    Shader::Profile profile{base_profile};

    const Shader::Stage stage{program.stage};
    const bool has_geometry{key.unique_hashes[4] != 0};
    const bool gl_ndc{key.state.ndc_minus_one_to_one != 0};
    const float point_size{Common::BitCast<float>(key.state.point_size)};
    switch (stage) {
    case Shader::Stage::VertexB:
        if (!has_geometry) {
            if (key.state.topology == Maxwell::PrimitiveTopology::Points) {
                profile.fixed_state_point_size = point_size;
            }
            if (key.state.xfb_enabled != 0) {
                profile.xfb_varyings = MakeTransformFeedbackVaryings(key);
            }
            profile.convert_depth_mode = gl_ndc;
        }
        std::ranges::transform(key.state.attributes, profile.generic_input_types.begin(),
                               &CastAttributeType);
        break;
    case Shader::Stage::TessellationEval:
        // We have to flip tessellation clockwise for some reason...
        profile.tess_clockwise = key.state.tessellation_clockwise == 0;
        profile.tess_primitive = [&key] {
            const u32 raw{key.state.tessellation_primitive.Value()};
            switch (static_cast<Maxwell::TessellationPrimitive>(raw)) {
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
        profile.tess_spacing = [&] {
            const u32 raw{key.state.tessellation_spacing};
            switch (static_cast<Maxwell::TessellationSpacing>(raw)) {
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
    case Shader::Stage::Geometry:
        if (program.output_topology == Shader::OutputTopology::PointList) {
            profile.fixed_state_point_size = point_size;
        }
        if (key.state.xfb_enabled != 0) {
            profile.xfb_varyings = MakeTransformFeedbackVaryings(key);
        }
        profile.convert_depth_mode = gl_ndc;
        break;
    case Shader::Stage::Fragment:
        profile.alpha_test_func = MaxwellToCompareFunction(
            key.state.UnpackComparisonOp(key.state.alpha_test_func.Value()));
        profile.alpha_test_reference = Common::BitCast<float>(key.state.alpha_test_ref);
        break;
    default:
        break;
    }
    switch (key.state.topology) {
    case Maxwell::PrimitiveTopology::Points:
        profile.input_topology = Shader::InputTopology::Points;
        break;
    case Maxwell::PrimitiveTopology::Lines:
    case Maxwell::PrimitiveTopology::LineLoop:
    case Maxwell::PrimitiveTopology::LineStrip:
        profile.input_topology = Shader::InputTopology::Lines;
        break;
    case Maxwell::PrimitiveTopology::Triangles:
    case Maxwell::PrimitiveTopology::TriangleStrip:
    case Maxwell::PrimitiveTopology::TriangleFan:
    case Maxwell::PrimitiveTopology::Quads:
    case Maxwell::PrimitiveTopology::QuadStrip:
    case Maxwell::PrimitiveTopology::Polygon:
    case Maxwell::PrimitiveTopology::Patches:
        profile.input_topology = Shader::InputTopology::Triangles;
        break;
    case Maxwell::PrimitiveTopology::LinesAdjacency:
    case Maxwell::PrimitiveTopology::LineStripAdjacency:
        profile.input_topology = Shader::InputTopology::LinesAdjacency;
        break;
    case Maxwell::PrimitiveTopology::TrianglesAdjacency:
    case Maxwell::PrimitiveTopology::TriangleStripAdjacency:
        profile.input_topology = Shader::InputTopology::TrianglesAdjacency;
        break;
    }
    profile.force_early_z = key.state.early_z != 0;
    profile.y_negate = key.state.y_negate != 0;
    return profile;
}

} // namespace Vulkan
