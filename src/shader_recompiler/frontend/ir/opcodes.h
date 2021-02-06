// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string_view>

#include <fmt/format.h>

#include "shader_recompiler/frontend/ir/type.h"

namespace Shader::IR {

enum class Opcode {
#define OPCODE(name, ...) name,
#include "opcodes.inc"
#undef OPCODE
};

/// Get return type of an opcode
[[nodiscard]] Type TypeOf(Opcode op);

/// Get the number of arguments an opcode accepts
[[nodiscard]] size_t NumArgsOf(Opcode op);

/// Get the required type of an argument of an opcode
[[nodiscard]] Type ArgTypeOf(Opcode op, size_t arg_index);

/// Get the name of an opcode
[[nodiscard]] std::string_view NameOf(Opcode op);

} // namespace Shader::IR

template <>
struct fmt::formatter<Shader::IR::Opcode> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::IR::Opcode& op, FormatContext& ctx) {
        return format_to(ctx.out(), "{}", Shader::IR::NameOf(op));
    }
};
