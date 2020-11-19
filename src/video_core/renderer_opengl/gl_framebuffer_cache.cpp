// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>
#include <utility>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_opengl/gl_framebuffer_cache.h"

namespace OpenGL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using VideoCore::Surface::SurfaceType;

FramebufferCacheOpenGL::FramebufferCacheOpenGL() = default;

FramebufferCacheOpenGL::~FramebufferCacheOpenGL() = default;

GLuint FramebufferCacheOpenGL::GetFramebuffer(const FramebufferCacheKey& key) {
    const auto [entry, is_cache_miss] = cache.try_emplace(key);
    auto& framebuffer{entry->second};
    if (is_cache_miss) {
        framebuffer = CreateFramebuffer(key);
    }
    return framebuffer.handle;
}

OGLFramebuffer FramebufferCacheOpenGL::CreateFramebuffer(const FramebufferCacheKey& key) {
    OGLFramebuffer framebuffer;
    framebuffer.Create();

    // TODO(Rodrigo): Use DSA here after Nvidia fixes their framebuffer DSA bugs.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.handle);

    if (key.zeta) {
        const bool stencil = key.zeta->GetSurfaceParams().type == SurfaceType::DepthStencil;
        const GLenum attach_target = stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
        key.zeta->Attach(attach_target, GL_DRAW_FRAMEBUFFER);
    }

    std::size_t num_buffers = 0;
    std::array<GLenum, Maxwell::NumRenderTargets> targets;

    for (std::size_t index = 0; index < Maxwell::NumRenderTargets; ++index) {
        if (!key.colors[index]) {
            targets[index] = GL_NONE;
            continue;
        }
        const GLenum attach_target = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(index);
        key.colors[index]->Attach(attach_target, GL_DRAW_FRAMEBUFFER);

        const u32 attachment = (key.color_attachments >> (BitsPerAttachment * index)) & 0b1111;
        targets[index] = GL_COLOR_ATTACHMENT0 + attachment;
        num_buffers = index + 1;
    }

    if (num_buffers > 0) {
        glDrawBuffers(static_cast<GLsizei>(num_buffers), std::data(targets));
    } else {
        glDrawBuffer(GL_NONE);
    }

    return framebuffer;
}

std::size_t FramebufferCacheKey::Hash() const noexcept {
    std::size_t hash = std::hash<View>{}(zeta);
    for (const auto& color : colors) {
        hash ^= std::hash<View>{}(color);
    }
    hash ^= static_cast<std::size_t>(color_attachments) << 16;
    return hash;
}

} // namespace OpenGL
