// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Layout {
struct FramebufferLayout;
}

namespace Vulkan {

class Device;
class VKScheduler;

class VKSwapchain {
public:
    explicit VKSwapchain(VkSurfaceKHR surface, const Device& device, VKScheduler& scheduler,
                         u32 width, u32 height, bool srgb);
    ~VKSwapchain();

    /// Creates (or recreates) the swapchain with a given size.
    void Create(u32 width, u32 height, bool srgb);

    /// Acquires the next image in the swapchain, waits as needed.
    void AcquireNextImage();

    /// Presents the rendered image to the swapchain.
    void Present(VkSemaphore render_semaphore);

    /// Returns true when the color space has changed.
    bool HasColorSpaceChanged(bool is_srgb) const {
        return current_srgb != is_srgb;
    }

    /// Returns true when the swapchain is outdated.
    bool IsOutDated() const {
        return is_outdated;
    }

    /// Returns true when the swapchain is suboptimal.
    bool IsSubOptimal() const {
        return is_suboptimal;
    }

    VkExtent2D GetSize() const {
        return extent;
    }

    std::size_t GetImageCount() const {
        return image_count;
    }

    std::size_t GetImageIndex() const {
        return image_index;
    }

    VkImage GetImageIndex(std::size_t index) const {
        return images[index];
    }

    VkImageView GetImageViewIndex(std::size_t index) const {
        return *image_views[index];
    }

    VkFormat GetImageFormat() const {
        return image_format;
    }

private:
    void CreateSwapchain(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height,
                         bool srgb);
    void CreateSemaphores();
    void CreateImageViews();

    void Destroy();

    const VkSurfaceKHR surface;
    const Device& device;
    VKScheduler& scheduler;

    vk::SwapchainKHR swapchain;

    std::size_t image_count{};
    std::vector<VkImage> images;
    std::vector<vk::ImageView> image_views;
    std::vector<vk::Framebuffer> framebuffers;
    std::vector<u64> resource_ticks;
    std::vector<vk::Semaphore> present_semaphores;

    u32 image_index{};
    u32 frame_index{};

    VkFormat image_format{};
    VkExtent2D extent{};

    bool current_srgb{};
    bool is_outdated{};
    bool is_suboptimal{};
};

} // namespace Vulkan
