// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <limits>

#include <glad/glad.h>

#include "common/common_types.h"
#include "core/core.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/maxwell_3d.h"

namespace Core {
class System;
}

namespace OpenGL {

namespace Dirty {

enum : u8 {
    First = VideoCommon::Dirty::LastCommonEntry,

    VertexFormats,
    VertexFormat0,
    VertexFormat31 = VertexFormat0 + 31,

    VertexBuffers,
    VertexBuffer0,
    VertexBuffer31 = VertexBuffer0 + 31,

    VertexInstances,
    VertexInstance0,
    VertexInstance31 = VertexInstance0 + 31,

    ViewportTransform,
    Viewports,
    Viewport0,
    Viewport15 = Viewport0 + 15,

    Scissors,
    Scissor0,
    Scissor15 = Scissor0 + 15,

    ColorMaskCommon,
    ColorMasks,
    ColorMask0,
    ColorMask7 = ColorMask0 + 7,

    BlendColor,
    BlendIndependentEnabled,
    BlendStates,
    BlendState0,
    BlendState7 = BlendState0 + 7,

    Shaders,
    ClipDistances,

    ColorMask,
    FrontFace,
    CullTest,
    DepthMask,
    DepthTest,
    StencilTest,
    AlphaTest,
    PrimitiveRestart,
    PolygonOffset,
    MultisampleControl,
    RasterizeEnable,
    FramebufferSRGB,
    LogicOp,
    FragmentClampColor,
    PointSize,
    ClipControl,

    Last
};
static_assert(Last <= std::numeric_limits<u8>::max());

} // namespace Dirty

class StateTracker {
public:
    explicit StateTracker(Core::System& system);

    void Initialize();

    void BindIndexBuffer(GLuint new_index_buffer) {
        if (index_buffer == new_index_buffer) {
            return;
        }
        index_buffer = new_index_buffer;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, new_index_buffer);
    }

    void NotifyScreenDrawVertexArray() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::VertexFormats] = true;
        flags[OpenGL::Dirty::VertexFormat0 + 0] = true;
        flags[OpenGL::Dirty::VertexFormat0 + 1] = true;

        flags[OpenGL::Dirty::VertexBuffers] = true;
        flags[OpenGL::Dirty::VertexBuffer0] = true;

        flags[OpenGL::Dirty::VertexInstances] = true;
        flags[OpenGL::Dirty::VertexInstance0 + 0] = true;
        flags[OpenGL::Dirty::VertexInstance0 + 1] = true;
    }

    void NotifyViewport0() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::Viewports] = true;
        flags[OpenGL::Dirty::Viewport0] = true;
    }

    void NotifyScissor0() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::Scissors] = true;
        flags[OpenGL::Dirty::Scissor0] = true;
    }

    void NotifyColorMask0() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::ColorMasks] = true;
        flags[OpenGL::Dirty::ColorMask0] = true;
    }

    void NotifyBlend0() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::BlendStates] = true;
        flags[OpenGL::Dirty::BlendState0] = true;
    }

    void NotifyFramebuffer() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[VideoCommon::Dirty::RenderTargets] = true;
    }

    void NotifyFrontFace() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::FrontFace] = true;
    }

    void NotifyCullTest() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::CullTest] = true;
    }

    void NotifyDepthTest() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::DepthTest] = true;
    }

    void NotifyStencilTest() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::StencilTest] = true;
    }

    void NotifyPolygonOffset() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::PolygonOffset] = true;
    }

    void NotifyRasterizeEnable() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::RasterizeEnable] = true;
    }

    void NotifyFramebufferSRGB() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::FramebufferSRGB] = true;
    }

    void NotifyLogicOp() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::LogicOp] = true;
    }

    void NotifyClipControl() {
        auto& flags = system.GPU().Maxwell3D().dirty.flags;
        flags[OpenGL::Dirty::ClipControl] = true;
    }

private:
    Core::System& system;

    GLuint index_buffer = 0;
};

} // namespace OpenGL
