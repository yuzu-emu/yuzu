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
#include "shader_recompiler/environment.h"
#include "shader_recompiler/recompiler.h"
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

using Tegra::Engines::ShaderType;

namespace {
class Environment final : public Shader::Environment {
public:
    explicit Environment(Tegra::Engines::KeplerCompute& kepler_compute_,
                         Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_)
        : kepler_compute{kepler_compute_}, gpu_memory{gpu_memory_}, program_base{program_base_} {}

    ~Environment() override = default;

    [[nodiscard]] std::optional<u128> Analyze(u32 start_address) {
        const std::optional<u64> size{TryFindSize(start_address)};
        if (!size) {
            return std::nullopt;
        }
        cached_lowest = start_address;
        cached_highest = start_address + static_cast<u32>(*size);
        return Common::CityHash128(reinterpret_cast<const char*>(code.data()), code.size());
    }

    [[nodiscard]] size_t ShaderSize() const noexcept {
        return read_highest - read_lowest + INST_SIZE;
    }

    [[nodiscard]] u128 ComputeHash() const {
        const size_t size{ShaderSize()};
        auto data = std::make_unique<u64[]>(size);
        gpu_memory.ReadBlock(program_base + read_lowest, data.get(), size);
        return Common::CityHash128(reinterpret_cast<const char*>(data.get()), size);
    }

    u64 ReadInstruction(u32 address) override {
        read_lowest = std::min(read_lowest, address);
        read_highest = std::max(read_highest, address);

        if (address >= cached_lowest && address < cached_highest) {
            return code[address / INST_SIZE];
        }
        return gpu_memory.Read<u64>(program_base + address);
    }

    u32 TextureBoundBuffer() override {
        return kepler_compute.regs.tex_cb_index;
    }

    std::array<u32, 3> WorkgroupSize() override {
        const auto& qmd{kepler_compute.launch_description};
        return {qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z};
    }

private:
    static constexpr size_t INST_SIZE = sizeof(u64);
    static constexpr size_t BLOCK_SIZE = 0x1000;
    static constexpr size_t MAXIMUM_SIZE = 0x100000;

    static constexpr u64 SELF_BRANCH_A = 0xE2400FFFFF87000FULL;
    static constexpr u64 SELF_BRANCH_B = 0xE2400FFFFF07000FULL;

    std::optional<u64> TryFindSize(u32 start_address) {
        GPUVAddr guest_addr = program_base + start_address;
        size_t offset = 0;
        size_t size = BLOCK_SIZE;
        while (size <= MAXIMUM_SIZE) {
            code.resize(size / INST_SIZE);
            u64* const data = code.data() + offset / INST_SIZE;
            gpu_memory.ReadBlock(guest_addr, data, BLOCK_SIZE);
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

    Tegra::Engines::KeplerCompute& kepler_compute;
    Tegra::MemoryManager& gpu_memory;
    GPUVAddr program_base;

    u32 read_lowest = 0;
    u32 read_highest = 0;

    std::vector<u64> code;
    u32 cached_lowest = std::numeric_limits<u32>::max();
    u32 cached_highest = 0;
};
} // Anonymous namespace

size_t ComputePipelineCacheKey::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this);
    return static_cast<size_t>(hash);
}

bool ComputePipelineCacheKey::operator==(const ComputePipelineCacheKey& rhs) const noexcept {
    return std::memcmp(&rhs, this, sizeof *this) == 0;
}

PipelineCache::PipelineCache(RasterizerVulkan& rasterizer_, Tegra::GPU& gpu_,
                             Tegra::Engines::Maxwell3D& maxwell3d_,
                             Tegra::Engines::KeplerCompute& kepler_compute_,
                             Tegra::MemoryManager& gpu_memory_, const Device& device_,
                             VKScheduler& scheduler_, VKDescriptorPool& descriptor_pool_,
                             VKUpdateDescriptorQueue& update_descriptor_queue_)
    : VideoCommon::ShaderCache<ShaderInfo>{rasterizer_}, gpu{gpu_}, maxwell3d{maxwell3d_},
      kepler_compute{kepler_compute_}, gpu_memory{gpu_memory_}, device{device_},
      scheduler{scheduler_}, descriptor_pool{descriptor_pool_}, update_descriptor_queue{
                                                                    update_descriptor_queue_} {}

PipelineCache::~PipelineCache() = default;

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
    shader->compute_users.push_back(key);
    return &pipeline;
}

ComputePipeline PipelineCache::CreateComputePipeline(ShaderInfo* shader_info) {
    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    Environment env{kepler_compute, gpu_memory, program_base};
    if (const std::optional<u128> cached_hash{env.Analyze(qmd.program_start)}) {
        // TODO: Load from cache
    }
    const auto& float_control{device.FloatControlProperties()};
    const Shader::Profile profile{
        .unified_descriptor_binding = true,
        .support_float_controls = true,
        .support_separate_denorm_behavior = float_control.denormBehaviorIndependence ==
                                            VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR,
        .support_separate_rounding_mode =
            float_control.roundingModeIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR,
        .support_fp16_denorm_preserve = float_control.shaderDenormPreserveFloat16 != VK_FALSE,
        .support_fp32_denorm_preserve = float_control.shaderDenormPreserveFloat32 != VK_FALSE,
        .support_fp16_denorm_flush = float_control.shaderDenormFlushToZeroFloat16 != VK_FALSE,
        .support_fp32_denorm_flush = float_control.shaderDenormFlushToZeroFloat32 != VK_FALSE,
        .has_broken_spirv_clamp = true, // TODO: is_intel
    };
    const auto [info, code]{Shader::RecompileSPIRV(profile, env, qmd.program_start)};
    /*
    FILE* file = fopen("D:\\shader.spv", "wb");
    fwrite(code.data(), 4, code.size(), file);
    fclose(file);
    std::system("spirv-dis D:\\shader.spv");
    */
    shader_info->unique_hash = env.ComputeHash();
    shader_info->size_bytes = env.ShaderSize();
    return ComputePipeline{device, descriptor_pool, update_descriptor_queue, info,
                           BuildShader(device, code)};
}

ComputePipeline* PipelineCache::CreateComputePipelineWithoutShader(VAddr shader_cpu_addr) {
    ShaderInfo shader;
    ComputePipeline pipeline{CreateComputePipeline(&shader)};
    const ComputePipelineCacheKey key{MakeComputePipelineKey(shader.unique_hash)};
    shader.compute_users.push_back(key);
    pipeline.AddRef();

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

void PipelineCache::OnShaderRemoval(ShaderInfo* shader) {
    for (const ComputePipelineCacheKey& key : shader->compute_users) {
        const auto it = compute_cache.find(key);
        ASSERT(it != compute_cache.end());

        Pipeline& pipeline = it->second;
        if (pipeline.RemoveRef()) {
            // Wait for the pipeline to be free of GPU usage before destroying it
            scheduler.Wait(pipeline.UsageTick());
            compute_cache.erase(it);
        }
    }
}

} // namespace Vulkan
