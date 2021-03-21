// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "common/bit_cast.h"
#include "common/cityhash.h"
#include "common/microprofile.h"
#include "core/core.h"
#include "core/memory.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/program_header.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_notify.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
MICROPROFILE_DECLARE(Vulkan_PipelineCache);

namespace {
using Shader::Backend::SPIRV::EmitSPIRV;

class GenericEnvironment : public Shader::Environment {
public:
    explicit GenericEnvironment() = default;
    explicit GenericEnvironment(Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_)
        : gpu_memory{&gpu_memory_}, program_base{program_base_} {}

    ~GenericEnvironment() override = default;

    std::optional<u128> Analyze(u32 start_address) {
        const std::optional<u64> size{TryFindSize(start_address)};
        if (!size) {
            return std::nullopt;
        }
        cached_lowest = start_address;
        cached_highest = start_address + static_cast<u32>(*size);
        return Common::CityHash128(reinterpret_cast<const char*>(code.data()), code.size());
    }

    [[nodiscard]] size_t CachedSize() const noexcept {
        return cached_highest - cached_lowest + INST_SIZE;
    }

    [[nodiscard]] size_t ReadSize() const noexcept {
        return read_highest - read_lowest + INST_SIZE;
    }

    [[nodiscard]] u128 CalculateHash() const {
        const size_t size{ReadSize()};
        auto data = std::make_unique<u64[]>(size);
        gpu_memory->ReadBlock(program_base + read_lowest, data.get(), size);
        return Common::CityHash128(reinterpret_cast<const char*>(data.get()), size);
    }

    u64 ReadInstruction(u32 address) final {
        read_lowest = std::min(read_lowest, address);
        read_highest = std::max(read_highest, address);

        if (address >= cached_lowest && address < cached_highest) {
            return code[address / INST_SIZE];
        }
        return gpu_memory->Read<u64>(program_base + address);
    }

protected:
    static constexpr size_t INST_SIZE = sizeof(u64);

    std::optional<u64> TryFindSize(GPUVAddr guest_addr) {
        constexpr size_t BLOCK_SIZE = 0x1000;
        constexpr size_t MAXIMUM_SIZE = 0x100000;

        constexpr u64 SELF_BRANCH_A = 0xE2400FFFFF87000FULL;
        constexpr u64 SELF_BRANCH_B = 0xE2400FFFFF07000FULL;

        size_t offset = 0;
        size_t size = BLOCK_SIZE;
        while (size <= MAXIMUM_SIZE) {
            code.resize(size / INST_SIZE);
            u64* const data = code.data() + offset / INST_SIZE;
            gpu_memory->ReadBlock(guest_addr, data, BLOCK_SIZE);
            for (size_t i = 0; i < BLOCK_SIZE; i += INST_SIZE) {
                const u64 inst = data[i / INST_SIZE];
                if (inst == SELF_BRANCH_A || inst == SELF_BRANCH_B) {
                    return offset + i;
                }
            }
            guest_addr += BLOCK_SIZE;
            size += BLOCK_SIZE;
            offset += BLOCK_SIZE;
        }
        return std::nullopt;
    }

    Tegra::MemoryManager* gpu_memory{};
    GPUVAddr program_base{};

    std::vector<u64> code;

    u32 read_lowest = std::numeric_limits<u32>::max();
    u32 read_highest = 0;

    u32 cached_lowest = std::numeric_limits<u32>::max();
    u32 cached_highest = 0;
};

class GraphicsEnvironment final : public GenericEnvironment {
public:
    explicit GraphicsEnvironment() = default;
    explicit GraphicsEnvironment(Tegra::Engines::Maxwell3D& maxwell3d_,
                                 Tegra::MemoryManager& gpu_memory_, Maxwell::ShaderProgram program,
                                 GPUVAddr program_base_, u32 start_offset)
        : GenericEnvironment{gpu_memory_, program_base_}, maxwell3d{&maxwell3d_} {
        gpu_memory->ReadBlock(program_base + start_offset, &sph, sizeof(sph));
        switch (program) {
        case Maxwell::ShaderProgram::VertexA:
            stage = Shader::Stage::VertexA;
            break;
        case Maxwell::ShaderProgram::VertexB:
            stage = Shader::Stage::VertexB;
            break;
        case Maxwell::ShaderProgram::TesselationControl:
            stage = Shader::Stage::TessellationControl;
            break;
        case Maxwell::ShaderProgram::TesselationEval:
            stage = Shader::Stage::TessellationEval;
            break;
        case Maxwell::ShaderProgram::Geometry:
            stage = Shader::Stage::Geometry;
            break;
        case Maxwell::ShaderProgram::Fragment:
            stage = Shader::Stage::Fragment;
            break;
        default:
            UNREACHABLE_MSG("Invalid program={}", program);
        }
    }

    ~GraphicsEnvironment() override = default;

    u32 TextureBoundBuffer() override {
        return maxwell3d->regs.tex_cb_index;
    }

    std::array<u32, 3> WorkgroupSize() override {
        throw Shader::LogicError("Requesting workgroup size in a graphics stage");
    }

private:
    Tegra::Engines::Maxwell3D* maxwell3d{};
};

class ComputeEnvironment final : public GenericEnvironment {
public:
    explicit ComputeEnvironment() = default;
    explicit ComputeEnvironment(Tegra::Engines::KeplerCompute& kepler_compute_,
                                Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_)
        : GenericEnvironment{gpu_memory_, program_base_}, kepler_compute{&kepler_compute_} {
        stage = Shader::Stage::Compute;
    }

    ~ComputeEnvironment() override = default;

    u32 TextureBoundBuffer() override {
        return kepler_compute->regs.tex_cb_index;
    }

    std::array<u32, 3> WorkgroupSize() override {
        const auto& qmd{kepler_compute->launch_description};
        return {qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z};
    }

private:
    Tegra::Engines::KeplerCompute* kepler_compute{};
};
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

PipelineCache::PipelineCache(RasterizerVulkan& rasterizer_, Tegra::GPU& gpu_,
                             Tegra::Engines::Maxwell3D& maxwell3d_,
                             Tegra::Engines::KeplerCompute& kepler_compute_,
                             Tegra::MemoryManager& gpu_memory_, const Device& device_,
                             VKScheduler& scheduler_, VKDescriptorPool& descriptor_pool_,
                             VKUpdateDescriptorQueue& update_descriptor_queue_,
                             RenderPassCache& render_pass_cache_, BufferCache& buffer_cache_,
                             TextureCache& texture_cache_)
    : VideoCommon::ShaderCache<ShaderInfo>{rasterizer_}, gpu{gpu_}, maxwell3d{maxwell3d_},
      kepler_compute{kepler_compute_}, gpu_memory{gpu_memory_}, device{device_},
      scheduler{scheduler_}, descriptor_pool{descriptor_pool_},
      update_descriptor_queue{update_descriptor_queue_}, render_pass_cache{render_pass_cache_},
      buffer_cache{buffer_cache_}, texture_cache{texture_cache_} {
    const auto& float_control{device.FloatControlProperties()};
    const VkDriverIdKHR driver_id{device.GetDriverID()};
    profile = Shader::Profile{
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
        .has_broken_spirv_clamp = driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS_KHR,
    };
}

PipelineCache::~PipelineCache() = default;

GraphicsPipeline* PipelineCache::CurrentGraphicsPipeline() {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    if (!RefreshStages()) {
        return nullptr;
    }
    graphics_key.state.Refresh(maxwell3d, device.IsExtExtendedDynamicStateSupported());

    const auto [pair, is_new]{graphics_cache.try_emplace(graphics_key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return &pipeline;
    }
    pipeline = CreateGraphicsPipeline();
    return &pipeline;
}

ComputePipeline* PipelineCache::CurrentComputePipeline() {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    const GPUVAddr shader_addr{program_base + qmd.program_start};
    const std::optional<VAddr> cpu_shader_addr{gpu_memory.GpuToCpuAddress(shader_addr)};
    if (!cpu_shader_addr) {
        return nullptr;
    }
    ShaderInfo* const shader{TryGet(*cpu_shader_addr)};
    if (!shader) {
        return CreateComputePipelineWithoutShader(*cpu_shader_addr);
    }
    const ComputePipelineCacheKey key{MakeComputePipelineKey(shader->unique_hash)};
    const auto [pair, is_new]{compute_cache.try_emplace(key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return &pipeline;
    }
    pipeline = CreateComputePipeline(shader);
    return &pipeline;
}

bool PipelineCache::RefreshStages() {
    const GPUVAddr base_addr{maxwell3d.regs.code_address.CodeAddress()};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (!maxwell3d.regs.IsShaderConfigEnabled(index)) {
            graphics_key.unique_hashes[index] = u128{};
            continue;
        }
        const auto& shader_config{maxwell3d.regs.shader_config[index]};
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};
        const GPUVAddr shader_addr{base_addr + shader_config.offset};
        const std::optional<VAddr> cpu_shader_addr{gpu_memory.GpuToCpuAddress(shader_addr)};
        if (!cpu_shader_addr) {
            LOG_ERROR(Render_Vulkan, "Invalid GPU address for shader 0x{:016x}", shader_addr);
            return false;
        }
        const ShaderInfo* shader_info{TryGet(*cpu_shader_addr)};
        if (!shader_info) {
            const u32 offset{shader_config.offset};
            shader_info = MakeShaderInfo(program, base_addr, offset, *cpu_shader_addr);
        }
        graphics_key.unique_hashes[index] = shader_info->unique_hash;
    }
    return true;
}

const ShaderInfo* PipelineCache::MakeShaderInfo(Maxwell::ShaderProgram program, GPUVAddr base_addr,
                                                u32 start_address, VAddr cpu_addr) {
    GraphicsEnvironment env{maxwell3d, gpu_memory, program, base_addr, start_address};
    auto info = std::make_unique<ShaderInfo>();
    if (const std::optional<u128> cached_hash{env.Analyze(start_address)}) {
        info->unique_hash = *cached_hash;
        info->size_bytes = env.CachedSize();
    } else {
        // Slow path, not really hit on commercial games
        // Build a control flow graph to get the real shader size
        flow_block_pool.ReleaseContents();
        Shader::Maxwell::Flow::CFG cfg{env, flow_block_pool, start_address};
        info->unique_hash = env.CalculateHash();
        info->size_bytes = env.ReadSize();
    }
    const size_t size_bytes{info->size_bytes};
    const ShaderInfo* const result{info.get()};
    Register(std::move(info), cpu_addr, size_bytes);
    return result;
}

GraphicsPipeline PipelineCache::CreateGraphicsPipeline() {
    flow_block_pool.ReleaseContents();
    inst_pool.ReleaseContents();
    block_pool.ReleaseContents();

    std::array<GraphicsEnvironment, Maxwell::MaxShaderProgram> envs;
    std::array<Shader::IR::Program, Maxwell::MaxShaderProgram> programs;

    const GPUVAddr base_addr{maxwell3d.regs.code_address.CodeAddress()};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (graphics_key.unique_hashes[index] == u128{}) {
            continue;
        }
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};
        GraphicsEnvironment& env{envs[index]};
        const u32 start_address{maxwell3d.regs.shader_config[index].offset};
        env = GraphicsEnvironment{maxwell3d, gpu_memory, program, base_addr, start_address};

        const u32 cfg_offset = start_address + sizeof(Shader::ProgramHeader);
        Shader::Maxwell::Flow::CFG cfg(env, flow_block_pool, cfg_offset);
        programs[index] = Shader::Maxwell::TranslateProgram(inst_pool, block_pool, env, cfg);
    }
    std::array<const Shader::Info*, Maxwell::MaxShaderStage> infos{};
    std::array<vk::ShaderModule, Maxwell::MaxShaderStage> modules;

    u32 binding{0};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (graphics_key.unique_hashes[index] == u128{}) {
            continue;
        }
        UNIMPLEMENTED_IF(index == 0);

        GraphicsEnvironment& env{envs[index]};
        Shader::IR::Program& program{programs[index]};

        const size_t stage_index{index - 1};
        infos[stage_index] = &program.info;
        std::vector<u32> code{EmitSPIRV(profile, env, program, binding)};

        FILE* file = fopen("D:\\shader.spv", "wb");
        fwrite(code.data(), 4, code.size(), file);
        fclose(file);
        std::system("spirv-cross --vulkan-semantics D:\\shader.spv");

        modules[stage_index] = BuildShader(device, code);
    }
    return GraphicsPipeline(maxwell3d, gpu_memory, scheduler, buffer_cache, texture_cache, device,
                            descriptor_pool, update_descriptor_queue, render_pass_cache,
                            graphics_key.state, std::move(modules), infos);
}

ComputePipeline PipelineCache::CreateComputePipeline(ShaderInfo* shader_info) {
    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    ComputeEnvironment env{kepler_compute, gpu_memory, program_base};
    if (const std::optional<u128> cached_hash{env.Analyze(qmd.program_start)}) {
        // TODO: Load from cache
    }
    flow_block_pool.ReleaseContents();
    inst_pool.ReleaseContents();
    block_pool.ReleaseContents();

    Shader::Maxwell::Flow::CFG cfg{env, flow_block_pool, qmd.program_start};
    Shader::IR::Program program{Shader::Maxwell::TranslateProgram(inst_pool, block_pool, env, cfg)};
    u32 binding{0};
    std::vector<u32> code{EmitSPIRV(profile, env, program, binding)};
    /*
    FILE* file = fopen("D:\\shader.spv", "wb");
    fwrite(code.data(), 4, code.size(), file);
    fclose(file);
    std::system("spirv-dis D:\\shader.spv");
    */
    shader_info->unique_hash = env.CalculateHash();
    shader_info->size_bytes = env.ReadSize();
    return ComputePipeline{device, descriptor_pool, update_descriptor_queue, program.info,
                           BuildShader(device, code)};
}

ComputePipeline* PipelineCache::CreateComputePipelineWithoutShader(VAddr shader_cpu_addr) {
    ShaderInfo shader;
    ComputePipeline pipeline{CreateComputePipeline(&shader)};
    const ComputePipelineCacheKey key{MakeComputePipelineKey(shader.unique_hash)};
    const size_t size_bytes{shader.size_bytes};
    Register(std::make_unique<ShaderInfo>(std::move(shader)), shader_cpu_addr, size_bytes);
    return &compute_cache.emplace(key, std::move(pipeline)).first->second;
}

ComputePipelineCacheKey PipelineCache::MakeComputePipelineKey(u128 unique_hash) const {
    const auto& qmd{kepler_compute.launch_description};
    return {
        .unique_hash = unique_hash,
        .shared_memory_size = qmd.shared_alloc,
        .workgroup_size{qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z},
    };
}

} // namespace Vulkan
