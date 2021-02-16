// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <boost/functional/hash.hpp>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/shader_cache.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Core {
class System;
}

namespace Vulkan {

class Device;
class RasterizerVulkan;
class ComputePipeline;
class VKDescriptorPool;
class VKScheduler;
class VKUpdateDescriptorQueue;

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

struct ComputePipelineCacheKey {
    GPUVAddr shader;
    u32 shared_memory_size;
    std::array<u32, 3> workgroup_size;

    size_t Hash() const noexcept;

    bool operator==(const ComputePipelineCacheKey& rhs) const noexcept;

    bool operator!=(const ComputePipelineCacheKey& rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(std::has_unique_object_representations_v<ComputePipelineCacheKey>);
static_assert(std::is_trivially_copyable_v<ComputePipelineCacheKey>);
static_assert(std::is_trivially_constructible_v<ComputePipelineCacheKey>);

} // namespace Vulkan

namespace std {

template <>
struct hash<Vulkan::ComputePipelineCacheKey> {
    size_t operator()(const Vulkan::ComputePipelineCacheKey& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std

namespace Vulkan {

class Shader {
public:
    explicit Shader();
    ~Shader();
};

class PipelineCache final : public VideoCommon::ShaderCache<Shader> {
public:
    explicit PipelineCache(RasterizerVulkan& rasterizer, Tegra::GPU& gpu,
                           Tegra::Engines::Maxwell3D& maxwell3d,
                           Tegra::Engines::KeplerCompute& kepler_compute,
                           Tegra::MemoryManager& gpu_memory, const Device& device,
                           VKScheduler& scheduler, VKDescriptorPool& descriptor_pool,
                           VKUpdateDescriptorQueue& update_descriptor_queue);
    ~PipelineCache() override;

    ComputePipeline& GetComputePipeline(const ComputePipelineCacheKey& key);

protected:
    void OnShaderRemoval(Shader* shader) final;

private:
    Tegra::GPU& gpu;
    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::Engines::KeplerCompute& kepler_compute;
    Tegra::MemoryManager& gpu_memory;

    const Device& device;
    VKScheduler& scheduler;
    VKDescriptorPool& descriptor_pool;
    VKUpdateDescriptorQueue& update_descriptor_queue;

    std::unique_ptr<Shader> null_shader;
    std::unique_ptr<Shader> null_kernel;

    std::array<Shader*, Maxwell::MaxShaderProgram> last_shaders{};

    std::mutex pipeline_cache;
    std::unordered_map<ComputePipelineCacheKey, std::unique_ptr<ComputePipeline>> compute_cache;
};

} // namespace Vulkan
