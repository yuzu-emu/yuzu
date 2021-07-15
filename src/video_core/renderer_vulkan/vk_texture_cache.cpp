// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include "common/bit_cast.h"
#include "common/settings.h"

#include "video_core/engines/fermi_2d.h"
#include "video_core/renderer_vulkan/blit_image.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_compute_pass.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_render_pass_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using Tegra::Engines::Fermi2D;
using Tegra::Texture::SwizzleSource;
using Tegra::Texture::TextureMipmapFilter;
using VideoCommon::BufferImageCopy;
using VideoCommon::ImageInfo;
using VideoCommon::ImageType;
using VideoCommon::SubresourceRange;
using VideoCore::Surface::IsPixelFormatASTC;

namespace {
constexpr VkBorderColor ConvertBorderColor(const std::array<float, 4>& color) {
    if (color == std::array<float, 4>{0, 0, 0, 0}) {
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    } else if (color == std::array<float, 4>{0, 0, 0, 1}) {
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    } else if (color == std::array<float, 4>{1, 1, 1, 1}) {
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }
    if (color[0] + color[1] + color[2] > 1.35f) {
        // If color elements are brighter than roughly 0.5 average, use white border
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    } else if (color[3] > 0.5f) {
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    } else {
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }
}

[[nodiscard]] VkImageType ConvertImageType(const ImageType type) {
    switch (type) {
    case ImageType::e1D:
        return VK_IMAGE_TYPE_1D;
    case ImageType::e2D:
    case ImageType::Linear:
        return VK_IMAGE_TYPE_2D;
    case ImageType::e3D:
        return VK_IMAGE_TYPE_3D;
    case ImageType::Buffer:
        break;
    }
    UNREACHABLE_MSG("Invalid image type={}", type);
    return {};
}

[[nodiscard]] VkSampleCountFlagBits ConvertSampleCount(u32 num_samples) {
    switch (num_samples) {
    case 1:
        return VK_SAMPLE_COUNT_1_BIT;
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    default:
        UNREACHABLE_MSG("Invalid number of samples={}", num_samples);
        return VK_SAMPLE_COUNT_1_BIT;
    }
}

[[nodiscard]] VkImageUsageFlags ImageUsageFlags(const MaxwellToVK::FormatInfo& info,
                                                PixelFormat format) {
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT;
    if (info.attachable) {
        switch (VideoCore::Surface::GetFormatType(format)) {
        case VideoCore::Surface::SurfaceType::ColorTexture:
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;
        case VideoCore::Surface::SurfaceType::Depth:
        case VideoCore::Surface::SurfaceType::DepthStencil:
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        default:
            UNREACHABLE_MSG("Invalid surface type");
        }
    }
    if (info.storage) {
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    return usage;
}

/// Returns the preferred format for a VkImage
[[nodiscard]] PixelFormat StorageFormat(PixelFormat format) {
    switch (format) {
    case PixelFormat::A8B8G8R8_SRGB:
        return PixelFormat::A8B8G8R8_UNORM;
    default:
        return format;
    }
}

[[nodiscard]] VkImageCreateInfo MakeImageCreateInfo(const Device& device, const ImageInfo& info) {
    const PixelFormat format = StorageFormat(info.format);
    const auto format_info = MaxwellToVK::SurfaceFormat(device, FormatType::Optimal, false, format);
    VkImageCreateFlags flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    if (info.type == ImageType::e2D && info.resources.layers >= 6 &&
        info.size.width == info.size.height) {
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }
    if (info.type == ImageType::e3D) {
        flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }
    const auto [samples_x, samples_y] = VideoCommon::SamplesLog2(info.num_samples);
    return VkImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .imageType = ConvertImageType(info.type),
        .format = format_info.format,
        .extent{
            .width = info.size.width >> samples_x,
            .height = info.size.height >> samples_y,
            .depth = info.size.depth,
        },
        .mipLevels = static_cast<u32>(info.resources.levels),
        .arrayLayers = static_cast<u32>(info.resources.layers),
        .samples = ConvertSampleCount(info.num_samples),
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = ImageUsageFlags(format_info, format),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
}

[[nodiscard]] vk::Image MakeImage(const Device& device, const ImageInfo& info) {
    if (info.type == ImageType::Buffer) {
        return vk::Image{};
    }
    return device.GetLogical().CreateImage(MakeImageCreateInfo(device, info));
}

[[nodiscard]] VkImageAspectFlags ImageAspectMask(PixelFormat format) {
    switch (VideoCore::Surface::GetFormatType(format)) {
    case VideoCore::Surface::SurfaceType::ColorTexture:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case VideoCore::Surface::SurfaceType::Depth:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VideoCore::Surface::SurfaceType::DepthStencil:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        UNREACHABLE_MSG("Invalid surface type");
        return VkImageAspectFlags{};
    }
}

[[nodiscard]] VkImageAspectFlags ImageViewAspectMask(const VideoCommon::ImageViewInfo& info) {
    if (info.IsRenderTarget()) {
        return ImageAspectMask(info.format);
    }
    const bool is_first = info.Swizzle()[0] == SwizzleSource::R;
    switch (info.format) {
    case PixelFormat::D24_UNORM_S8_UINT:
    case PixelFormat::D32_FLOAT_S8_UINT:
        return is_first ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
    case PixelFormat::S8_UINT_D24_UNORM:
        return is_first ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    case PixelFormat::D16_UNORM:
    case PixelFormat::D32_FLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

[[nodiscard]] VkComponentSwizzle ComponentSwizzle(SwizzleSource swizzle) {
    switch (swizzle) {
    case SwizzleSource::Zero:
        return VK_COMPONENT_SWIZZLE_ZERO;
    case SwizzleSource::R:
        return VK_COMPONENT_SWIZZLE_R;
    case SwizzleSource::G:
        return VK_COMPONENT_SWIZZLE_G;
    case SwizzleSource::B:
        return VK_COMPONENT_SWIZZLE_B;
    case SwizzleSource::A:
        return VK_COMPONENT_SWIZZLE_A;
    case SwizzleSource::OneFloat:
    case SwizzleSource::OneInt:
        return VK_COMPONENT_SWIZZLE_ONE;
    }
    UNREACHABLE_MSG("Invalid swizzle={}", swizzle);
    return VK_COMPONENT_SWIZZLE_ZERO;
}

[[nodiscard]] VkImageViewType ImageViewType(Shader::TextureType type) {
    switch (type) {
    case Shader::TextureType::Color1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case Shader::TextureType::Color2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case Shader::TextureType::ColorCube:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case Shader::TextureType::Color3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case Shader::TextureType::ColorArray1D:
        return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case Shader::TextureType::ColorArray2D:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case Shader::TextureType::ColorArrayCube:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case Shader::TextureType::Buffer:
        UNREACHABLE_MSG("Texture buffers can't be image views");
        return VK_IMAGE_VIEW_TYPE_1D;
    }
    UNREACHABLE_MSG("Invalid image view type={}", type);
    return VK_IMAGE_VIEW_TYPE_2D;
}

[[nodiscard]] VkImageViewType ImageViewType(VideoCommon::ImageViewType type) {
    switch (type) {
    case VideoCommon::ImageViewType::e1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case VideoCommon::ImageViewType::e2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case VideoCommon::ImageViewType::Cube:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case VideoCommon::ImageViewType::e3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case VideoCommon::ImageViewType::e1DArray:
        return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case VideoCommon::ImageViewType::e2DArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case VideoCommon::ImageViewType::CubeArray:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case VideoCommon::ImageViewType::Rect:
        UNIMPLEMENTED_MSG("Rect image view");
        return VK_IMAGE_VIEW_TYPE_2D;
    case VideoCommon::ImageViewType::Buffer:
        UNREACHABLE_MSG("Texture buffers can't be image views");
        return VK_IMAGE_VIEW_TYPE_1D;
    }
    UNREACHABLE_MSG("Invalid image view type={}", type);
    return VK_IMAGE_VIEW_TYPE_2D;
}

[[nodiscard]] VkImageSubresourceLayers MakeImageSubresourceLayers(
    VideoCommon::SubresourceLayers subresource, VkImageAspectFlags aspect_mask) {
    return VkImageSubresourceLayers{
        .aspectMask = aspect_mask,
        .mipLevel = static_cast<u32>(subresource.base_level),
        .baseArrayLayer = static_cast<u32>(subresource.base_layer),
        .layerCount = static_cast<u32>(subresource.num_layers),
    };
}

[[nodiscard]] VkOffset3D MakeOffset3D(VideoCommon::Offset3D offset3d) {
    return VkOffset3D{
        .x = offset3d.x,
        .y = offset3d.y,
        .z = offset3d.z,
    };
}

[[nodiscard]] VkExtent3D MakeExtent3D(VideoCommon::Extent3D extent3d) {
    return VkExtent3D{
        .width = static_cast<u32>(extent3d.width),
        .height = static_cast<u32>(extent3d.height),
        .depth = static_cast<u32>(extent3d.depth),
    };
}

[[nodiscard]] VkImageCopy MakeImageCopy(const VideoCommon::ImageCopy& copy,
                                        VkImageAspectFlags aspect_mask) noexcept {
    return VkImageCopy{
        .srcSubresource = MakeImageSubresourceLayers(copy.src_subresource, aspect_mask),
        .srcOffset = MakeOffset3D(copy.src_offset),
        .dstSubresource = MakeImageSubresourceLayers(copy.dst_subresource, aspect_mask),
        .dstOffset = MakeOffset3D(copy.dst_offset),
        .extent = MakeExtent3D(copy.extent),
    };
}

[[maybe_unused]] [[nodiscard]] std::vector<VkBufferCopy> TransformBufferCopies(
    std::span<const VideoCommon::BufferCopy> copies, size_t buffer_offset) {
    std::vector<VkBufferCopy> result(copies.size());
    std::ranges::transform(
        copies, result.begin(), [buffer_offset](const VideoCommon::BufferCopy& copy) {
            return VkBufferCopy{
                .srcOffset = static_cast<VkDeviceSize>(copy.src_offset + buffer_offset),
                .dstOffset = static_cast<VkDeviceSize>(copy.dst_offset),
                .size = static_cast<VkDeviceSize>(copy.size),
            };
        });
    return result;
}

[[nodiscard]] std::vector<VkBufferImageCopy> TransformBufferImageCopies(
    std::span<const BufferImageCopy> copies, size_t buffer_offset, VkImageAspectFlags aspect_mask) {
    struct Maker {
        VkBufferImageCopy operator()(const BufferImageCopy& copy) const {
            return VkBufferImageCopy{
                .bufferOffset = copy.buffer_offset + buffer_offset,
                .bufferRowLength = copy.buffer_row_length,
                .bufferImageHeight = copy.buffer_image_height,
                .imageSubresource =
                    {
                        .aspectMask = aspect_mask,
                        .mipLevel = static_cast<u32>(copy.image_subresource.base_level),
                        .baseArrayLayer = static_cast<u32>(copy.image_subresource.base_layer),
                        .layerCount = static_cast<u32>(copy.image_subresource.num_layers),
                    },
                .imageOffset =
                    {
                        .x = copy.image_offset.x,
                        .y = copy.image_offset.y,
                        .z = copy.image_offset.z,
                    },
                .imageExtent =
                    {
                        .width = copy.image_extent.width,
                        .height = copy.image_extent.height,
                        .depth = copy.image_extent.depth,
                    },
            };
        }
        size_t buffer_offset;
        VkImageAspectFlags aspect_mask;
    };
    if (aspect_mask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        std::vector<VkBufferImageCopy> result(copies.size() * 2);
        std::ranges::transform(copies, result.begin(),
                               Maker{buffer_offset, VK_IMAGE_ASPECT_DEPTH_BIT});
        std::ranges::transform(copies, result.begin() + copies.size(),
                               Maker{buffer_offset, VK_IMAGE_ASPECT_STENCIL_BIT});
        return result;
    } else {
        std::vector<VkBufferImageCopy> result(copies.size());
        std::ranges::transform(copies, result.begin(), Maker{buffer_offset, aspect_mask});
        return result;
    }
}

[[nodiscard]] VkImageSubresourceRange MakeSubresourceRange(VkImageAspectFlags aspect_mask,
                                                           const SubresourceRange& range) {
    return VkImageSubresourceRange{
        .aspectMask = aspect_mask,
        .baseMipLevel = static_cast<u32>(range.base.level),
        .levelCount = static_cast<u32>(range.extent.levels),
        .baseArrayLayer = static_cast<u32>(range.base.layer),
        .layerCount = static_cast<u32>(range.extent.layers),
    };
}

[[nodiscard]] VkImageSubresourceRange MakeSubresourceRange(const ImageView* image_view) {
    SubresourceRange range = image_view->range;
    if (True(image_view->flags & VideoCommon::ImageViewFlagBits::Slice)) {
        // Slice image views always affect a single layer, but their subresource range corresponds
        // to the slice. Override the value to affect a single layer.
        range.base.layer = 0;
        range.extent.layers = 1;
    }
    return MakeSubresourceRange(ImageAspectMask(image_view->format), range);
}

[[nodiscard]] VkImageSubresourceLayers MakeSubresourceLayers(const ImageView* image_view) {
    return VkImageSubresourceLayers{
        .aspectMask = ImageAspectMask(image_view->format),
        .mipLevel = static_cast<u32>(image_view->range.base.level),
        .baseArrayLayer = static_cast<u32>(image_view->range.base.layer),
        .layerCount = static_cast<u32>(image_view->range.extent.layers),
    };
}

[[nodiscard]] SwizzleSource ConvertGreenRed(SwizzleSource value) {
    switch (value) {
    case SwizzleSource::G:
        return SwizzleSource::R;
    default:
        return value;
    }
}

[[nodiscard]] SwizzleSource SwapBlueRed(SwizzleSource value) {
    switch (value) {
    case SwizzleSource::R:
        return SwizzleSource::B;
    case SwizzleSource::B:
        return SwizzleSource::R;
    default:
        return value;
    }
}

void CopyBufferToImage(vk::CommandBuffer cmdbuf, VkBuffer src_buffer, VkImage image,
                       VkImageAspectFlags aspect_mask, bool is_initialized,
                       std::span<const VkBufferImageCopy> copies) {
    static constexpr VkAccessFlags WRITE_ACCESS_FLAGS =
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    static constexpr VkAccessFlags READ_ACCESS_FLAGS = VK_ACCESS_SHADER_READ_BIT |
                                                       VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    const VkImageMemoryBarrier read_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = WRITE_ACCESS_FLAGS,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = is_initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange{
            .aspectMask = aspect_mask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };
    const VkImageMemoryBarrier write_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = WRITE_ACCESS_FLAGS | READ_ACCESS_FLAGS,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange{
            .aspectMask = aspect_mask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };
    cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                           read_barrier);
    cmdbuf.CopyBufferToImage(src_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copies);
    // TODO: Move this to another API
    cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                           write_barrier);
}

[[nodiscard]] VkImageBlit MakeImageBlit(const Region2D& dst_region, const Region2D& src_region,
                                        const VkImageSubresourceLayers& dst_layers,
                                        const VkImageSubresourceLayers& src_layers) {
    return VkImageBlit{
        .srcSubresource = src_layers,
        .srcOffsets =
            {
                {
                    .x = src_region.start.x,
                    .y = src_region.start.y,
                    .z = 0,
                },
                {
                    .x = src_region.end.x,
                    .y = src_region.end.y,
                    .z = 1,
                },
            },
        .dstSubresource = dst_layers,
        .dstOffsets =
            {
                {
                    .x = dst_region.start.x,
                    .y = dst_region.start.y,
                    .z = 0,
                },
                {
                    .x = dst_region.end.x,
                    .y = dst_region.end.y,
                    .z = 1,
                },
            },
    };
}

[[nodiscard]] VkImageResolve MakeImageResolve(const Region2D& dst_region,
                                              const Region2D& src_region,
                                              const VkImageSubresourceLayers& dst_layers,
                                              const VkImageSubresourceLayers& src_layers) {
    return VkImageResolve{
        .srcSubresource = src_layers,
        .srcOffset =
            {
                .x = src_region.start.x,
                .y = src_region.start.y,
                .z = 0,
            },
        .dstSubresource = dst_layers,
        .dstOffset =
            {
                .x = dst_region.start.x,
                .y = dst_region.start.y,
                .z = 0,
            },
        .extent =
            {
                .width = static_cast<u32>(dst_region.end.x - dst_region.start.x),
                .height = static_cast<u32>(dst_region.end.y - dst_region.start.y),
                .depth = 1,
            },
    };
}

[[nodiscard]] bool IsFormatFlipped(PixelFormat format) {
    switch (format) {
    case PixelFormat::A1B5G5R5_UNORM:
        return true;
    default:
        return false;
    }
}

struct RangedBarrierRange {
    u32 min_mip = std::numeric_limits<u32>::max();
    u32 max_mip = std::numeric_limits<u32>::min();
    u32 min_layer = std::numeric_limits<u32>::max();
    u32 max_layer = std::numeric_limits<u32>::min();

    void AddLayers(const VkImageSubresourceLayers& layers) {
        min_mip = std::min(min_mip, layers.mipLevel);
        max_mip = std::max(max_mip, layers.mipLevel + 1);
        min_layer = std::min(min_layer, layers.baseArrayLayer);
        max_layer = std::max(max_layer, layers.baseArrayLayer + layers.layerCount);
    }

    VkImageSubresourceRange SubresourceRange(VkImageAspectFlags aspect_mask) const noexcept {
        return VkImageSubresourceRange{
            .aspectMask = aspect_mask,
            .baseMipLevel = min_mip,
            .levelCount = max_mip - min_mip,
            .baseArrayLayer = min_layer,
            .layerCount = max_layer - min_layer,
        };
    }
};

[[nodiscard]] VkFormat Format(Shader::ImageFormat format) {
    switch (format) {
    case Shader::ImageFormat::Typeless:
        break;
    case Shader::ImageFormat::R8_SINT:
        return VK_FORMAT_R8_SINT;
    case Shader::ImageFormat::R8_UINT:
        return VK_FORMAT_R8_UINT;
    case Shader::ImageFormat::R16_UINT:
        return VK_FORMAT_R16_UINT;
    case Shader::ImageFormat::R16_SINT:
        return VK_FORMAT_R16_SINT;
    case Shader::ImageFormat::R32_UINT:
        return VK_FORMAT_R32_UINT;
    case Shader::ImageFormat::R32G32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case Shader::ImageFormat::R32G32B32A32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;
    }
    UNREACHABLE_MSG("Invalid image format={}", format);
    return VK_FORMAT_R32_UINT;
}
} // Anonymous namespace

void TextureCacheRuntime::Finish() {
    scheduler.Finish();
}

StagingBufferRef TextureCacheRuntime::UploadStagingBuffer(size_t size) {
    return staging_buffer_pool.Request(size, MemoryUsage::Upload);
}

StagingBufferRef TextureCacheRuntime::DownloadStagingBuffer(size_t size) {
    return staging_buffer_pool.Request(size, MemoryUsage::Download);
}

void TextureCacheRuntime::BlitImage(Framebuffer* dst_framebuffer, ImageView& dst, ImageView& src,
                                    const Region2D& dst_region, const Region2D& src_region,
                                    Tegra::Engines::Fermi2D::Filter filter,
                                    Tegra::Engines::Fermi2D::Operation operation) {
    const VkImageAspectFlags aspect_mask = ImageAspectMask(src.format);
    const bool is_dst_msaa = dst.Samples() != VK_SAMPLE_COUNT_1_BIT;
    const bool is_src_msaa = src.Samples() != VK_SAMPLE_COUNT_1_BIT;
    ASSERT(aspect_mask == ImageAspectMask(dst.format));
    if (aspect_mask == VK_IMAGE_ASPECT_COLOR_BIT && !is_src_msaa && !is_dst_msaa) {
        blit_image_helper.BlitColor(dst_framebuffer, src, dst_region, src_region, filter,
                                    operation);
        return;
    }
    if (aspect_mask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        if (!device.IsBlitDepthStencilSupported()) {
            UNIMPLEMENTED_IF(is_src_msaa || is_dst_msaa);
            blit_image_helper.BlitDepthStencil(dst_framebuffer, src.DepthView(), src.StencilView(),
                                               dst_region, src_region, filter, operation);
            return;
        }
    }
    ASSERT(src.format == dst.format);
    ASSERT(!(is_dst_msaa && !is_src_msaa));
    ASSERT(operation == Fermi2D::Operation::SrcCopy);

    const VkImage dst_image = dst.ImageHandle();
    const VkImage src_image = src.ImageHandle();
    const VkImageSubresourceLayers dst_layers = MakeSubresourceLayers(&dst);
    const VkImageSubresourceLayers src_layers = MakeSubresourceLayers(&src);
    const bool is_resolve = is_src_msaa && !is_dst_msaa;
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([filter, dst_region, src_region, dst_image, src_image, dst_layers, src_layers,
                      aspect_mask, is_resolve](vk::CommandBuffer cmdbuf) {
        const std::array read_barriers{
            VkImageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = src_image,
                .subresourceRange{
                    .aspectMask = aspect_mask,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            },
            VkImageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = dst_image,
                .subresourceRange{
                    .aspectMask = aspect_mask,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            },
        };
        VkImageMemoryBarrier write_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst_image,
            .subresourceRange{
                .aspectMask = aspect_mask,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               0, nullptr, nullptr, read_barriers);
        if (is_resolve) {
            cmdbuf.ResolveImage(src_image, VK_IMAGE_LAYOUT_GENERAL, dst_image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                MakeImageResolve(dst_region, src_region, dst_layers, src_layers));
        } else {
            const bool is_linear = filter == Fermi2D::Filter::Bilinear;
            const VkFilter vk_filter = is_linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            cmdbuf.BlitImage(
                src_image, VK_IMAGE_LAYOUT_GENERAL, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                MakeImageBlit(dst_region, src_region, dst_layers, src_layers), vk_filter);
        }
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               0, write_barrier);
    });
}

void TextureCacheRuntime::ConvertImage(Framebuffer* dst, ImageView& dst_view, ImageView& src_view) {
    switch (dst_view.format) {
    case PixelFormat::R16_UNORM:
        if (src_view.format == PixelFormat::D16_UNORM) {
            return blit_image_helper.ConvertD16ToR16(dst, src_view);
        }
        break;
    case PixelFormat::R32_FLOAT:
        if (src_view.format == PixelFormat::D32_FLOAT) {
            return blit_image_helper.ConvertD32ToR32(dst, src_view);
        }
        break;
    case PixelFormat::D16_UNORM:
        if (src_view.format == PixelFormat::R16_UNORM) {
            return blit_image_helper.ConvertR16ToD16(dst, src_view);
        }
        break;
    case PixelFormat::D32_FLOAT:
        if (src_view.format == PixelFormat::R32_FLOAT) {
            return blit_image_helper.ConvertR32ToD32(dst, src_view);
        }
        break;
    default:
        break;
    }
    UNIMPLEMENTED_MSG("Unimplemented format copy from {} to {}", src_view.format, dst_view.format);
}

void TextureCacheRuntime::CopyImage(Image& dst, Image& src,
                                    std::span<const VideoCommon::ImageCopy> copies) {
    std::vector<VkImageCopy> vk_copies(copies.size());
    const VkImageAspectFlags aspect_mask = dst.AspectMask();
    ASSERT(aspect_mask == src.AspectMask());

    std::ranges::transform(copies, vk_copies.begin(), [aspect_mask](const auto& copy) {
        return MakeImageCopy(copy, aspect_mask);
    });
    const VkImage dst_image = dst.Handle();
    const VkImage src_image = src.Handle();
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([dst_image, src_image, aspect_mask, vk_copies](vk::CommandBuffer cmdbuf) {
        RangedBarrierRange dst_range;
        RangedBarrierRange src_range;
        for (const VkImageCopy& copy : vk_copies) {
            dst_range.AddLayers(copy.dstSubresource);
            src_range.AddLayers(copy.srcSubresource);
        }
        const std::array read_barriers{
            VkImageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = src_image,
                .subresourceRange = src_range.SubresourceRange(aspect_mask),
            },
            VkImageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = dst_image,
                .subresourceRange = dst_range.SubresourceRange(aspect_mask),
            },
        };
        const VkImageMemoryBarrier write_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst_image,
            .subresourceRange = dst_range.SubresourceRange(aspect_mask),
        };
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               0, {}, {}, read_barriers);
        cmdbuf.CopyImage(src_image, VK_IMAGE_LAYOUT_GENERAL, dst_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vk_copies);
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               0, write_barrier);
    });
}

u64 TextureCacheRuntime::GetDeviceLocalMemory() const {
    return device.GetDeviceLocalMemory();
}

Image::Image(TextureCacheRuntime& runtime, const ImageInfo& info_, GPUVAddr gpu_addr_,
             VAddr cpu_addr_)
    : VideoCommon::ImageBase(info_, gpu_addr_, cpu_addr_), scheduler{&runtime.scheduler},
      image(MakeImage(runtime.device, info)),
      commit(runtime.memory_allocator.Commit(image, MemoryUsage::DeviceLocal)),
      aspect_mask(ImageAspectMask(info.format)) {
    if (IsPixelFormatASTC(info.format) && !runtime.device.IsOptimalAstcSupported()) {
        if (Settings::values.accelerate_astc.GetValue()) {
            flags |= VideoCommon::ImageFlagBits::AcceleratedUpload;
        } else {
            flags |= VideoCommon::ImageFlagBits::Converted;
        }
    }
    if (runtime.device.HasDebuggingToolAttached()) {
        image.SetObjectNameEXT(VideoCommon::Name(*this).c_str());
    }
    static constexpr VkImageViewUsageCreateInfo storage_image_view_usage_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .pNext = nullptr,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT,
    };
    if (IsPixelFormatASTC(info.format) && !runtime.device.IsOptimalAstcSupported()) {
        const auto& device = runtime.device.GetLogical();
        storage_image_views.reserve(info.resources.levels);
        for (s32 level = 0; level < info.resources.levels; ++level) {
            storage_image_views.push_back(device.CreateImageView(VkImageViewCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = &storage_image_view_usage_create_info,
                .flags = 0,
                .image = *image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                .format = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                .components{
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange{
                    .aspectMask = aspect_mask,
                    .baseMipLevel = static_cast<u32>(level),
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            }));
        }
    }
}

Image::~Image() = default;

void Image::UploadMemory(const StagingBufferRef& map, std::span<const BufferImageCopy> copies) {
    // TODO: Move this to another API
    scheduler->RequestOutsideRenderPassOperationContext();
    std::vector vk_copies = TransformBufferImageCopies(copies, map.offset, aspect_mask);
    const VkBuffer src_buffer = map.buffer;
    const VkImage vk_image = *image;
    const VkImageAspectFlags vk_aspect_mask = aspect_mask;
    const bool is_initialized = std::exchange(initialized, true);
    scheduler->Record([src_buffer, vk_image, vk_aspect_mask, is_initialized,
                       vk_copies](vk::CommandBuffer cmdbuf) {
        CopyBufferToImage(cmdbuf, src_buffer, vk_image, vk_aspect_mask, is_initialized, vk_copies);
    });
}

void Image::DownloadMemory(const StagingBufferRef& map, std::span<const BufferImageCopy> copies) {
    std::vector vk_copies = TransformBufferImageCopies(copies, map.offset, aspect_mask);
    scheduler->Record([buffer = map.buffer, image = *image, aspect_mask = aspect_mask,
                       vk_copies](vk::CommandBuffer cmdbuf) {
        const VkImageMemoryBarrier read_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange{
                .aspectMask = aspect_mask,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        const VkImageMemoryBarrier image_write_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange{
                .aspectMask = aspect_mask,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        const VkMemoryBarrier memory_write_barrier{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        };
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               0, read_barrier);
        cmdbuf.CopyImageToBuffer(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, vk_copies);
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               0, memory_write_barrier, nullptr, image_write_barrier);
    });
}

ImageView::ImageView(TextureCacheRuntime& runtime, const VideoCommon::ImageViewInfo& info,
                     ImageId image_id_, Image& image)
    : VideoCommon::ImageViewBase{info, image.info, image_id_}, device{&runtime.device},
      image_handle{image.Handle()}, samples{ConvertSampleCount(image.info.num_samples)} {
    using Shader::TextureType;

    const VkImageAspectFlags aspect_mask = ImageViewAspectMask(info);
    std::array<SwizzleSource, 4> swizzle{
        SwizzleSource::R,
        SwizzleSource::G,
        SwizzleSource::B,
        SwizzleSource::A,
    };
    if (!info.IsRenderTarget()) {
        swizzle = info.Swizzle();
        if (IsFormatFlipped(format)) {
            std::ranges::transform(swizzle, swizzle.begin(), SwapBlueRed);
        }
        if ((aspect_mask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0) {
            std::ranges::transform(swizzle, swizzle.begin(), ConvertGreenRed);
        }
    }
    const auto format_info = MaxwellToVK::SurfaceFormat(*device, FormatType::Optimal, true, format);
    const VkImageViewUsageCreateInfo image_view_usage{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
        .pNext = nullptr,
        .usage = ImageUsageFlags(format_info, format),
    };
    const VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = &image_view_usage,
        .flags = 0,
        .image = image.Handle(),
        .viewType = VkImageViewType{},
        .format = format_info.format,
        .components{
            .r = ComponentSwizzle(swizzle[0]),
            .g = ComponentSwizzle(swizzle[1]),
            .b = ComponentSwizzle(swizzle[2]),
            .a = ComponentSwizzle(swizzle[3]),
        },
        .subresourceRange = MakeSubresourceRange(aspect_mask, info.range),
    };
    const auto create = [&](TextureType tex_type, std::optional<u32> num_layers) {
        VkImageViewCreateInfo ci{create_info};
        ci.viewType = ImageViewType(tex_type);
        if (num_layers) {
            ci.subresourceRange.layerCount = *num_layers;
        }
        vk::ImageView handle = device->GetLogical().CreateImageView(ci);
        if (device->HasDebuggingToolAttached()) {
            handle.SetObjectNameEXT(VideoCommon::Name(*this).c_str());
        }
        image_views[static_cast<size_t>(tex_type)] = std::move(handle);
    };
    switch (info.type) {
    case VideoCommon::ImageViewType::e1D:
    case VideoCommon::ImageViewType::e1DArray:
        create(TextureType::Color1D, 1);
        create(TextureType::ColorArray1D, std::nullopt);
        render_target = Handle(TextureType::ColorArray1D);
        break;
    case VideoCommon::ImageViewType::e2D:
    case VideoCommon::ImageViewType::e2DArray:
        create(TextureType::Color2D, 1);
        create(TextureType::ColorArray2D, std::nullopt);
        render_target = Handle(Shader::TextureType::ColorArray2D);
        break;
    case VideoCommon::ImageViewType::e3D:
        create(TextureType::Color3D, std::nullopt);
        render_target = Handle(Shader::TextureType::Color3D);
        break;
    case VideoCommon::ImageViewType::Cube:
    case VideoCommon::ImageViewType::CubeArray:
        create(TextureType::ColorCube, 6);
        create(TextureType::ColorArrayCube, std::nullopt);
        break;
    case VideoCommon::ImageViewType::Rect:
        UNIMPLEMENTED();
        break;
    case VideoCommon::ImageViewType::Buffer:
        UNREACHABLE();
        break;
    }
}

ImageView::ImageView(TextureCacheRuntime&, const VideoCommon::ImageInfo& info,
                     const VideoCommon::ImageViewInfo& view_info, GPUVAddr gpu_addr_)
    : VideoCommon::ImageViewBase{info, view_info}, gpu_addr{gpu_addr_},
      buffer_size{VideoCommon::CalculateGuestSizeInBytes(info)} {}

ImageView::ImageView(TextureCacheRuntime&, const VideoCommon::NullImageParams& params)
    : VideoCommon::ImageViewBase{params} {}

VkImageView ImageView::DepthView() {
    if (depth_view) {
        return *depth_view;
    }
    const auto& info = MaxwellToVK::SurfaceFormat(*device, FormatType::Optimal, true, format);
    depth_view = MakeView(info.format, VK_IMAGE_ASPECT_DEPTH_BIT);
    return *depth_view;
}

VkImageView ImageView::StencilView() {
    if (stencil_view) {
        return *stencil_view;
    }
    const auto& info = MaxwellToVK::SurfaceFormat(*device, FormatType::Optimal, true, format);
    stencil_view = MakeView(info.format, VK_IMAGE_ASPECT_STENCIL_BIT);
    return *stencil_view;
}

VkImageView ImageView::StorageView(Shader::TextureType texture_type,
                                   Shader::ImageFormat image_format) {
    if (image_format == Shader::ImageFormat::Typeless) {
        return Handle(texture_type);
    }
    const bool is_signed{image_format == Shader::ImageFormat::R8_SINT ||
                         image_format == Shader::ImageFormat::R16_SINT};
    if (!storage_views) {
        storage_views = std::make_unique<StorageViews>();
    }
    auto& views{is_signed ? storage_views->signeds : storage_views->unsigneds};
    auto& view{views[static_cast<size_t>(texture_type)]};
    if (view) {
        return *view;
    }
    view = MakeView(Format(image_format), VK_IMAGE_ASPECT_COLOR_BIT);
    return *view;
}

vk::ImageView ImageView::MakeView(VkFormat vk_format, VkImageAspectFlags aspect_mask) {
    return device->GetLogical().CreateImageView({
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image_handle,
        .viewType = ImageViewType(type),
        .format = vk_format,
        .components{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = MakeSubresourceRange(aspect_mask, range),
    });
}

Sampler::Sampler(TextureCacheRuntime& runtime, const Tegra::Texture::TSCEntry& tsc) {
    const auto& device = runtime.device;
    const bool arbitrary_borders = runtime.device.IsExtCustomBorderColorSupported();
    const auto color = tsc.BorderColor();

    const VkSamplerCustomBorderColorCreateInfoEXT border_ci{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT,
        .pNext = nullptr,
        // TODO: Make use of std::bit_cast once libc++ supports it.
        .customBorderColor = Common::BitCast<VkClearColorValue>(color),
        .format = VK_FORMAT_UNDEFINED,
    };
    const void* pnext = nullptr;
    if (arbitrary_borders) {
        pnext = &border_ci;
    }
    const VkSamplerReductionModeCreateInfoEXT reduction_ci{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT,
        .pNext = pnext,
        .reductionMode = MaxwellToVK::SamplerReduction(tsc.reduction_filter),
    };
    if (runtime.device.IsExtSamplerFilterMinmaxSupported()) {
        pnext = &reduction_ci;
    } else if (reduction_ci.reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT) {
        LOG_WARNING(Render_Vulkan, "VK_EXT_sampler_filter_minmax is required");
    }
    // Some games have samplers with garbage. Sanitize them here.
    const float max_anisotropy = std::clamp(tsc.MaxAnisotropy(), 1.0f, 16.0f);
    sampler = device.GetLogical().CreateSampler(VkSamplerCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = pnext,
        .flags = 0,
        .magFilter = MaxwellToVK::Sampler::Filter(tsc.mag_filter),
        .minFilter = MaxwellToVK::Sampler::Filter(tsc.min_filter),
        .mipmapMode = MaxwellToVK::Sampler::MipmapMode(tsc.mipmap_filter),
        .addressModeU = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_u, tsc.mag_filter),
        .addressModeV = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_v, tsc.mag_filter),
        .addressModeW = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_p, tsc.mag_filter),
        .mipLodBias = tsc.LodBias(),
        .anisotropyEnable = static_cast<VkBool32>(max_anisotropy > 1.0f ? VK_TRUE : VK_FALSE),
        .maxAnisotropy = max_anisotropy,
        .compareEnable = tsc.depth_compare_enabled,
        .compareOp = MaxwellToVK::Sampler::DepthCompareFunction(tsc.depth_compare_func),
        .minLod = tsc.mipmap_filter == TextureMipmapFilter::None ? 0.0f : tsc.MinLod(),
        .maxLod = tsc.mipmap_filter == TextureMipmapFilter::None ? 0.25f : tsc.MaxLod(),
        .borderColor =
            arbitrary_borders ? VK_BORDER_COLOR_INT_CUSTOM_EXT : ConvertBorderColor(color),
        .unnormalizedCoordinates = VK_FALSE,
    });
}

Framebuffer::Framebuffer(TextureCacheRuntime& runtime, std::span<ImageView*, NUM_RT> color_buffers,
                         ImageView* depth_buffer, const VideoCommon::RenderTargets& key) {
    std::vector<VkImageView> attachments;
    RenderPassKey renderpass_key{};
    s32 num_layers = 1;

    for (size_t index = 0; index < NUM_RT; ++index) {
        const ImageView* const color_buffer = color_buffers[index];
        if (!color_buffer) {
            renderpass_key.color_formats[index] = PixelFormat::Invalid;
            continue;
        }
        attachments.push_back(color_buffer->RenderTarget());
        renderpass_key.color_formats[index] = color_buffer->format;
        num_layers = std::max(num_layers, color_buffer->range.extent.layers);
        images[num_images] = color_buffer->ImageHandle();
        image_ranges[num_images] = MakeSubresourceRange(color_buffer);
        samples = color_buffer->Samples();
        ++num_images;
    }
    const size_t num_colors = attachments.size();
    if (depth_buffer) {
        attachments.push_back(depth_buffer->RenderTarget());
        renderpass_key.depth_format = depth_buffer->format;
        num_layers = std::max(num_layers, depth_buffer->range.extent.layers);
        images[num_images] = depth_buffer->ImageHandle();
        image_ranges[num_images] = MakeSubresourceRange(depth_buffer);
        samples = depth_buffer->Samples();
        ++num_images;
    } else {
        renderpass_key.depth_format = PixelFormat::Invalid;
    }
    renderpass_key.samples = samples;

    renderpass = runtime.render_pass_cache.Get(renderpass_key);

    render_area = VkExtent2D{
        .width = key.size.width,
        .height = key.size.height,
    };
    num_color_buffers = static_cast<u32>(num_colors);
    framebuffer = runtime.device.GetLogical().CreateFramebuffer({
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = renderpass,
        .attachmentCount = static_cast<u32>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = key.size.width,
        .height = key.size.height,
        .layers = static_cast<u32>(std::max(num_layers, 1)),
    });
    if (runtime.device.HasDebuggingToolAttached()) {
        framebuffer.SetObjectNameEXT(VideoCommon::Name(key).c_str());
    }
}

void TextureCacheRuntime::AccelerateImageUpload(
    Image& image, const StagingBufferRef& map,
    std::span<const VideoCommon::SwizzleParameters> swizzles) {
    if (IsPixelFormatASTC(image.info.format)) {
        return astc_decoder_pass.Assemble(image, map, swizzles);
    }
    UNREACHABLE();
}

} // namespace Vulkan
