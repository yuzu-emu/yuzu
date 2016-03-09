// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"

#include "video_core/rasterizer_interface.h"

class EmuWindow;

class RendererBase : NonCopyable {
public:

    /// Used to reference a framebuffer
    enum kFramebuffer {
        kFramebuffer_VirtualXFB = 0,
        kFramebuffer_EFB,
        kFramebuffer_Texture
    };

    RendererBase() : m_current_fps(0), m_current_frame(0) {
    }

    virtual ~RendererBase() {
    }

    /// Swap buffers (render frame)
    virtual void SwapBuffers() = 0;

    /**
     * Set the emulator window to use for renderer
     * @param window EmuWindow handle to emulator window to use for rendering
     */
    virtual void SetWindow(EmuWindow* window) = 0;

    /// Initialize the renderer
    virtual bool Init() = 0;

    /// Shutdown the renderer
    virtual void ShutDown() = 0;

    // Getter/setter functions:
    // ------------------------

    f32 GetCurrentframe() const {
        return m_current_fps;
    }

    int current_frame() const {
        return m_current_frame;
    }

    void RefreshRasterizerSetting();

    std::unique_ptr<VideoCore::RasterizerInterface> rasterizer;

protected:
    f32 m_current_fps;              ///< Current framerate, should be set by the renderer
    int m_current_frame;            ///< Current frame, should be set by the renderer

private:
    bool opengl_rasterizer_active = false;
};
