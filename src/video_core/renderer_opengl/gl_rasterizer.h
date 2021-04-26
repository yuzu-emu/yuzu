// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include <boost/container/static_vector.hpp>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/engines/const_buffer_info.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/rasterizer_accelerated.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_fence_manager.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/shader/async_shaders.h"
#include "video_core/textures/texture.h"

namespace Core::Memory {
class Memory;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra {
class MemoryManager;
}

namespace OpenGL {

struct ScreenInfo;
struct ShaderEntries;

struct BindlessSSBO {
    GLuint64EXT address;
    GLsizei length;
    GLsizei padding;
};
static_assert(sizeof(BindlessSSBO) * CHAR_BIT == 128);

class RasterizerOpenGL : public VideoCore::RasterizerAccelerated {
public:
    explicit RasterizerOpenGL(Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                              Core::Memory::Memory& cpu_memory_, const Device& device_,
                              ScreenInfo& screen_info_, ProgramManager& program_manager_,
                              StateTracker& state_tracker_);
    ~RasterizerOpenGL() override;

    void Draw(bool is_indexed, bool is_instanced) override;
    void Clear() override;
    void DispatchCompute(GPUVAddr code_addr) override;
    void ResetCounter(VideoCore::QueryType type) override;
    void Query(GPUVAddr gpu_addr, VideoCore::QueryType type, std::optional<u64> timestamp) override;
    void BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr, u32 size) override;
    void DisableGraphicsUniformBuffer(size_t stage, u32 index) override;
    void FlushAll() override;
    void FlushRegion(VAddr addr, u64 size) override;
    void InvalidateExceptTextureCache(VAddr addr, u64 size) override;
    void InvalidateTextureCache(VAddr addr, u64 size) override;
    bool MustFlushRegion(VAddr addr, u64 size) override;
    void InvalidateRegion(VAddr addr, u64 size) override;
    void OnCPUWrite(VAddr addr, u64 size) override;
    void SyncGuestHost() override;
    void UnmapMemory(VAddr addr, u64 size) override;
    void ModifyGPUMemory(GPUVAddr addr, u64 size) override;
    void SignalSemaphore(GPUVAddr addr, u32 value) override;
    void SignalSyncPoint(u32 value) override;
    void ReleaseFences() override;
    void FlushAndInvalidateRegion(VAddr addr, u64 size) override;
    void WaitForIdle() override;
    void FragmentBarrier() override;
    void TiledCacheBarrier() override;
    void FlushCommands() override;
    void TickFrame() override;
    bool AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Surface& src,
                               const Tegra::Engines::Fermi2D::Surface& dst,
                               const Tegra::Engines::Fermi2D::Config& copy_config) override;
    bool AccelerateDisplay(const Tegra::FramebufferConfig& config, VAddr framebuffer_addr,
                           u32 pixel_stride) override;
    void LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                           const VideoCore::DiskResourceLoadCallback& callback) override;

    /// Returns true when there are commands queued to the OpenGL server.
    bool AnyCommandQueued() const {
        return num_queued_commands > 0;
    }

    VideoCommon::Shader::AsyncShaders& GetAsyncShaders() {
        return async_shaders;
    }

    const VideoCommon::Shader::AsyncShaders& GetAsyncShaders() const {
        return async_shaders;
    }

private:
    static constexpr size_t MAX_TEXTURES = 192;
    static constexpr size_t MAX_IMAGES = 48;
    static constexpr size_t MAX_IMAGE_VIEWS = MAX_TEXTURES + MAX_IMAGES;

    void BindComputeTextures(Shader* kernel);

    void BindTextures(const ShaderEntries& entries, GLuint base_texture, GLuint base_image,
                      size_t& image_view_index, size_t& texture_index, size_t& image_index);

    /// Configures the current textures to use for the draw command.
    void SetupDrawTextures(const Shader* shader, size_t stage_index);

    /// Configures the textures used in a compute shader.
    void SetupComputeTextures(const Shader* kernel);

    /// Configures images in a graphics shader.
    void SetupDrawImages(const Shader* shader, size_t stage_index);

    /// Configures images in a compute shader.
    void SetupComputeImages(const Shader* shader);

    /// Syncs state to match guest's
    void SyncState();

    /// Syncs the viewport and depth range to match the guest state
    void SyncViewport();

    /// Syncs the depth clamp state
    void SyncDepthClamp();

    /// Syncs the clip enabled status to match the guest state
    void SyncClipEnabled(u32 clip_mask);

    /// Syncs the clip coefficients to match the guest state
    void SyncClipCoef();

    /// Syncs the cull mode to match the guest state
    void SyncCullMode();

    /// Syncs the primitve restart to match the guest state
    void SyncPrimitiveRestart();

    /// Syncs the depth test state to match the guest state
    void SyncDepthTestState();

    /// Syncs the stencil test state to match the guest state
    void SyncStencilTestState();

    /// Syncs the blend state to match the guest state
    void SyncBlendState();

    /// Syncs the LogicOp state to match the guest state
    void SyncLogicOpState();

    /// Syncs the the color clamp state
    void SyncFragmentColorClampState();

    /// Syncs the alpha coverage and alpha to one
    void SyncMultiSampleState();

    /// Syncs the scissor test state to match the guest state
    void SyncScissorTest();

    /// Syncs the point state to match the guest state
    void SyncPointState();

    /// Syncs the line state to match the guest state
    void SyncLineState();

    /// Syncs the rasterizer enable state to match the guest state
    void SyncRasterizeEnable();

    /// Syncs polygon modes to match the guest state
    void SyncPolygonModes();

    /// Syncs Color Mask
    void SyncColorMask();

    /// Syncs the polygon offsets
    void SyncPolygonOffset();

    /// Syncs the alpha test state to match the guest state
    void SyncAlphaTest();

    /// Syncs the framebuffer sRGB state to match the guest state
    void SyncFramebufferSRGB();

    /// Syncs vertex formats to match the guest state
    void SyncVertexFormats();

    /// Syncs vertex instances to match the guest state
    void SyncVertexInstances();

    /// Syncs transform feedback state to match guest state
    /// @note Only valid on assembly shaders
    void SyncTransformFeedback();

    /// Begin a transform feedback
    void BeginTransformFeedback(GLenum primitive_mode);

    /// End a transform feedback
    void EndTransformFeedback();

    void SetupShaders(bool is_indexed);

    Tegra::GPU& gpu;
    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::Engines::KeplerCompute& kepler_compute;
    Tegra::MemoryManager& gpu_memory;

    const Device& device;
    ScreenInfo& screen_info;
    ProgramManager& program_manager;
    StateTracker& state_tracker;

    TextureCacheRuntime texture_cache_runtime;
    TextureCache texture_cache;
    BufferCacheRuntime buffer_cache_runtime;
    BufferCache buffer_cache;
    ShaderCacheOpenGL shader_cache;
    QueryCache query_cache;
    FenceManagerOpenGL fence_manager;

    VideoCommon::Shader::AsyncShaders async_shaders;

    boost::container::static_vector<u32, MAX_IMAGE_VIEWS> image_view_indices;
    std::array<ImageViewId, MAX_IMAGE_VIEWS> image_view_ids;
    boost::container::static_vector<GLuint, MAX_TEXTURES> sampler_handles;
    std::array<GLuint, MAX_TEXTURES> texture_handles{};
    std::array<GLuint, MAX_IMAGES> image_handles{};

    /// Number of commands queued to the OpenGL driver. Resetted on flush.
    std::size_t num_queued_commands = 0;

    u32 last_clip_distance_mask = 0;
};

} // namespace OpenGL
