// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>

#include <fmt/format.h>

#include "shader_recompiler/backend/glasm/reg_alloc.h"

namespace Shader::IR {
class Inst;
struct Program;
} // namespace Shader::IR

namespace Shader::Backend::GLASM {

class EmitContext {
public:
    explicit EmitContext(IR::Program& program);

    template <typename... Args>
    void Add(const char* fmt, IR::Inst& inst, Args&&... args) {
        code += fmt::format(fmt, reg_alloc.Define(inst), std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    template <typename... Args>
    void Add(const char* fmt, Args&&... args) {
        code += fmt::format(fmt, std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    std::string code;
    RegAlloc reg_alloc{*this};
};

} // namespace Shader::Backend::GLASM
