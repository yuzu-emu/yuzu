// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <bitset>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <glad/glad.h>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/kernel/k_process.h"
#include "core/memory.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/renderer_opengl/maxwell_to_gl.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/shader_cache.h"
#include "video_core/texture_cache/texture_cache.h"

namespace OpenGL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using GLvec4 = std::array<GLfloat, 4>;

using Tegra::Engines::ShaderType;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceTarget;
using VideoCore::Surface::SurfaceType;

MICROPROFILE_DEFINE(OpenGL_Drawing, "OpenGL", "Drawing", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Clears, "OpenGL", "Clears", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Blits, "OpenGL", "Blits", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_CacheManagement, "OpenGL", "Cache Management", MP_RGB(100, 255, 100));

namespace {

constexpr size_t NUM_SUPPORTED_VERTEX_ATTRIBUTES = 16;

struct TextureHandle {
    constexpr TextureHandle(u32 data, bool via_header_index) {
        const Tegra::Texture::TextureHandle handle{data};
        image = handle.tic_id;
        sampler = via_header_index ? image : handle.tsc_id.Value();
    }

    u32 image;
    u32 sampler;
};

template <typename Engine, typename Entry>
TextureHandle GetTextureInfo(const Engine& engine, bool via_header_index, const Entry& entry,
                             ShaderType shader_type, size_t index = 0) {
    if constexpr (std::is_same_v<Entry, SamplerEntry>) {
        if (entry.is_separated) {
            const u32 buffer_1 = entry.buffer;
            const u32 buffer_2 = entry.secondary_buffer;
            const u32 offset_1 = entry.offset;
            const u32 offset_2 = entry.secondary_offset;
            const u32 handle_1 = engine.AccessConstBuffer32(shader_type, buffer_1, offset_1);
            const u32 handle_2 = engine.AccessConstBuffer32(shader_type, buffer_2, offset_2);
            return TextureHandle(handle_1 | handle_2, via_header_index);
        }
    }
    if (entry.is_bindless) {
        const u32 raw = engine.AccessConstBuffer32(shader_type, entry.buffer, entry.offset);
        return TextureHandle(raw, via_header_index);
    }
    const u32 buffer = engine.GetBoundBuffer();
    const u64 offset = (entry.offset + index) * sizeof(u32);
    return TextureHandle(engine.AccessConstBuffer32(shader_type, buffer, offset), via_header_index);
}

/// Translates hardware transform feedback indices
/// @param location Hardware location
/// @return Pair of ARB_transform_feedback3 token stream first and third arguments
/// @note Read https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_transform_feedback3.txt
std::pair<GLint, GLint> TransformFeedbackEnum(u8 location) {
    const u8 index = location / 4;
    if (index >= 8 && index <= 39) {
        return {GL_GENERIC_ATTRIB_NV, index - 8};
    }
    if (index >= 48 && index <= 55) {
        return {GL_TEXTURE_COORD_NV, index - 48};
    }
    switch (index) {
    case 7:
        return {GL_POSITION, 0};
    case 40:
        return {GL_PRIMARY_COLOR_NV, 0};
    case 41:
        return {GL_SECONDARY_COLOR_NV, 0};
    case 42:
        return {GL_BACK_PRIMARY_COLOR_NV, 0};
    case 43:
        return {GL_BACK_SECONDARY_COLOR_NV, 0};
    }
    UNIMPLEMENTED_MSG("index={}", index);
    return {GL_POSITION, 0};
}

void oglEnable(GLenum cap, bool state) {
    (state ? glEnable : glDisable)(cap);
}

ImageViewType ImageViewTypeFromEntry(const SamplerEntry& entry) {
    if (entry.is_buffer) {
        return ImageViewType::Buffer;
    }
    switch (entry.type) {
    case Tegra::Shader::TextureType::Texture1D:
        return entry.is_array ? ImageViewType::e1DArray : ImageViewType::e1D;
    case Tegra::Shader::TextureType::Texture2D:
        return entry.is_array ? ImageViewType::e2DArray : ImageViewType::e2D;
    case Tegra::Shader::TextureType::Texture3D:
        return ImageViewType::e3D;
    case Tegra::Shader::TextureType::TextureCube:
        return entry.is_array ? ImageViewType::CubeArray : ImageViewType::Cube;
    }
    UNREACHABLE();
    return ImageViewType::e2D;
}

ImageViewType ImageViewTypeFromEntry(const ImageEntry& entry) {
    switch (entry.type) {
    case Tegra::Shader::ImageType::Texture1D:
        return ImageViewType::e1D;
    case Tegra::Shader::ImageType::Texture1DArray:
        return ImageViewType::e1DArray;
    case Tegra::Shader::ImageType::Texture2D:
        return ImageViewType::e2D;
    case Tegra::Shader::ImageType::Texture2DArray:
        return ImageViewType::e2DArray;
    case Tegra::Shader::ImageType::Texture3D:
        return ImageViewType::e3D;
    case Tegra::Shader::ImageType::TextureBuffer:
        return ImageViewType::Buffer;
    }
    UNREACHABLE();
    return ImageViewType::e2D;
}

} // Anonymous namespace

RasterizerOpenGL::RasterizerOpenGL(Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                                   Core::Memory::Memory& cpu_memory_, const Device& device_,
                                   ScreenInfo& screen_info_, ProgramManager& program_manager_,
                                   StateTracker& state_tracker_)
    : RasterizerAccelerated(cpu_memory_), gpu(gpu_), maxwell3d(gpu.Maxwell3D()),
      kepler_compute(gpu.KeplerCompute()), gpu_memory(gpu.MemoryManager()), device(device_),
      screen_info(screen_info_), program_manager(program_manager_), state_tracker(state_tracker_),
      texture_cache_runtime(device, program_manager, state_tracker),
      texture_cache(texture_cache_runtime, *this, maxwell3d, kepler_compute, gpu_memory),
      buffer_cache_runtime(device),
      buffer_cache(*this, maxwell3d, kepler_compute, gpu_memory, cpu_memory_, buffer_cache_runtime),
      shader_cache(*this, emu_window_, gpu, maxwell3d, kepler_compute, gpu_memory, device),
      query_cache(*this, maxwell3d, gpu_memory),
      fence_manager(*this, gpu, texture_cache, buffer_cache, query_cache),
      async_shaders(emu_window_) {
    if (device.UseAsynchronousShaders()) {
        async_shaders.AllocateWorkers();
    }
}

RasterizerOpenGL::~RasterizerOpenGL() = default;

void RasterizerOpenGL::SyncVertexFormats() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::VertexFormats]) {
        return;
    }
    flags[Dirty::VertexFormats] = false;

    // Use the vertex array as-is, assumes that the data is formatted correctly for OpenGL. Enables
    // the first 16 vertex attributes always, as we don't know which ones are actually used until
    // shader time. Note, Tegra technically supports 32, but we're capping this to 16 for now to
    // avoid OpenGL errors.
    // TODO(Subv): Analyze the shader to identify which attributes are actually used and don't
    // assume every shader uses them all.
    for (std::size_t index = 0; index < NUM_SUPPORTED_VERTEX_ATTRIBUTES; ++index) {
        if (!flags[Dirty::VertexFormat0 + index]) {
            continue;
        }
        flags[Dirty::VertexFormat0 + index] = false;

        const auto attrib = maxwell3d.regs.vertex_attrib_format[index];
        const auto gl_index = static_cast<GLuint>(index);

        // Disable constant attributes.
        if (attrib.IsConstant()) {
            glDisableVertexAttribArray(gl_index);
            continue;
        }
        glEnableVertexAttribArray(gl_index);

        if (attrib.type == Maxwell::VertexAttribute::Type::SignedInt ||
            attrib.type == Maxwell::VertexAttribute::Type::UnsignedInt) {
            glVertexAttribIFormat(gl_index, attrib.ComponentCount(),
                                  MaxwellToGL::VertexFormat(attrib), attrib.offset);
        } else {
            glVertexAttribFormat(gl_index, attrib.ComponentCount(),
                                 MaxwellToGL::VertexFormat(attrib),
                                 attrib.IsNormalized() ? GL_TRUE : GL_FALSE, attrib.offset);
        }
        glVertexAttribBinding(gl_index, attrib.buffer);
    }
}

void RasterizerOpenGL::SyncVertexInstances() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::VertexInstances]) {
        return;
    }
    flags[Dirty::VertexInstances] = false;

    const auto& regs = maxwell3d.regs;
    for (std::size_t index = 0; index < NUM_SUPPORTED_VERTEX_ATTRIBUTES; ++index) {
        if (!flags[Dirty::VertexInstance0 + index]) {
            continue;
        }
        flags[Dirty::VertexInstance0 + index] = false;

        const auto gl_index = static_cast<GLuint>(index);
        const bool instancing_enabled = regs.instanced_arrays.IsInstancingEnabled(gl_index);
        const GLuint divisor = instancing_enabled ? regs.vertex_array[index].divisor : 0;
        glVertexBindingDivisor(gl_index, divisor);
    }
}

void RasterizerOpenGL::SetupShaders(bool is_indexed) {
    u32 clip_distances = 0;

    std::array<Shader*, Maxwell::MaxShaderStage> shaders{};
    image_view_indices.clear();
    sampler_handles.clear();

    texture_cache.SynchronizeGraphicsDescriptors();

    for (std::size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        const auto& shader_config = maxwell3d.regs.shader_config[index];
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};

        // Skip stages that are not enabled
        if (!maxwell3d.regs.IsShaderConfigEnabled(index)) {
            switch (program) {
            case Maxwell::ShaderProgram::Geometry:
                program_manager.UseGeometryShader(0);
                break;
            case Maxwell::ShaderProgram::Fragment:
                program_manager.UseFragmentShader(0);
                break;
            default:
                break;
            }
            continue;
        }
        // Currently this stages are not supported in the OpenGL backend.
        // TODO(Blinkhawk): Port tesselation shaders from Vulkan to OpenGL
        if (program == Maxwell::ShaderProgram::TesselationControl ||
            program == Maxwell::ShaderProgram::TesselationEval) {
            continue;
        }

        Shader* const shader = shader_cache.GetStageProgram(program, async_shaders);
        const GLuint program_handle = shader->IsBuilt() ? shader->GetHandle() : 0;
        switch (program) {
        case Maxwell::ShaderProgram::VertexA:
        case Maxwell::ShaderProgram::VertexB:
            program_manager.UseVertexShader(program_handle);
            break;
        case Maxwell::ShaderProgram::Geometry:
            program_manager.UseGeometryShader(program_handle);
            break;
        case Maxwell::ShaderProgram::Fragment:
            program_manager.UseFragmentShader(program_handle);
            break;
        default:
            UNIMPLEMENTED_MSG("Unimplemented shader index={}, enable={}, offset=0x{:08X}", index,
                              shader_config.enable.Value(), shader_config.offset);
            break;
        }

        // Stage indices are 0 - 5
        const size_t stage = index == 0 ? 0 : index - 1;
        shaders[stage] = shader;

        SetupDrawTextures(shader, stage);
        SetupDrawImages(shader, stage);

        buffer_cache.SetEnabledUniformBuffers(stage, shader->GetEntries().enabled_uniform_buffers);

        buffer_cache.UnbindGraphicsStorageBuffers(stage);
        u32 ssbo_index = 0;
        for (const auto& buffer : shader->GetEntries().global_memory_entries) {
            buffer_cache.BindGraphicsStorageBuffer(stage, ssbo_index, buffer.cbuf_index,
                                                   buffer.cbuf_offset, buffer.is_written);
            ++ssbo_index;
        }

        // Workaround for Intel drivers.
        // When a clip distance is enabled but not set in the shader it crops parts of the screen
        // (sometimes it's half the screen, sometimes three quarters). To avoid this, enable the
        // clip distances only when it's written by a shader stage.
        clip_distances |= shader->GetEntries().clip_distances;

        // When VertexA is enabled, we have dual vertex shaders
        if (program == Maxwell::ShaderProgram::VertexA) {
            // VertexB was combined with VertexA, so we skip the VertexB iteration
            ++index;
        }
    }
    SyncClipEnabled(clip_distances);
    maxwell3d.dirty.flags[Dirty::Shaders] = false;

    buffer_cache.UpdateGraphicsBuffers(is_indexed);

    const std::span indices_span(image_view_indices.data(), image_view_indices.size());
    texture_cache.FillGraphicsImageViews(indices_span, image_view_ids);

    buffer_cache.BindHostGeometryBuffers(is_indexed);

    size_t image_view_index = 0;
    size_t texture_index = 0;
    size_t image_index = 0;
    for (size_t stage = 0; stage < Maxwell::MaxShaderStage; ++stage) {
        const Shader* const shader = shaders[stage];
        if (!shader) {
            continue;
        }
        buffer_cache.BindHostStageBuffers(stage);
        const auto& base = device.GetBaseBindings(stage);
        BindTextures(shader->GetEntries(), base.sampler, base.image, image_view_index,
                     texture_index, image_index);
    }
}

void RasterizerOpenGL::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                         const VideoCore::DiskResourceLoadCallback& callback) {
    shader_cache.LoadDiskCache(title_id, stop_loading, callback);
}

void RasterizerOpenGL::Clear() {
    MICROPROFILE_SCOPE(OpenGL_Clears);
    if (!maxwell3d.ShouldExecute()) {
        return;
    }

    const auto& regs = maxwell3d.regs;
    bool use_color{};
    bool use_depth{};
    bool use_stencil{};

    if (regs.clear_buffers.R || regs.clear_buffers.G || regs.clear_buffers.B ||
        regs.clear_buffers.A) {
        use_color = true;

        const GLuint index = regs.clear_buffers.RT;
        state_tracker.NotifyColorMask(index);
        glColorMaski(index, regs.clear_buffers.R != 0, regs.clear_buffers.G != 0,
                     regs.clear_buffers.B != 0, regs.clear_buffers.A != 0);

        // TODO(Rodrigo): Determine if clamping is used on clears
        SyncFragmentColorClampState();
        SyncFramebufferSRGB();
    }
    if (regs.clear_buffers.Z) {
        ASSERT_MSG(regs.zeta_enable != 0, "Tried to clear Z but buffer is not enabled!");
        use_depth = true;

        state_tracker.NotifyDepthMask();
        glDepthMask(GL_TRUE);
    }
    if (regs.clear_buffers.S) {
        ASSERT_MSG(regs.zeta_enable, "Tried to clear stencil but buffer is not enabled!");
        use_stencil = true;
    }

    if (!use_color && !use_depth && !use_stencil) {
        // No color surface nor depth/stencil surface are enabled
        return;
    }

    SyncRasterizeEnable();
    SyncStencilTestState();

    if (regs.clear_flags.scissor) {
        SyncScissorTest();
    } else {
        state_tracker.NotifyScissor0();
        glDisablei(GL_SCISSOR_TEST, 0);
    }
    UNIMPLEMENTED_IF(regs.clear_flags.viewport);

    std::scoped_lock lock{texture_cache.mutex};
    texture_cache.UpdateRenderTargets(true);
    state_tracker.BindFramebuffer(texture_cache.GetFramebuffer()->Handle());

    if (use_color) {
        glClearBufferfv(GL_COLOR, regs.clear_buffers.RT, regs.clear_color);
    }
    if (use_depth && use_stencil) {
        glClearBufferfi(GL_DEPTH_STENCIL, 0, regs.clear_depth, regs.clear_stencil);
    } else if (use_depth) {
        glClearBufferfv(GL_DEPTH, 0, &regs.clear_depth);
    } else if (use_stencil) {
        glClearBufferiv(GL_STENCIL, 0, &regs.clear_stencil);
    }
    ++num_queued_commands;
}

void RasterizerOpenGL::Draw(bool is_indexed, bool is_instanced) {
    MICROPROFILE_SCOPE(OpenGL_Drawing);

    query_cache.UpdateCounters();

    SyncState();

    // Setup shaders and their used resources.
    std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
    SetupShaders(is_indexed);

    texture_cache.UpdateRenderTargets(false);
    state_tracker.BindFramebuffer(texture_cache.GetFramebuffer()->Handle());
    program_manager.BindGraphicsPipeline();

    const GLenum primitive_mode = MaxwellToGL::PrimitiveTopology(maxwell3d.regs.draw.topology);
    BeginTransformFeedback(primitive_mode);

    const GLuint base_instance = static_cast<GLuint>(maxwell3d.regs.vb_base_instance);
    const GLsizei num_instances =
        static_cast<GLsizei>(is_instanced ? maxwell3d.mme_draw.instance_count : 1);
    if (is_indexed) {
        const GLint base_vertex = static_cast<GLint>(maxwell3d.regs.vb_element_base);
        const GLsizei num_vertices = static_cast<GLsizei>(maxwell3d.regs.index_array.count);
        const GLvoid* const offset = buffer_cache_runtime.IndexOffset();
        const GLenum format = MaxwellToGL::IndexFormat(maxwell3d.regs.index_array.format);
        if (num_instances == 1 && base_instance == 0 && base_vertex == 0) {
            glDrawElements(primitive_mode, num_vertices, format, offset);
        } else if (num_instances == 1 && base_instance == 0) {
            glDrawElementsBaseVertex(primitive_mode, num_vertices, format, offset, base_vertex);
        } else if (base_vertex == 0 && base_instance == 0) {
            glDrawElementsInstanced(primitive_mode, num_vertices, format, offset, num_instances);
        } else if (base_vertex == 0) {
            glDrawElementsInstancedBaseInstance(primitive_mode, num_vertices, format, offset,
                                                num_instances, base_instance);
        } else if (base_instance == 0) {
            glDrawElementsInstancedBaseVertex(primitive_mode, num_vertices, format, offset,
                                              num_instances, base_vertex);
        } else {
            glDrawElementsInstancedBaseVertexBaseInstance(primitive_mode, num_vertices, format,
                                                          offset, num_instances, base_vertex,
                                                          base_instance);
        }
    } else {
        const GLint base_vertex = static_cast<GLint>(maxwell3d.regs.vertex_buffer.first);
        const GLsizei num_vertices = static_cast<GLsizei>(maxwell3d.regs.vertex_buffer.count);
        if (num_instances == 1 && base_instance == 0) {
            glDrawArrays(primitive_mode, base_vertex, num_vertices);
        } else if (base_instance == 0) {
            glDrawArraysInstanced(primitive_mode, base_vertex, num_vertices, num_instances);
        } else {
            glDrawArraysInstancedBaseInstance(primitive_mode, base_vertex, num_vertices,
                                              num_instances, base_instance);
        }
    }

    EndTransformFeedback();

    ++num_queued_commands;

    gpu.TickWork();
}

void RasterizerOpenGL::DispatchCompute(GPUVAddr code_addr) {
    Shader* const kernel = shader_cache.GetComputeKernel(code_addr);

    std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
    BindComputeTextures(kernel);

    const auto& entries = kernel->GetEntries();
    buffer_cache.SetEnabledComputeUniformBuffers(entries.enabled_uniform_buffers);
    buffer_cache.UnbindComputeStorageBuffers();
    u32 ssbo_index = 0;
    for (const auto& buffer : entries.global_memory_entries) {
        buffer_cache.BindComputeStorageBuffer(ssbo_index, buffer.cbuf_index, buffer.cbuf_offset,
                                              buffer.is_written);
        ++ssbo_index;
    }
    buffer_cache.UpdateComputeBuffers();
    buffer_cache.BindHostComputeBuffers();

    const auto& launch_desc = kepler_compute.launch_description;
    glDispatchCompute(launch_desc.grid_dim_x, launch_desc.grid_dim_y, launch_desc.grid_dim_z);
    ++num_queued_commands;
}

void RasterizerOpenGL::ResetCounter(VideoCore::QueryType type) {
    query_cache.ResetCounter(type);
}

void RasterizerOpenGL::Query(GPUVAddr gpu_addr, VideoCore::QueryType type,
                             std::optional<u64> timestamp) {
    query_cache.Query(gpu_addr, type, timestamp);
}

void RasterizerOpenGL::BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr,
                                                 u32 size) {
    std::scoped_lock lock{buffer_cache.mutex};
    buffer_cache.BindGraphicsUniformBuffer(stage, index, gpu_addr, size);
}

void RasterizerOpenGL::DisableGraphicsUniformBuffer(size_t stage, u32 index) {
    buffer_cache.DisableGraphicsUniformBuffer(stage, index);
}

void RasterizerOpenGL::FlushAll() {}

void RasterizerOpenGL::FlushRegion(VAddr addr, u64 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return;
    }
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.DownloadMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.DownloadMemory(addr, size);
    }
    query_cache.FlushRegion(addr, size);
}

void RasterizerOpenGL::InvalidateExceptTextureCache(VAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    shader_cache.InvalidateRegion(addr, size);
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    query_cache.InvalidateRegion(addr, size);
}

void RasterizerOpenGL::InvalidateTextureCache(VAddr addr, u64 size) {
    if (addr == 0 || size == 0) {
        return;
    }
    std::scoped_lock lock{texture_cache.mutex};
    texture_cache.UnmapMemory(addr, size);
}

bool RasterizerOpenGL::MustFlushRegion(VAddr addr, u64 size) {
    std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
    if (!Settings::IsGPULevelHigh()) {
        return buffer_cache.IsRegionGpuModified(addr, size);
    }
    return texture_cache.IsRegionGpuModified(addr, size) ||
           buffer_cache.IsRegionGpuModified(addr, size);
}

void RasterizerOpenGL::InvalidateRegion(VAddr addr, u64 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return;
    }
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    shader_cache.InvalidateRegion(addr, size);
    query_cache.InvalidateRegion(addr, size);
}

void RasterizerOpenGL::OnCPUWrite(VAddr addr, u64 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return;
    }
    shader_cache.OnCPUWrite(addr, size);
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.CachedWriteMemory(addr, size);
    }
}

void RasterizerOpenGL::SyncGuestHost() {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    shader_cache.SyncGuestHost();
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.FlushCachedWrites();
    }
}

void RasterizerOpenGL::UnmapMemory(VAddr addr, u64 size) {
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.UnmapMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    shader_cache.OnCPUWrite(addr, size);
}

void RasterizerOpenGL::ModifyGPUMemory(GPUVAddr addr, u64 size) {
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.UnmapGPUMemory(addr, size);
    }
}

void RasterizerOpenGL::SignalSemaphore(GPUVAddr addr, u32 value) {
    if (!gpu.IsAsync()) {
        gpu_memory.Write<u32>(addr, value);
        return;
    }
    fence_manager.SignalSemaphore(addr, value);
}

void RasterizerOpenGL::SignalSyncPoint(u32 value) {
    if (!gpu.IsAsync()) {
        gpu.IncrementSyncPoint(value);
        return;
    }
    fence_manager.SignalSyncPoint(value);
}

void RasterizerOpenGL::ReleaseFences() {
    if (!gpu.IsAsync()) {
        return;
    }
    fence_manager.WaitPendingFences();
}

void RasterizerOpenGL::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    if (Settings::IsGPULevelExtreme()) {
        FlushRegion(addr, size);
    }
    InvalidateRegion(addr, size);
}

void RasterizerOpenGL::WaitForIdle() {
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void RasterizerOpenGL::FragmentBarrier() {
    glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
}

void RasterizerOpenGL::TiledCacheBarrier() {
    glTextureBarrier();
}

void RasterizerOpenGL::FlushCommands() {
    // Only flush when we have commands queued to OpenGL.
    if (num_queued_commands == 0) {
        return;
    }
    num_queued_commands = 0;
    glFlush();
}

void RasterizerOpenGL::TickFrame() {
    // Ticking a frame means that buffers will be swapped, calling glFlush implicitly.
    num_queued_commands = 0;

    fence_manager.TickFrame();
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.TickFrame();
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.TickFrame();
    }
}

bool RasterizerOpenGL::AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Surface& src,
                                             const Tegra::Engines::Fermi2D::Surface& dst,
                                             const Tegra::Engines::Fermi2D::Config& copy_config) {
    MICROPROFILE_SCOPE(OpenGL_Blits);
    std::scoped_lock lock{texture_cache.mutex};
    texture_cache.BlitImage(dst, src, copy_config);
    return true;
}

bool RasterizerOpenGL::AccelerateDisplay(const Tegra::FramebufferConfig& config,
                                         VAddr framebuffer_addr, u32 pixel_stride) {
    if (framebuffer_addr == 0) {
        return false;
    }
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);

    std::scoped_lock lock{texture_cache.mutex};
    ImageView* const image_view{texture_cache.TryFindFramebufferImageView(framebuffer_addr)};
    if (!image_view) {
        return false;
    }
    // Verify that the cached surface is the same size and format as the requested framebuffer
    // ASSERT_MSG(image_view->size.width == config.width, "Framebuffer width is different");
    // ASSERT_MSG(image_view->size.height == config.height, "Framebuffer height is different");

    screen_info.display_texture = image_view->Handle(ImageViewType::e2D);
    screen_info.display_srgb = VideoCore::Surface::IsPixelFormatSRGB(image_view->format);
    return true;
}

void RasterizerOpenGL::BindComputeTextures(Shader* kernel) {
    image_view_indices.clear();
    sampler_handles.clear();

    texture_cache.SynchronizeComputeDescriptors();

    SetupComputeTextures(kernel);
    SetupComputeImages(kernel);

    const std::span indices_span(image_view_indices.data(), image_view_indices.size());
    texture_cache.FillComputeImageViews(indices_span, image_view_ids);

    program_manager.BindCompute(kernel->GetHandle());
    size_t image_view_index = 0;
    size_t texture_index = 0;
    size_t image_index = 0;
    BindTextures(kernel->GetEntries(), 0, 0, image_view_index, texture_index, image_index);
}

void RasterizerOpenGL::BindTextures(const ShaderEntries& entries, GLuint base_texture,
                                    GLuint base_image, size_t& image_view_index,
                                    size_t& texture_index, size_t& image_index) {
    const GLuint* const samplers = sampler_handles.data() + texture_index;
    const GLuint* const textures = texture_handles.data() + texture_index;
    const GLuint* const images = image_handles.data() + image_index;

    const size_t num_samplers = entries.samplers.size();
    for (const auto& sampler : entries.samplers) {
        for (size_t i = 0; i < sampler.size; ++i) {
            const ImageViewId image_view_id = image_view_ids[image_view_index++];
            const ImageView& image_view = texture_cache.GetImageView(image_view_id);
            const GLuint handle = image_view.Handle(ImageViewTypeFromEntry(sampler));
            texture_handles[texture_index++] = handle;
        }
    }
    const size_t num_images = entries.images.size();
    for (size_t unit = 0; unit < num_images; ++unit) {
        // TODO: Mark as modified
        const ImageViewId image_view_id = image_view_ids[image_view_index++];
        const ImageView& image_view = texture_cache.GetImageView(image_view_id);
        const GLuint handle = image_view.Handle(ImageViewTypeFromEntry(entries.images[unit]));
        image_handles[image_index] = handle;
        ++image_index;
    }
    if (num_samplers > 0) {
        glBindSamplers(base_texture, static_cast<GLsizei>(num_samplers), samplers);
        glBindTextures(base_texture, static_cast<GLsizei>(num_samplers), textures);
    }
    if (num_images > 0) {
        glBindImageTextures(base_image, static_cast<GLsizei>(num_images), images);
    }
}

void RasterizerOpenGL::SetupDrawTextures(const Shader* shader, size_t stage_index) {
    const bool via_header_index =
        maxwell3d.regs.sampler_index == Maxwell::SamplerIndex::ViaHeaderIndex;
    for (const auto& entry : shader->GetEntries().samplers) {
        const auto shader_type = static_cast<ShaderType>(stage_index);
        for (size_t index = 0; index < entry.size; ++index) {
            const auto handle =
                GetTextureInfo(maxwell3d, via_header_index, entry, shader_type, index);
            const Sampler* const sampler = texture_cache.GetGraphicsSampler(handle.sampler);
            sampler_handles.push_back(sampler->Handle());
            image_view_indices.push_back(handle.image);
        }
    }
}

void RasterizerOpenGL::SetupComputeTextures(const Shader* kernel) {
    const bool via_header_index = kepler_compute.launch_description.linked_tsc;
    for (const auto& entry : kernel->GetEntries().samplers) {
        for (size_t i = 0; i < entry.size; ++i) {
            const auto handle =
                GetTextureInfo(kepler_compute, via_header_index, entry, ShaderType::Compute, i);
            const Sampler* const sampler = texture_cache.GetComputeSampler(handle.sampler);
            sampler_handles.push_back(sampler->Handle());
            image_view_indices.push_back(handle.image);
        }
    }
}

void RasterizerOpenGL::SetupDrawImages(const Shader* shader, size_t stage_index) {
    const bool via_header_index =
        maxwell3d.regs.sampler_index == Maxwell::SamplerIndex::ViaHeaderIndex;
    for (const auto& entry : shader->GetEntries().images) {
        const auto shader_type = static_cast<ShaderType>(stage_index);
        const auto handle = GetTextureInfo(maxwell3d, via_header_index, entry, shader_type);
        image_view_indices.push_back(handle.image);
    }
}

void RasterizerOpenGL::SetupComputeImages(const Shader* shader) {
    const bool via_header_index = kepler_compute.launch_description.linked_tsc;
    for (const auto& entry : shader->GetEntries().images) {
        const auto handle =
            GetTextureInfo(kepler_compute, via_header_index, entry, ShaderType::Compute);
        image_view_indices.push_back(handle.image);
    }
}

void RasterizerOpenGL::SyncState() {
    SyncViewport();
    SyncRasterizeEnable();
    SyncPolygonModes();
    SyncColorMask();
    SyncFragmentColorClampState();
    SyncMultiSampleState();
    SyncDepthTestState();
    SyncDepthClamp();
    SyncStencilTestState();
    SyncBlendState();
    SyncLogicOpState();
    SyncCullMode();
    SyncPrimitiveRestart();
    SyncScissorTest();
    SyncPointState();
    SyncLineState();
    SyncPolygonOffset();
    SyncAlphaTest();
    SyncFramebufferSRGB();
    SyncVertexFormats();
    SyncVertexInstances();
}

void RasterizerOpenGL::SyncViewport() {
    auto& flags = maxwell3d.dirty.flags;
    const auto& regs = maxwell3d.regs;

    const bool dirty_viewport = flags[Dirty::Viewports];
    const bool dirty_clip_control = flags[Dirty::ClipControl];

    if (dirty_clip_control || flags[Dirty::FrontFace]) {
        flags[Dirty::FrontFace] = false;

        GLenum mode = MaxwellToGL::FrontFace(regs.front_face);
        if (regs.screen_y_control.triangle_rast_flip != 0 &&
            regs.viewport_transform[0].scale_y < 0.0f) {
            switch (mode) {
            case GL_CW:
                mode = GL_CCW;
                break;
            case GL_CCW:
                mode = GL_CW;
                break;
            }
        }
        glFrontFace(mode);
    }

    if (dirty_viewport || flags[Dirty::ClipControl]) {
        flags[Dirty::ClipControl] = false;

        bool flip_y = false;
        if (regs.viewport_transform[0].scale_y < 0.0f) {
            flip_y = !flip_y;
        }
        if (regs.screen_y_control.y_negate != 0) {
            flip_y = !flip_y;
        }
        const bool is_zero_to_one = regs.depth_mode == Maxwell::DepthMode::ZeroToOne;
        const GLenum origin = flip_y ? GL_UPPER_LEFT : GL_LOWER_LEFT;
        const GLenum depth = is_zero_to_one ? GL_ZERO_TO_ONE : GL_NEGATIVE_ONE_TO_ONE;
        state_tracker.ClipControl(origin, depth);
        state_tracker.SetYNegate(regs.screen_y_control.y_negate != 0);
    }

    if (dirty_viewport) {
        flags[Dirty::Viewports] = false;

        const bool force = flags[Dirty::ViewportTransform];
        flags[Dirty::ViewportTransform] = false;

        for (std::size_t i = 0; i < Maxwell::NumViewports; ++i) {
            if (!force && !flags[Dirty::Viewport0 + i]) {
                continue;
            }
            flags[Dirty::Viewport0 + i] = false;

            const auto& src = regs.viewport_transform[i];
            const Common::Rectangle<f32> rect{src.GetRect()};
            glViewportIndexedf(static_cast<GLuint>(i), rect.left, rect.bottom, rect.GetWidth(),
                               rect.GetHeight());

            const GLdouble reduce_z = regs.depth_mode == Maxwell::DepthMode::MinusOneToOne;
            const GLdouble near_depth = src.translate_z - src.scale_z * reduce_z;
            const GLdouble far_depth = src.translate_z + src.scale_z;
            if (device.HasDepthBufferFloat()) {
                glDepthRangeIndexeddNV(static_cast<GLuint>(i), near_depth, far_depth);
            } else {
                glDepthRangeIndexed(static_cast<GLuint>(i), near_depth, far_depth);
            }

            if (!GLAD_GL_NV_viewport_swizzle) {
                continue;
            }
            glViewportSwizzleNV(static_cast<GLuint>(i), MaxwellToGL::ViewportSwizzle(src.swizzle.x),
                                MaxwellToGL::ViewportSwizzle(src.swizzle.y),
                                MaxwellToGL::ViewportSwizzle(src.swizzle.z),
                                MaxwellToGL::ViewportSwizzle(src.swizzle.w));
        }
    }
}

void RasterizerOpenGL::SyncDepthClamp() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::DepthClampEnabled]) {
        return;
    }
    flags[Dirty::DepthClampEnabled] = false;

    oglEnable(GL_DEPTH_CLAMP, maxwell3d.regs.view_volume_clip_control.depth_clamp_disabled == 0);
}

void RasterizerOpenGL::SyncClipEnabled(u32 clip_mask) {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::ClipDistances] && !flags[Dirty::Shaders]) {
        return;
    }
    flags[Dirty::ClipDistances] = false;

    clip_mask &= maxwell3d.regs.clip_distance_enabled;
    if (clip_mask == last_clip_distance_mask) {
        return;
    }
    last_clip_distance_mask = clip_mask;

    for (std::size_t i = 0; i < Maxwell::Regs::NumClipDistances; ++i) {
        oglEnable(static_cast<GLenum>(GL_CLIP_DISTANCE0 + i), (clip_mask >> i) & 1);
    }
}

void RasterizerOpenGL::SyncClipCoef() {
    UNIMPLEMENTED();
}

void RasterizerOpenGL::SyncCullMode() {
    auto& flags = maxwell3d.dirty.flags;
    const auto& regs = maxwell3d.regs;

    if (flags[Dirty::CullTest]) {
        flags[Dirty::CullTest] = false;

        if (regs.cull_test_enabled) {
            glEnable(GL_CULL_FACE);
            glCullFace(MaxwellToGL::CullFace(regs.cull_face));
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
}

void RasterizerOpenGL::SyncPrimitiveRestart() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::PrimitiveRestart]) {
        return;
    }
    flags[Dirty::PrimitiveRestart] = false;

    if (maxwell3d.regs.primitive_restart.enabled) {
        glEnable(GL_PRIMITIVE_RESTART);
        glPrimitiveRestartIndex(maxwell3d.regs.primitive_restart.index);
    } else {
        glDisable(GL_PRIMITIVE_RESTART);
    }
}

void RasterizerOpenGL::SyncDepthTestState() {
    auto& flags = maxwell3d.dirty.flags;
    const auto& regs = maxwell3d.regs;

    if (flags[Dirty::DepthMask]) {
        flags[Dirty::DepthMask] = false;
        glDepthMask(regs.depth_write_enabled ? GL_TRUE : GL_FALSE);
    }

    if (flags[Dirty::DepthTest]) {
        flags[Dirty::DepthTest] = false;
        if (regs.depth_test_enable) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(MaxwellToGL::ComparisonOp(regs.depth_test_func));
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }
}

void RasterizerOpenGL::SyncStencilTestState() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::StencilTest]) {
        return;
    }
    flags[Dirty::StencilTest] = false;

    const auto& regs = maxwell3d.regs;
    oglEnable(GL_STENCIL_TEST, regs.stencil_enable);

    glStencilFuncSeparate(GL_FRONT, MaxwellToGL::ComparisonOp(regs.stencil_front_func_func),
                          regs.stencil_front_func_ref, regs.stencil_front_func_mask);
    glStencilOpSeparate(GL_FRONT, MaxwellToGL::StencilOp(regs.stencil_front_op_fail),
                        MaxwellToGL::StencilOp(regs.stencil_front_op_zfail),
                        MaxwellToGL::StencilOp(regs.stencil_front_op_zpass));
    glStencilMaskSeparate(GL_FRONT, regs.stencil_front_mask);

    if (regs.stencil_two_side_enable) {
        glStencilFuncSeparate(GL_BACK, MaxwellToGL::ComparisonOp(regs.stencil_back_func_func),
                              regs.stencil_back_func_ref, regs.stencil_back_func_mask);
        glStencilOpSeparate(GL_BACK, MaxwellToGL::StencilOp(regs.stencil_back_op_fail),
                            MaxwellToGL::StencilOp(regs.stencil_back_op_zfail),
                            MaxwellToGL::StencilOp(regs.stencil_back_op_zpass));
        glStencilMaskSeparate(GL_BACK, regs.stencil_back_mask);
    } else {
        glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xFFFFFFFF);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMaskSeparate(GL_BACK, 0xFFFFFFFF);
    }
}

void RasterizerOpenGL::SyncRasterizeEnable() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::RasterizeEnable]) {
        return;
    }
    flags[Dirty::RasterizeEnable] = false;

    oglEnable(GL_RASTERIZER_DISCARD, maxwell3d.regs.rasterize_enable == 0);
}

void RasterizerOpenGL::SyncPolygonModes() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::PolygonModes]) {
        return;
    }
    flags[Dirty::PolygonModes] = false;

    const auto& regs = maxwell3d.regs;
    if (regs.fill_rectangle) {
        if (!GLAD_GL_NV_fill_rectangle) {
            LOG_ERROR(Render_OpenGL, "GL_NV_fill_rectangle used and not supported");
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            return;
        }

        flags[Dirty::PolygonModeFront] = true;
        flags[Dirty::PolygonModeBack] = true;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL_RECTANGLE_NV);
        return;
    }

    if (regs.polygon_mode_front == regs.polygon_mode_back) {
        flags[Dirty::PolygonModeFront] = false;
        flags[Dirty::PolygonModeBack] = false;
        glPolygonMode(GL_FRONT_AND_BACK, MaxwellToGL::PolygonMode(regs.polygon_mode_front));
        return;
    }

    if (flags[Dirty::PolygonModeFront]) {
        flags[Dirty::PolygonModeFront] = false;
        glPolygonMode(GL_FRONT, MaxwellToGL::PolygonMode(regs.polygon_mode_front));
    }

    if (flags[Dirty::PolygonModeBack]) {
        flags[Dirty::PolygonModeBack] = false;
        glPolygonMode(GL_BACK, MaxwellToGL::PolygonMode(regs.polygon_mode_back));
    }
}

void RasterizerOpenGL::SyncColorMask() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::ColorMasks]) {
        return;
    }
    flags[Dirty::ColorMasks] = false;

    const bool force = flags[Dirty::ColorMaskCommon];
    flags[Dirty::ColorMaskCommon] = false;

    const auto& regs = maxwell3d.regs;
    if (regs.color_mask_common) {
        if (!force && !flags[Dirty::ColorMask0]) {
            return;
        }
        flags[Dirty::ColorMask0] = false;

        auto& mask = regs.color_mask[0];
        glColorMask(mask.R != 0, mask.B != 0, mask.G != 0, mask.A != 0);
        return;
    }

    // Path without color_mask_common set
    for (std::size_t i = 0; i < Maxwell::NumRenderTargets; ++i) {
        if (!force && !flags[Dirty::ColorMask0 + i]) {
            continue;
        }
        flags[Dirty::ColorMask0 + i] = false;

        const auto& mask = regs.color_mask[i];
        glColorMaski(static_cast<GLuint>(i), mask.R != 0, mask.G != 0, mask.B != 0, mask.A != 0);
    }
}

void RasterizerOpenGL::SyncMultiSampleState() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::MultisampleControl]) {
        return;
    }
    flags[Dirty::MultisampleControl] = false;

    const auto& regs = maxwell3d.regs;
    oglEnable(GL_SAMPLE_ALPHA_TO_COVERAGE, regs.multisample_control.alpha_to_coverage);
    oglEnable(GL_SAMPLE_ALPHA_TO_ONE, regs.multisample_control.alpha_to_one);
}

void RasterizerOpenGL::SyncFragmentColorClampState() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::FragmentClampColor]) {
        return;
    }
    flags[Dirty::FragmentClampColor] = false;

    glClampColor(GL_CLAMP_FRAGMENT_COLOR, maxwell3d.regs.frag_color_clamp ? GL_TRUE : GL_FALSE);
}

void RasterizerOpenGL::SyncBlendState() {
    auto& flags = maxwell3d.dirty.flags;
    const auto& regs = maxwell3d.regs;

    if (flags[Dirty::BlendColor]) {
        flags[Dirty::BlendColor] = false;
        glBlendColor(regs.blend_color.r, regs.blend_color.g, regs.blend_color.b,
                     regs.blend_color.a);
    }

    // TODO(Rodrigo): Revisit blending, there are several registers we are not reading

    if (!flags[Dirty::BlendStates]) {
        return;
    }
    flags[Dirty::BlendStates] = false;

    if (!regs.independent_blend_enable) {
        if (!regs.blend.enable[0]) {
            glDisable(GL_BLEND);
            return;
        }
        glEnable(GL_BLEND);
        glBlendFuncSeparate(MaxwellToGL::BlendFunc(regs.blend.factor_source_rgb),
                            MaxwellToGL::BlendFunc(regs.blend.factor_dest_rgb),
                            MaxwellToGL::BlendFunc(regs.blend.factor_source_a),
                            MaxwellToGL::BlendFunc(regs.blend.factor_dest_a));
        glBlendEquationSeparate(MaxwellToGL::BlendEquation(regs.blend.equation_rgb),
                                MaxwellToGL::BlendEquation(regs.blend.equation_a));
        return;
    }

    const bool force = flags[Dirty::BlendIndependentEnabled];
    flags[Dirty::BlendIndependentEnabled] = false;

    for (std::size_t i = 0; i < Maxwell::NumRenderTargets; ++i) {
        if (!force && !flags[Dirty::BlendState0 + i]) {
            continue;
        }
        flags[Dirty::BlendState0 + i] = false;

        if (!regs.blend.enable[i]) {
            glDisablei(GL_BLEND, static_cast<GLuint>(i));
            continue;
        }
        glEnablei(GL_BLEND, static_cast<GLuint>(i));

        const auto& src = regs.independent_blend[i];
        glBlendFuncSeparatei(static_cast<GLuint>(i), MaxwellToGL::BlendFunc(src.factor_source_rgb),
                             MaxwellToGL::BlendFunc(src.factor_dest_rgb),
                             MaxwellToGL::BlendFunc(src.factor_source_a),
                             MaxwellToGL::BlendFunc(src.factor_dest_a));
        glBlendEquationSeparatei(static_cast<GLuint>(i),
                                 MaxwellToGL::BlendEquation(src.equation_rgb),
                                 MaxwellToGL::BlendEquation(src.equation_a));
    }
}

void RasterizerOpenGL::SyncLogicOpState() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::LogicOp]) {
        return;
    }
    flags[Dirty::LogicOp] = false;

    const auto& regs = maxwell3d.regs;
    if (regs.logic_op.enable) {
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(MaxwellToGL::LogicOp(regs.logic_op.operation));
    } else {
        glDisable(GL_COLOR_LOGIC_OP);
    }
}

void RasterizerOpenGL::SyncScissorTest() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::Scissors]) {
        return;
    }
    flags[Dirty::Scissors] = false;

    const auto& regs = maxwell3d.regs;
    for (std::size_t index = 0; index < Maxwell::NumViewports; ++index) {
        if (!flags[Dirty::Scissor0 + index]) {
            continue;
        }
        flags[Dirty::Scissor0 + index] = false;

        const auto& src = regs.scissor_test[index];
        if (src.enable) {
            glEnablei(GL_SCISSOR_TEST, static_cast<GLuint>(index));
            glScissorIndexed(static_cast<GLuint>(index), src.min_x, src.min_y,
                             src.max_x - src.min_x, src.max_y - src.min_y);
        } else {
            glDisablei(GL_SCISSOR_TEST, static_cast<GLuint>(index));
        }
    }
}

void RasterizerOpenGL::SyncPointState() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::PointSize]) {
        return;
    }
    flags[Dirty::PointSize] = false;

    oglEnable(GL_POINT_SPRITE, maxwell3d.regs.point_sprite_enable);
    oglEnable(GL_PROGRAM_POINT_SIZE, maxwell3d.regs.vp_point_size.enable);

    glPointSize(std::max(1.0f, maxwell3d.regs.point_size));
}

void RasterizerOpenGL::SyncLineState() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::LineWidth]) {
        return;
    }
    flags[Dirty::LineWidth] = false;

    const auto& regs = maxwell3d.regs;
    oglEnable(GL_LINE_SMOOTH, regs.line_smooth_enable);
    glLineWidth(regs.line_smooth_enable ? regs.line_width_smooth : regs.line_width_aliased);
}

void RasterizerOpenGL::SyncPolygonOffset() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::PolygonOffset]) {
        return;
    }
    flags[Dirty::PolygonOffset] = false;

    const auto& regs = maxwell3d.regs;
    oglEnable(GL_POLYGON_OFFSET_FILL, regs.polygon_offset_fill_enable);
    oglEnable(GL_POLYGON_OFFSET_LINE, regs.polygon_offset_line_enable);
    oglEnable(GL_POLYGON_OFFSET_POINT, regs.polygon_offset_point_enable);

    if (regs.polygon_offset_fill_enable || regs.polygon_offset_line_enable ||
        regs.polygon_offset_point_enable) {
        // Hardware divides polygon offset units by two
        glPolygonOffsetClamp(regs.polygon_offset_factor, regs.polygon_offset_units / 2.0f,
                             regs.polygon_offset_clamp);
    }
}

void RasterizerOpenGL::SyncAlphaTest() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::AlphaTest]) {
        return;
    }
    flags[Dirty::AlphaTest] = false;

    const auto& regs = maxwell3d.regs;
    if (regs.alpha_test_enabled) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(MaxwellToGL::ComparisonOp(regs.alpha_test_func), regs.alpha_test_ref);
    } else {
        glDisable(GL_ALPHA_TEST);
    }
}

void RasterizerOpenGL::SyncFramebufferSRGB() {
    auto& flags = maxwell3d.dirty.flags;
    if (!flags[Dirty::FramebufferSRGB]) {
        return;
    }
    flags[Dirty::FramebufferSRGB] = false;

    oglEnable(GL_FRAMEBUFFER_SRGB, maxwell3d.regs.framebuffer_srgb);
}

void RasterizerOpenGL::SyncTransformFeedback() {
    // TODO(Rodrigo): Inject SKIP_COMPONENTS*_NV when required. An unimplemented message will signal
    // when this is required.
    const auto& regs = maxwell3d.regs;

    static constexpr std::size_t STRIDE = 3;
    std::array<GLint, 128 * STRIDE * Maxwell::NumTransformFeedbackBuffers> attribs;
    std::array<GLint, Maxwell::NumTransformFeedbackBuffers> streams;

    GLint* cursor = attribs.data();
    GLint* current_stream = streams.data();

    for (std::size_t feedback = 0; feedback < Maxwell::NumTransformFeedbackBuffers; ++feedback) {
        const auto& layout = regs.tfb_layouts[feedback];
        UNIMPLEMENTED_IF_MSG(layout.stride != layout.varying_count * 4, "Stride padding");
        if (layout.varying_count == 0) {
            continue;
        }

        *current_stream = static_cast<GLint>(feedback);
        if (current_stream != streams.data()) {
            // When stepping one stream, push the expected token
            cursor[0] = GL_NEXT_BUFFER_NV;
            cursor[1] = 0;
            cursor[2] = 0;
            cursor += STRIDE;
        }
        ++current_stream;

        const auto& locations = regs.tfb_varying_locs[feedback];
        std::optional<u8> current_index;
        for (u32 offset = 0; offset < layout.varying_count; ++offset) {
            const u8 location = locations[offset];
            const u8 index = location / 4;

            if (current_index == index) {
                // Increase number of components of the previous attachment
                ++cursor[-2];
                continue;
            }
            current_index = index;

            std::tie(cursor[0], cursor[2]) = TransformFeedbackEnum(location);
            cursor[1] = 1;
            cursor += STRIDE;
        }
    }

    const GLsizei num_attribs = static_cast<GLsizei>((cursor - attribs.data()) / STRIDE);
    const GLsizei num_strides = static_cast<GLsizei>(current_stream - streams.data());
    glTransformFeedbackStreamAttribsNV(num_attribs, attribs.data(), num_strides, streams.data(),
                                       GL_INTERLEAVED_ATTRIBS);
}

void RasterizerOpenGL::BeginTransformFeedback(GLenum primitive_mode) {
    const auto& regs = maxwell3d.regs;
    if (regs.tfb_enabled == 0) {
        return;
    }
    if (device.UseAssemblyShaders()) {
        SyncTransformFeedback();
    }
    UNIMPLEMENTED_IF(regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::TesselationControl) ||
                     regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::TesselationEval) ||
                     regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::Geometry));
    UNIMPLEMENTED_IF(primitive_mode != GL_POINTS);

    // We may have to call BeginTransformFeedbackNV here since they seem to call different
    // implementations on Nvidia's driver (the pointer is different) but we are using
    // ARB_transform_feedback3 features with NV_transform_feedback interactions and the ARB
    // extension doesn't define BeginTransformFeedback (without NV) interactions. It just works.
    glBeginTransformFeedback(GL_POINTS);
}

void RasterizerOpenGL::EndTransformFeedback() {
    const auto& regs = maxwell3d.regs;
    if (regs.tfb_enabled == 0) {
        return;
    }
    glEndTransformFeedback();
}

} // namespace OpenGL
