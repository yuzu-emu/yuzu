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

// using enum Type;
constexpr Type Void{Type::Void};
constexpr Type Opaque{Type::Opaque};
constexpr Type Label{Type::Label};
constexpr Type Reg{Type::Reg};
constexpr Type Pred{Type::Pred};
constexpr Type Attribute{Type::Attribute};
constexpr Type Patch{Type::Patch};
constexpr Type U1{Type::U1};
constexpr Type U8{Type::U8};
constexpr Type U16{Type::U16};
constexpr Type U32{Type::U32};
constexpr Type U64{Type::U64};
constexpr Type F16{Type::F16};
constexpr Type F32{Type::F32};
constexpr Type F64{Type::F64};
constexpr Type U32x2{Type::U32x2};
constexpr Type U32x3{Type::U32x3};
constexpr Type U32x4{Type::U32x4};
constexpr Type F16x2{Type::F16x2};
constexpr Type F16x3{Type::F16x3};
constexpr Type F16x4{Type::F16x4};
constexpr Type F32x2{Type::F32x2};
constexpr Type F32x3{Type::F32x3};
constexpr Type F32x4{Type::F32x4};
constexpr Type F64x2{Type::F64x2};
constexpr Type F64x3{Type::F64x3};
constexpr Type F64x4{Type::F64x4};

constexpr std::array META_TABLE{
#define OPCODE(name_token, type_token, ...)                                                        \
    OpcodeMeta{                                                                                    \
        .name{#name_token},                                                                        \
        .type = type_token,                                                                        \
        .arg_types{__VA_ARGS__},                                                                   \
    },
#include "opcodes.inc"
#undef OPCODE
};

constexpr size_t CalculateNumArgsOf(Opcode op) {
    const auto& arg_types{META_TABLE[static_cast<size_t>(op)].arg_types};
    return std::distance(arg_types.begin(), std::ranges::find(arg_types, Type::Void));
}

constexpr std::array NUM_ARGS{
#define OPCODE(name_token, type_token, ...) CalculateNumArgsOf(Opcode::name_token),
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
    return NUM_ARGS[static_cast<size_t>(op)];
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
