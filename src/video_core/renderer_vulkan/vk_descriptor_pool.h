// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <shared_mutex>
#include <span>
#include <vector>

#include "shader_recompiler/shader_info.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class VKScheduler;

struct DescriptorBank;

struct DescriptorBankInfo {
    [[nodiscard]] bool IsSuperset(const DescriptorBankInfo& subset) const noexcept;

    u32 uniform_buffers{}; ///< Number of uniform buffer descriptors
    u32 storage_buffers{}; ///< Number of storage buffer descriptors
    u32 texture_buffers{}; ///< Number of texture buffer descriptors
    u32 image_buffers{};   ///< Number of image buffer descriptors
    u32 textures{};        ///< Number of texture descriptors
    u32 images{};          ///< Number of image descriptors
    s32 score{};           ///< Number of descriptors in total
};

class DescriptorAllocator final : public ResourcePool {
    friend class DescriptorPool;

public:
    explicit DescriptorAllocator() = default;
    ~DescriptorAllocator() override = default;

    DescriptorAllocator& operator=(DescriptorAllocator&&) noexcept = default;
    DescriptorAllocator(DescriptorAllocator&&) noexcept = default;

    DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;
    DescriptorAllocator(const DescriptorAllocator&) = delete;

    VkDescriptorSet Commit();

private:
    explicit DescriptorAllocator(const Device& device_, MasterSemaphore& master_semaphore_,
                                 DescriptorBank& bank_, VkDescriptorSetLayout layout_);

    void Allocate(size_t begin, size_t end) override;

    vk::DescriptorSets AllocateDescriptors(size_t count);

    const Device* device{};
    DescriptorBank* bank{};
    VkDescriptorSetLayout layout{};

    std::vector<vk::DescriptorSets> sets;
};

class DescriptorPool {
public:
    explicit DescriptorPool(const Device& device, VKScheduler& scheduler);
    ~DescriptorPool();

    DescriptorPool& operator=(const DescriptorPool&) = delete;
    DescriptorPool(const DescriptorPool&) = delete;

    DescriptorAllocator Allocator(VkDescriptorSetLayout layout,
                                  std::span<const Shader::Info> infos);
    DescriptorAllocator Allocator(VkDescriptorSetLayout layout, const Shader::Info& info);
    DescriptorAllocator Allocator(VkDescriptorSetLayout layout, const DescriptorBankInfo& info);

private:
    DescriptorBank& Bank(const DescriptorBankInfo& reqs);

    const Device& device;
    MasterSemaphore& master_semaphore;

    std::shared_mutex banks_mutex;
    std::vector<DescriptorBankInfo> bank_infos;
    std::vector<std::unique_ptr<DescriptorBank>> banks;
};

} // namespace Vulkan