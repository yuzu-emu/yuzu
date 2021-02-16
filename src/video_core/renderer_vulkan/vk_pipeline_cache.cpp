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
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_notify.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
MICROPROFILE_DECLARE(Vulkan_PipelineCache);

using Tegra::Engines::ShaderType;

namespace {
size_t StageFromProgram(size_t program) {
    return program == 0 ? 0 : program - 1;
}

ShaderType StageFromProgram(Maxwell::ShaderProgram program) {
    return static_cast<ShaderType>(StageFromProgram(static_cast<size_t>(program)));
}

ShaderType GetShaderType(Maxwell::ShaderProgram program) {
    switch (program) {
    case Maxwell::ShaderProgram::VertexB:
        return ShaderType::Vertex;
    case Maxwell::ShaderProgram::TesselationControl:
        return ShaderType::TesselationControl;
    case Maxwell::ShaderProgram::TesselationEval:
        return ShaderType::TesselationEval;
    case Maxwell::ShaderProgram::Geometry:
        return ShaderType::Geometry;
    case Maxwell::ShaderProgram::Fragment:
        return ShaderType::Fragment;
    default:
        UNIMPLEMENTED_MSG("program={}", program);
        return ShaderType::Vertex;
    }
}
} // Anonymous namespace

size_t ComputePipelineCacheKey::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this);
    return static_cast<size_t>(hash);
}

bool ComputePipelineCacheKey::operator==(const ComputePipelineCacheKey& rhs) const noexcept {
    return std::memcmp(&rhs, this, sizeof *this) == 0;
}

Shader::Shader() = default;

Shader::~Shader() = default;

PipelineCache::PipelineCache(RasterizerVulkan& rasterizer_, Tegra::GPU& gpu_,
                             Tegra::Engines::Maxwell3D& maxwell3d_,
                             Tegra::Engines::KeplerCompute& kepler_compute_,
                             Tegra::MemoryManager& gpu_memory_, const Device& device_,
                             VKScheduler& scheduler_, VKDescriptorPool& descriptor_pool_,
                             VKUpdateDescriptorQueue& update_descriptor_queue_)
    : VideoCommon::ShaderCache<Shader>{rasterizer_}, gpu{gpu_}, maxwell3d{maxwell3d_},
      kepler_compute{kepler_compute_}, gpu_memory{gpu_memory_}, device{device_},
      scheduler{scheduler_}, descriptor_pool{descriptor_pool_}, update_descriptor_queue{
                                                                    update_descriptor_queue_} {}

PipelineCache::~PipelineCache() = default;

ComputePipeline& PipelineCache::GetComputePipeline(const ComputePipelineCacheKey& key) {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    const auto [pair, is_cache_miss] = compute_cache.try_emplace(key);
    auto& entry = pair->second;
    if (!is_cache_miss) {
        return *entry;
    }
    LOG_INFO(Render_Vulkan, "Compile 0x{:016X}", key.Hash());
    throw "Bad";
}

void PipelineCache::OnShaderRemoval(Shader*) {}

} // namespace Vulkan
