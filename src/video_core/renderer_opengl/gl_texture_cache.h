// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>

#include <glad/glad.h>

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/util_shaders.h"
#include "video_core/texture_cache/texture_cache.h"

namespace OpenGL {

class Device;
class ProgramManager;
class StateTracker;

class Framebuffer;
class Image;
class ImageView;
class Sampler;

using VideoCommon::ImageId;
using VideoCommon::ImageViewId;
using VideoCommon::ImageViewType;
using VideoCommon::NUM_RT;
using VideoCommon::Region2D;
using VideoCommon::RenderTargets;

struct ImageBufferMap {
    ~ImageBufferMap();

    std::span<u8> mapped_span;
    size_t offset = 0;
    OGLSync* sync;
    GLuint buffer;
};

struct FormatProperties {
    GLenum compatibility_class;
    bool compatibility_by_size;
    bool is_compressed;
};

class TextureCacheRuntime {
    friend Framebuffer;
    friend Image;
    friend ImageView;
    friend Sampler;

public:
    explicit TextureCacheRuntime(const Device& device, ProgramManager& program_manager,
                                 StateTracker& state_tracker);
    ~TextureCacheRuntime();

    void Finish();

    ImageBufferMap UploadStagingBuffer(size_t size);

    ImageBufferMap DownloadStagingBuffer(size_t size);

    void CopyImage(Image& dst, Image& src, std::span<const VideoCommon::ImageCopy> copies);

    void ConvertImage(Framebuffer* dst, ImageView& dst_view, ImageView& src_view) {
        UNIMPLEMENTED();
    }

    bool CanImageBeCopied(const Image& dst, const Image& src);

    void EmulateCopyImage(Image& dst, Image& src, std::span<const VideoCommon::ImageCopy> copies);

    void BlitFramebuffer(Framebuffer* dst, Framebuffer* src, const Region2D& dst_region,
                         const Region2D& src_region, Tegra::Engines::Fermi2D::Filter filter,
                         Tegra::Engines::Fermi2D::Operation operation);

    void AccelerateImageUpload(Image& image, const ImageBufferMap& map,
                               std::span<const VideoCommon::SwizzleParameters> swizzles);

    void InsertUploadMemoryBarrier();

    FormatProperties FormatInfo(VideoCommon::ImageType type, GLenum internal_format) const;

    bool HasNativeBgr() const noexcept {
        // OpenGL does not have native support for the BGR internal format
        return false;
    }

    bool HasBrokenTextureViewFormats() const noexcept {
        return has_broken_texture_view_formats;
    }

    bool HasNativeASTC() const noexcept;

private:
    struct StagingBuffers {
        explicit StagingBuffers(GLenum storage_flags_, GLenum map_flags_);
        ~StagingBuffers();

        ImageBufferMap RequestMap(size_t requested_size, bool insert_fence);

        size_t RequestBuffer(size_t requested_size);

        std::optional<size_t> FindBuffer(size_t requested_size);

        std::vector<OGLSync> syncs;
        std::vector<OGLBuffer> buffers;
        std::vector<u8*> maps;
        std::vector<size_t> sizes;
        GLenum storage_flags;
        GLenum map_flags;
    };

    const Device& device;
    StateTracker& state_tracker;
    UtilShaders util_shaders;

    std::array<std::unordered_map<GLenum, FormatProperties>, 3> format_properties;
    bool has_broken_texture_view_formats = false;

    StagingBuffers upload_buffers{GL_MAP_WRITE_BIT, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT};
    StagingBuffers download_buffers{GL_MAP_READ_BIT, GL_MAP_READ_BIT};

    OGLTexture null_image_1d_array;
    OGLTexture null_image_cube_array;
    OGLTexture null_image_3d;
    OGLTexture null_image_rect;
    OGLTextureView null_image_view_1d;
    OGLTextureView null_image_view_2d;
    OGLTextureView null_image_view_2d_array;
    OGLTextureView null_image_view_cube;

    std::array<GLuint, VideoCommon::NUM_IMAGE_VIEW_TYPES> null_image_views;
};

class Image : public VideoCommon::ImageBase {
    friend ImageView;

public:
    explicit Image(TextureCacheRuntime&, const VideoCommon::ImageInfo& info, GPUVAddr gpu_addr,
                   VAddr cpu_addr);

    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&&) = default;
    Image& operator=(Image&&) = default;

    void UploadMemory(const ImageBufferMap& map,
                      std::span<const VideoCommon::BufferImageCopy> copies);

    void UploadMemory(const ImageBufferMap& map, std::span<const VideoCommon::BufferCopy> copies);

    void DownloadMemory(ImageBufferMap& map, std::span<const VideoCommon::BufferImageCopy> copies);

    GLuint StorageHandle() noexcept;

    GLuint Handle() const noexcept {
        return texture.handle;
    }

private:
    void CopyBufferToImage(const VideoCommon::BufferImageCopy& copy, size_t buffer_offset);

    void CopyImageToBuffer(const VideoCommon::BufferImageCopy& copy, size_t buffer_offset);

    OGLTexture texture;
    OGLBuffer buffer;
    OGLTextureView store_view;
    GLenum gl_internal_format = GL_NONE;
    GLenum gl_format = GL_NONE;
    GLenum gl_type = GL_NONE;
};

class ImageView : public VideoCommon::ImageViewBase {
    friend Image;

public:
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::ImageViewInfo&, ImageId, Image&);
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::ImageInfo& info,
                       const VideoCommon::ImageViewInfo& view_info);
    explicit ImageView(TextureCacheRuntime&, const VideoCommon::NullImageParams&);

    [[nodiscard]] GLuint Handle(ImageViewType query_type) const noexcept {
        return views[static_cast<size_t>(query_type)];
    }

    [[nodiscard]] GLuint DefaultHandle() const noexcept {
        return default_handle;
    }

    [[nodiscard]] GLenum Format() const noexcept {
        return internal_format;
    }

private:
    void SetupView(const Device& device, Image& image, ImageViewType view_type, GLuint handle,
                   const VideoCommon::ImageViewInfo& info,
                   VideoCommon::SubresourceRange view_range);

    std::array<GLuint, VideoCommon::NUM_IMAGE_VIEW_TYPES> views{};
    std::vector<OGLTextureView> stored_views;
    GLuint default_handle = 0;
    GLenum internal_format = GL_NONE;
};

class ImageAlloc : public VideoCommon::ImageAllocBase {};

class Sampler {
public:
    explicit Sampler(TextureCacheRuntime&, const Tegra::Texture::TSCEntry&);

    GLuint Handle() const noexcept {
        return sampler.handle;
    }

private:
    OGLSampler sampler;
};

class Framebuffer {
public:
    explicit Framebuffer(TextureCacheRuntime&, std::span<ImageView*, NUM_RT> color_buffers,
                         ImageView* depth_buffer, const VideoCommon::RenderTargets& key);

    [[nodiscard]] GLuint Handle() const noexcept {
        return framebuffer.handle;
    }

    [[nodiscard]] GLbitfield BufferBits() const noexcept {
        return buffer_bits;
    }

private:
    OGLFramebuffer framebuffer;
    GLbitfield buffer_bits = GL_NONE;
};

struct TextureCacheParams {
    static constexpr bool ENABLE_VALIDATION = true;
    static constexpr bool FRAMEBUFFER_BLITS = true;
    static constexpr bool HAS_EMULATED_COPIES = true;
    static constexpr bool HAS_DEVICE_MEMORY_INFO = false;

    using Runtime = OpenGL::TextureCacheRuntime;
    using Image = OpenGL::Image;
    using ImageAlloc = OpenGL::ImageAlloc;
    using ImageView = OpenGL::ImageView;
    using Sampler = OpenGL::Sampler;
    using Framebuffer = OpenGL::Framebuffer;
};

using TextureCache = VideoCommon::TextureCache<TextureCacheParams>;

} // namespace OpenGL
