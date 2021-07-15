// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <unordered_map>

#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

struct RenderPassKey {
    auto operator<=>(const RenderPassKey&) const noexcept = default;

    std::array<VideoCore::Surface::PixelFormat, 8> color_formats;
    VideoCore::Surface::PixelFormat depth_format;
    VkSampleCountFlagBits samples;
};

} // namespace Vulkan

namespace std {
template <>
struct hash<Vulkan::RenderPassKey> {
    [[nodiscard]] size_t operator()(const Vulkan::RenderPassKey& key) const noexcept {
        size_t value = static_cast<size_t>(key.depth_format) << 48;
        value ^= static_cast<size_t>(key.samples) << 52;
        for (size_t i = 0; i < key.color_formats.size(); ++i) {
            value ^= static_cast<size_t>(key.color_formats[i]) << (i * 6);
        }
        return value;
    }
};
} // namespace std

namespace Vulkan {

class Device;

class RenderPassCache {
public:
    explicit RenderPassCache(const Device& device_);

    VkRenderPass Get(const RenderPassKey& key);

private:
    const Device* device{};
    std::unordered_map<RenderPassKey, vk::RenderPass> cache;
    std::mutex mutex;
};

} // namespace Vulkan
