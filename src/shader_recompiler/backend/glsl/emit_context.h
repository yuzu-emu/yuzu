// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "shader_recompiler/backend/glsl/reg_alloc.h"

namespace Shader::Backend::GLSL {

class EmitContext {
public:
    std::string code;
    RegAlloc reg_alloc;

private:
};

} // namespace Shader::Backend::GLSL
