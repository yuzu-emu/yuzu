// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>
#include <string_view>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/div_ceil.h"
#include "video_core/host_shaders/astc_decoder_comp.h"
#include "video_core/host_shaders/block_linear_unswizzle_2d_comp.h"
#include "video_core/host_shaders/block_linear_unswizzle_3d_comp.h"
#include "video_core/host_shaders/opengl_copy_bc4_comp.h"
#include "video_core/host_shaders/opengl_copy_bgra_comp.h"
#include "video_core/host_shaders/pitch_unswizzle_comp.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/renderer_opengl/util_shaders.h"
#include "video_core/texture_cache/accelerated_swizzle.h"
#include "video_core/texture_cache/types.h"
#include "video_core/texture_cache/util.h"
#include "video_core/textures/astc.h"
#include "video_core/textures/decoders.h"

namespace OpenGL {

using namespace HostShaders;
using namespace Tegra::Texture::ASTC;

using VideoCommon::Extent2D;
using VideoCommon::Extent3D;
using VideoCommon::ImageCopy;
using VideoCommon::ImageType;
using VideoCommon::SwizzleParameters;
using VideoCommon::Accelerated::MakeBlockLinearSwizzle2DParams;
using VideoCommon::Accelerated::MakeBlockLinearSwizzle3DParams;
using VideoCore::Surface::BytesPerBlock;

namespace {
OGLProgram MakeProgram(std::string_view source) {
    return CreateProgram(source, GL_COMPUTE_SHADER);
}

size_t NumPixelsInCopy(const VideoCommon::ImageCopy& copy) {
    return static_cast<size_t>(copy.extent.width * copy.extent.height *
                               copy.src_subresource.num_layers);
}
} // Anonymous namespace

UtilShaders::UtilShaders(ProgramManager& program_manager_)
    : program_manager{program_manager_}, astc_decoder_program(MakeProgram(ASTC_DECODER_COMP)),
      block_linear_unswizzle_2d_program(MakeProgram(BLOCK_LINEAR_UNSWIZZLE_2D_COMP)),
      block_linear_unswizzle_3d_program(MakeProgram(BLOCK_LINEAR_UNSWIZZLE_3D_COMP)),
      pitch_unswizzle_program(MakeProgram(PITCH_UNSWIZZLE_COMP)),
      copy_bgra_program(MakeProgram(OPENGL_COPY_BGRA_COMP)),
      copy_bc4_program(MakeProgram(OPENGL_COPY_BC4_COMP)) {
    const auto swizzle_table = Tegra::Texture::MakeSwizzleTable();
    swizzle_table_buffer.Create();
    astc_buffer.Create();
    glNamedBufferStorage(swizzle_table_buffer.handle, sizeof(swizzle_table), &swizzle_table, 0);
    glNamedBufferStorage(astc_buffer.handle, sizeof(ASTC_ENCODINGS_VALUES), &ASTC_ENCODINGS_VALUES,
                         0);
}

UtilShaders::~UtilShaders() = default;

void UtilShaders::ASTCDecode(Image& image, const ImageBufferMap& map,
                             std::span<const VideoCommon::SwizzleParameters> swizzles) {
    static constexpr GLuint BINDING_SWIZZLE_BUFFER = 0;
    static constexpr GLuint BINDING_INPUT_BUFFER = 1;
    static constexpr GLuint BINDING_ENC_BUFFER = 2;
    static constexpr GLuint BINDING_OUTPUT_IMAGE = 0;

    const Extent2D tile_size{
        .width = VideoCore::Surface::DefaultBlockWidth(image.info.format),
        .height = VideoCore::Surface::DefaultBlockHeight(image.info.format),
    };
    program_manager.BindComputeProgram(astc_decoder_program.handle);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_SWIZZLE_BUFFER, swizzle_table_buffer.handle);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_ENC_BUFFER, astc_buffer.handle);

    glFlushMappedNamedBufferRange(map.buffer, map.offset, image.guest_size_bytes);
    glUniform2ui(1, tile_size.width, tile_size.height);
    // Ensure buffer data is valid before dispatching
    glFlush();
    for (const SwizzleParameters& swizzle : swizzles) {
        const size_t input_offset = swizzle.buffer_offset + map.offset;
        const u32 num_dispatches_x = Common::DivCeil(swizzle.num_tiles.width, 32U);
        const u32 num_dispatches_y = Common::DivCeil(swizzle.num_tiles.height, 32U);

        const auto params = MakeBlockLinearSwizzle2DParams(swizzle, image.info);
        ASSERT(params.origin == (std::array<u32, 3>{0, 0, 0}));
        ASSERT(params.destination == (std::array<s32, 3>{0, 0, 0}));

        glUniform1ui(2, params.bytes_per_block_log2);
        glUniform1ui(3, params.layer_stride);
        glUniform1ui(4, params.block_size);
        glUniform1ui(5, params.x_shift);
        glUniform1ui(6, params.block_height);
        glUniform1ui(7, params.block_height_mask);

        glBindImageTexture(BINDING_OUTPUT_IMAGE, image.StorageHandle(), swizzle.level, GL_TRUE, 0,
                           GL_WRITE_ONLY, GL_RGBA8);
        // ASTC texture data
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, BINDING_INPUT_BUFFER, map.buffer, input_offset,
                          image.guest_size_bytes - swizzle.buffer_offset);

        glDispatchCompute(num_dispatches_x, num_dispatches_y, image.info.resources.layers);
    }
    // Precautionary barrier to ensure the compute shader is done decoding prior to texture access.
    // GL_TEXTURE_FETCH_BARRIER_BIT and GL_SHADER_IMAGE_ACCESS_BARRIER_BIT are used in a separate
    // glMemoryBarrier call by the texture cache runtime
    glMemoryBarrier(GL_UNIFORM_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT |
                    GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT |
                    GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    program_manager.RestoreGuestCompute();
}

void UtilShaders::BlockLinearUpload2D(Image& image, const ImageBufferMap& map,
                                      std::span<const SwizzleParameters> swizzles) {
    static constexpr Extent3D WORKGROUP_SIZE{32, 32, 1};
    static constexpr GLuint BINDING_SWIZZLE_BUFFER = 0;
    static constexpr GLuint BINDING_INPUT_BUFFER = 1;
    static constexpr GLuint BINDING_OUTPUT_IMAGE = 0;

    program_manager.BindComputeProgram(block_linear_unswizzle_2d_program.handle);
    glFlushMappedNamedBufferRange(map.buffer, map.offset, image.guest_size_bytes);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_SWIZZLE_BUFFER, swizzle_table_buffer.handle);

    const GLenum store_format = StoreFormat(BytesPerBlock(image.info.format));
    for (const SwizzleParameters& swizzle : swizzles) {
        const Extent3D num_tiles = swizzle.num_tiles;
        const size_t input_offset = swizzle.buffer_offset + map.offset;

        const u32 num_dispatches_x = Common::DivCeil(num_tiles.width, WORKGROUP_SIZE.width);
        const u32 num_dispatches_y = Common::DivCeil(num_tiles.height, WORKGROUP_SIZE.height);

        const auto params = MakeBlockLinearSwizzle2DParams(swizzle, image.info);
        glUniform3uiv(0, 1, params.origin.data());
        glUniform3iv(1, 1, params.destination.data());
        glUniform1ui(2, params.bytes_per_block_log2);
        glUniform1ui(3, params.layer_stride);
        glUniform1ui(4, params.block_size);
        glUniform1ui(5, params.x_shift);
        glUniform1ui(6, params.block_height);
        glUniform1ui(7, params.block_height_mask);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, BINDING_INPUT_BUFFER, map.buffer, input_offset,
                          image.guest_size_bytes - swizzle.buffer_offset);
        glBindImageTexture(BINDING_OUTPUT_IMAGE, image.StorageHandle(), swizzle.level, GL_TRUE, 0,
                           GL_WRITE_ONLY, store_format);
        glDispatchCompute(num_dispatches_x, num_dispatches_y, image.info.resources.layers);
    }
    program_manager.RestoreGuestCompute();
}

void UtilShaders::BlockLinearUpload3D(Image& image, const ImageBufferMap& map,
                                      std::span<const SwizzleParameters> swizzles) {
    static constexpr Extent3D WORKGROUP_SIZE{16, 8, 8};

    static constexpr GLuint BINDING_SWIZZLE_BUFFER = 0;
    static constexpr GLuint BINDING_INPUT_BUFFER = 1;
    static constexpr GLuint BINDING_OUTPUT_IMAGE = 0;

    glFlushMappedNamedBufferRange(map.buffer, map.offset, image.guest_size_bytes);
    program_manager.BindComputeProgram(block_linear_unswizzle_3d_program.handle);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BINDING_SWIZZLE_BUFFER, swizzle_table_buffer.handle);

    const GLenum store_format = StoreFormat(BytesPerBlock(image.info.format));
    for (const SwizzleParameters& swizzle : swizzles) {
        const Extent3D num_tiles = swizzle.num_tiles;
        const size_t input_offset = swizzle.buffer_offset + map.offset;

        const u32 num_dispatches_x = Common::DivCeil(num_tiles.width, WORKGROUP_SIZE.width);
        const u32 num_dispatches_y = Common::DivCeil(num_tiles.height, WORKGROUP_SIZE.height);
        const u32 num_dispatches_z = Common::DivCeil(num_tiles.depth, WORKGROUP_SIZE.depth);

        const auto params = MakeBlockLinearSwizzle3DParams(swizzle, image.info);
        glUniform3uiv(0, 1, params.origin.data());
        glUniform3iv(1, 1, params.destination.data());
        glUniform1ui(2, params.bytes_per_block_log2);
        glUniform1ui(3, params.slice_size);
        glUniform1ui(4, params.block_size);
        glUniform1ui(5, params.x_shift);
        glUniform1ui(6, params.block_height);
        glUniform1ui(7, params.block_height_mask);
        glUniform1ui(8, params.block_depth);
        glUniform1ui(9, params.block_depth_mask);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, BINDING_INPUT_BUFFER, map.buffer, input_offset,
                          image.guest_size_bytes - swizzle.buffer_offset);
        glBindImageTexture(BINDING_OUTPUT_IMAGE, image.StorageHandle(), swizzle.level, GL_TRUE, 0,
                           GL_WRITE_ONLY, store_format);
        glDispatchCompute(num_dispatches_x, num_dispatches_y, num_dispatches_z);
    }
    program_manager.RestoreGuestCompute();
}

void UtilShaders::PitchUpload(Image& image, const ImageBufferMap& map,
                              std::span<const SwizzleParameters> swizzles) {
    static constexpr Extent3D WORKGROUP_SIZE{32, 32, 1};
    static constexpr GLuint BINDING_INPUT_BUFFER = 0;
    static constexpr GLuint BINDING_OUTPUT_IMAGE = 0;
    static constexpr GLuint LOC_ORIGIN = 0;
    static constexpr GLuint LOC_DESTINATION = 1;
    static constexpr GLuint LOC_BYTES_PER_BLOCK = 2;
    static constexpr GLuint LOC_PITCH = 3;

    const u32 bytes_per_block = BytesPerBlock(image.info.format);
    const GLenum format = StoreFormat(bytes_per_block);
    const u32 pitch = image.info.pitch;

    UNIMPLEMENTED_IF_MSG(!std::has_single_bit(bytes_per_block),
                         "Non-power of two images are not implemented");

    program_manager.BindComputeProgram(pitch_unswizzle_program.handle);
    glFlushMappedNamedBufferRange(map.buffer, map.offset, image.guest_size_bytes);
    glUniform2ui(LOC_ORIGIN, 0, 0);
    glUniform2i(LOC_DESTINATION, 0, 0);
    glUniform1ui(LOC_BYTES_PER_BLOCK, bytes_per_block);
    glUniform1ui(LOC_PITCH, pitch);
    glBindImageTexture(BINDING_OUTPUT_IMAGE, image.StorageHandle(), 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       format);
    for (const SwizzleParameters& swizzle : swizzles) {
        const Extent3D num_tiles = swizzle.num_tiles;
        const size_t input_offset = swizzle.buffer_offset + map.offset;

        const u32 num_dispatches_x = Common::DivCeil(num_tiles.width, WORKGROUP_SIZE.width);
        const u32 num_dispatches_y = Common::DivCeil(num_tiles.height, WORKGROUP_SIZE.height);

        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, BINDING_INPUT_BUFFER, map.buffer, input_offset,
                          image.guest_size_bytes - swizzle.buffer_offset);
        glDispatchCompute(num_dispatches_x, num_dispatches_y, 1);
    }
    program_manager.RestoreGuestCompute();
}

void UtilShaders::CopyBC4(Image& dst_image, Image& src_image, std::span<const ImageCopy> copies) {
    static constexpr GLuint BINDING_INPUT_IMAGE = 0;
    static constexpr GLuint BINDING_OUTPUT_IMAGE = 1;
    static constexpr GLuint LOC_SRC_OFFSET = 0;
    static constexpr GLuint LOC_DST_OFFSET = 1;

    program_manager.BindComputeProgram(copy_bc4_program.handle);

    for (const ImageCopy& copy : copies) {
        ASSERT(copy.src_subresource.base_layer == 0);
        ASSERT(copy.src_subresource.num_layers == 1);
        ASSERT(copy.dst_subresource.base_layer == 0);
        ASSERT(copy.dst_subresource.num_layers == 1);

        glUniform3ui(LOC_SRC_OFFSET, copy.src_offset.x, copy.src_offset.y, copy.src_offset.z);
        glUniform3ui(LOC_DST_OFFSET, copy.dst_offset.x, copy.dst_offset.y, copy.dst_offset.z);
        glBindImageTexture(BINDING_INPUT_IMAGE, src_image.StorageHandle(),
                           copy.src_subresource.base_level, GL_TRUE, 0, GL_READ_ONLY, GL_RG32UI);
        glBindImageTexture(BINDING_OUTPUT_IMAGE, dst_image.StorageHandle(),
                           copy.dst_subresource.base_level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8UI);
        glDispatchCompute(copy.extent.width, copy.extent.height, copy.extent.depth);
    }
    program_manager.RestoreGuestCompute();
}

void UtilShaders::CopyBGR(Image& dst_image, Image& src_image,
                          std::span<const VideoCommon::ImageCopy> copies) {
    static constexpr GLuint BINDING_INPUT_IMAGE = 0;
    static constexpr GLuint BINDING_OUTPUT_IMAGE = 1;
    static constexpr VideoCommon::Offset3D zero_offset{0, 0, 0};
    const u32 bytes_per_block = BytesPerBlock(dst_image.info.format);
    switch (bytes_per_block) {
    case 2:
        // BGR565 copy
        for (const ImageCopy& copy : copies) {
            ASSERT(copy.src_offset == zero_offset);
            ASSERT(copy.dst_offset == zero_offset);
            bgr_copy_pass.Execute(dst_image, src_image, copy);
        }
        break;
    case 4: {
        // BGRA8 copy
        program_manager.BindComputeProgram(copy_bgra_program.handle);
        constexpr GLenum FORMAT = GL_RGBA8;
        for (const ImageCopy& copy : copies) {
            ASSERT(copy.src_offset == zero_offset);
            ASSERT(copy.dst_offset == zero_offset);
            glBindImageTexture(BINDING_INPUT_IMAGE, src_image.StorageHandle(),
                               copy.src_subresource.base_level, GL_FALSE, 0, GL_READ_ONLY, FORMAT);
            glBindImageTexture(BINDING_OUTPUT_IMAGE, dst_image.StorageHandle(),
                               copy.dst_subresource.base_level, GL_FALSE, 0, GL_WRITE_ONLY, FORMAT);
            glDispatchCompute(copy.extent.width, copy.extent.height, copy.extent.depth);
        }
        program_manager.RestoreGuestCompute();
        break;
    }
    default:
        UNREACHABLE();
        break;
    }
}

GLenum StoreFormat(u32 bytes_per_block) {
    switch (bytes_per_block) {
    case 1:
        return GL_R8UI;
    case 2:
        return GL_R16UI;
    case 4:
        return GL_R32UI;
    case 8:
        return GL_RG32UI;
    case 16:
        return GL_RGBA32UI;
    }
    UNREACHABLE();
    return GL_R8UI;
}

void Bgr565CopyPass::Execute(const Image& dst_image, const Image& src_image,
                             const ImageCopy& copy) {
    if (CopyBufferCreationNeeded(copy)) {
        CreateNewCopyBuffer(copy, GL_TEXTURE_2D_ARRAY, GL_RGB565);
    }
    // Copy from source to PBO
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, copy.extent.width);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, bgr16_pbo.handle);
    glGetTextureSubImage(src_image.Handle(), 0, 0, 0, 0, copy.extent.width, copy.extent.height,
                         copy.src_subresource.num_layers, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                         static_cast<GLsizei>(bgr16_pbo_size), nullptr);

    // Copy from PBO to destination in reverse order
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, copy.extent.width);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, bgr16_pbo.handle);
    glTextureSubImage3D(dst_image.Handle(), 0, 0, 0, 0, copy.extent.width, copy.extent.height,
                        copy.dst_subresource.num_layers, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV,
                        nullptr);
}

bool Bgr565CopyPass::CopyBufferCreationNeeded(const ImageCopy& copy) {
    return bgr16_pbo_size < NumPixelsInCopy(copy) * sizeof(u16);
}

void Bgr565CopyPass::CreateNewCopyBuffer(const ImageCopy& copy, GLenum target, GLuint format) {
    bgr16_pbo.Create();
    bgr16_pbo_size = NumPixelsInCopy(copy) * sizeof(u16);
    glNamedBufferData(bgr16_pbo.handle, bgr16_pbo_size, nullptr, GL_STREAM_COPY);
}

} // namespace OpenGL
