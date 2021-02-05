// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <fmt/format.h>

#include "common/common_funcs.h"
#include "shader_recompiler/exception.h"

namespace Shader::IR {

enum class Type {
    Void = 0,
    Opaque = 1 << 0,
    Label = 1 << 1,
    Reg = 1 << 2,
    Pred = 1 << 3,
    Attribute = 1 << 4,
    U1 = 1 << 5,
    U8 = 1 << 6,
    U16 = 1 << 7,
    U32 = 1 << 8,
    U64 = 1 << 9,
};
DECLARE_ENUM_FLAG_OPERATORS(Type)

[[nodiscard]] std::string NameOf(Type type);

[[nodiscard]] bool AreTypesCompatible(Type lhs, Type rhs) noexcept;

} // namespace Shader::IR

template <>
struct fmt::formatter<Shader::IR::Type> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::IR::Type& type, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{}", NameOf(type));
    }
};
