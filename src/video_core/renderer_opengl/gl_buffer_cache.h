// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <span>

#include "common/alignment.h"
#include "common/common_types.h"
#include "common/dynamic_library.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_stream_buffer.h"

namespace OpenGL {

class BufferCacheRuntime;

class Buffer : public VideoCommon::BufferBase<VideoCore::RasterizerInterface> {
public:
    explicit Buffer(BufferCacheRuntime&, VideoCore::RasterizerInterface& rasterizer, VAddr cpu_addr,
                    u64 size_bytes);
    explicit Buffer(BufferCacheRuntime&, VideoCommon::NullBufferParams);

    void ImmediateUpload(size_t offset, std::span<const u8> data) noexcept;

    void ImmediateDownload(size_t offset, std::span<u8> data) noexcept;

    void MakeResident(GLenum access) noexcept;

    [[nodiscard]] GLuint64EXT HostGpuAddr() const noexcept {
        return address;
    }

    [[nodiscard]] GLuint Handle() const noexcept {
        return buffer.handle;
    }

private:
    GLuint64EXT address = 0;
    OGLBuffer buffer;
    GLenum current_residency_access = GL_NONE;
};

class BufferCacheRuntime {
    friend Buffer;

public:
    static constexpr u8 INVALID_BINDING = std::numeric_limits<u8>::max();

    explicit BufferCacheRuntime(const Device& device_);

    void CopyBuffer(Buffer& dst_buffer, Buffer& src_buffer,
                    std::span<const VideoCommon::BufferCopy> copies);

    void ClearBuffer(Buffer& dest_buffer, u32 offset, size_t size, u32 value);

    void BindIndexBuffer(Buffer& buffer, u32 offset, u32 size);

    void BindVertexBuffer(u32 index, Buffer& buffer, u32 offset, u32 size, u32 stride);

    void BindUniformBuffer(size_t stage, u32 binding_index, Buffer& buffer, u32 offset, u32 size);

    void BindComputeUniformBuffer(u32 binding_index, Buffer& buffer, u32 offset, u32 size);

    void BindStorageBuffer(size_t stage, u32 binding_index, Buffer& buffer, u32 offset, u32 size,
                           bool is_written);

    void BindComputeStorageBuffer(u32 binding_index, Buffer& buffer, u32 offset, u32 size,
                                  bool is_written);

    void BindTransformFeedbackBuffer(u32 index, Buffer& buffer, u32 offset, u32 size);

    void BindFastUniformBuffer(size_t stage, u32 binding_index, u32 size) {
        if (use_assembly_shaders) {
            const GLuint handle = fast_uniforms[stage][binding_index].handle;
            const GLsizeiptr gl_size = static_cast<GLsizeiptr>(size);
            glBindBufferRangeNV(PABO_LUT[stage], binding_index, handle, 0, gl_size);
        } else {
            const GLuint base_binding = device.GetBaseBindings(stage).uniform_buffer;
            const GLuint binding = base_binding + binding_index;
            glBindBufferRange(GL_UNIFORM_BUFFER, binding,
                              fast_uniforms[stage][binding_index].handle, 0,
                              static_cast<GLsizeiptr>(size));
        }
    }

    void PushFastUniformBuffer(size_t stage, u32 binding_index, std::span<const u8> data) {
        if (use_assembly_shaders) {
            glProgramBufferParametersIuivNV(
                PABO_LUT[stage], binding_index, 0,
                static_cast<GLsizei>(data.size_bytes() / sizeof(GLuint)),
                reinterpret_cast<const GLuint*>(data.data()));
        } else {
            glNamedBufferSubData(fast_uniforms[stage][binding_index].handle, 0,
                                 static_cast<GLsizeiptr>(data.size_bytes()), data.data());
        }
    }

    std::span<u8> BindMappedUniformBuffer(size_t stage, u32 binding_index, u32 size) noexcept {
        const auto [mapped_span, offset] = stream_buffer->Request(static_cast<size_t>(size));
        const GLuint base_binding = device.GetBaseBindings(stage).uniform_buffer;
        const GLuint binding = base_binding + binding_index;
        glBindBufferRange(GL_UNIFORM_BUFFER, binding, stream_buffer->Handle(),
                          static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
        return mapped_span;
    }

    [[nodiscard]] const GLvoid* IndexOffset() const noexcept {
        return reinterpret_cast<const GLvoid*>(static_cast<uintptr_t>(index_buffer_offset));
    }

    [[nodiscard]] bool HasFastBufferSubData() const noexcept {
        return has_fast_buffer_sub_data;
    }

private:
    static constexpr std::array PABO_LUT{
        GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV,          GL_TESS_CONTROL_PROGRAM_PARAMETER_BUFFER_NV,
        GL_TESS_EVALUATION_PROGRAM_PARAMETER_BUFFER_NV, GL_GEOMETRY_PROGRAM_PARAMETER_BUFFER_NV,
        GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV,
    };

    const Device& device;

    bool has_fast_buffer_sub_data = false;
    bool use_assembly_shaders = false;
    bool has_unified_vertex_buffers = false;

    u32 max_attributes = 0;

    std::optional<StreamBuffer> stream_buffer;

    std::array<std::array<OGLBuffer, VideoCommon::NUM_GRAPHICS_UNIFORM_BUFFERS>,
               VideoCommon::NUM_STAGES>
        fast_uniforms;
    std::array<std::array<OGLBuffer, VideoCommon::NUM_GRAPHICS_UNIFORM_BUFFERS>,
               VideoCommon::NUM_STAGES>
        copy_uniforms;
    std::array<OGLBuffer, VideoCommon::NUM_COMPUTE_UNIFORM_BUFFERS> copy_compute_uniforms;

    u32 index_buffer_offset = 0;
};

struct BufferCacheParams {
    using Runtime = OpenGL::BufferCacheRuntime;
    using Buffer = OpenGL::Buffer;

    static constexpr bool IS_OPENGL = true;
    static constexpr bool HAS_PERSISTENT_UNIFORM_BUFFER_BINDINGS = true;
    static constexpr bool HAS_FULL_INDEX_AND_PRIMITIVE_SUPPORT = true;
    static constexpr bool NEEDS_BIND_UNIFORM_INDEX = true;
    static constexpr bool NEEDS_BIND_STORAGE_INDEX = true;
    static constexpr bool USE_MEMORY_MAPS = false;
};

using BufferCache = VideoCommon::BufferCache<BufferCacheParams>;

} // namespace OpenGL
