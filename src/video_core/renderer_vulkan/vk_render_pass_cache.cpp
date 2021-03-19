// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#include <boost/container/static_vector.hpp>

#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_render_pass_cache.h"
#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
using VideoCore::Surface::PixelFormat;

constexpr std::array ATTACHMENT_REFERENCES{
    VkAttachmentReference{0, VK_IMAGE_LAYOUT_GENERAL},
    VkAttachmentReference{1, VK_IMAGE_LAYOUT_GENERAL},
    VkAttachmentReference{2, VK_IMAGE_LAYOUT_GENERAL},
    VkAttachmentReference{3, VK_IMAGE_LAYOUT_GENERAL},
    VkAttachmentReference{4, VK_IMAGE_LAYOUT_GENERAL},
    VkAttachmentReference{5, VK_IMAGE_LAYOUT_GENERAL},
    VkAttachmentReference{6, VK_IMAGE_LAYOUT_GENERAL},
    VkAttachmentReference{7, VK_IMAGE_LAYOUT_GENERAL},
    VkAttachmentReference{8, VK_IMAGE_LAYOUT_GENERAL},
};

VkAttachmentDescription AttachmentDescription(const Device& device, PixelFormat format,
                                              VkSampleCountFlagBits samples) {
    using MaxwellToVK::SurfaceFormat;
    return {
        .flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
        .format = SurfaceFormat(device, FormatType::Optimal, true, format).format,
        .samples = samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
}
} // Anonymous namespace

RenderPassCache::RenderPassCache(const Device& device_) : device{&device_} {}

VkRenderPass RenderPassCache::Get(const RenderPassKey& key) {
    const auto [pair, is_new] = cache.try_emplace(key);
    if (!is_new) {
        return *pair->second;
    }
    boost::container::static_vector<VkAttachmentDescription, 9> descriptions;
    u32 num_images{0};

    for (size_t index = 0; index < key.color_formats.size(); ++index) {
        const PixelFormat format{key.color_formats[index]};
        if (format == PixelFormat::Invalid) {
            continue;
        }
        descriptions.push_back(AttachmentDescription(*device, format, key.samples));
        ++num_images;
    }
    const size_t num_colors{descriptions.size()};
    const VkAttachmentReference* depth_attachment{};
    if (key.depth_format != PixelFormat::Invalid) {
        depth_attachment = &ATTACHMENT_REFERENCES[num_colors];
        descriptions.push_back(AttachmentDescription(*device, key.depth_format, key.samples));
    }
    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = static_cast<u32>(num_colors),
        .pColorAttachments = num_colors != 0 ? ATTACHMENT_REFERENCES.data() : nullptr,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = depth_attachment,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };
    pair->second = device->GetLogical().CreateRenderPass({
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<u32>(descriptions.size()),
        .pAttachments = descriptions.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 0,
        .pDependencies = nullptr,
    });
    return *pair->second;
}

} // namespace Vulkan
