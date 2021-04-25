// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <span>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

// Prefer small grow rates to avoid saturating the descriptor pool with barely used pipelines
constexpr size_t SETS_GROW_RATE = 16;
constexpr s32 SCORE_THRESHOLD = 3;
constexpr u32 SETS_PER_POOL = 64;

struct DescriptorBank {
    DescriptorBankInfo info;
    std::vector<vk::DescriptorPool> pools;
};

bool DescriptorBankInfo::IsSuperset(const DescriptorBankInfo& subset) const noexcept {
    return uniform_buffers >= subset.uniform_buffers && storage_buffers >= subset.storage_buffers &&
           texture_buffers >= subset.texture_buffers && image_buffers >= subset.image_buffers &&
           textures >= subset.textures && images >= subset.image_buffers;
}

template <typename Descriptors>
static u32 Accumulate(const Descriptors& descriptors) {
    u32 count = 0;
    for (const auto& descriptor : descriptors) {
        count += descriptor.count;
    }
    return count;
}

static DescriptorBankInfo MakeBankInfo(std::span<const Shader::Info> infos) {
    DescriptorBankInfo bank;
    for (const Shader::Info& info : infos) {
        bank.uniform_buffers += Accumulate(info.constant_buffer_descriptors);
        bank.storage_buffers += Accumulate(info.storage_buffers_descriptors);
        bank.texture_buffers += Accumulate(info.texture_buffer_descriptors);
        bank.image_buffers += Accumulate(info.image_buffer_descriptors);
        bank.textures += Accumulate(info.texture_descriptors);
        bank.images += Accumulate(info.image_descriptors);
    }
    bank.score = bank.uniform_buffers + bank.storage_buffers + bank.texture_buffers +
                 bank.image_buffers + bank.textures + bank.images;
    return bank;
}

static void AllocatePool(const Device& device, DescriptorBank& bank) {
    std::array<VkDescriptorPoolSize, 6> pool_sizes;
    size_t pool_cursor{};
    const auto add = [&](VkDescriptorType type, u32 count) {
        if (count > 0) {
            pool_sizes[pool_cursor++] = {
                .type = type,
                .descriptorCount = count * SETS_PER_POOL,
            };
        }
    };
    const auto& info{bank.info};
    add(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, info.uniform_buffers);
    add(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, info.storage_buffers);
    add(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, info.texture_buffers);
    add(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, info.image_buffers);
    add(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, info.textures);
    add(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, info.images);
    bank.pools.push_back(device.GetLogical().CreateDescriptorPool({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = SETS_PER_POOL,
        .poolSizeCount = static_cast<u32>(pool_cursor),
        .pPoolSizes = std::data(pool_sizes),
    }));
}

DescriptorAllocator::DescriptorAllocator(const Device& device_, MasterSemaphore& master_semaphore_,
                                         DescriptorBank& bank_, VkDescriptorSetLayout layout_)
    : ResourcePool(master_semaphore_, SETS_GROW_RATE), device{&device_}, bank{&bank_},
      layout{layout_} {}

VkDescriptorSet DescriptorAllocator::Commit() {
    const size_t index = CommitResource();
    return sets[index / SETS_GROW_RATE][index % SETS_GROW_RATE];
}

void DescriptorAllocator::Allocate(size_t begin, size_t end) {
    sets.push_back(AllocateDescriptors(end - begin));
}

vk::DescriptorSets DescriptorAllocator::AllocateDescriptors(size_t count) {
    const std::vector<VkDescriptorSetLayout> layouts(count, layout);
    VkDescriptorSetAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = *bank->pools.back(),
        .descriptorSetCount = static_cast<u32>(count),
        .pSetLayouts = layouts.data(),
    };
    vk::DescriptorSets new_sets = bank->pools.back().Allocate(allocate_info);
    if (!new_sets.IsOutOfPoolMemory()) {
        return new_sets;
    }
    // Our current pool is out of memory. Allocate a new one and retry
    AllocatePool(*device, *bank);
    allocate_info.descriptorPool = *bank->pools.back();
    new_sets = bank->pools.back().Allocate(allocate_info);
    if (!new_sets.IsOutOfPoolMemory()) {
        return new_sets;
    }
    // After allocating a new pool, we are out of memory again. We can't handle this from here.
    throw vk::Exception(VK_ERROR_OUT_OF_POOL_MEMORY);
}

DescriptorPool::DescriptorPool(const Device& device_, VKScheduler& scheduler)
    : device{device_}, master_semaphore{scheduler.GetMasterSemaphore()} {}

DescriptorPool::~DescriptorPool() = default;

DescriptorAllocator DescriptorPool::Allocator(VkDescriptorSetLayout layout,
                                              std::span<const Shader::Info> infos) {
    return Allocator(layout, MakeBankInfo(infos));
}

DescriptorAllocator DescriptorPool::Allocator(VkDescriptorSetLayout layout,
                                              const Shader::Info& info) {
    return Allocator(layout, MakeBankInfo(std::array{info}));
}

DescriptorAllocator DescriptorPool::Allocator(VkDescriptorSetLayout layout,
                                              const DescriptorBankInfo& info) {
    return DescriptorAllocator(device, master_semaphore, Bank(info), layout);
}

DescriptorBank& DescriptorPool::Bank(const DescriptorBankInfo& reqs) {
    std::shared_lock read_lock{banks_mutex};
    const auto it = std::ranges::find_if(bank_infos, [&reqs](const DescriptorBankInfo& bank) {
        return std::abs(bank.score - reqs.score) < SCORE_THRESHOLD && bank.IsSuperset(reqs);
    });
    if (it != bank_infos.end()) {
        return *banks[std::distance(bank_infos.begin(), it)].get();
    }
    read_lock.unlock();

    std::unique_lock write_lock{banks_mutex};
    bank_infos.push_back(reqs);

    auto& bank = *banks.emplace_back(std::make_unique<DescriptorBank>());
    bank.info = reqs;
    AllocatePool(device, bank);
    return bank;
}

} // namespace Vulkan
