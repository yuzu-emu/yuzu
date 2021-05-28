// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "shader_recompiler/backend/glsl/reg_alloc.h"
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

namespace Shader::Backend::GLSL {

class EmitContext {
public:
    explicit EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_,
                         const RuntimeInfo& runtime_info_);

    template <Type type, typename... Args>
    void Add(const char* format_str, IR::Inst& inst, Args&&... args) {
        code += fmt::format(format_str, reg_alloc.Define(inst, type), std::forward<Args>(args)...);
        // TODO: Remove this
        code += '\n';
    }

    template <typename... Args>
    void AddU1(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::U1>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF16x2(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::F16x2>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU32(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::U32>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddS32(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::S32>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF32(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::F32>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddS64(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::S64>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU64(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::U64>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF64(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::F64>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU32x2(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::U32x2>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF32x2(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::F32x2>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU32x3(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::U32x3>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF32x3(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::F32x3>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddU32x4(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::U32x4>(format_str, inst, args...);
    }

    template <typename... Args>
    void AddF32x4(const char* format_str, IR::Inst& inst, Args&&... args) {
        Add<Type::F32x4>(format_str, inst, args...);
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
    const RuntimeInfo& runtime_info;

    Stage stage{};
    std::string_view stage_name = "invalid";
    std::string_view attrib_name = "invalid";

    std::vector<u32> texture_buffer_bindings;
    std::vector<u32> image_buffer_bindings;
    std::vector<u32> texture_bindings;
    std::vector<u32> image_bindings;

    bool uses_y_direction{};

private:
    void SetupExtensions(std::string& header);
    void DefineConstantBuffers();
    void DefineStorageBuffers();
    void DefineHelperFunctions();
    void SetupImages(Bindings& bindings);
};

} // namespace Shader::Backend::GLSL
