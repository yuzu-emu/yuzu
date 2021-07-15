// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class VKScheduler;

struct DescriptorUpdateEntry {
    struct Empty {};

    DescriptorUpdateEntry() = default;
    DescriptorUpdateEntry(VkDescriptorImageInfo image_) : image{image_} {}
    DescriptorUpdateEntry(VkDescriptorBufferInfo buffer_) : buffer{buffer_} {}
    DescriptorUpdateEntry(VkBufferView texel_buffer_) : texel_buffer{texel_buffer_} {}

    union {
        Empty empty{};
        VkDescriptorImageInfo image;
        VkDescriptorBufferInfo buffer;
        VkBufferView texel_buffer;
    };
};

class VKUpdateDescriptorQueue final {
public:
    explicit VKUpdateDescriptorQueue(const Device& device_, VKScheduler& scheduler_);
    ~VKUpdateDescriptorQueue();

    void TickFrame();

    void Acquire();

    const DescriptorUpdateEntry* UpdateData() const noexcept {
        return upload_start;
    }

    void AddSampledImage(VkImageView image_view, VkSampler sampler) {
        *(payload_cursor++) = VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = image_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
    }

    void AddImage(VkImageView image_view) {
        *(payload_cursor++) = VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = image_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
    }

    void AddBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size) {
        *(payload_cursor++) = VkDescriptorBufferInfo{
            .buffer = buffer,
            .offset = offset,
            .range = size,
        };
    }

    void AddTexelBuffer(VkBufferView texel_buffer) {
        *(payload_cursor++) = texel_buffer;
    }

private:
    const Device& device;
    VKScheduler& scheduler;

    DescriptorUpdateEntry* payload_cursor = nullptr;
    const DescriptorUpdateEntry* upload_start = nullptr;
    std::array<DescriptorUpdateEntry, 0x10000> payload;
};

} // namespace Vulkan
