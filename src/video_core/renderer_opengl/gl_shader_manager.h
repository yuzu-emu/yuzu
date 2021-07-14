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
        glCreateProgramPipelines(1, &pipeline.handle);
        if (device.UseAssemblyShaders()) {
            glEnable(GL_COMPUTE_PROGRAM_NV);
        }
    }

    void BindComputeProgram(GLuint program) {
        glUseProgram(program);
        is_compute_bound = true;
    }

    void BindComputeAssemblyProgram(GLuint program) {
        if (current_assembly_compute_program != program) {
            current_assembly_compute_program = program;
            glBindProgramARB(GL_COMPUTE_PROGRAM_NV, program);
        }
        UnbindPipeline();
    }

    void BindSourcePrograms(std::span<const OGLProgram, NUM_STAGES> programs) {
        static constexpr std::array<GLenum, 5> stage_enums{
            GL_VERTEX_SHADER_BIT,   GL_TESS_CONTROL_SHADER_BIT, GL_TESS_EVALUATION_SHADER_BIT,
            GL_GEOMETRY_SHADER_BIT, GL_FRAGMENT_SHADER_BIT,
        };
        for (size_t stage = 0; stage < NUM_STAGES; ++stage) {
            if (current_programs[stage] != programs[stage].handle) {
                current_programs[stage] = programs[stage].handle;
                glUseProgramStages(pipeline.handle, stage_enums[stage], programs[stage].handle);
            }
        }
        BindPipeline();
    }

    void BindPresentPrograms(GLuint vertex, GLuint fragment) {
        if (current_programs[0] != vertex) {
            current_programs[0] = vertex;
            glUseProgramStages(pipeline.handle, GL_VERTEX_SHADER_BIT, vertex);
        }
        if (current_programs[4] != fragment) {
            current_programs[4] = fragment;
            glUseProgramStages(pipeline.handle, GL_FRAGMENT_SHADER_BIT, fragment);
        }
        glUseProgramStages(
            pipeline.handle,
            GL_TESS_CONTROL_SHADER_BIT | GL_TESS_EVALUATION_SHADER_BIT | GL_GEOMETRY_SHADER_BIT, 0);
        current_programs[1] = 0;
        current_programs[2] = 0;
        current_programs[3] = 0;

        if (current_stage_mask != 0) {
            current_stage_mask = 0;
            for (const GLenum program_type : ASSEMBLY_PROGRAM_ENUMS) {
                glDisable(program_type);
            }
        }
        BindPipeline();
    }

    void BindAssemblyPrograms(std::span<const OGLAssemblyProgram, NUM_STAGES> programs,
                              u32 stage_mask) {
        const u32 changed_mask = current_stage_mask ^ stage_mask;
        current_stage_mask = stage_mask;

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
            if (current_programs[stage] != programs[stage].handle) {
                current_programs[stage] = programs[stage].handle;
                glBindProgramARB(ASSEMBLY_PROGRAM_ENUMS[stage], programs[stage].handle);
            }
        }
        UnbindPipeline();
    }

    void RestoreGuestCompute() {}

private:
    void BindPipeline() {
        if (!is_pipeline_bound) {
            is_pipeline_bound = true;
            glBindProgramPipeline(pipeline.handle);
        }
        UnbindCompute();
    }

    void UnbindPipeline() {
        if (is_pipeline_bound) {
            is_pipeline_bound = false;
            glBindProgramPipeline(0);
        }
        UnbindCompute();
    }

    void UnbindCompute() {
        if (is_compute_bound) {
            is_compute_bound = false;
            glUseProgram(0);
        }
    }

    OGLPipeline pipeline;
    bool is_pipeline_bound{};
    bool is_compute_bound{};

    u32 current_stage_mask = 0;
    std::array<GLuint, NUM_STAGES> current_programs{};
    GLuint current_assembly_compute_program = 0;
};

} // namespace OpenGL
