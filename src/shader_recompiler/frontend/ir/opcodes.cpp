// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <string_view>

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/opcodes.h"

namespace Shader::IR {
namespace {
struct OpcodeMeta {
    std::string_view name;
    Type type;
    std::array<Type, 5> arg_types;
};

using enum Type;

constexpr std::array META_TABLE{
#define OPCODE(name_token, type_token, ...)                                                        \
    OpcodeMeta{                                                                                    \
        .name{#name_token},                                                                        \
        .type{type_token},                                                                         \
        .arg_types{__VA_ARGS__},                                                                   \
    },
#include "opcodes.inc"
#undef OPCODE
};

void ValidateOpcode(Opcode op) {
    const size_t raw{static_cast<size_t>(op)};
    if (raw >= META_TABLE.size()) {
        throw InvalidArgument("Invalid opcode with raw value {}", raw);
    }
}
} // Anonymous namespace

Type TypeOf(Opcode op) {
    ValidateOpcode(op);
    return META_TABLE[static_cast<size_t>(op)].type;
}

size_t NumArgsOf(Opcode op) {
    ValidateOpcode(op);
    const auto& arg_types{META_TABLE[static_cast<size_t>(op)].arg_types};
    const auto distance{std::distance(arg_types.begin(), std::ranges::find(arg_types, Type::Void))};
    return static_cast<size_t>(distance);
}

Type ArgTypeOf(Opcode op, size_t arg_index) {
    ValidateOpcode(op);
    const auto& arg_types{META_TABLE[static_cast<size_t>(op)].arg_types};
    if (arg_index >= arg_types.size() || arg_types[arg_index] == Type::Void) {
        throw InvalidArgument("Out of bounds argument");
    }
    return arg_types[arg_index];
}

std::string_view NameOf(Opcode op) {
    ValidateOpcode(op);
    return META_TABLE[static_cast<size_t>(op)].name;
}

} // namespace Shader::IR
