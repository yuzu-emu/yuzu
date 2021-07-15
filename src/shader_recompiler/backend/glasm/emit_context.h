// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "shader_recompiler/backend/glasm/reg_alloc.h"
#include "shader_recompiler/stage.h"

namespace Shader {
struct Info;
struct Profile;
struct RuntimeInfo;
} // namespace Shader

namespace Shader::Backend {
struct Bindings;
}

namespace Shader::IR {
class Inst;
struct Program;
} // namespace Shader::IR

namespace Shader::Backend::GLASM {

class EmitContext {
public:
    explicit EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_,
                         const RuntimeInfo& runtime_info_);

    template <typename... Args>
    void Add(const char* format_str, IR::Inst& inst, Args&&... args) {
        code += fmt::format(fmt::runtime(format_str), reg_alloc.Define(inst),
                            std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    template <typename... Args>
    void LongAdd(const char* format_str, IR::Inst& inst, Args&&... args) {
        code += fmt::format(fmt::runtime(format_str), reg_alloc.LongDefine(inst),
                            std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    template <typename... Args>
    void Add(const char* format_str, Args&&... args) {
        code += fmt::format(fmt::runtime(format_str), std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    std::string code;
    RegAlloc reg_alloc{};
    const Info& info;
    const Profile& profile;
    const RuntimeInfo& runtime_info;

    std::vector<u32> texture_buffer_bindings;
    std::vector<u32> image_buffer_bindings;
    std::vector<u32> texture_bindings;
    std::vector<u32> image_bindings;

    Stage stage{};
    std::string_view stage_name = "invalid";
    std::string_view attrib_name = "invalid";

    u32 num_safety_loop_vars{};
    bool uses_y_direction{};
};

} // namespace Shader::Backend::GLASM
