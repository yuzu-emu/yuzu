// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <fmt/format.h>

namespace Shader::Maxwell {

enum class Opcode {
#define INST(name, cute, encode) name,
#include "maxwell.inc"
#undef INST
};

const char* NameOf(Opcode opcode);

} // namespace Shader::Maxwell

template <>
struct fmt::formatter<Shader::Maxwell::Opcode> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::Maxwell::Opcode& opcode, FormatContext& ctx) {
        return format_to(ctx.out(), "{}", NameOf(opcode));
    }
};
