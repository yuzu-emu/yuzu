// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/attribute.h"
#include "shader_recompiler/frontend/ir/pred.h"
#include "shader_recompiler/frontend/ir/reg.h"
#include "shader_recompiler/frontend/ir/patch.h"
#include "shader_recompiler/frontend/ir/type.h"

namespace Shader::IR {

class Block;
class Inst;

class Value {
public:
    Value() noexcept : type{IR::Type::Void}, inst{nullptr} {}
    explicit Value(IR::Inst* value) noexcept;
    explicit Value(IR::Block* value) noexcept;
    explicit Value(IR::Reg value) noexcept;
    explicit Value(IR::Pred value) noexcept;
    explicit Value(IR::Attribute value) noexcept;
    explicit Value(IR::Patch value) noexcept;
    explicit Value(bool value) noexcept;
    explicit Value(u8 value) noexcept;
    explicit Value(u16 value) noexcept;
    explicit Value(u32 value) noexcept;
    explicit Value(f32 value) noexcept;
    explicit Value(u64 value) noexcept;
    explicit Value(f64 value) noexcept;

    [[nodiscard]] bool IsIdentity() const noexcept;
    [[nodiscard]] bool IsPhi() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] bool IsImmediate() const noexcept;
    [[nodiscard]] bool IsLabel() const noexcept;
    [[nodiscard]] IR::Type Type() const noexcept;

    [[nodiscard]] IR::Inst* Inst() const;
    [[nodiscard]] IR::Block* Label() const;
    [[nodiscard]] IR::Inst* InstRecursive() const;
    [[nodiscard]] IR::Value Resolve() const;
    [[nodiscard]] IR::Reg Reg() const;
    [[nodiscard]] IR::Pred Pred() const;
    [[nodiscard]] IR::Attribute Attribute() const;
    [[nodiscard]] IR::Patch Patch() const;
    [[nodiscard]] bool U1() const;
    [[nodiscard]] u8 U8() const;
    [[nodiscard]] u16 U16() const;
    [[nodiscard]] u32 U32() const;
    [[nodiscard]] f32 F32() const;
    [[nodiscard]] u64 U64() const;
    [[nodiscard]] f64 F64() const;

    [[nodiscard]] bool operator==(const Value& other) const;
    [[nodiscard]] bool operator!=(const Value& other) const;

private:
    void ValidateAccess(IR::Type expected) const;

    IR::Type type;
    union {
        IR::Inst* inst;
        IR::Block* label;
        IR::Reg reg;
        IR::Pred pred;
        IR::Attribute attribute;
        IR::Patch patch;
        bool imm_u1;
        u8 imm_u8;
        u16 imm_u16;
        u32 imm_u32;
        f32 imm_f32;
        u64 imm_u64;
        f64 imm_f64;
    };
};
static_assert(std::is_trivially_copyable_v<Value>);

template <IR::Type type_>
class TypedValue : public Value {
public:
    TypedValue() = default;

    template <IR::Type other_type>
    requires((other_type & type_) != IR::Type::Void) explicit(false)
        TypedValue(const TypedValue<other_type>& value)
        : Value(value) {}

    explicit TypedValue(const Value& value) : Value(value) {
        if ((value.Type() & type_) == IR::Type::Void) {
            throw InvalidArgument("Incompatible types {} and {}", type_, value.Type());
        }
    }

    explicit TypedValue(IR::Inst* inst_) : TypedValue(Value(inst_)) {}
};

using U1 = TypedValue<Type::U1>;
using U8 = TypedValue<Type::U8>;
using U16 = TypedValue<Type::U16>;
using U32 = TypedValue<Type::U32>;
using U64 = TypedValue<Type::U64>;
using F16 = TypedValue<Type::F16>;
using F32 = TypedValue<Type::F32>;
using F64 = TypedValue<Type::F64>;
using U32U64 = TypedValue<Type::U32 | Type::U64>;
using F32F64 = TypedValue<Type::F32 | Type::F64>;
using U16U32U64 = TypedValue<Type::U16 | Type::U32 | Type::U64>;
using F16F32F64 = TypedValue<Type::F16 | Type::F32 | Type::F64>;
using UAny = TypedValue<Type::U8 | Type::U16 | Type::U32 | Type::U64>;

} // namespace Shader::IR
