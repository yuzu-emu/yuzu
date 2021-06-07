// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <bit>
#include <string>

#include <glad/glad.h>

#include "common/settings.h"

#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/renderer_opengl/maxwell_to_gl.h"
#include "video_core/renderer_opengl/util_shaders.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/format_lookup_table.h"
#include "video_core/texture_cache/samples_helper.h"
#include "video_core/texture_cache/texture_cache.h"
#include "video_core/textures/decoders.h"

namespace OpenGL {
namespace {
using Tegra::Texture::SwizzleSource;
using Tegra::Texture::TextureMipmapFilter;
using Tegra::Texture::TextureType;
using Tegra::Texture::TICEntry;
using Tegra::Texture::TSCEntry;
using VideoCommon::CalculateLevelStrideAlignment;
using VideoCommon::ImageCopy;
using VideoCommon::ImageFlagBits;
using VideoCommon::ImageType;
using VideoCommon::NUM_RT;
using VideoCommon::SamplesLog2;
using VideoCommon::SwizzleParameters;
using VideoCore::Surface::BytesPerBlock;
using VideoCore::Surface::IsPixelFormatASTC;
using VideoCore::Surface::IsPixelFormatSRGB;
using VideoCore::Surface::MaxPixelFormat;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceType;

struct CopyOrigin {
    GLint level;
    GLint x;
    GLint y;
    GLint z;
};

struct CopyRegion {
    GLsizei width;
    GLsizei height;
    GLsizei depth;
};

constexpr std::array ACCELERATED_FORMATS{
    GL_RGBA32F,   GL_RGBA16F,   GL_RG32F,    GL_RG16F,        GL_R11F_G11F_B10F, GL_R32F,
    GL_R16F,      GL_RGBA32UI,  GL_RGBA16UI, GL_RGB10_A2UI,   GL_RGBA8UI,        GL_RG32UI,
    GL_RG16UI,    GL_RG8UI,     GL_R32UI,    GL_R16UI,        GL_R8UI,           GL_RGBA32I,
    GL_RGBA16I,   GL_RGBA8I,    GL_RG32I,    GL_RG16I,        GL_RG8I,           GL_R32I,
    GL_R16I,      GL_R8I,       GL_RGBA16,   GL_RGB10_A2,     GL_RGBA8,          GL_RG16,
    GL_RG8,       GL_R16,       GL_R8,       GL_RGBA16_SNORM, GL_RGBA8_SNORM,    GL_RG16_SNORM,
    GL_RG8_SNORM, GL_R16_SNORM, GL_R8_SNORM,
};

GLenum ImageTarget(const VideoCommon::ImageInfo& info) {
    switch (info.type) {
    case ImageType::e1D:
        return GL_TEXTURE_1D_ARRAY;
    case ImageType::e2D:
        if (info.num_samples > 1) {
            return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        }
        return GL_TEXTURE_2D_ARRAY;
    case ImageType::e3D:
        return GL_TEXTURE_3D;
    case ImageType::Linear:
        return GL_TEXTURE_2D_ARRAY;
    case ImageType::Buffer:
        return GL_TEXTURE_BUFFER;
    }
    UNREACHABLE_MSG("Invalid image type={}", info.type);
    return GL_NONE;
}

GLenum ImageTarget(Shader::TextureType type, int num_samples = 1) {
    const bool is_multisampled = num_samples > 1;
    switch (type) {
    case Shader::TextureType::Color1D:
        return GL_TEXTURE_1D;
    case Shader::TextureType::Color2D:
        return is_multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    case Shader::TextureType::ColorCube:
        return GL_TEXTURE_CUBE_MAP;
    case Shader::TextureType::Color3D:
        return GL_TEXTURE_3D;
    case Shader::TextureType::ColorArray1D:
        return GL_TEXTURE_1D_ARRAY;
    case Shader::TextureType::ColorArray2D:
        return is_multisampled ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
    case Shader::TextureType::ColorArrayCube:
        return GL_TEXTURE_CUBE_MAP_ARRAY;
    case Shader::TextureType::Buffer:
        return GL_TEXTURE_BUFFER;
    }
    UNREACHABLE_MSG("Invalid image view type={}", type);
    return GL_NONE;
}

GLenum TextureMode(PixelFormat format, bool is_first) {
    switch (format) {
    case PixelFormat::D24_UNORM_S8_UINT:
    case PixelFormat::D32_FLOAT_S8_UINT:
        return is_first ? GL_DEPTH_COMPONENT : GL_STENCIL_INDEX;
    case PixelFormat::S8_UINT_D24_UNORM:
        return is_first ? GL_STENCIL_INDEX : GL_DEPTH_COMPONENT;
    default:
        UNREACHABLE();
        return GL_DEPTH_COMPONENT;
    }
}

GLint Swizzle(SwizzleSource source) {
    switch (source) {
    case SwizzleSource::Zero:
        return GL_ZERO;
    case SwizzleSource::R:
        return GL_RED;
    case SwizzleSource::G:
        return GL_GREEN;
    case SwizzleSource::B:
        return GL_BLUE;
    case SwizzleSource::A:
        return GL_ALPHA;
    case SwizzleSource::OneInt:
    case SwizzleSource::OneFloat:
        return GL_ONE;
    }
    UNREACHABLE_MSG("Invalid swizzle source={}", source);
    return GL_NONE;
}

GLenum AttachmentType(PixelFormat format) {
    switch (const SurfaceType type = VideoCore::Surface::GetFormatType(format); type) {
    case SurfaceType::Depth:
        return GL_DEPTH_ATTACHMENT;
    case SurfaceType::DepthStencil:
        return GL_DEPTH_STENCIL_ATTACHMENT;
    default:
        UNIMPLEMENTED_MSG("Unimplemented type={}", type);
        return GL_NONE;
    }
}

[[nodiscard]] bool IsConverted(const Device& device, PixelFormat format, ImageType type) {
    if (!device.HasASTC() && IsPixelFormatASTC(format)) {
        return true;
    }
    switch (format) {
    case PixelFormat::BC4_UNORM:
    case PixelFormat::BC5_UNORM:
        return type == ImageType::e3D;
    default:
        break;
    }
    return false;
}

[[nodiscard]] constexpr SwizzleSource ConvertGreenRed(SwizzleSource value) {
    switch (value) {
    case SwizzleSource::G:
        return SwizzleSource::R;
    default:
        return value;
    }
}

void ApplySwizzle(GLuint handle, PixelFormat format, std::array<SwizzleSource, 4> swizzle) {
    switch (format) {
    case PixelFormat::D24_UNORM_S8_UINT:
    case PixelFormat::D32_FLOAT_S8_UINT:
    case PixelFormat::S8_UINT_D24_UNORM:
        UNIMPLEMENTED_IF(swizzle[0] != SwizzleSource::R && swizzle[0] != SwizzleSource::G);
        glTextureParameteri(handle, GL_DEPTH_STENCIL_TEXTURE_MODE,
                            TextureMode(format, swizzle[0] == SwizzleSource::R));
        std::ranges::transform(swizzle, swizzle.begin(), ConvertGreenRed);
        break;
    default:
        break;
    }
    std::array<GLint, 4> gl_swizzle;
    std::ranges::transform(swizzle, gl_swizzle.begin(), Swizzle);
    glTextureParameteriv(handle, GL_TEXTURE_SWIZZLE_RGBA, gl_swizzle.data());
}

[[nodiscard]] bool CanBeAccelerated(const TextureCacheRuntime& runtime,
                                    const VideoCommon::ImageInfo& info) {
    if (IsPixelFormatASTC(info.format)) {
        return !runtime.HasNativeASTC() && Settings::values.accelerate_astc.GetValue();
    }
    // Disable other accelerated uploads for now as they don't implement swizzled uploads
    return false;
    switch (info.type) {
    case ImageType::e2D:
    case ImageType::e3D:
    case ImageType::Linear:
        break;
    default:
        return false;
    }
    const GLenum internal_format = MaxwellToGL::GetFormatTuple(info.format).internal_format;
    const auto& format_info = runtime.FormatInfo(info.type, internal_format);
    if (format_info.is_compressed) {
        return false;
    }
    if (std::ranges::find(ACCELERATED_FORMATS, static_cast<int>(internal_format)) ==
        ACCELERATED_FORMATS.end()) {
        return false;
    }
    if (format_info.compatibility_by_size) {
        return true;
    }
    const GLenum store_format = StoreFormat(BytesPerBlock(info.format));
    const GLenum store_class = runtime.FormatInfo(info.type, store_format).compatibility_class;
    return format_info.compatibility_class == store_class;
}

[[nodiscard]] CopyOrigin MakeCopyOrigin(VideoCommon::Offset3D offset,
                                        VideoCommon::SubresourceLayers subresource, GLenum target) {
    switch (target) {
    case GL_TEXTURE_1D:
        return CopyOrigin{
            .level = static_cast<GLint>(subresource.base_level),
            .x = static_cast<GLint>(offset.x),
            .y = static_cast<GLint>(0),
            .z = static_cast<GLint>(0),
        };
    case GL_TEXTURE_1D_ARRAY:
        return CopyOrigin{
            .level = static_cast<GLint>(subresource.base_level),
            .x = static_cast<GLint>(offset.x),
            .y = static_cast<GLint>(0),
            .z = static_cast<GLint>(subresource.base_layer),
        };
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        return CopyOrigin{
            .level = static_cast<GLint>(subresource.base_level),
            .x = static_cast<GLint>(offset.x),
            .y = static_cast<GLint>(offset.y),
            .z = static_cast<GLint>(subresource.base_layer),
        };
    case GL_TEXTURE_3D:
        return CopyOrigin{
            .level = static_cast<GLint>(subresource.base_level),
            .x = static_cast<GLint>(offset.x),
            .y = static_cast<GLint>(offset.y),
            .z = static_cast<GLint>(offset.z),
        };
    default:
        UNIMPLEMENTED_MSG("Unimplemented copy target={}", target);
        return CopyOrigin{.level = 0, .x = 0, .y = 0, .z = 0};
    }
}

[[nodiscard]] CopyRegion MakeCopyRegion(VideoCommon::Extent3D extent,
                                        VideoCommon::SubresourceLayers dst_subresource,
                                        GLenum target) {
    switch (target) {
    case GL_TEXTURE_1D:
        return CopyRegion{
            .width = static_cast<GLsizei>(extent.width),
            .height = static_cast<GLsizei>(1),
            .depth = static_cast<GLsizei>(1),
        };
    case GL_TEXTURE_1D_ARRAY:
        return CopyRegion{
            .width = static_cast<GLsizei>(extent.width),
            .height = static_cast<GLsizei>(1),
            .depth = static_cast<GLsizei>(dst_subresource.num_layers),
        };
    case GL_TEXTURE_2D_ARRAY:
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        return CopyRegion{
            .width = static_cast<GLsizei>(extent.width),
            .height = static_cast<GLsizei>(extent.height),
            .depth = static_cast<GLsizei>(dst_subresource.num_layers),
        };
    case GL_TEXTURE_3D:
        return CopyRegion{
            .width = static_cast<GLsizei>(extent.width),
            .height = static_cast<GLsizei>(extent.height),
            .depth = static_cast<GLsizei>(extent.depth),
        };
    default:
        UNIMPLEMENTED_MSG("Unimplemented copy target={}", target);
        return CopyRegion{.width = 0, .height = 0, .depth = 0};
    }
}

void AttachTexture(GLuint fbo, GLenum attachment, const ImageView* image_view) {
    if (False(image_view->flags & VideoCommon::ImageViewFlagBits::Slice)) {
        glNamedFramebufferTexture(fbo, attachment, image_view->DefaultHandle(), 0);
        return;
    }
    const GLuint texture = image_view->Handle(Shader::TextureType::Color3D);
    if (image_view->range.extent.layers > 1) {
        // TODO: OpenGL doesn't support rendering to a fixed number of slices
        glNamedFramebufferTexture(fbo, attachment, texture, 0);
    } else {
        const u32 slice = image_view->range.base.layer;
        glNamedFramebufferTextureLayer(fbo, attachment, texture, 0, slice);
    }
}

[[nodiscard]] bool IsPixelFormatBGR(PixelFormat format) {
    switch (format) {
    case PixelFormat::B5G6R5_UNORM:
    case PixelFormat::B8G8R8A8_UNORM:
    case PixelFormat::B8G8R8A8_SRGB:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] GLenum ShaderFormat(Shader::ImageFormat format) {
    switch (format) {
    case Shader::ImageFormat::Typeless:
        break;
    case Shader::ImageFormat::R8_SINT:
        return GL_R8I;
    case Shader::ImageFormat::R8_UINT:
        return GL_R8UI;
    case Shader::ImageFormat::R16_UINT:
        return GL_R16UI;
    case Shader::ImageFormat::R16_SINT:
        return GL_R16I;
    case Shader::ImageFormat::R32_UINT:
        return GL_R32UI;
    case Shader::ImageFormat::R32G32_UINT:
        return GL_RG32UI;
    case Shader::ImageFormat::R32G32B32A32_UINT:
        return GL_RGBA32UI;
    }
    UNREACHABLE_MSG("Invalid image format={}", format);
    return GL_R32UI;
}
} // Anonymous namespace

ImageBufferMap::~ImageBufferMap() {
    if (sync) {
        sync->Create();
    }
}

TextureCacheRuntime::TextureCacheRuntime(const Device& device_, ProgramManager& program_manager,
                                         StateTracker& state_tracker_)
    : device{device_}, state_tracker{state_tracker_}, util_shaders(program_manager) {
    static constexpr std::array TARGETS{GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_3D};
    for (size_t i = 0; i < TARGETS.size(); ++i) {
        const GLenum target = TARGETS[i];
        for (const MaxwellToGL::FormatTuple& tuple : MaxwellToGL::FORMAT_TABLE) {
            const GLenum format = tuple.internal_format;
            GLint compat_class;
            GLint compat_type;
            GLint is_compressed;
            glGetInternalformativ(target, format, GL_IMAGE_COMPATIBILITY_CLASS, 1, &compat_class);
            glGetInternalformativ(target, format, GL_IMAGE_FORMAT_COMPATIBILITY_TYPE, 1,
                                  &compat_type);
            glGetInternalformativ(target, format, GL_TEXTURE_COMPRESSED, 1, &is_compressed);
            const FormatProperties properties{
                .compatibility_class = static_cast<GLenum>(compat_class),
                .compatibility_by_size = compat_type == GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE,
                .is_compressed = is_compressed == GL_TRUE,
            };
            format_properties[i].emplace(format, properties);
        }
    }
    has_broken_texture_view_formats = device.HasBrokenTextureViewFormats();

    null_image_1d_array.Create(GL_TEXTURE_1D_ARRAY);
    null_image_cube_array.Create(GL_TEXTURE_CUBE_MAP_ARRAY);
    null_image_3d.Create(GL_TEXTURE_3D);
    glTextureStorage2D(null_image_1d_array.handle, 1, GL_R8, 1, 1);
    glTextureStorage3D(null_image_cube_array.handle, 1, GL_R8, 1, 1, 6);
    glTextureStorage3D(null_image_3d.handle, 1, GL_R8, 1, 1, 1);

    std::array<GLuint, 4> new_handles;
    glGenTextures(static_cast<GLsizei>(new_handles.size()), new_handles.data());
    null_image_view_1d.handle = new_handles[0];
    null_image_view_2d.handle = new_handles[1];
    null_image_view_2d_array.handle = new_handles[2];
    null_image_view_cube.handle = new_handles[3];
    glTextureView(null_image_view_1d.handle, GL_TEXTURE_1D, null_image_1d_array.handle, GL_R8, 0, 1,
                  0, 1);
    glTextureView(null_image_view_2d.handle, GL_TEXTURE_2D, null_image_cube_array.handle, GL_R8, 0,
                  1, 0, 1);
    glTextureView(null_image_view_2d_array.handle, GL_TEXTURE_2D_ARRAY,
                  null_image_cube_array.handle, GL_R8, 0, 1, 0, 1);
    glTextureView(null_image_view_cube.handle, GL_TEXTURE_CUBE_MAP, null_image_cube_array.handle,
                  GL_R8, 0, 1, 0, 6);
    const std::array texture_handles{
        null_image_1d_array.handle,  null_image_cube_array.handle, null_image_3d.handle,
        null_image_view_1d.handle,   null_image_view_2d.handle,    null_image_view_2d_array.handle,
        null_image_view_cube.handle,
    };
    for (const GLuint handle : texture_handles) {
        static constexpr std::array NULL_SWIZZLE{GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO};
        glTextureParameteriv(handle, GL_TEXTURE_SWIZZLE_RGBA, NULL_SWIZZLE.data());
    }
    const auto set_view = [this](Shader::TextureType type, GLuint handle) {
        if (device.HasDebuggingToolAttached()) {
            const std::string name = fmt::format("NullImage {}", type);
            glObjectLabel(GL_TEXTURE, handle, static_cast<GLsizei>(name.size()), name.data());
        }
        null_image_views[static_cast<size_t>(type)] = handle;
    };
    set_view(Shader::TextureType::Color1D, null_image_view_1d.handle);
    set_view(Shader::TextureType::Color2D, null_image_view_2d.handle);
    set_view(Shader::TextureType::ColorCube, null_image_view_cube.handle);
    set_view(Shader::TextureType::Color3D, null_image_3d.handle);
    set_view(Shader::TextureType::ColorArray1D, null_image_1d_array.handle);
    set_view(Shader::TextureType::ColorArray2D, null_image_view_2d_array.handle);
    set_view(Shader::TextureType::ColorArrayCube, null_image_cube_array.handle);
}

TextureCacheRuntime::~TextureCacheRuntime() = default;

void TextureCacheRuntime::Finish() {
    glFinish();
}

ImageBufferMap TextureCacheRuntime::UploadStagingBuffer(size_t size) {
    return upload_buffers.RequestMap(size, true);
}

ImageBufferMap TextureCacheRuntime::DownloadStagingBuffer(size_t size) {
    return download_buffers.RequestMap(size, false);
}

void TextureCacheRuntime::CopyImage(Image& dst_image, Image& src_image,
                                    std::span<const ImageCopy> copies) {
    const GLuint dst_name = dst_image.Handle();
    const GLuint src_name = src_image.Handle();
    const GLenum dst_target = ImageTarget(dst_image.info);
    const GLenum src_target = ImageTarget(src_image.info);
    for (const ImageCopy& copy : copies) {
        const auto src_origin = MakeCopyOrigin(copy.src_offset, copy.src_subresource, src_target);
        const auto dst_origin = MakeCopyOrigin(copy.dst_offset, copy.dst_subresource, dst_target);
        const auto region = MakeCopyRegion(copy.extent, copy.dst_subresource, dst_target);
        glCopyImageSubData(src_name, src_target, src_origin.level, src_origin.x, src_origin.y,
                           src_origin.z, dst_name, dst_target, dst_origin.level, dst_origin.x,
                           dst_origin.y, dst_origin.z, region.width, region.height, region.depth);
    }
}

bool TextureCacheRuntime::CanImageBeCopied(const Image& dst, const Image& src) {
    if (dst.info.type == ImageType::e3D && dst.info.format == PixelFormat::BC4_UNORM) {
        return false;
    }
    if (IsPixelFormatBGR(dst.info.format) || IsPixelFormatBGR(src.info.format)) {
        return false;
    }
    return true;
}

void TextureCacheRuntime::EmulateCopyImage(Image& dst, Image& src,
                                           std::span<const ImageCopy> copies) {
    if (dst.info.type == ImageType::e3D && dst.info.format == PixelFormat::BC4_UNORM) {
        ASSERT(src.info.type == ImageType::e3D);
        util_shaders.CopyBC4(dst, src, copies);
    } else if (IsPixelFormatBGR(dst.info.format) || IsPixelFormatBGR(src.info.format)) {
        util_shaders.CopyBGR(dst, src, copies);
    } else {
        UNREACHABLE();
    }
}

void TextureCacheRuntime::BlitFramebuffer(Framebuffer* dst, Framebuffer* src,
                                          const Region2D& dst_region, const Region2D& src_region,
                                          Tegra::Engines::Fermi2D::Filter filter,
                                          Tegra::Engines::Fermi2D::Operation operation) {
    state_tracker.NotifyScissor0();
    state_tracker.NotifyRasterizeEnable();
    state_tracker.NotifyFramebufferSRGB();

    ASSERT(dst->BufferBits() == src->BufferBits());

    glEnable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisablei(GL_SCISSOR_TEST, 0);

    const GLbitfield buffer_bits = dst->BufferBits();
    const bool has_depth = (buffer_bits & ~GL_COLOR_BUFFER_BIT) != 0;
    const bool is_linear = !has_depth && filter == Tegra::Engines::Fermi2D::Filter::Bilinear;
    glBlitNamedFramebuffer(src->Handle(), dst->Handle(), src_region.start.x, src_region.start.y,
                           src_region.end.x, src_region.end.y, dst_region.start.x,
                           dst_region.start.y, dst_region.end.x, dst_region.end.y, buffer_bits,
                           is_linear ? GL_LINEAR : GL_NEAREST);
}

void TextureCacheRuntime::AccelerateImageUpload(Image& image, const ImageBufferMap& map,
                                                std::span<const SwizzleParameters> swizzles) {
    switch (image.info.type) {
    case ImageType::e2D:
        if (IsPixelFormatASTC(image.info.format)) {
            return util_shaders.ASTCDecode(image, map, swizzles);
        } else {
            return util_shaders.BlockLinearUpload2D(image, map, swizzles);
        }
    case ImageType::e3D:
        return util_shaders.BlockLinearUpload3D(image, map, swizzles);
    case ImageType::Linear:
        return util_shaders.PitchUpload(image, map, swizzles);
    default:
        UNREACHABLE();
        break;
    }
}

void TextureCacheRuntime::InsertUploadMemoryBarrier() {
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

FormatProperties TextureCacheRuntime::FormatInfo(ImageType type, GLenum internal_format) const {
    switch (type) {
    case ImageType::e1D:
        return format_properties[0].at(internal_format);
    case ImageType::e2D:
    case ImageType::Linear:
        return format_properties[1].at(internal_format);
    case ImageType::e3D:
        return format_properties[2].at(internal_format);
    default:
        UNREACHABLE();
        return FormatProperties{};
    }
}

bool TextureCacheRuntime::HasNativeASTC() const noexcept {
    return device.HasASTC();
}

TextureCacheRuntime::StagingBuffers::StagingBuffers(GLenum storage_flags_, GLenum map_flags_)
    : storage_flags{storage_flags_}, map_flags{map_flags_} {}

TextureCacheRuntime::StagingBuffers::~StagingBuffers() = default;

ImageBufferMap TextureCacheRuntime::StagingBuffers::RequestMap(size_t requested_size,
                                                               bool insert_fence) {
    const size_t index = RequestBuffer(requested_size);
    OGLSync* const sync = insert_fence ? &syncs[index] : nullptr;
    return ImageBufferMap{
        .mapped_span = std::span(maps[index], requested_size),
        .sync = sync,
        .buffer = buffers[index].handle,
    };
}

size_t TextureCacheRuntime::StagingBuffers::RequestBuffer(size_t requested_size) {
    if (const std::optional<size_t> index = FindBuffer(requested_size); index) {
        return *index;
    }

    OGLBuffer& buffer = buffers.emplace_back();
    buffer.Create();
    glNamedBufferStorage(buffer.handle, requested_size, nullptr,
                         storage_flags | GL_MAP_PERSISTENT_BIT);
    maps.push_back(static_cast<u8*>(glMapNamedBufferRange(buffer.handle, 0, requested_size,
                                                          map_flags | GL_MAP_PERSISTENT_BIT)));

    syncs.emplace_back();
    sizes.push_back(requested_size);

    ASSERT(syncs.size() == buffers.size() && buffers.size() == maps.size() &&
           maps.size() == sizes.size());

    return buffers.size() - 1;
}

std::optional<size_t> TextureCacheRuntime::StagingBuffers::FindBuffer(size_t requested_size) {
    size_t smallest_buffer = std::numeric_limits<size_t>::max();
    std::optional<size_t> found;
    const size_t num_buffers = sizes.size();
    for (size_t index = 0; index < num_buffers; ++index) {
        const size_t buffer_size = sizes[index];
        if (buffer_size < requested_size || buffer_size >= smallest_buffer) {
            continue;
        }
        if (syncs[index].handle != 0) {
            GLint status;
            glGetSynciv(syncs[index].handle, GL_SYNC_STATUS, 1, nullptr, &status);
            if (status != GL_SIGNALED) {
                continue;
            }
            syncs[index].Release();
        }
        smallest_buffer = buffer_size;
        found = index;
    }
    return found;
}

Image::Image(TextureCacheRuntime& runtime, const VideoCommon::ImageInfo& info_, GPUVAddr gpu_addr_,
             VAddr cpu_addr_)
    : VideoCommon::ImageBase(info_, gpu_addr_, cpu_addr_) {
    if (CanBeAccelerated(runtime, info)) {
        flags |= ImageFlagBits::AcceleratedUpload;
    }
    if (IsConverted(runtime.device, info.format, info.type)) {
        flags |= ImageFlagBits::Converted;
        gl_internal_format = IsPixelFormatSRGB(info.format) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        gl_format = GL_RGBA;
        gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
    } else {
        const auto& tuple = MaxwellToGL::GetFormatTuple(info.format);
        gl_internal_format = tuple.internal_format;
        gl_format = tuple.format;
        gl_type = tuple.type;
    }
    const GLenum target = ImageTarget(info);
    const GLsizei width = info.size.width;
    const GLsizei height = info.size.height;
    const GLsizei depth = info.size.depth;
    const int max_host_mip_levels = std::bit_width(info.size.width);
    const GLsizei num_levels = std::min(info.resources.levels, max_host_mip_levels);
    const GLsizei num_layers = info.resources.layers;
    const GLsizei num_samples = info.num_samples;

    GLuint handle = 0;
    if (target != GL_TEXTURE_BUFFER) {
        texture.Create(target);
        handle = texture.handle;
    }
    switch (target) {
    case GL_TEXTURE_1D_ARRAY:
        glTextureStorage2D(handle, num_levels, gl_internal_format, width, num_layers);
        break;
    case GL_TEXTURE_2D_ARRAY:
        glTextureStorage3D(handle, num_levels, gl_internal_format, width, height, num_layers);
        break;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: {
        // TODO: Where should 'fixedsamplelocations' come from?
        const auto [samples_x, samples_y] = SamplesLog2(info.num_samples);
        glTextureStorage3DMultisample(handle, num_samples, gl_internal_format, width >> samples_x,
                                      height >> samples_y, num_layers, GL_FALSE);
        break;
    }
    case GL_TEXTURE_RECTANGLE:
        glTextureStorage2D(handle, num_levels, gl_internal_format, width, height);
        break;
    case GL_TEXTURE_3D:
        glTextureStorage3D(handle, num_levels, gl_internal_format, width, height, depth);
        break;
    case GL_TEXTURE_BUFFER:
        UNREACHABLE();
        break;
    default:
        UNREACHABLE_MSG("Invalid target=0x{:x}", target);
        break;
    }
    if (runtime.device.HasDebuggingToolAttached()) {
        const std::string name = VideoCommon::Name(*this);
        glObjectLabel(target == GL_TEXTURE_BUFFER ? GL_BUFFER : GL_TEXTURE, handle,
                      static_cast<GLsizei>(name.size()), name.data());
    }
}

Image::~Image() = default;

void Image::UploadMemory(const ImageBufferMap& map,
                         std::span<const VideoCommon::BufferImageCopy> copies) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, map.buffer);
    glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, map.offset, unswizzled_size_bytes);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    u32 current_row_length = std::numeric_limits<u32>::max();
    u32 current_image_height = std::numeric_limits<u32>::max();

    for (const VideoCommon::BufferImageCopy& copy : copies) {
        if (current_row_length != copy.buffer_row_length) {
            current_row_length = copy.buffer_row_length;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, current_row_length);
        }
        if (current_image_height != copy.buffer_image_height) {
            current_image_height = copy.buffer_image_height;
            glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, current_image_height);
        }
        CopyBufferToImage(copy, map.offset);
    }
}

void Image::DownloadMemory(ImageBufferMap& map,
                           std::span<const VideoCommon::BufferImageCopy> copies) {
    glMemoryBarrier(GL_PIXEL_BUFFER_BARRIER_BIT); // TODO: Move this to its own API

    glBindBuffer(GL_PIXEL_PACK_BUFFER, map.buffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    u32 current_row_length = std::numeric_limits<u32>::max();
    u32 current_image_height = std::numeric_limits<u32>::max();

    for (const VideoCommon::BufferImageCopy& copy : copies) {
        if (current_row_length != copy.buffer_row_length) {
            current_row_length = copy.buffer_row_length;
            glPixelStorei(GL_PACK_ROW_LENGTH, current_row_length);
        }
        if (current_image_height != copy.buffer_image_height) {
            current_image_height = copy.buffer_image_height;
            glPixelStorei(GL_PACK_IMAGE_HEIGHT, current_image_height);
        }
        CopyImageToBuffer(copy, map.offset);
    }
}

GLuint Image::StorageHandle() noexcept {
    switch (info.format) {
    case PixelFormat::A8B8G8R8_SRGB:
    case PixelFormat::B8G8R8A8_SRGB:
    case PixelFormat::BC1_RGBA_SRGB:
    case PixelFormat::BC2_SRGB:
    case PixelFormat::BC3_SRGB:
    case PixelFormat::BC7_SRGB:
    case PixelFormat::ASTC_2D_4X4_SRGB:
    case PixelFormat::ASTC_2D_8X8_SRGB:
    case PixelFormat::ASTC_2D_8X5_SRGB:
    case PixelFormat::ASTC_2D_5X4_SRGB:
    case PixelFormat::ASTC_2D_5X5_SRGB:
    case PixelFormat::ASTC_2D_10X8_SRGB:
    case PixelFormat::ASTC_2D_6X6_SRGB:
    case PixelFormat::ASTC_2D_10X10_SRGB:
    case PixelFormat::ASTC_2D_12X12_SRGB:
    case PixelFormat::ASTC_2D_8X6_SRGB:
    case PixelFormat::ASTC_2D_6X5_SRGB:
        if (store_view.handle != 0) {
            return store_view.handle;
        }
        store_view.Create();
        glTextureView(store_view.handle, ImageTarget(info), texture.handle, GL_RGBA8, 0,
                      info.resources.levels, 0, info.resources.layers);
        return store_view.handle;
    default:
        return texture.handle;
    }
}

void Image::CopyBufferToImage(const VideoCommon::BufferImageCopy& copy, size_t buffer_offset) {
    // Compressed formats don't have a pixel format or type
    const bool is_compressed = gl_format == GL_NONE;
    const void* const offset = reinterpret_cast<const void*>(copy.buffer_offset + buffer_offset);

    switch (info.type) {
    case ImageType::e1D:
        if (is_compressed) {
            glCompressedTextureSubImage2D(texture.handle, copy.image_subresource.base_level,
                                          copy.image_offset.x, copy.image_subresource.base_layer,
                                          copy.image_extent.width,
                                          copy.image_subresource.num_layers, gl_internal_format,
                                          static_cast<GLsizei>(copy.buffer_size), offset);
        } else {
            glTextureSubImage2D(texture.handle, copy.image_subresource.base_level,
                                copy.image_offset.x, copy.image_subresource.base_layer,
                                copy.image_extent.width, copy.image_subresource.num_layers,
                                gl_format, gl_type, offset);
        }
        break;
    case ImageType::e2D:
    case ImageType::Linear:
        if (is_compressed) {
            glCompressedTextureSubImage3D(
                texture.handle, copy.image_subresource.base_level, copy.image_offset.x,
                copy.image_offset.y, copy.image_subresource.base_layer, copy.image_extent.width,
                copy.image_extent.height, copy.image_subresource.num_layers, gl_internal_format,
                static_cast<GLsizei>(copy.buffer_size), offset);
        } else {
            glTextureSubImage3D(texture.handle, copy.image_subresource.base_level,
                                copy.image_offset.x, copy.image_offset.y,
                                copy.image_subresource.base_layer, copy.image_extent.width,
                                copy.image_extent.height, copy.image_subresource.num_layers,
                                gl_format, gl_type, offset);
        }
        break;
    case ImageType::e3D:
        if (is_compressed) {
            glCompressedTextureSubImage3D(
                texture.handle, copy.image_subresource.base_level, copy.image_offset.x,
                copy.image_offset.y, copy.image_offset.z, copy.image_extent.width,
                copy.image_extent.height, copy.image_extent.depth, gl_internal_format,
                static_cast<GLsizei>(copy.buffer_size), offset);
        } else {
            glTextureSubImage3D(texture.handle, copy.image_subresource.base_level,
                                copy.image_offset.x, copy.image_offset.y, copy.image_offset.z,
                                copy.image_extent.width, copy.image_extent.height,
                                copy.image_extent.depth, gl_format, gl_type, offset);
        }
        break;
    default:
        UNREACHABLE();
    }
}

void Image::CopyImageToBuffer(const VideoCommon::BufferImageCopy& copy, size_t buffer_offset) {
    const GLint x_offset = copy.image_offset.x;
    const GLsizei width = copy.image_extent.width;

    const GLint level = copy.image_subresource.base_level;
    const GLsizei buffer_size = static_cast<GLsizei>(copy.buffer_size);
    void* const offset = reinterpret_cast<void*>(copy.buffer_offset + buffer_offset);

    GLint y_offset = 0;
    GLint z_offset = 0;
    GLsizei height = 1;
    GLsizei depth = 1;

    switch (info.type) {
    case ImageType::e1D:
        y_offset = copy.image_subresource.base_layer;
        height = copy.image_subresource.num_layers;
        break;
    case ImageType::e2D:
    case ImageType::Linear:
        y_offset = copy.image_offset.y;
        z_offset = copy.image_subresource.base_layer;
        height = copy.image_extent.height;
        depth = copy.image_subresource.num_layers;
        break;
    case ImageType::e3D:
        y_offset = copy.image_offset.y;
        z_offset = copy.image_offset.z;
        height = copy.image_extent.height;
        depth = copy.image_extent.depth;
        break;
    default:
        UNREACHABLE();
    }
    // Compressed formats don't have a pixel format or type
    const bool is_compressed = gl_format == GL_NONE;
    if (is_compressed) {
        glGetCompressedTextureSubImage(texture.handle, level, x_offset, y_offset, z_offset, width,
                                       height, depth, buffer_size, offset);
    } else {
        glGetTextureSubImage(texture.handle, level, x_offset, y_offset, z_offset, width, height,
                             depth, gl_format, gl_type, buffer_size, offset);
    }
}

ImageView::ImageView(TextureCacheRuntime& runtime, const VideoCommon::ImageViewInfo& info,
                     ImageId image_id_, Image& image)
    : VideoCommon::ImageViewBase{info, image.info, image_id_}, views{runtime.null_image_views} {
    const Device& device = runtime.device;
    if (True(image.flags & ImageFlagBits::Converted)) {
        internal_format = IsPixelFormatSRGB(info.format) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    } else {
        internal_format = MaxwellToGL::GetFormatTuple(format).internal_format;
    }
    full_range = info.range;
    flat_range = info.range;
    set_object_label = device.HasDebuggingToolAttached();
    is_render_target = info.IsRenderTarget();
    original_texture = image.texture.handle;
    num_samples = image.info.num_samples;
    if (!is_render_target) {
        swizzle[0] = info.x_source;
        swizzle[1] = info.y_source;
        swizzle[2] = info.z_source;
        swizzle[3] = info.w_source;
    }
    switch (info.type) {
    case ImageViewType::e1DArray:
        flat_range.extent.layers = 1;
        [[fallthrough]];
    case ImageViewType::e1D:
        SetupView(Shader::TextureType::Color1D);
        SetupView(Shader::TextureType::ColorArray1D);
        break;
    case ImageViewType::e2DArray:
        flat_range.extent.layers = 1;
        [[fallthrough]];
    case ImageViewType::e2D:
        if (True(flags & VideoCommon::ImageViewFlagBits::Slice)) {
            // 2D and 2D array views on a 3D textures are used exclusively for render targets
            ASSERT(info.range.extent.levels == 1);
            const VideoCommon::SubresourceRange slice_range{
                .base = {.level = info.range.base.level, .layer = 0},
                .extent = {.levels = 1, .layers = 1},
            };
            full_range = slice_range;

            SetupView(Shader::TextureType::Color3D);
        } else {
            SetupView(Shader::TextureType::Color2D);
            SetupView(Shader::TextureType::ColorArray2D);
        }
        break;
    case ImageViewType::e3D:
        SetupView(Shader::TextureType::Color3D);
        break;
    case ImageViewType::CubeArray:
        flat_range.extent.layers = 6;
        [[fallthrough]];
    case ImageViewType::Cube:
        SetupView(Shader::TextureType::ColorCube);
        SetupView(Shader::TextureType::ColorArrayCube);
        break;
    case ImageViewType::Rect:
        UNIMPLEMENTED();
        break;
    case ImageViewType::Buffer:
        UNREACHABLE();
        break;
    }
    switch (info.type) {
    case ImageViewType::e1D:
        default_handle = Handle(Shader::TextureType::Color1D);
        break;
    case ImageViewType::e1DArray:
        default_handle = Handle(Shader::TextureType::ColorArray1D);
        break;
    case ImageViewType::e2D:
        default_handle = Handle(Shader::TextureType::Color2D);
        break;
    case ImageViewType::e2DArray:
        default_handle = Handle(Shader::TextureType::ColorArray2D);
        break;
    case ImageViewType::e3D:
        default_handle = Handle(Shader::TextureType::Color3D);
        break;
    case ImageViewType::Cube:
        default_handle = Handle(Shader::TextureType::ColorCube);
        break;
    case ImageViewType::CubeArray:
        default_handle = Handle(Shader::TextureType::ColorArrayCube);
        break;
    default:
        break;
    }
}

ImageView::ImageView(TextureCacheRuntime&, const VideoCommon::ImageInfo& info,
                     const VideoCommon::ImageViewInfo& view_info, GPUVAddr gpu_addr_)
    : VideoCommon::ImageViewBase{info, view_info}, gpu_addr{gpu_addr_},
      buffer_size{VideoCommon::CalculateGuestSizeInBytes(info)} {}

ImageView::ImageView(TextureCacheRuntime&, const VideoCommon::ImageInfo& info,
                     const VideoCommon::ImageViewInfo& view_info)
    : VideoCommon::ImageViewBase{info, view_info} {}

ImageView::ImageView(TextureCacheRuntime& runtime, const VideoCommon::NullImageParams& params)
    : VideoCommon::ImageViewBase{params}, views{runtime.null_image_views} {}

GLuint ImageView::StorageView(Shader::TextureType texture_type, Shader::ImageFormat image_format) {
    if (image_format == Shader::ImageFormat::Typeless) {
        return Handle(texture_type);
    }
    const bool is_signed{image_format == Shader::ImageFormat::R8_SINT ||
                         image_format == Shader::ImageFormat::R16_SINT};
    if (!storage_views) {
        storage_views = std::make_unique<StorageViews>();
    }
    auto& type_views{is_signed ? storage_views->signeds : storage_views->unsigneds};
    GLuint& view{type_views[static_cast<size_t>(texture_type)]};
    if (view == 0) {
        view = MakeView(texture_type, ShaderFormat(image_format));
    }
    return view;
}

void ImageView::SetupView(Shader::TextureType view_type) {
    views[static_cast<size_t>(view_type)] = MakeView(view_type, internal_format);
}

GLuint ImageView::MakeView(Shader::TextureType view_type, GLenum view_format) {
    VideoCommon::SubresourceRange view_range;
    switch (view_type) {
    case Shader::TextureType::Color1D:
    case Shader::TextureType::Color2D:
    case Shader::TextureType::ColorCube:
        view_range = flat_range;
        break;
    case Shader::TextureType::ColorArray1D:
    case Shader::TextureType::ColorArray2D:
    case Shader::TextureType::Color3D:
    case Shader::TextureType::ColorArrayCube:
        view_range = full_range;
        break;
    default:
        UNREACHABLE();
    }
    OGLTextureView& view = stored_views.emplace_back();
    view.Create();

    const GLenum target = ImageTarget(view_type, num_samples);
    glTextureView(view.handle, target, original_texture, view_format, view_range.base.level,
                  view_range.extent.levels, view_range.base.layer, view_range.extent.layers);
    if (!is_render_target) {
        std::array<SwizzleSource, 4> casted_swizzle;
        std::ranges::transform(swizzle, casted_swizzle.begin(), [](u8 component_swizzle) {
            return static_cast<SwizzleSource>(component_swizzle);
        });
        ApplySwizzle(view.handle, format, casted_swizzle);
    }
    if (set_object_label) {
        const std::string name = VideoCommon::Name(*this);
        glObjectLabel(GL_TEXTURE, view.handle, static_cast<GLsizei>(name.size()), name.data());
    }
    return view.handle;
}

Sampler::Sampler(TextureCacheRuntime& runtime, const TSCEntry& config) {
    const GLenum compare_mode = config.depth_compare_enabled ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE;
    const GLenum compare_func = MaxwellToGL::DepthCompareFunc(config.depth_compare_func);
    const GLenum mag = MaxwellToGL::TextureFilterMode(config.mag_filter, TextureMipmapFilter::None);
    const GLenum min = MaxwellToGL::TextureFilterMode(config.min_filter, config.mipmap_filter);
    const GLenum reduction_filter = MaxwellToGL::ReductionFilter(config.reduction_filter);
    const GLint seamless = config.cubemap_interface_filtering ? GL_TRUE : GL_FALSE;

    UNIMPLEMENTED_IF(config.cubemap_anisotropy != 1);
    UNIMPLEMENTED_IF(config.float_coord_normalization != 0);

    sampler.Create();
    const GLuint handle = sampler.handle;
    glSamplerParameteri(handle, GL_TEXTURE_WRAP_S, MaxwellToGL::WrapMode(config.wrap_u));
    glSamplerParameteri(handle, GL_TEXTURE_WRAP_T, MaxwellToGL::WrapMode(config.wrap_v));
    glSamplerParameteri(handle, GL_TEXTURE_WRAP_R, MaxwellToGL::WrapMode(config.wrap_p));
    glSamplerParameteri(handle, GL_TEXTURE_COMPARE_MODE, compare_mode);
    glSamplerParameteri(handle, GL_TEXTURE_COMPARE_FUNC, compare_func);
    glSamplerParameteri(handle, GL_TEXTURE_MAG_FILTER, mag);
    glSamplerParameteri(handle, GL_TEXTURE_MIN_FILTER, min);
    glSamplerParameterf(handle, GL_TEXTURE_LOD_BIAS, config.LodBias());
    glSamplerParameterf(handle, GL_TEXTURE_MIN_LOD, config.MinLod());
    glSamplerParameterf(handle, GL_TEXTURE_MAX_LOD, config.MaxLod());
    glSamplerParameterfv(handle, GL_TEXTURE_BORDER_COLOR, config.BorderColor().data());

    if (GLAD_GL_ARB_texture_filter_anisotropic || GLAD_GL_EXT_texture_filter_anisotropic) {
        glSamplerParameterf(handle, GL_TEXTURE_MAX_ANISOTROPY, config.MaxAnisotropy());
    } else {
        LOG_WARNING(Render_OpenGL, "GL_ARB_texture_filter_anisotropic is required");
    }
    if (GLAD_GL_ARB_texture_filter_minmax || GLAD_GL_EXT_texture_filter_minmax) {
        glSamplerParameteri(handle, GL_TEXTURE_REDUCTION_MODE_ARB, reduction_filter);
    } else if (reduction_filter != GL_WEIGHTED_AVERAGE_ARB) {
        LOG_WARNING(Render_OpenGL, "GL_ARB_texture_filter_minmax is required");
    }
    if (GLAD_GL_ARB_seamless_cubemap_per_texture || GLAD_GL_AMD_seamless_cubemap_per_texture) {
        glSamplerParameteri(handle, GL_TEXTURE_CUBE_MAP_SEAMLESS, seamless);
    } else if (seamless == GL_FALSE) {
        // We default to false because it's more common
        LOG_WARNING(Render_OpenGL, "GL_ARB_seamless_cubemap_per_texture is required");
    }
}

Framebuffer::Framebuffer(TextureCacheRuntime& runtime, std::span<ImageView*, NUM_RT> color_buffers,
                         ImageView* depth_buffer, const VideoCommon::RenderTargets& key) {
    // Bind to READ_FRAMEBUFFER to stop Nvidia's driver from creating an EXT_framebuffer instead of
    // a core framebuffer. EXT framebuffer attachments have to match in size and can be shared
    // across contexts. yuzu doesn't share framebuffers across contexts and we need attachments with
    // mismatching size, this is why core framebuffers are preferred.
    GLuint handle;
    glGenFramebuffers(1, &handle);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, handle);

    GLsizei num_buffers = 0;
    std::array<GLenum, NUM_RT> gl_draw_buffers;
    gl_draw_buffers.fill(GL_NONE);

    for (size_t index = 0; index < color_buffers.size(); ++index) {
        const ImageView* const image_view = color_buffers[index];
        if (!image_view) {
            continue;
        }
        buffer_bits |= GL_COLOR_BUFFER_BIT;
        gl_draw_buffers[index] = GL_COLOR_ATTACHMENT0 + key.draw_buffers[index];
        num_buffers = static_cast<GLsizei>(index + 1);

        const GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index);
        AttachTexture(handle, attachment, image_view);
    }

    if (const ImageView* const image_view = depth_buffer; image_view) {
        if (GetFormatType(image_view->format) == SurfaceType::DepthStencil) {
            buffer_bits |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        } else {
            buffer_bits |= GL_DEPTH_BUFFER_BIT;
        }
        const GLenum attachment = AttachmentType(image_view->format);
        AttachTexture(handle, attachment, image_view);
    }

    if (num_buffers > 1) {
        glNamedFramebufferDrawBuffers(handle, num_buffers, gl_draw_buffers.data());
    } else if (num_buffers > 0) {
        glNamedFramebufferDrawBuffer(handle, gl_draw_buffers[0]);
    } else {
        glNamedFramebufferDrawBuffer(handle, GL_NONE);
    }

    glNamedFramebufferParameteri(handle, GL_FRAMEBUFFER_DEFAULT_WIDTH, key.size.width);
    glNamedFramebufferParameteri(handle, GL_FRAMEBUFFER_DEFAULT_HEIGHT, key.size.height);
    // TODO
    // glNamedFramebufferParameteri(handle, GL_FRAMEBUFFER_DEFAULT_LAYERS, ...);
    // glNamedFramebufferParameteri(handle, GL_FRAMEBUFFER_DEFAULT_SAMPLES, ...);
    // glNamedFramebufferParameteri(handle, GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS, ...);

    if (runtime.device.HasDebuggingToolAttached()) {
        const std::string name = VideoCommon::Name(key);
        glObjectLabel(GL_FRAMEBUFFER, handle, static_cast<GLsizei>(name.size()), name.data());
    }
    framebuffer.handle = handle;
}

} // namespace OpenGL
