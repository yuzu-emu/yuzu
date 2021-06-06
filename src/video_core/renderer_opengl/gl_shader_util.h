// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

void AttachShader(GLenum stage, GLuint program, std::string_view code);

void AttachShader(GLenum stage, GLuint program, std::span<const u32> code);

void LinkProgram(GLuint program);

OGLAssemblyProgram CompileProgram(std::string_view code, GLenum target);

} // namespace OpenGL
