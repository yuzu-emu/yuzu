// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>
#include <fmt/format.h>

#include "shader_recompiler/backend/glsl/reg_alloc.h"
#include "shader_recompiler/stage.h"

namespace Shader {
struct Info;
struct Profile;
} // namespace Shader

namespace Shader::Backend {
struct Bindings;
}

namespace Shader::IR {
class Inst;
struct Program;
} // namespace Shader::IR

namespace Shader::Backend::GLSL {

class EmitContext {
public:
    explicit EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_);

    template <typename... Args>
    void Add(const char* format_str, IR::Inst& inst, Args&&... args) {
        code += fmt::format(format_str, reg_alloc.Define(inst), std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    template <typename... Args>
    void Add(const char* format_str, Args&&... args) {
        code += fmt::format(format_str, std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    std::string code;
    RegAlloc reg_alloc;
    const Info& info;
    const Profile& profile;

private:
    void DefineConstantBuffers();
    void DefineStorageBuffers();
};

} // namespace Shader::Backend::GLSL
