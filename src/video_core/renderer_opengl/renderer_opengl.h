// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"

namespace Core {
class System;
class TelemetrySession;
} // namespace Core

namespace Core::Frontend {
class EmuWindow;
}

namespace Core::Memory {
class Memory;
}

namespace Layout {
struct FramebufferLayout;
}

namespace Tegra {
class GPU;
}

namespace OpenGL {

/// Structure used for storing information about the textures for the Switch screen
struct TextureInfo {
    OGLTexture resource;
    GLsizei width;
    GLsizei height;
    GLenum gl_format;
    GLenum gl_type;
    Tegra::FramebufferConfig::PixelFormat pixel_format;
};

/// Structure used for storing information about the display target for the Switch screen
struct ScreenInfo {
    GLuint display_texture{};
    bool display_srgb{};
    const Common::Rectangle<float> display_texcoords{0.0f, 0.0f, 1.0f, 1.0f};
    TextureInfo texture;
};

class RendererOpenGL final : public VideoCore::RendererBase {
public:
    explicit RendererOpenGL(Core::TelemetrySession& telemetry_session_,
                            Core::Frontend::EmuWindow& emu_window_,
                            Core::Memory::Memory& cpu_memory_, Tegra::GPU& gpu_,
                            std::unique_ptr<Core::Frontend::GraphicsContext> context_);
    ~RendererOpenGL() override;

    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) override;

    VideoCore::RasterizerInterface* ReadRasterizer() override {
        return &rasterizer;
    }

    [[nodiscard]] std::string GetDeviceVendor() const override {
        return device.GetVendorName();
    }

private:
    /// Initializes the OpenGL state and creates persistent objects.
    void InitOpenGLObjects();

    void AddTelemetryFields();

    void ConfigureFramebufferTexture(TextureInfo& texture,
                                     const Tegra::FramebufferConfig& framebuffer);

    /// Draws the emulated screens to the emulator window.
    void DrawScreen(const Layout::FramebufferLayout& layout);

    void RenderScreenshot();

    /// Loads framebuffer from emulated memory into the active OpenGL texture.
    void LoadFBToScreenInfo(const Tegra::FramebufferConfig& framebuffer);

    /// Fills active OpenGL texture with the given RGB color.Since the color is solid, the texture
    /// can be 1x1 but will stretch across whatever it's rendered on.
    void LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, u8 color_a,
                                    const TextureInfo& texture);

    void PrepareRendertarget(const Tegra::FramebufferConfig* framebuffer);

    Core::TelemetrySession& telemetry_session;
    Core::Frontend::EmuWindow& emu_window;
    Core::Memory::Memory& cpu_memory;
    Tegra::GPU& gpu;

    Device device;
    StateTracker state_tracker;
    ProgramManager program_manager;
    RasterizerOpenGL rasterizer;

    // OpenGL object IDs
    OGLSampler present_sampler;
    OGLBuffer vertex_buffer;
    OGLProgram present_program;
    OGLFramebuffer screenshot_framebuffer;

    // GPU address of the vertex buffer
    GLuint64EXT vertex_buffer_address = 0;

    /// Display information for Switch screen
    ScreenInfo screen_info;

    /// OpenGL framebuffer data
    std::vector<u8> gl_framebuffer_data;

    /// Used for transforming the framebuffer orientation
    Tegra::FramebufferConfig::TransformFlags framebuffer_transform_flags{};
    Common::Rectangle<int> framebuffer_crop_rect;
};

} // namespace OpenGL
