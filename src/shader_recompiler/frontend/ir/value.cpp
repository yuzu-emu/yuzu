// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/ir/opcode.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

Value::Value(IR::Inst* value) noexcept : type{Type::Opaque}, inst{value} {}

Value::Value(IR::Block* value) noexcept : type{Type::Label}, label{value} {}

Value::Value(IR::Reg value) noexcept : type{Type::Reg}, reg{value} {}

Value::Value(IR::Pred value) noexcept : type{Type::Pred}, pred{value} {}

Value::Value(IR::Attribute value) noexcept : type{Type::Attribute}, attribute{value} {}

Value::Value(bool value) noexcept : type{Type::U1}, imm_u1{value} {}

Value::Value(u8 value) noexcept : type{Type::U8}, imm_u8{value} {}

Value::Value(u16 value) noexcept : type{Type::U16}, imm_u16{value} {}

Value::Value(u32 value) noexcept : type{Type::U32}, imm_u32{value} {}

Value::Value(f32 value) noexcept : type{Type::F32}, imm_f32{value} {}

Value::Value(u64 value) noexcept : type{Type::U64}, imm_u64{value} {}

Value::Value(f64 value) noexcept : type{Type::F64}, imm_f64{value} {}

bool Value::IsIdentity() const noexcept {
    return type == Type::Opaque && inst->Opcode() == Opcode::Identity;
}

bool Value::IsEmpty() const noexcept {
    return type == Type::Void;
}

bool Value::IsImmediate() const noexcept {
    if (IsIdentity()) {
        return inst->Arg(0).IsImmediate();
    }
    return type != Type::Opaque;
}

bool Value::IsLabel() const noexcept {
    return type == Type::Label;
}

IR::Type Value::Type() const noexcept {
    if (IsIdentity()) {
        return inst->Arg(0).Type();
    }
    if (type == Type::Opaque) {
        return inst->Type();
    }
    return type;
}

IR::Inst* Value::Inst() const {
    ValidateAccess(Type::Opaque);
    return inst;
}

IR::Block* Value::Label() const {
    ValidateAccess(Type::Label);
    return label;
}

IR::Inst* Value::InstRecursive() const {
    ValidateAccess(Type::Opaque);
    if (IsIdentity()) {
        return inst->Arg(0).InstRecursive();
    }
    return inst;
}

IR::Reg Value::Reg() const {
    ValidateAccess(Type::Reg);
    return reg;
}

IR::Pred Value::Pred() const {
    ValidateAccess(Type::Pred);
    return pred;
}

IR::Attribute Value::Attribute() const {
    ValidateAccess(Type::Attribute);
    return attribute;
}

bool Value::U1() const {
    if (IsIdentity()) {
        return inst->Arg(0).U1();
    }
    ValidateAccess(Type::U1);
    return imm_u1;
}

u8 Value::U8() const {
    if (IsIdentity()) {
        return inst->Arg(0).U8();
    }
    ValidateAccess(Type::U8);
    return imm_u8;
}

u16 Value::U16() const {
    if (IsIdentity()) {
        return inst->Arg(0).U16();
    }
    ValidateAccess(Type::U16);
    return imm_u16;
}

u32 Value::U32() const {
    if (IsIdentity()) {
        return inst->Arg(0).U32();
    }
    ValidateAccess(Type::U32);
    return imm_u32;
}

f32 Value::F32() const {
    if (IsIdentity()) {
        return inst->Arg(0).F32();
    }
    ValidateAccess(Type::F32);
    return imm_f32;
}

u64 Value::U64() const {
    if (IsIdentity()) {
        return inst->Arg(0).U64();
    }
    ValidateAccess(Type::U64);
    return imm_u64;
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
    case Type::Label:
        return label == other.label;
    case Type::Reg:
        return reg == other.reg;
    case Type::Pred:
        return pred == other.pred;
    case Type::Attribute:
        return attribute == other.attribute;
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

void Value::ValidateAccess(IR::Type expected) const {
    if (type != expected) {
        throw LogicError("Reading {} out of {}", expected, type);
    }
}

} // namespace Shader::IR
