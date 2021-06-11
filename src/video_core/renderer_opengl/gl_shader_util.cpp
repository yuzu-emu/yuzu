// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>
#include <vector>
#include <glad/glad.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

namespace OpenGL {

static void LogShader(GLuint shader, std::string_view code = {}) {
    GLint shader_status{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_status);
    if (shader_status == GL_FALSE) {
        LOG_ERROR(Render_OpenGL, "Failed to build shader");
    }
    GLint log_length{};
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length == 0) {
        return;
    }
    std::string log(log_length, 0);
    glGetShaderInfoLog(shader, log_length, nullptr, log.data());
    if (shader_status == GL_FALSE) {
        LOG_ERROR(Render_OpenGL, "{}", log);
        if (!code.empty()) {
            LOG_INFO(Render_OpenGL, "\n{}", code);
        }
    } else {
        LOG_WARNING(Render_OpenGL, "{}", log);
    }
}

void AttachShader(GLenum stage, GLuint program, std::string_view code) {
    OGLShader shader;
    shader.handle = glCreateShader(stage);

    const GLint length = static_cast<GLint>(code.size());
    const GLchar* const code_ptr = code.data();
    glShaderSource(shader.handle, 1, &code_ptr, &length);
    glCompileShader(shader.handle);
    glAttachShader(program, shader.handle);
    if (Settings::values.renderer_debug) {
        LogShader(shader.handle, code);
    }
}

void AttachShader(GLenum stage, GLuint program, std::span<const u32> code) {
    OGLShader shader;
    shader.handle = glCreateShader(stage);

    glShaderBinary(1, &shader.handle, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, code.data(),
                   static_cast<GLsizei>(code.size_bytes()));
    glSpecializeShader(shader.handle, "main", 0, nullptr, nullptr);
    glAttachShader(program, shader.handle);
    if (Settings::values.renderer_debug) {
        LogShader(shader.handle);
    }
}

void LinkProgram(GLuint program) {
    glLinkProgram(program);
    if (!Settings::values.renderer_debug) {
        return;
    }
    GLint link_status{};
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);

    GLint log_length{};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length == 0) {
        return;
    }
    std::string log(log_length, 0);
    glGetProgramInfoLog(program, log_length, nullptr, log.data());
    if (link_status == GL_FALSE) {
        LOG_ERROR(Render_OpenGL, "{}", log);
    } else {
        LOG_WARNING(Render_OpenGL, "{}", log);
    }
}

OGLAssemblyProgram CompileProgram(std::string_view code, GLenum target) {
    OGLAssemblyProgram program;
    glGenProgramsARB(1, &program.handle);
    glNamedProgramStringEXT(program.handle, target, GL_PROGRAM_FORMAT_ASCII_ARB,
                            static_cast<GLsizei>(code.size()), code.data());
    if (Settings::values.renderer_debug) {
        const auto err = reinterpret_cast<const char*>(glGetString(GL_PROGRAM_ERROR_STRING_NV));
        if (err && *err) {
            if (std::strstr(err, "error")) {
                LOG_CRITICAL(Render_OpenGL, "\n{}", err);
                LOG_INFO(Render_OpenGL, "\n{}", code);
            } else {
                LOG_WARNING(Render_OpenGL, "\n{}", err);
            }
        }
    }
    return program;
}

} // namespace OpenGL
