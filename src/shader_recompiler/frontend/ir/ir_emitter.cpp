// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_cast.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

[[noreturn]] static void ThrowInvalidType(Type type) {
    throw InvalidArgument("Invalid type {}", type);
}

U1 IREmitter::Imm1(bool value) const {
    return U1{Value{value}};
}

U8 IREmitter::Imm8(u8 value) const {
    return U8{Value{value}};
}

U16 IREmitter::Imm16(u16 value) const {
    return U16{Value{value}};
}

U32 IREmitter::Imm32(u32 value) const {
    return U32{Value{value}};
}

U32 IREmitter::Imm32(s32 value) const {
    return U32{Value{static_cast<u32>(value)}};
}

U32 IREmitter::Imm32(f32 value) const {
    return U32{Value{Common::BitCast<u32>(value)}};
}

U64 IREmitter::Imm64(u64 value) const {
    return U64{Value{value}};
}

U64 IREmitter::Imm64(f64 value) const {
    return U64{Value{Common::BitCast<u64>(value)}};
}

void IREmitter::Branch(IR::Block* label) {
    Inst(Opcode::Branch, label);
}

void IREmitter::BranchConditional(const U1& cond, IR::Block* true_label, IR::Block* false_label) {
    Inst(Opcode::BranchConditional, cond, true_label, false_label);
}

void IREmitter::Exit() {
    Inst(Opcode::Exit);
}

void IREmitter::Return() {
    Inst(Opcode::Return);
}

void IREmitter::Unreachable() {
    Inst(Opcode::Unreachable);
}

U32 IREmitter::GetReg(IR::Reg reg) {
    return Inst<U32>(Opcode::GetRegister, reg);
}

void IREmitter::SetReg(IR::Reg reg, const U32& value) {
    Inst(Opcode::SetRegister, reg, value);
}

U1 IREmitter::GetPred(IR::Pred pred, bool is_negated) {
    const U1 value{Inst<U1>(Opcode::GetPred, pred)};
    if (is_negated) {
        return Inst<U1>(Opcode::LogicalNot, value);
    } else {
        return value;
    }
}

void IREmitter::SetPred(IR::Pred pred, const U1& value) {
    Inst(Opcode::SetPred, pred, value);
}

U32 IREmitter::GetCbuf(const U32& binding, const U32& byte_offset) {
    return Inst<U32>(Opcode::GetCbuf, binding, byte_offset);
}

U1 IREmitter::GetZFlag() {
    return Inst<U1>(Opcode::GetZFlag);
}

U1 IREmitter::GetSFlag() {
    return Inst<U1>(Opcode::GetSFlag);
}

U1 IREmitter::GetCFlag() {
    return Inst<U1>(Opcode::GetCFlag);
}

U1 IREmitter::GetOFlag() {
    return Inst<U1>(Opcode::GetOFlag);
}

void IREmitter::SetZFlag(const U1& value) {
    Inst(Opcode::SetZFlag, value);
}

void IREmitter::SetSFlag(const U1& value) {
    Inst(Opcode::SetSFlag, value);
}

void IREmitter::SetCFlag(const U1& value) {
    Inst(Opcode::SetCFlag, value);
}

void IREmitter::SetOFlag(const U1& value) {
    Inst(Opcode::SetOFlag, value);
}

U32 IREmitter::GetAttribute(IR::Attribute attribute) {
    return Inst<U32>(Opcode::GetAttribute, attribute);
}

void IREmitter::SetAttribute(IR::Attribute attribute, const U32& value) {
    Inst(Opcode::SetAttribute, attribute, value);
}

U32 IREmitter::WorkgroupIdX() {
    return Inst<U32>(Opcode::WorkgroupIdX);
}

U32 IREmitter::WorkgroupIdY() {
    return Inst<U32>(Opcode::WorkgroupIdY);
}

U32 IREmitter::WorkgroupIdZ() {
    return Inst<U32>(Opcode::WorkgroupIdZ);
}

U32 IREmitter::LocalInvocationIdX() {
    return Inst<U32>(Opcode::LocalInvocationIdX);
}

U32 IREmitter::LocalInvocationIdY() {
    return Inst<U32>(Opcode::LocalInvocationIdY);
}

U32 IREmitter::LocalInvocationIdZ() {
    return Inst<U32>(Opcode::LocalInvocationIdZ);
}

U32 IREmitter::LoadGlobalU8(const U64& address) {
    return Inst<U32>(Opcode::LoadGlobalU8, address);
}

U32 IREmitter::LoadGlobalS8(const U64& address) {
    return Inst<U32>(Opcode::LoadGlobalS8, address);
}

U32 IREmitter::LoadGlobalU16(const U64& address) {
    return Inst<U32>(Opcode::LoadGlobalU16, address);
}

U32 IREmitter::LoadGlobalS16(const U64& address) {
    return Inst<U32>(Opcode::LoadGlobalS16, address);
}

U32 IREmitter::LoadGlobal32(const U64& address) {
    return Inst<U32>(Opcode::LoadGlobal32, address);
}

Value IREmitter::LoadGlobal64(const U64& address) {
    return Inst<Value>(Opcode::LoadGlobal64, address);
}

Value IREmitter::LoadGlobal128(const U64& address) {
    return Inst<Value>(Opcode::LoadGlobal128, address);
}

void IREmitter::WriteGlobalU8(const U64& address, const U32& value) {
    Inst(Opcode::WriteGlobalU8, address, value);
}

void IREmitter::WriteGlobalS8(const U64& address, const U32& value) {
    Inst(Opcode::WriteGlobalS8, address, value);
}

void IREmitter::WriteGlobalU16(const U64& address, const U32& value) {
    Inst(Opcode::WriteGlobalU16, address, value);
}

void IREmitter::WriteGlobalS16(const U64& address, const U32& value) {
    Inst(Opcode::WriteGlobalS16, address, value);
}

void IREmitter::WriteGlobal32(const U64& address, const U32& value) {
    Inst(Opcode::WriteGlobal32, address, value);
}

void IREmitter::WriteGlobal64(const U64& address, const IR::Value& vector) {
    Inst(Opcode::WriteGlobal64, address, vector);
}

void IREmitter::WriteGlobal128(const U64& address, const IR::Value& vector) {
    Inst(Opcode::WriteGlobal128, address, vector);
}

U1 IREmitter::GetZeroFromOp(const Value& op) {
    return Inst<U1>(Opcode::GetZeroFromOp, op);
}

U1 IREmitter::GetSignFromOp(const Value& op) {
    return Inst<U1>(Opcode::GetSignFromOp, op);
}

U1 IREmitter::GetCarryFromOp(const Value& op) {
    return Inst<U1>(Opcode::GetCarryFromOp, op);
}

U1 IREmitter::GetOverflowFromOp(const Value& op) {
    return Inst<U1>(Opcode::GetOverflowFromOp, op);
}

U16U32U64 IREmitter::FPAdd(const U16U32U64& a, const U16U32U64& b, FpControl control) {
    if (a.Type() != a.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", a.Type(), b.Type());
    }
    switch (a.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPAdd16, Flags{control}, a, b);
    case Type::U32:
        return Inst<U32>(Opcode::FPAdd32, Flags{control}, a, b);
    case Type::U64:
        return Inst<U64>(Opcode::FPAdd64, Flags{control}, a, b);
    default:
        ThrowInvalidType(a.Type());
    }
}

Value IREmitter::CompositeConstruct(const UAny& e1, const UAny& e2) {
    if (e1.Type() != e2.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", e1.Type(), e2.Type());
    }
    return Inst(Opcode::CompositeConstruct2, e1, e2);
}

Value IREmitter::CompositeConstruct(const UAny& e1, const UAny& e2, const UAny& e3) {
    if (e1.Type() != e2.Type() || e1.Type() != e3.Type()) {
        throw InvalidArgument("Mismatching types {}, {}, and {}", e1.Type(), e2.Type(), e3.Type());
    }
    return Inst(Opcode::CompositeConstruct3, e1, e2, e3);
}

Value IREmitter::CompositeConstruct(const UAny& e1, const UAny& e2, const UAny& e3,
                                    const UAny& e4) {
    if (e1.Type() != e2.Type() || e1.Type() != e3.Type() || e1.Type() != e4.Type()) {
        throw InvalidArgument("Mismatching types {}, {}, {}, and {}", e1.Type(), e2.Type(),
                              e3.Type(), e4.Type());
    }
    return Inst(Opcode::CompositeConstruct4, e1, e2, e3, e4);
}

UAny IREmitter::CompositeExtract(const Value& vector, size_t element) {
    if (element >= 4) {
        throw InvalidArgument("Out of bounds element {}", element);
    }
    return Inst<UAny>(Opcode::CompositeExtract, vector, Imm32(static_cast<u32>(element)));
}

UAny IREmitter::Select(const U1& condition, const UAny& true_value, const UAny& false_value) {
    if (true_value.Type() != false_value.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", true_value.Type(), false_value.Type());
    }
    switch (true_value.Type()) {
    case Type::U8:
        return Inst<UAny>(Opcode::Select8, condition, true_value, false_value);
    case Type::U16:
        return Inst<UAny>(Opcode::Select16, condition, true_value, false_value);
    case Type::U32:
        return Inst<UAny>(Opcode::Select32, condition, true_value, false_value);
    case Type::U64:
        return Inst<UAny>(Opcode::Select64, condition, true_value, false_value);
    default:
        throw InvalidArgument("Invalid type {}", true_value.Type());
    }
}

U64 IREmitter::PackUint2x32(const Value& vector) {
    return Inst<U64>(Opcode::PackUint2x32, vector);
}

Value IREmitter::UnpackUint2x32(const U64& value) {
    return Inst<Value>(Opcode::UnpackUint2x32, value);
}

U32 IREmitter::PackFloat2x16(const Value& vector) {
    return Inst<U32>(Opcode::PackFloat2x16, vector);
}

Value IREmitter::UnpackFloat2x16(const U32& value) {
    return Inst<Value>(Opcode::UnpackFloat2x16, value);
}

U64 IREmitter::PackDouble2x32(const Value& vector) {
    return Inst<U64>(Opcode::PackDouble2x32, vector);
}

Value IREmitter::UnpackDouble2x32(const U64& value) {
    return Inst<Value>(Opcode::UnpackDouble2x32, value);
}

U16U32U64 IREmitter::FPMul(const U16U32U64& a, const U16U32U64& b, FpControl control) {
    if (a.Type() != b.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", a.Type(), b.Type());
    }
    switch (a.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPMul16, Flags{control}, a, b);
    case Type::U32:
        return Inst<U32>(Opcode::FPMul32, Flags{control}, a, b);
    case Type::U64:
        return Inst<U64>(Opcode::FPMul64, Flags{control}, a, b);
    default:
        ThrowInvalidType(a.Type());
    }
}

U16U32U64 IREmitter::FPFma(const U16U32U64& a, const U16U32U64& b, const U16U32U64& c,
                           FpControl control) {
    if (a.Type() != b.Type() || a.Type() != c.Type()) {
        throw InvalidArgument("Mismatching types {}, {}, and {}", a.Type(), b.Type(), c.Type());
    }
    switch (a.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPFma16, Flags{control}, a, b, c);
    case Type::U32:
        return Inst<U32>(Opcode::FPFma32, Flags{control}, a, b, c);
    case Type::U64:
        return Inst<U64>(Opcode::FPFma64, Flags{control}, a, b, c);
    default:
        ThrowInvalidType(a.Type());
    }
}

U16U32U64 IREmitter::FPAbs(const U16U32U64& value) {
    switch (value.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPAbs16, value);
    case Type::U32:
        return Inst<U32>(Opcode::FPAbs32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPAbs64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U16U32U64 IREmitter::FPNeg(const U16U32U64& value) {
    switch (value.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPNeg16, value);
    case Type::U32:
        return Inst<U32>(Opcode::FPNeg32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPNeg64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U16U32U64 IREmitter::FPAbsNeg(const U16U32U64& value, bool abs, bool neg) {
    U16U32U64 result{value};
    if (abs) {
        result = FPAbs(value);
    }
    if (neg) {
        result = FPNeg(value);
    }
    return result;
}

U32 IREmitter::FPCosNotReduced(const U32& value) {
    return Inst<U32>(Opcode::FPCosNotReduced, value);
}

U32 IREmitter::FPExp2NotReduced(const U32& value) {
    return Inst<U32>(Opcode::FPExp2NotReduced, value);
}

U32 IREmitter::FPLog2(const U32& value) {
    return Inst<U32>(Opcode::FPLog2, value);
}

U32U64 IREmitter::FPRecip(const U32U64& value) {
    switch (value.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::FPRecip32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPRecip64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U32U64 IREmitter::FPRecipSqrt(const U32U64& value) {
    switch (value.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::FPRecipSqrt32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPRecipSqrt64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U32 IREmitter::FPSinNotReduced(const U32& value) {
    return Inst<U32>(Opcode::FPSinNotReduced, value);
}

U32 IREmitter::FPSqrt(const U32& value) {
    return Inst<U32>(Opcode::FPSqrt, value);
}

U16U32U64 IREmitter::FPSaturate(const U16U32U64& value) {
    switch (value.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPSaturate16, value);
    case Type::U32:
        return Inst<U32>(Opcode::FPSaturate32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPSaturate64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U16U32U64 IREmitter::FPRoundEven(const U16U32U64& value) {
    switch (value.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPRoundEven16, value);
    case Type::U32:
        return Inst<U32>(Opcode::FPRoundEven32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPRoundEven64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U16U32U64 IREmitter::FPFloor(const U16U32U64& value) {
    switch (value.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPFloor16, value);
    case Type::U32:
        return Inst<U32>(Opcode::FPFloor32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPFloor64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U16U32U64 IREmitter::FPCeil(const U16U32U64& value) {
    switch (value.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPCeil16, value);
    case Type::U32:
        return Inst<U32>(Opcode::FPCeil32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPCeil64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U16U32U64 IREmitter::FPTrunc(const U16U32U64& value) {
    switch (value.Type()) {
    case Type::U16:
        return Inst<U16>(Opcode::FPTrunc16, value);
    case Type::U32:
        return Inst<U32>(Opcode::FPTrunc32, value);
    case Type::U64:
        return Inst<U64>(Opcode::FPTrunc64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U32U64 IREmitter::IAdd(const U32U64& a, const U32U64& b) {
    if (a.Type() != b.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", a.Type(), b.Type());
    }
    switch (a.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::IAdd32, a, b);
    case Type::U64:
        return Inst<U64>(Opcode::IAdd64, a, b);
    default:
        ThrowInvalidType(a.Type());
    }
}

U32U64 IREmitter::ISub(const U32U64& a, const U32U64& b) {
    if (a.Type() != b.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", a.Type(), b.Type());
    }
    switch (a.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::ISub32, a, b);
    case Type::U64:
        return Inst<U64>(Opcode::ISub64, a, b);
    default:
        ThrowInvalidType(a.Type());
    }
}

U32 IREmitter::IMul(const U32& a, const U32& b) {
    return Inst<U32>(Opcode::IMul32, a, b);
}

U32 IREmitter::INeg(const U32& value) {
    return Inst<U32>(Opcode::INeg32, value);
}

U32 IREmitter::IAbs(const U32& value) {
    return Inst<U32>(Opcode::IAbs32, value);
}

U32 IREmitter::ShiftLeftLogical(const U32& base, const U32& shift) {
    return Inst<U32>(Opcode::ShiftLeftLogical32, base, shift);
}

U32 IREmitter::ShiftRightLogical(const U32& base, const U32& shift) {
    return Inst<U32>(Opcode::ShiftRightLogical32, base, shift);
}

U32 IREmitter::ShiftRightArithmetic(const U32& base, const U32& shift) {
    return Inst<U32>(Opcode::ShiftRightArithmetic32, base, shift);
}

U32 IREmitter::BitwiseAnd(const U32& a, const U32& b) {
    return Inst<U32>(Opcode::BitwiseAnd32, a, b);
}

U32 IREmitter::BitwiseOr(const U32& a, const U32& b) {
    return Inst<U32>(Opcode::BitwiseOr32, a, b);
}

U32 IREmitter::BitwiseXor(const U32& a, const U32& b) {
    return Inst<U32>(Opcode::BitwiseXor32, a, b);
}

U32 IREmitter::BitFieldInsert(const U32& base, const U32& insert, const U32& offset,
                              const U32& count) {
    return Inst<U32>(Opcode::BitFieldInsert, base, insert, offset, count);
}

U32 IREmitter::BitFieldExtract(const U32& base, const U32& offset, const U32& count,
                               bool is_signed) {
    return Inst<U32>(is_signed ? Opcode::BitFieldSExtract : Opcode::BitFieldUExtract, base, offset,
                     count);
}

U1 IREmitter::ILessThan(const U32& lhs, const U32& rhs, bool is_signed) {
    return Inst<U1>(is_signed ? Opcode::SLessThan : Opcode::ULessThan, lhs, rhs);
}

U1 IREmitter::IEqual(const U32& lhs, const U32& rhs) {
    return Inst<U1>(Opcode::IEqual, lhs, rhs);
}

U1 IREmitter::ILessThanEqual(const U32& lhs, const U32& rhs, bool is_signed) {
    return Inst<U1>(is_signed ? Opcode::SLessThanEqual : Opcode::ULessThanEqual, lhs, rhs);
}

U1 IREmitter::IGreaterThan(const U32& lhs, const U32& rhs, bool is_signed) {
    return Inst<U1>(is_signed ? Opcode::SGreaterThan : Opcode::UGreaterThan, lhs, rhs);
}

U1 IREmitter::INotEqual(const U32& lhs, const U32& rhs) {
    return Inst<U1>(Opcode::INotEqual, lhs, rhs);
}

U1 IREmitter::IGreaterThanEqual(const U32& lhs, const U32& rhs, bool is_signed) {
    return Inst<U1>(is_signed ? Opcode::SGreaterThanEqual : Opcode::UGreaterThanEqual, lhs, rhs);
}

U1 IREmitter::LogicalOr(const U1& a, const U1& b) {
    return Inst<U1>(Opcode::LogicalOr, a, b);
}

U1 IREmitter::LogicalAnd(const U1& a, const U1& b) {
    return Inst<U1>(Opcode::LogicalAnd, a, b);
}

U1 IREmitter::LogicalXor(const U1& a, const U1& b) {
    return Inst<U1>(Opcode::LogicalXor, a, b);
}

U1 IREmitter::LogicalNot(const U1& value) {
    return Inst<U1>(Opcode::LogicalNot, value);
}

U32U64 IREmitter::ConvertFToS(size_t bitsize, const U16U32U64& value) {
    switch (bitsize) {
    case 16:
        switch (value.Type()) {
        case Type::U16:
            return Inst<U32>(Opcode::ConvertS16F16, value);
        case Type::U32:
            return Inst<U32>(Opcode::ConvertS16F32, value);
        case Type::U64:
            return Inst<U32>(Opcode::ConvertS16F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    case 32:
        switch (value.Type()) {
        case Type::U16:
            return Inst<U32>(Opcode::ConvertS32F16, value);
        case Type::U32:
            return Inst<U32>(Opcode::ConvertS32F32, value);
        case Type::U64:
            return Inst<U32>(Opcode::ConvertS32F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    case 64:
        switch (value.Type()) {
        case Type::U16:
            return Inst<U64>(Opcode::ConvertS64F16, value);
        case Type::U32:
            return Inst<U64>(Opcode::ConvertS64F32, value);
        case Type::U64:
            return Inst<U64>(Opcode::ConvertS64F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    default:
        throw InvalidArgument("Invalid destination bitsize {}", bitsize);
    }
}

U32U64 IREmitter::ConvertFToU(size_t bitsize, const U16U32U64& value) {
    switch (bitsize) {
    case 16:
        switch (value.Type()) {
        case Type::U16:
            return Inst<U32>(Opcode::ConvertU16F16, value);
        case Type::U32:
            return Inst<U32>(Opcode::ConvertU16F32, value);
        case Type::U64:
            return Inst<U32>(Opcode::ConvertU16F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    case 32:
        switch (value.Type()) {
        case Type::U16:
            return Inst<U32>(Opcode::ConvertU32F16, value);
        case Type::U32:
            return Inst<U32>(Opcode::ConvertU32F32, value);
        case Type::U64:
            return Inst<U32>(Opcode::ConvertU32F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    case 64:
        switch (value.Type()) {
        case Type::U16:
            return Inst<U64>(Opcode::ConvertU64F16, value);
        case Type::U32:
            return Inst<U64>(Opcode::ConvertU64F32, value);
        case Type::U64:
            return Inst<U64>(Opcode::ConvertU64F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    default:
        throw InvalidArgument("Invalid destination bitsize {}", bitsize);
    }
}

U32U64 IREmitter::ConvertFToI(size_t bitsize, bool is_signed, const U16U32U64& value) {
    if (is_signed) {
        return ConvertFToS(bitsize, value);
    } else {
        return ConvertFToU(bitsize, value);
    }
}

U32U64 IREmitter::ConvertU(size_t result_bitsize, const U32U64& value) {
    switch (result_bitsize) {
    case 32:
        switch (value.Type()) {
        case Type::U32:
            // Nothing to do
            return value;
        case Type::U64:
            return Inst<U32>(Opcode::ConvertU32U64, value);
        default:
            break;
        }
        break;
    case 64:
        switch (value.Type()) {
        case Type::U32:
            // Nothing to do
            return value;
        case Type::U64:
            return Inst<U64>(Opcode::ConvertU64U32, value);
        default:
            break;
        }
    }
    throw NotImplementedException("Conversion from {} to {} bits", value.Type(), result_bitsize);
}

} // namespace Shader::IR
