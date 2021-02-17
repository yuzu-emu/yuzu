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
    u128 unique_hash;
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

struct ShaderInfo {
    u128 unique_hash{};
    size_t size_bytes{};
    std::vector<ComputePipelineCacheKey> compute_users;
};

class PipelineCache final : public VideoCommon::ShaderCache<ShaderInfo> {
public:
    explicit PipelineCache(RasterizerVulkan& rasterizer, Tegra::GPU& gpu,
                           Tegra::Engines::Maxwell3D& maxwell3d,
                           Tegra::Engines::KeplerCompute& kepler_compute,
                           Tegra::MemoryManager& gpu_memory, const Device& device,
                           VKScheduler& scheduler, VKDescriptorPool& descriptor_pool,
                           VKUpdateDescriptorQueue& update_descriptor_queue);
    ~PipelineCache() override;

    [[nodiscard]] ComputePipeline* CurrentComputePipeline();

protected:
    void OnShaderRemoval(ShaderInfo* shader) override;

private:
    ComputePipeline CreateComputePipeline(ShaderInfo* shader);

    ComputePipeline* CreateComputePipelineWithoutShader(VAddr shader_cpu_addr);

    ComputePipelineCacheKey MakeComputePipelineKey(u128 unique_hash) const;

    Tegra::GPU& gpu;
    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::Engines::KeplerCompute& kepler_compute;
    Tegra::MemoryManager& gpu_memory;

    const Device& device;
    VKScheduler& scheduler;
    VKDescriptorPool& descriptor_pool;
    VKUpdateDescriptorQueue& update_descriptor_queue;

    std::unordered_map<ComputePipelineCacheKey, ComputePipeline> compute_cache;
};

} // namespace Vulkan
