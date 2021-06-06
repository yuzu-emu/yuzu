// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>
#include <utility>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/microprofile.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

MICROPROFILE_DEFINE(OpenGL_ResourceCreation, "OpenGL", "Resource Creation", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_ResourceDeletion, "OpenGL", "Resource Deletion", MP_RGB(128, 128, 192));

namespace OpenGL {

void OGLRenderbuffer::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateRenderbuffers(1, &handle);
}

void OGLRenderbuffer::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteRenderbuffers(1, &handle);
    handle = 0;
}

void OGLTexture::Create(GLenum target) {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateTextures(target, 1, &handle);
}

void OGLTexture::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteTextures(1, &handle);
    handle = 0;
}

void OGLTextureView::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenTextures(1, &handle);
}

void OGLTextureView::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteTextures(1, &handle);
    handle = 0;
}

void OGLSampler::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateSamplers(1, &handle);
}

void OGLSampler::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteSamplers(1, &handle);
    handle = 0;
}

void OGLShader::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteShader(handle);
    handle = 0;
}

void OGLProgram::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteProgram(handle);
    handle = 0;
}

void OGLAssemblyProgram::Release() {
    if (handle == 0) {
        return;
    }
    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteProgramsARB(1, &handle);
    handle = 0;
}

void OGLPipeline::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenProgramPipelines(1, &handle);
}

void OGLPipeline::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteProgramPipelines(1, &handle);
    handle = 0;
}

void OGLBuffer::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateBuffers(1, &handle);
}

void OGLBuffer::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteBuffers(1, &handle);
    handle = 0;
}

void OGLSync::Create() {
    if (handle != 0)
        return;

    // Don't profile here, this one is expected to happen ingame.
    handle = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void OGLSync::Release() {
    if (handle == 0)
        return;

    // Don't profile here, this one is expected to happen ingame.
    glDeleteSync(handle);
    handle = 0;
}

void OGLFramebuffer::Create() {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glGenFramebuffers(1, &handle);
}

void OGLFramebuffer::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteFramebuffers(1, &handle);
    handle = 0;
}

void OGLQuery::Create(GLenum target) {
    if (handle != 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceCreation);
    glCreateQueries(target, 1, &handle);
}

void OGLQuery::Release() {
    if (handle == 0)
        return;

    MICROPROFILE_SCOPE(OpenGL_ResourceDeletion);
    glDeleteQueries(1, &handle);
    handle = 0;
}

} // namespace OpenGL
