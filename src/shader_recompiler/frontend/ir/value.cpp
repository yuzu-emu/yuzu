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

Value::Value(u64 value) noexcept : type{Type::U64}, imm_u64{value} {}

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
    ValidateAccess(Type::U1);
    return imm_u1;
}

u8 Value::U8() const {
    ValidateAccess(Type::U8);
    return imm_u8;
}

u16 Value::U16() const {
    ValidateAccess(Type::U16);
    return imm_u16;
}

u32 Value::U32() const {
    ValidateAccess(Type::U32);
    return imm_u32;
}

u64 Value::U64() const {
    ValidateAccess(Type::U64);
    return imm_u64;
}

void Value::ValidateAccess(IR::Type expected) const {
    if (type != expected) {
        throw LogicError("Reading {} out of {}", expected, type);
    }
}

} // namespace Shader::IR
