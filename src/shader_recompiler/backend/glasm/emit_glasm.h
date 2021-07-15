// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::GLASM {

[[nodiscard]] std::string EmitGLASM(const Profile& profile, const RuntimeInfo& runtime_info,
                                    IR::Program& program, Bindings& bindings);

[[nodiscard]] inline std::string EmitGLASM(const Profile& profile, const RuntimeInfo& runtime_info,
                                           IR::Program& program) {
    Bindings binding;
    return EmitGLASM(profile, runtime_info, program, binding);
}

} // namespace Shader::Backend::GLASM
