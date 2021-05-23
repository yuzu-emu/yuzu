// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <glad/glad.h>

namespace OpenGL {

class ProgramManager {
public:
    void BindProgram(GLuint program) {
        if (bound_program == program) {
            return;
        }
        bound_program = program;
        glUseProgram(program);
    }

    void RestoreGuestCompute() {}

private:
    GLuint bound_program = 0;
};

} // namespace OpenGL
