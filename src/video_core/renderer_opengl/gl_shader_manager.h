// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <span>

#include <glad/glad.h>

#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

class ProgramManager {
    static constexpr size_t NUM_STAGES = 5;

    static constexpr std::array ASSEMBLY_PROGRAM_ENUMS{
        GL_VERTEX_PROGRAM_NV,   GL_TESS_CONTROL_PROGRAM_NV, GL_TESS_EVALUATION_PROGRAM_NV,
        GL_GEOMETRY_PROGRAM_NV, GL_FRAGMENT_PROGRAM_NV,
    };

public:
    explicit ProgramManager(const Device& device) {
        if (device.UseAssemblyShaders()) {
            glEnable(GL_COMPUTE_PROGRAM_NV);
        }
    }

    void BindProgram(GLuint program) {
        if (current_source_program == program) {
            return;
        }
        current_source_program = program;
        glUseProgram(program);
    }

    void BindComputeAssemblyProgram(GLuint program) {
        if (current_compute_assembly_program != program) {
            current_compute_assembly_program = program;
            glBindProgramARB(GL_COMPUTE_PROGRAM_NV, program);
        }
        if (current_source_program != 0) {
            current_source_program = 0;
            glUseProgram(0);
        }
    }

    void BindAssemblyPrograms(std::span<const OGLAssemblyProgram, NUM_STAGES> programs,
                              u32 stage_mask) {
        const u32 changed_mask = current_assembly_mask ^ stage_mask;
        current_assembly_mask = stage_mask;

        if (changed_mask != 0) {
            for (size_t stage = 0; stage < NUM_STAGES; ++stage) {
                if (((changed_mask >> stage) & 1) != 0) {
                    if (((stage_mask >> stage) & 1) != 0) {
                        glEnable(ASSEMBLY_PROGRAM_ENUMS[stage]);
                    } else {
                        glDisable(ASSEMBLY_PROGRAM_ENUMS[stage]);
                    }
                }
            }
        }
        for (size_t stage = 0; stage < NUM_STAGES; ++stage) {
            if (current_assembly_programs[stage] != programs[stage].handle) {
                current_assembly_programs[stage] = programs[stage].handle;
                glBindProgramARB(ASSEMBLY_PROGRAM_ENUMS[stage], programs[stage].handle);
            }
        }
        if (current_source_program != 0) {
            current_source_program = 0;
            glUseProgram(0);
        }
    }

    void RestoreGuestCompute() {}

private:
    GLuint current_source_program = 0;

    u32 current_assembly_mask = 0;
    std::array<GLuint, NUM_STAGES> current_assembly_programs{};
    GLuint current_compute_assembly_program = 0;
};

} // namespace OpenGL
