// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <compare>
#include <span>

#include "shader_recompiler/shader_info.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/texture_cache/texture_cache.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using VideoCommon::ImageId;
using VideoCommon::NUM_RT;
using VideoCommon::Region2D;
using VideoCommon::RenderTargets;
using VideoCore::Surface::PixelFormat;

class ASTCDecoderPass;
class BlitImageHelper;
class Device;
class Image;
class ImageView;
class Framebuffer;
class RenderPassCache;
class StagingBufferPool;
class VKScheduler;

struct TextureCacheRuntime {
    const Device& device;
    VKScheduler& scheduler;
    MemoryAllocator& memory_allocator;
    StagingBufferPool& staging_buffer_pool;
    BlitImageHelper& blit_image_helper;
    ASTCDecoderPass& astc_decoder_pass;
    RenderPassCache& render_pass_cache;

    void Finish();

    StagingBufferRef UploadStagingBuffer(size_t size);

    StagingBufferRef DownloadStagingBuffer(size_t size);

    void BlitImage(Framebuffer* dst_framebuffer, ImageView& dst, ImageView& src,
                   const Region2D& dst_region, const Region2D& src_region,
                   Tegra::Engines::Fermi2D::Filter filter,
                   Tegra::Engines::Fermi2D::Operation operation);

    void CopyImage(Image& dst, Image& src, std::span<const VideoCommon::ImageCopy> copies);

    void ConvertImage(Framebuffer* dst, ImageView& dst_view, ImageView& src_view);

    bool CanAccelerateImageUpload(Image&) const noexcept {
        return false;
    }

    void AccelerateImageUpload(Image&, const StagingBufferRef&,
                               std::span<const VideoCommon::SwizzleParameters>);

    void InsertUploadMemoryBarrier() {}

    bool HasBrokenTextureViewFormats() const noexcept {
        // No known Vulkan driver has broken image views
        return false;
    }

    bool HasNativeBgr() const noexcept {
        // All known Vulkan drivers can natively handle BGR textures
        return true;
    }

    u64 GetDeviceLocalMemory() const;
};

class Image : public VideoCommon::ImageBase {
public:
    explicit Image(TextureCacheRuntime&, const VideoCommon::ImageInfo& info, GPUVAddr gpu_addr,
                   VAddr cpu_addr);

    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&&) = default;
    Image& operator=(Image&&) = default;

    void UploadMemory(const StagingBufferRef& map,
                      std::span<const VideoCommon::BufferImageCopy> copies);

    void DownloadMemory(const StagingBufferRef& map,
                        std::span<const VideoCommon::BufferImageCopy> copies);

    [[nodiscard]] VkImage Handle() const noexcept {
        return *image;
    }

    [[nodiscard]] VkImageAspectFlags AspectMask() const noexcept {
        return aspect_mask;
    }

    [[nodiscard]] VkImageView StorageImageView(s32 level) const noexcept {
        return *storage_image_views[level];
    }

    /// Returns true when the image is already initialized and mark it as initialized
    [[nodiscard]] bool ExchangeInitialization() noexcept {
        return std::exchange(initialized, true);
    }

private:
    VKScheduler* scheduler;
    vk::Image image;
    MemoryCommit commit;
    vk::ImageView image_view;
    std::vector<vk::ImageView> storage_image_views;
    VkImageAspectFlags aspect_mask = 0;
    bool initialized = false;
};

class ImageView : public VideoCommon::ImageViewBase {
public:
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::ImageViewInfo&, ImageId, Image&);
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::ImageInfo&,
                       const VideoCommon::ImageViewInfo&, GPUVAddr);
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::NullImageParams&);

    [[nodiscard]] VkImageView DepthView();

    [[nodiscard]] VkImageView StencilView();

    [[nodiscard]] VkImageView StorageView(Shader::TextureType texture_type,
                                          Shader::ImageFormat image_format);

    [[nodiscard]] VkImageView Handle(Shader::TextureType texture_type) const noexcept {
        return *image_views[static_cast<size_t>(texture_type)];
    }

    [[nodiscard]] VkImage ImageHandle() const noexcept {
        return image_handle;
    }

    [[nodiscard]] VkImageView RenderTarget() const noexcept {
        return render_target;
    }

    [[nodiscard]] VkSampleCountFlagBits Samples() const noexcept {
        return samples;
    }

    [[nodiscard]] GPUVAddr GpuAddr() const noexcept {
        return gpu_addr;
    }

    [[nodiscard]] u32 BufferSize() const noexcept {
        return buffer_size;
    }

private:
    struct StorageViews {
        std::array<vk::ImageView, Shader::NUM_TEXTURE_TYPES> signeds;
        std::array<vk::ImageView, Shader::NUM_TEXTURE_TYPES> unsigneds;
    };

    [[nodiscard]] vk::ImageView MakeView(VkFormat vk_format, VkImageAspectFlags aspect_mask);

    const Device* device = nullptr;
    std::array<vk::ImageView, Shader::NUM_TEXTURE_TYPES> image_views;
    std::unique_ptr<StorageViews> storage_views;
    vk::ImageView depth_view;
    vk::ImageView stencil_view;
    VkImage image_handle = VK_NULL_HANDLE;
    VkImageView render_target = VK_NULL_HANDLE;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    GPUVAddr gpu_addr = 0;
    u32 buffer_size = 0;
};

class ImageAlloc : public VideoCommon::ImageAllocBase {};

class Sampler {
public:
    explicit Sampler(TextureCacheRuntime&, const Tegra::Texture::TSCEntry&);

    [[nodiscard]] VkSampler Handle() const noexcept {
        return *sampler;
    }

private:
    vk::Sampler sampler;
};

class Framebuffer {
public:
    explicit Framebuffer(TextureCacheRuntime&, std::span<ImageView*, NUM_RT> color_buffers,
                         ImageView* depth_buffer, const VideoCommon::RenderTargets& key);

    [[nodiscard]] VkFramebuffer Handle() const noexcept {
        return *framebuffer;
    }

    [[nodiscard]] VkRenderPass RenderPass() const noexcept {
        return renderpass;
    }

    [[nodiscard]] VkExtent2D RenderArea() const noexcept {
        return render_area;
    }

    [[nodiscard]] VkSampleCountFlagBits Samples() const noexcept {
        return samples;
    }

    [[nodiscard]] u32 NumColorBuffers() const noexcept {
        return num_color_buffers;
    }

    [[nodiscard]] u32 NumImages() const noexcept {
        return num_images;
    }

    [[nodiscard]] const std::array<VkImage, 9>& Images() const noexcept {
        return images;
    }

    [[nodiscard]] const std::array<VkImageSubresourceRange, 9>& ImageRanges() const noexcept {
        return image_ranges;
    }

private:
    vk::Framebuffer framebuffer;
    VkRenderPass renderpass{};
    VkExtent2D render_area{};
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    u32 num_color_buffers = 0;
    u32 num_images = 0;
    std::array<VkImage, 9> images{};
    std::array<VkImageSubresourceRange, 9> image_ranges{};
};

struct TextureCacheParams {
    static constexpr bool ENABLE_VALIDATION = true;
    static constexpr bool FRAMEBUFFER_BLITS = false;
    static constexpr bool HAS_EMULATED_COPIES = false;
    static constexpr bool HAS_DEVICE_MEMORY_INFO = true;

    using Runtime = Vulkan::TextureCacheRuntime;
    using Image = Vulkan::Image;
    using ImageAlloc = Vulkan::ImageAlloc;
    using ImageView = Vulkan::ImageView;
    using Sampler = Vulkan::Sampler;
    using Framebuffer = Vulkan::Framebuffer;
};

using TextureCache = VideoCommon::TextureCache<TextureCacheParams>;

} // namespace Vulkan
