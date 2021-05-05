// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLASM {

[[nodiscard]] std::string EmitGLASM(const Profile& profile, IR::Program& program,
                                    Bindings& binding);

[[nodiscard]] inline std::string EmitGLASM(const Profile& profile, IR::Program& program) {
    Bindings binding;
    return EmitGLASM(profile, program, binding);
}

} // namespace Shader::Backend::GLASM
