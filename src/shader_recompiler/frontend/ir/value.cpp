// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/ir/opcodes.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

Value::Value(IR::Inst* value) noexcept : type{Type::Opaque}, inst{value} {}

Value::Value(IR::Reg value) noexcept : type{Type::Reg}, reg{value} {}

Value::Value(IR::Pred value) noexcept : type{Type::Pred}, pred{value} {}

Value::Value(IR::Attribute value) noexcept : type{Type::Attribute}, attribute{value} {}

Value::Value(IR::Patch value) noexcept : type{Type::Patch}, patch{value} {}

Value::Value(bool value) noexcept : type{Type::U1}, imm_u1{value} {}

Value::Value(u8 value) noexcept : type{Type::U8}, imm_u8{value} {}

Value::Value(u16 value) noexcept : type{Type::U16}, imm_u16{value} {}

Value::Value(u32 value) noexcept : type{Type::U32}, imm_u32{value} {}

Value::Value(f32 value) noexcept : type{Type::F32}, imm_f32{value} {}

Value::Value(u64 value) noexcept : type{Type::U64}, imm_u64{value} {}

Value::Value(f64 value) noexcept : type{Type::F64}, imm_f64{value} {}

IR::Type Value::Type() const noexcept {
    if (IsPhi()) {
        // The type of a phi node is stored in its flags
        return inst->Flags<IR::Type>();
    }
    if (IsIdentity()) {
        return inst->Arg(0).Type();
    }
    if (type == Type::Opaque) {
        return inst->Type();
    }
    return type;
}

bool Value::operator==(const Value& other) const {
    if (type != other.type) {
        return false;
    }
    switch (type) {
    case Type::Void:
        return true;
    case Type::Opaque:
        return inst == other.inst;
    case Type::Reg:
        return reg == other.reg;
    case Type::Pred:
        return pred == other.pred;
    case Type::Attribute:
        return attribute == other.attribute;
    case Type::Patch:
        return patch == other.patch;
    case Type::U1:
        return imm_u1 == other.imm_u1;
    case Type::U8:
        return imm_u8 == other.imm_u8;
    case Type::U16:
    case Type::F16:
        return imm_u16 == other.imm_u16;
    case Type::U32:
    case Type::F32:
        return imm_u32 == other.imm_u32;
    case Type::U64:
    case Type::F64:
        return imm_u64 == other.imm_u64;
    case Type::U32x2:
    case Type::U32x3:
    case Type::U32x4:
    case Type::F16x2:
    case Type::F16x3:
    case Type::F16x4:
    case Type::F32x2:
    case Type::F32x3:
    case Type::F32x4:
    case Type::F64x2:
    case Type::F64x3:
    case Type::F64x4:
        break;
    }
    throw LogicError("Invalid type {}", type);
}

bool Value::operator!=(const Value& other) const {
    return !operator==(other);
}

} // namespace Shader::IR
