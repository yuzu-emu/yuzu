// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/bit_cast.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {
namespace {
[[noreturn]] void ThrowInvalidType(Type type) {
    throw InvalidArgument("Invalid type {}", type);
}

Value MakeLodClampPair(IREmitter& ir, const F32& bias_lod, const F32& lod_clamp) {
    if (!bias_lod.IsEmpty() && !lod_clamp.IsEmpty()) {
        return ir.CompositeConstruct(bias_lod, lod_clamp);
    } else if (!bias_lod.IsEmpty()) {
        return bias_lod;
    } else if (!lod_clamp.IsEmpty()) {
        return lod_clamp;
    } else {
        return Value{};
    }
}
} // Anonymous namespace

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

F32 IREmitter::Imm32(f32 value) const {
    return F32{Value{value}};
}

U64 IREmitter::Imm64(u64 value) const {
    return U64{Value{value}};
}

U64 IREmitter::Imm64(s64 value) const {
    return U64{Value{static_cast<u64>(value)}};
}

F64 IREmitter::Imm64(f64 value) const {
    return F64{Value{value}};
}

void IREmitter::Branch(Block* label) {
    label->AddImmediatePredecessor(block);
    block->SetBranch(label);
    Inst(Opcode::Branch, label);
}

void IREmitter::BranchConditional(const U1& condition, Block* true_label, Block* false_label) {
    block->SetBranches(IR::Condition{true}, true_label, false_label);
    true_label->AddImmediatePredecessor(block);
    false_label->AddImmediatePredecessor(block);
    Inst(Opcode::BranchConditional, condition, true_label, false_label);
}

void IREmitter::LoopMerge(Block* merge_block, Block* continue_target) {
    Inst(Opcode::LoopMerge, merge_block, continue_target);
}

void IREmitter::SelectionMerge(Block* merge_block) {
    Inst(Opcode::SelectionMerge, merge_block);
}

void IREmitter::Return() {
    Inst(Opcode::Return);
}

void IREmitter::DemoteToHelperInvocation(Block* continue_label) {
    block->SetBranch(continue_label);
    continue_label->AddImmediatePredecessor(block);
    Inst(Opcode::DemoteToHelperInvocation, continue_label);
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

U1 IREmitter::GetGotoVariable(u32 id) {
    return Inst<U1>(Opcode::GetGotoVariable, id);
}

void IREmitter::SetGotoVariable(u32 id, const U1& value) {
    Inst(Opcode::SetGotoVariable, id, value);
}

void IREmitter::SetPred(IR::Pred pred, const U1& value) {
    Inst(Opcode::SetPred, pred, value);
}

U32 IREmitter::GetCbuf(const U32& binding, const U32& byte_offset) {
    return Inst<U32>(Opcode::GetCbufU32, binding, byte_offset);
}

UAny IREmitter::GetCbuf(const U32& binding, const U32& byte_offset, size_t bitsize,
                        bool is_signed) {
    switch (bitsize) {
    case 8:
        return Inst<U32>(is_signed ? Opcode::GetCbufS8 : Opcode::GetCbufU8, binding, byte_offset);
    case 16:
        return Inst<U32>(is_signed ? Opcode::GetCbufS16 : Opcode::GetCbufU16, binding, byte_offset);
    case 32:
        return Inst<U32>(Opcode::GetCbufU32, binding, byte_offset);
    case 64:
        return Inst<U64>(Opcode::GetCbufU64, binding, byte_offset);
    default:
        throw InvalidArgument("Invalid bit size {}", bitsize);
    }
}

F32 IREmitter::GetFloatCbuf(const U32& binding, const U32& byte_offset) {
    return Inst<F32>(Opcode::GetCbufF32, binding, byte_offset);
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

static U1 GetFlowTest(IREmitter& ir, FlowTest flow_test) {
    switch (flow_test) {
    case FlowTest::F:
        return ir.Imm1(false);
    case FlowTest::LT:
        return ir.LogicalXor(ir.LogicalAnd(ir.GetSFlag(), ir.LogicalNot(ir.GetZFlag())),
                             ir.GetOFlag());
    case FlowTest::EQ:
        return ir.LogicalAnd(ir.LogicalNot(ir.GetSFlag()), ir.GetZFlag());
    case FlowTest::LE:
        return ir.LogicalXor(ir.GetSFlag(), ir.LogicalOr(ir.GetZFlag(), ir.GetOFlag()));
    case FlowTest::GT:
        return ir.LogicalAnd(ir.LogicalXor(ir.LogicalNot(ir.GetSFlag()), ir.GetOFlag()),
                             ir.LogicalNot(ir.GetZFlag()));
    case FlowTest::NE:
        return ir.LogicalNot(ir.GetZFlag());
    case FlowTest::GE:
        return ir.LogicalNot(ir.LogicalXor(ir.GetSFlag(), ir.GetOFlag()));
    case FlowTest::NUM:
        return ir.LogicalOr(ir.LogicalNot(ir.GetSFlag()), ir.LogicalNot(ir.GetZFlag()));
    case FlowTest::NaN:
        return ir.LogicalAnd(ir.GetSFlag(), ir.GetZFlag());
    case FlowTest::LTU:
        return ir.LogicalXor(ir.GetSFlag(), ir.GetOFlag());
    case FlowTest::EQU:
        return ir.GetZFlag();
    case FlowTest::LEU:
        return ir.LogicalOr(ir.LogicalXor(ir.GetSFlag(), ir.GetOFlag()), ir.GetZFlag());
    case FlowTest::GTU:
        return ir.LogicalXor(ir.LogicalNot(ir.GetSFlag()),
                             ir.LogicalOr(ir.GetZFlag(), ir.GetOFlag()));
    case FlowTest::NEU:
        return ir.LogicalOr(ir.GetSFlag(), ir.LogicalNot(ir.GetZFlag()));
    case FlowTest::GEU:
        return ir.LogicalXor(ir.LogicalOr(ir.LogicalNot(ir.GetSFlag()), ir.GetZFlag()),
                             ir.GetOFlag());
    case FlowTest::T:
        return ir.Imm1(true);
    case FlowTest::OFF:
        return ir.LogicalNot(ir.GetOFlag());
    case FlowTest::LO:
        return ir.LogicalNot(ir.GetCFlag());
    case FlowTest::SFF:
        return ir.LogicalNot(ir.GetSFlag());
    case FlowTest::LS:
        return ir.LogicalOr(ir.GetZFlag(), ir.LogicalNot(ir.GetCFlag()));
    case FlowTest::HI:
        return ir.LogicalAnd(ir.GetCFlag(), ir.LogicalNot(ir.GetZFlag()));
    case FlowTest::SFT:
        return ir.GetSFlag();
    case FlowTest::HS:
        return ir.GetCFlag();
    case FlowTest::OFT:
        return ir.GetOFlag();
    case FlowTest::RLE:
        return ir.LogicalOr(ir.GetSFlag(), ir.GetZFlag());
    case FlowTest::RGT:
        return ir.LogicalAnd(ir.LogicalNot(ir.GetSFlag()), ir.LogicalNot(ir.GetZFlag()));
    default:
        throw NotImplementedException("Flow test {}", flow_test);
    }
}

U1 IREmitter::Condition(IR::Condition cond) {
    const FlowTest flow_test{cond.FlowTest()};
    const auto [pred, is_negated]{cond.Pred()};
    return LogicalAnd(GetPred(pred, is_negated), GetFlowTest(*this, flow_test));
}

U1 IREmitter::GetFlowTestResult(FlowTest test) {
    return GetFlowTest(*this, test);
}

F32 IREmitter::GetAttribute(IR::Attribute attribute) {
    return Inst<F32>(Opcode::GetAttribute, attribute);
}

void IREmitter::SetAttribute(IR::Attribute attribute, const F32& value) {
    Inst(Opcode::SetAttribute, attribute, value);
}

void IREmitter::SetFragColor(u32 index, u32 component, const F32& value) {
    Inst(Opcode::SetFragColor, Imm32(index), Imm32(component), value);
}

void IREmitter::SetFragDepth(const F32& value) {
    Inst(Opcode::SetFragDepth, value);
}

U32 IREmitter::WorkgroupIdX() {
    return U32{CompositeExtract(Inst(Opcode::WorkgroupId), 0)};
}

U32 IREmitter::WorkgroupIdY() {
    return U32{CompositeExtract(Inst(Opcode::WorkgroupId), 1)};
}

U32 IREmitter::WorkgroupIdZ() {
    return U32{CompositeExtract(Inst(Opcode::WorkgroupId), 2)};
}

U32 IREmitter::LocalInvocationIdX() {
    return U32{CompositeExtract(Inst(Opcode::LocalInvocationId), 0)};
}

U32 IREmitter::LocalInvocationIdY() {
    return U32{CompositeExtract(Inst(Opcode::LocalInvocationId), 1)};
}

U32 IREmitter::LocalInvocationIdZ() {
    return U32{CompositeExtract(Inst(Opcode::LocalInvocationId), 2)};
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

U1 IREmitter::GetSparseFromOp(const Value& op) {
    return Inst<U1>(Opcode::GetSparseFromOp, op);
}

F16F32F64 IREmitter::FPAdd(const F16F32F64& a, const F16F32F64& b, FpControl control) {
    if (a.Type() != b.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", a.Type(), b.Type());
    }
    switch (a.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPAdd16, Flags{control}, a, b);
    case Type::F32:
        return Inst<F32>(Opcode::FPAdd32, Flags{control}, a, b);
    case Type::F64:
        return Inst<F64>(Opcode::FPAdd64, Flags{control}, a, b);
    default:
        ThrowInvalidType(a.Type());
    }
}

Value IREmitter::CompositeConstruct(const Value& e1, const Value& e2) {
    if (e1.Type() != e2.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", e1.Type(), e2.Type());
    }
    switch (e1.Type()) {
    case Type::U32:
        return Inst(Opcode::CompositeConstructU32x2, e1, e2);
    case Type::F16:
        return Inst(Opcode::CompositeConstructF16x2, e1, e2);
    case Type::F32:
        return Inst(Opcode::CompositeConstructF32x2, e1, e2);
    case Type::F64:
        return Inst(Opcode::CompositeConstructF64x2, e1, e2);
    default:
        ThrowInvalidType(e1.Type());
    }
}

Value IREmitter::CompositeConstruct(const Value& e1, const Value& e2, const Value& e3) {
    if (e1.Type() != e2.Type() || e1.Type() != e3.Type()) {
        throw InvalidArgument("Mismatching types {}, {}, and {}", e1.Type(), e2.Type(), e3.Type());
    }
    switch (e1.Type()) {
    case Type::U32:
        return Inst(Opcode::CompositeConstructU32x3, e1, e2, e3);
    case Type::F16:
        return Inst(Opcode::CompositeConstructF16x3, e1, e2, e3);
    case Type::F32:
        return Inst(Opcode::CompositeConstructF32x3, e1, e2, e3);
    case Type::F64:
        return Inst(Opcode::CompositeConstructF64x3, e1, e2, e3);
    default:
        ThrowInvalidType(e1.Type());
    }
}

Value IREmitter::CompositeConstruct(const Value& e1, const Value& e2, const Value& e3,
                                    const Value& e4) {
    if (e1.Type() != e2.Type() || e1.Type() != e3.Type() || e1.Type() != e4.Type()) {
        throw InvalidArgument("Mismatching types {}, {}, {}, and {}", e1.Type(), e2.Type(),
                              e3.Type(), e4.Type());
    }
    switch (e1.Type()) {
    case Type::U32:
        return Inst(Opcode::CompositeConstructU32x4, e1, e2, e3, e4);
    case Type::F16:
        return Inst(Opcode::CompositeConstructF16x4, e1, e2, e3, e4);
    case Type::F32:
        return Inst(Opcode::CompositeConstructF32x4, e1, e2, e3, e4);
    case Type::F64:
        return Inst(Opcode::CompositeConstructF64x4, e1, e2, e3, e4);
    default:
        ThrowInvalidType(e1.Type());
    }
}

Value IREmitter::CompositeExtract(const Value& vector, size_t element) {
    const auto read{[&](Opcode opcode, size_t limit) -> Value {
        if (element >= limit) {
            throw InvalidArgument("Out of bounds element {}", element);
        }
        return Inst(opcode, vector, Value{static_cast<u32>(element)});
    }};
    switch (vector.Type()) {
    case Type::U32x2:
        return read(Opcode::CompositeExtractU32x2, 2);
    case Type::U32x3:
        return read(Opcode::CompositeExtractU32x3, 3);
    case Type::U32x4:
        return read(Opcode::CompositeExtractU32x4, 4);
    case Type::F16x2:
        return read(Opcode::CompositeExtractF16x2, 2);
    case Type::F16x3:
        return read(Opcode::CompositeExtractF16x3, 3);
    case Type::F16x4:
        return read(Opcode::CompositeExtractF16x4, 4);
    case Type::F32x2:
        return read(Opcode::CompositeExtractF32x2, 2);
    case Type::F32x3:
        return read(Opcode::CompositeExtractF32x3, 3);
    case Type::F32x4:
        return read(Opcode::CompositeExtractF32x4, 4);
    case Type::F64x2:
        return read(Opcode::CompositeExtractF64x2, 2);
    case Type::F64x3:
        return read(Opcode::CompositeExtractF64x3, 3);
    case Type::F64x4:
        return read(Opcode::CompositeExtractF64x4, 4);
    default:
        ThrowInvalidType(vector.Type());
    }
}

Value IREmitter::CompositeInsert(const Value& vector, const Value& object, size_t element) {
    const auto insert{[&](Opcode opcode, size_t limit) {
        if (element >= limit) {
            throw InvalidArgument("Out of bounds element {}", element);
        }
        return Inst(opcode, vector, object, Value{static_cast<u32>(element)});
    }};
    switch (vector.Type()) {
    case Type::U32x2:
        return insert(Opcode::CompositeInsertU32x2, 2);
    case Type::U32x3:
        return insert(Opcode::CompositeInsertU32x3, 3);
    case Type::U32x4:
        return insert(Opcode::CompositeInsertU32x4, 4);
    case Type::F16x2:
        return insert(Opcode::CompositeInsertF16x2, 2);
    case Type::F16x3:
        return insert(Opcode::CompositeInsertF16x3, 3);
    case Type::F16x4:
        return insert(Opcode::CompositeInsertF16x4, 4);
    case Type::F32x2:
        return insert(Opcode::CompositeInsertF32x2, 2);
    case Type::F32x3:
        return insert(Opcode::CompositeInsertF32x3, 3);
    case Type::F32x4:
        return insert(Opcode::CompositeInsertF32x4, 4);
    case Type::F64x2:
        return insert(Opcode::CompositeInsertF64x2, 2);
    case Type::F64x3:
        return insert(Opcode::CompositeInsertF64x3, 3);
    case Type::F64x4:
        return insert(Opcode::CompositeInsertF64x4, 4);
    default:
        ThrowInvalidType(vector.Type());
    }
}

Value IREmitter::Select(const U1& condition, const Value& true_value, const Value& false_value) {
    if (true_value.Type() != false_value.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", true_value.Type(), false_value.Type());
    }
    switch (true_value.Type()) {
    case Type::U1:
        return Inst(Opcode::SelectU1, condition, true_value, false_value);
    case Type::U8:
        return Inst(Opcode::SelectU8, condition, true_value, false_value);
    case Type::U16:
        return Inst(Opcode::SelectU16, condition, true_value, false_value);
    case Type::U32:
        return Inst(Opcode::SelectU32, condition, true_value, false_value);
    case Type::U64:
        return Inst(Opcode::SelectU64, condition, true_value, false_value);
    case Type::F32:
        return Inst(Opcode::SelectF32, condition, true_value, false_value);
    default:
        throw InvalidArgument("Invalid type {}", true_value.Type());
    }
}

template <>
IR::U32 IREmitter::BitCast<IR::U32, IR::F32>(const IR::F32& value) {
    return Inst<IR::U32>(Opcode::BitCastU32F32, value);
}

template <>
IR::F32 IREmitter::BitCast<IR::F32, IR::U32>(const IR::U32& value) {
    return Inst<IR::F32>(Opcode::BitCastF32U32, value);
}

template <>
IR::U16 IREmitter::BitCast<IR::U16, IR::F16>(const IR::F16& value) {
    return Inst<IR::U16>(Opcode::BitCastU16F16, value);
}

template <>
IR::F16 IREmitter::BitCast<IR::F16, IR::U16>(const IR::U16& value) {
    return Inst<IR::F16>(Opcode::BitCastF16U16, value);
}

template <>
IR::U64 IREmitter::BitCast<IR::U64, IR::F64>(const IR::F64& value) {
    return Inst<IR::U64>(Opcode::BitCastU64F64, value);
}

template <>
IR::F64 IREmitter::BitCast<IR::F64, IR::U64>(const IR::U64& value) {
    return Inst<IR::F64>(Opcode::BitCastF64U64, value);
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
    return Inst(Opcode::UnpackFloat2x16, value);
}

U32 IREmitter::PackHalf2x16(const Value& vector) {
    return Inst<U32>(Opcode::PackHalf2x16, vector);
}

Value IREmitter::UnpackHalf2x16(const U32& value) {
    return Inst(Opcode::UnpackHalf2x16, value);
}

F64 IREmitter::PackDouble2x32(const Value& vector) {
    return Inst<F64>(Opcode::PackDouble2x32, vector);
}

Value IREmitter::UnpackDouble2x32(const F64& value) {
    return Inst<Value>(Opcode::UnpackDouble2x32, value);
}

F16F32F64 IREmitter::FPMul(const F16F32F64& a, const F16F32F64& b, FpControl control) {
    if (a.Type() != b.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", a.Type(), b.Type());
    }
    switch (a.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPMul16, Flags{control}, a, b);
    case Type::F32:
        return Inst<F32>(Opcode::FPMul32, Flags{control}, a, b);
    case Type::F64:
        return Inst<F64>(Opcode::FPMul64, Flags{control}, a, b);
    default:
        ThrowInvalidType(a.Type());
    }
}

F16F32F64 IREmitter::FPFma(const F16F32F64& a, const F16F32F64& b, const F16F32F64& c,
                           FpControl control) {
    if (a.Type() != b.Type() || a.Type() != c.Type()) {
        throw InvalidArgument("Mismatching types {}, {}, and {}", a.Type(), b.Type(), c.Type());
    }
    switch (a.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPFma16, Flags{control}, a, b, c);
    case Type::F32:
        return Inst<F32>(Opcode::FPFma32, Flags{control}, a, b, c);
    case Type::F64:
        return Inst<F64>(Opcode::FPFma64, Flags{control}, a, b, c);
    default:
        ThrowInvalidType(a.Type());
    }
}

F16F32F64 IREmitter::FPAbs(const F16F32F64& value) {
    switch (value.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPAbs16, value);
    case Type::F32:
        return Inst<F32>(Opcode::FPAbs32, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPAbs64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

F16F32F64 IREmitter::FPNeg(const F16F32F64& value) {
    switch (value.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPNeg16, value);
    case Type::F32:
        return Inst<F32>(Opcode::FPNeg32, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPNeg64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

F16F32F64 IREmitter::FPAbsNeg(const F16F32F64& value, bool abs, bool neg) {
    F16F32F64 result{value};
    if (abs) {
        result = FPAbs(result);
    }
    if (neg) {
        result = FPNeg(result);
    }
    return result;
}

F32 IREmitter::FPCos(const F32& value) {
    return Inst<F32>(Opcode::FPCos, value);
}

F32 IREmitter::FPSin(const F32& value) {
    return Inst<F32>(Opcode::FPSin, value);
}

F32 IREmitter::FPExp2(const F32& value) {
    return Inst<F32>(Opcode::FPExp2, value);
}

F32 IREmitter::FPLog2(const F32& value) {
    return Inst<F32>(Opcode::FPLog2, value);
}

F32F64 IREmitter::FPRecip(const F32F64& value) {
    switch (value.Type()) {
    case Type::F32:
        return Inst<F32>(Opcode::FPRecip32, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPRecip64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

F32F64 IREmitter::FPRecipSqrt(const F32F64& value) {
    switch (value.Type()) {
    case Type::F32:
        return Inst<F32>(Opcode::FPRecipSqrt32, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPRecipSqrt64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

F32 IREmitter::FPSqrt(const F32& value) {
    return Inst<F32>(Opcode::FPSqrt, value);
}

F16F32F64 IREmitter::FPSaturate(const F16F32F64& value) {
    switch (value.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPSaturate16, value);
    case Type::F32:
        return Inst<F32>(Opcode::FPSaturate32, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPSaturate64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

F16F32F64 IREmitter::FPRoundEven(const F16F32F64& value, FpControl control) {
    switch (value.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPRoundEven16, Flags{control}, value);
    case Type::F32:
        return Inst<F32>(Opcode::FPRoundEven32, Flags{control}, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPRoundEven64, Flags{control}, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

F16F32F64 IREmitter::FPFloor(const F16F32F64& value, FpControl control) {
    switch (value.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPFloor16, Flags{control}, value);
    case Type::F32:
        return Inst<F32>(Opcode::FPFloor32, Flags{control}, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPFloor64, Flags{control}, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

F16F32F64 IREmitter::FPCeil(const F16F32F64& value, FpControl control) {
    switch (value.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPCeil16, Flags{control}, value);
    case Type::F32:
        return Inst<F32>(Opcode::FPCeil32, Flags{control}, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPCeil64, Flags{control}, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

F16F32F64 IREmitter::FPTrunc(const F16F32F64& value, FpControl control) {
    switch (value.Type()) {
    case Type::F16:
        return Inst<F16>(Opcode::FPTrunc16, Flags{control}, value);
    case Type::F32:
        return Inst<F32>(Opcode::FPTrunc32, Flags{control}, value);
    case Type::F64:
        return Inst<F64>(Opcode::FPTrunc64, Flags{control}, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U1 IREmitter::FPEqual(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control, bool ordered) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::F16:
        return Inst<U1>(ordered ? Opcode::FPOrdEqual16 : Opcode::FPUnordEqual16, Flags{control},
                        lhs, rhs);
    case Type::F32:
        return Inst<U1>(ordered ? Opcode::FPOrdEqual32 : Opcode::FPUnordEqual32, Flags{control},
                        lhs, rhs);
    case Type::F64:
        return Inst<U1>(ordered ? Opcode::FPOrdEqual64 : Opcode::FPUnordEqual64, Flags{control},
                        lhs, rhs);
    default:
        ThrowInvalidType(lhs.Type());
    }
}

U1 IREmitter::FPNotEqual(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control,
                         bool ordered) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::F16:
        return Inst<U1>(ordered ? Opcode::FPOrdNotEqual16 : Opcode::FPUnordNotEqual16,
                        Flags{control}, lhs, rhs);
    case Type::F32:
        return Inst<U1>(ordered ? Opcode::FPOrdNotEqual32 : Opcode::FPUnordNotEqual32,
                        Flags{control}, lhs, rhs);
    case Type::F64:
        return Inst<U1>(ordered ? Opcode::FPOrdNotEqual64 : Opcode::FPUnordNotEqual64,
                        Flags{control}, lhs, rhs);
    default:
        ThrowInvalidType(lhs.Type());
    }
}

U1 IREmitter::FPLessThan(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control,
                         bool ordered) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::F16:
        return Inst<U1>(ordered ? Opcode::FPOrdLessThan16 : Opcode::FPUnordLessThan16,
                        Flags{control}, lhs, rhs);
    case Type::F32:
        return Inst<U1>(ordered ? Opcode::FPOrdLessThan32 : Opcode::FPUnordLessThan32,
                        Flags{control}, lhs, rhs);
    case Type::F64:
        return Inst<U1>(ordered ? Opcode::FPOrdLessThan64 : Opcode::FPUnordLessThan64,
                        Flags{control}, lhs, rhs);
    default:
        ThrowInvalidType(lhs.Type());
    }
}

U1 IREmitter::FPGreaterThan(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control,
                            bool ordered) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::F16:
        return Inst<U1>(ordered ? Opcode::FPOrdGreaterThan16 : Opcode::FPUnordGreaterThan16,
                        Flags{control}, lhs, rhs);
    case Type::F32:
        return Inst<U1>(ordered ? Opcode::FPOrdGreaterThan32 : Opcode::FPUnordGreaterThan32,
                        Flags{control}, lhs, rhs);
    case Type::F64:
        return Inst<U1>(ordered ? Opcode::FPOrdGreaterThan64 : Opcode::FPUnordGreaterThan64,
                        Flags{control}, lhs, rhs);
    default:
        ThrowInvalidType(lhs.Type());
    }
}

U1 IREmitter::FPLessThanEqual(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control,
                              bool ordered) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::F16:
        return Inst<U1>(ordered ? Opcode::FPOrdLessThanEqual16 : Opcode::FPUnordLessThanEqual16,
                        Flags{control}, lhs, rhs);
    case Type::F32:
        return Inst<U1>(ordered ? Opcode::FPOrdLessThanEqual32 : Opcode::FPUnordLessThanEqual32,
                        Flags{control}, lhs, rhs);
    case Type::F64:
        return Inst<U1>(ordered ? Opcode::FPOrdLessThanEqual64 : Opcode::FPUnordLessThanEqual64,
                        Flags{control}, lhs, rhs);
    default:
        ThrowInvalidType(lhs.Type());
    }
}

U1 IREmitter::FPGreaterThanEqual(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control,
                                 bool ordered) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::F16:
        return Inst<U1>(ordered ? Opcode::FPOrdGreaterThanEqual16
                                : Opcode::FPUnordGreaterThanEqual16,
                        Flags{control}, lhs, rhs);
    case Type::F32:
        return Inst<U1>(ordered ? Opcode::FPOrdGreaterThanEqual32
                                : Opcode::FPUnordGreaterThanEqual32,
                        Flags{control}, lhs, rhs);
    case Type::F64:
        return Inst<U1>(ordered ? Opcode::FPOrdGreaterThanEqual64
                                : Opcode::FPUnordGreaterThanEqual64,
                        Flags{control}, lhs, rhs);
    default:
        ThrowInvalidType(lhs.Type());
    }
}

U1 IREmitter::FPIsNan(const F16F32F64& value) {
    switch (value.Type()) {
    case Type::F16:
        return Inst<U1>(Opcode::FPIsNan16, value);
    case Type::F32:
        return Inst<U1>(Opcode::FPIsNan32, value);
    case Type::F64:
        return Inst<U1>(Opcode::FPIsNan64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U1 IREmitter::FPOrdered(const F16F32F64& lhs, const F16F32F64& rhs) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    return LogicalAnd(LogicalNot(FPIsNan(lhs)), LogicalNot(FPIsNan(rhs)));
}

U1 IREmitter::FPUnordered(const F16F32F64& lhs, const F16F32F64& rhs) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    return LogicalOr(FPIsNan(lhs), FPIsNan(rhs));
}

F32F64 IREmitter::FPMax(const F32F64& lhs, const F32F64& rhs, FpControl control) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::F32:
        return Inst<F32>(Opcode::FPMax32, Flags{control}, lhs, rhs);
    case Type::F64:
        return Inst<F64>(Opcode::FPMax64, Flags{control}, lhs, rhs);
    default:
        ThrowInvalidType(lhs.Type());
    }
}

F32F64 IREmitter::FPMin(const F32F64& lhs, const F32F64& rhs, FpControl control) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::F32:
        return Inst<F32>(Opcode::FPMin32, Flags{control}, lhs, rhs);
    case Type::F64:
        return Inst<F64>(Opcode::FPMin64, Flags{control}, lhs, rhs);
    default:
        ThrowInvalidType(lhs.Type());
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

U32U64 IREmitter::INeg(const U32U64& value) {
    switch (value.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::INeg32, value);
    case Type::U64:
        return Inst<U64>(Opcode::INeg64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U32U64 IREmitter::IAbs(const U32U64& value) {
    switch (value.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::IAbs32, value);
    case Type::U64:
        return Inst<U64>(Opcode::IAbs64, value);
    default:
        ThrowInvalidType(value.Type());
    }
}

U32U64 IREmitter::ShiftLeftLogical(const U32U64& base, const U32& shift) {
    switch (base.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::ShiftLeftLogical32, base, shift);
    case Type::U64:
        return Inst<U64>(Opcode::ShiftLeftLogical64, base, shift);
    default:
        ThrowInvalidType(base.Type());
    }
}

U32U64 IREmitter::ShiftRightLogical(const U32U64& base, const U32& shift) {
    switch (base.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::ShiftRightLogical32, base, shift);
    case Type::U64:
        return Inst<U64>(Opcode::ShiftRightLogical64, base, shift);
    default:
        ThrowInvalidType(base.Type());
    }
}

U32U64 IREmitter::ShiftRightArithmetic(const U32U64& base, const U32& shift) {
    switch (base.Type()) {
    case Type::U32:
        return Inst<U32>(Opcode::ShiftRightArithmetic32, base, shift);
    case Type::U64:
        return Inst<U64>(Opcode::ShiftRightArithmetic64, base, shift);
    default:
        ThrowInvalidType(base.Type());
    }
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

U32 IREmitter::BitReverse(const U32& value) {
    return Inst<U32>(Opcode::BitReverse32, value);
}

U32 IREmitter::BitCount(const U32& value) {
    return Inst<U32>(Opcode::BitCount32, value);
}

U32 IREmitter::BitwiseNot(const U32& value) {
    return Inst<U32>(Opcode::BitwiseNot32, value);
}

U32 IREmitter::FindSMsb(const U32& value) {
    return Inst<U32>(Opcode::FindSMsb32, value);
}

U32 IREmitter::FindUMsb(const U32& value) {
    return Inst<U32>(Opcode::FindUMsb32, value);
}

U32 IREmitter::SMin(const U32& a, const U32& b) {
    return Inst<U32>(Opcode::SMin32, a, b);
}

U32 IREmitter::UMin(const U32& a, const U32& b) {
    return Inst<U32>(Opcode::UMin32, a, b);
}

U32 IREmitter::SMax(const U32& a, const U32& b) {
    return Inst<U32>(Opcode::SMax32, a, b);
}

U32 IREmitter::UMax(const U32& a, const U32& b) {
    return Inst<U32>(Opcode::UMax32, a, b);
}

U1 IREmitter::ILessThan(const U32& lhs, const U32& rhs, bool is_signed) {
    return Inst<U1>(is_signed ? Opcode::SLessThan : Opcode::ULessThan, lhs, rhs);
}

U1 IREmitter::IEqual(const U32U64& lhs, const U32U64& rhs) {
    if (lhs.Type() != rhs.Type()) {
        throw InvalidArgument("Mismatching types {} and {}", lhs.Type(), rhs.Type());
    }
    switch (lhs.Type()) {
    case Type::U32:
        return Inst<U1>(Opcode::IEqual, lhs, rhs);
    case Type::U64: {
        // Manually compare the unpacked values
        const Value lhs_vector{UnpackUint2x32(lhs)};
        const Value rhs_vector{UnpackUint2x32(rhs)};
        return LogicalAnd(IEqual(IR::U32{CompositeExtract(lhs_vector, 0)},
                                 IR::U32{CompositeExtract(rhs_vector, 0)}),
                          IEqual(IR::U32{CompositeExtract(lhs_vector, 1)},
                                 IR::U32{CompositeExtract(rhs_vector, 1)}));
    }
    default:
        ThrowInvalidType(lhs.Type());
    }
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

U32U64 IREmitter::ConvertFToS(size_t bitsize, const F16F32F64& value) {
    switch (bitsize) {
    case 16:
        switch (value.Type()) {
        case Type::F16:
            return Inst<U32>(Opcode::ConvertS16F16, value);
        case Type::F32:
            return Inst<U32>(Opcode::ConvertS16F32, value);
        case Type::F64:
            return Inst<U32>(Opcode::ConvertS16F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    case 32:
        switch (value.Type()) {
        case Type::F16:
            return Inst<U32>(Opcode::ConvertS32F16, value);
        case Type::F32:
            return Inst<U32>(Opcode::ConvertS32F32, value);
        case Type::F64:
            return Inst<U32>(Opcode::ConvertS32F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    case 64:
        switch (value.Type()) {
        case Type::F16:
            return Inst<U64>(Opcode::ConvertS64F16, value);
        case Type::F32:
            return Inst<U64>(Opcode::ConvertS64F32, value);
        case Type::F64:
            return Inst<U64>(Opcode::ConvertS64F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    default:
        throw InvalidArgument("Invalid destination bitsize {}", bitsize);
    }
}

U32U64 IREmitter::ConvertFToU(size_t bitsize, const F16F32F64& value) {
    switch (bitsize) {
    case 16:
        switch (value.Type()) {
        case Type::F16:
            return Inst<U32>(Opcode::ConvertU16F16, value);
        case Type::F32:
            return Inst<U32>(Opcode::ConvertU16F32, value);
        case Type::F64:
            return Inst<U32>(Opcode::ConvertU16F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    case 32:
        switch (value.Type()) {
        case Type::F16:
            return Inst<U32>(Opcode::ConvertU32F16, value);
        case Type::F32:
            return Inst<U32>(Opcode::ConvertU32F32, value);
        case Type::F64:
            return Inst<U32>(Opcode::ConvertU32F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    case 64:
        switch (value.Type()) {
        case Type::F16:
            return Inst<U64>(Opcode::ConvertU64F16, value);
        case Type::F32:
            return Inst<U64>(Opcode::ConvertU64F32, value);
        case Type::F64:
            return Inst<U64>(Opcode::ConvertU64F64, value);
        default:
            ThrowInvalidType(value.Type());
        }
    default:
        throw InvalidArgument("Invalid destination bitsize {}", bitsize);
    }
}

U32U64 IREmitter::ConvertFToI(size_t bitsize, bool is_signed, const F16F32F64& value) {
    if (is_signed) {
        return ConvertFToS(bitsize, value);
    } else {
        return ConvertFToU(bitsize, value);
    }
}

F16F32F64 IREmitter::ConvertSToF(size_t dest_bitsize, size_t src_bitsize, const Value& value) {
    switch (dest_bitsize) {
    case 16:
        switch (src_bitsize) {
        case 8:
            return Inst<F16>(Opcode::ConvertF16S8, value);
        case 16:
            return Inst<F16>(Opcode::ConvertF16S16, value);
        case 32:
            return Inst<F16>(Opcode::ConvertF16S32, value);
        case 64:
            return Inst<F16>(Opcode::ConvertF16S64, value);
        }
        break;
    case 32:
        switch (src_bitsize) {
        case 8:
            return Inst<F32>(Opcode::ConvertF32S8, value);
        case 16:
            return Inst<F32>(Opcode::ConvertF32S16, value);
        case 32:
            return Inst<F32>(Opcode::ConvertF32S32, value);
        case 64:
            return Inst<F32>(Opcode::ConvertF32S64, value);
        }
        break;
    case 64:
        switch (src_bitsize) {
        case 8:
            return Inst<F64>(Opcode::ConvertF64S8, value);
        case 16:
            return Inst<F64>(Opcode::ConvertF64S16, value);
        case 32:
            return Inst<F64>(Opcode::ConvertF64S32, value);
        case 64:
            return Inst<F64>(Opcode::ConvertF64S64, value);
        }
        break;
    }
    throw InvalidArgument("Invalid bit size combination dst={} src={}", dest_bitsize, src_bitsize);
}

F16F32F64 IREmitter::ConvertUToF(size_t dest_bitsize, size_t src_bitsize, const Value& value) {
    switch (dest_bitsize) {
    case 16:
        switch (src_bitsize) {
        case 8:
            return Inst<F16>(Opcode::ConvertF16U8, value);
        case 16:
            return Inst<F16>(Opcode::ConvertF16U16, value);
        case 32:
            return Inst<F16>(Opcode::ConvertF16U32, value);
        case 64:
            return Inst<F16>(Opcode::ConvertF16U64, value);
        }
        break;
    case 32:
        switch (src_bitsize) {
        case 8:
            return Inst<F32>(Opcode::ConvertF32U8, value);
        case 16:
            return Inst<F32>(Opcode::ConvertF32U16, value);
        case 32:
            return Inst<F32>(Opcode::ConvertF32U32, value);
        case 64:
            return Inst<F32>(Opcode::ConvertF32U64, value);
        }
        break;
    case 64:
        switch (src_bitsize) {
        case 8:
            return Inst<F64>(Opcode::ConvertF64U8, value);
        case 16:
            return Inst<F64>(Opcode::ConvertF64U16, value);
        case 32:
            return Inst<F64>(Opcode::ConvertF64U32, value);
        case 64:
            return Inst<F64>(Opcode::ConvertF64U64, value);
        }
        break;
    }
    throw InvalidArgument("Invalid bit size combination dst={} src={}", dest_bitsize, src_bitsize);
}

F16F32F64 IREmitter::ConvertIToF(size_t dest_bitsize, size_t src_bitsize, bool is_signed,
                                 const Value& value) {
    if (is_signed) {
        return ConvertSToF(dest_bitsize, src_bitsize, value);
    } else {
        return ConvertUToF(dest_bitsize, src_bitsize, value);
    }
}

U32U64 IREmitter::UConvert(size_t result_bitsize, const U32U64& value) {
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
            return Inst<U64>(Opcode::ConvertU64U32, value);
        case Type::U64:
            // Nothing to do
            return value;
        default:
            break;
        }
    }
    throw NotImplementedException("Conversion from {} to {} bits", value.Type(), result_bitsize);
}

F16F32F64 IREmitter::FPConvert(size_t result_bitsize, const F16F32F64& value) {
    switch (result_bitsize) {
    case 16:
        switch (value.Type()) {
        case Type::F16:
            // Nothing to do
            return value;
        case Type::F32:
            return Inst<F16>(Opcode::ConvertF16F32, value);
        case Type::F64:
            throw LogicError("Illegal conversion from F64 to F16");
        default:
            break;
        }
        break;
    case 32:
        switch (value.Type()) {
        case Type::F16:
            return Inst<F32>(Opcode::ConvertF32F16, value);
        case Type::F32:
            // Nothing to do
            return value;
        case Type::F64:
            return Inst<F64>(Opcode::ConvertF32F64, value);
        default:
            break;
        }
        break;
    case 64:
        switch (value.Type()) {
        case Type::F16:
            throw LogicError("Illegal conversion from F16 to F64");
        case Type::F32:
            // Nothing to do
            return value;
        case Type::F64:
            return Inst<F64>(Opcode::ConvertF32F64, value);
        default:
            break;
        }
        break;
    }
    throw NotImplementedException("Conversion from {} to {} bits", value.Type(), result_bitsize);
}

Value IREmitter::ImageSampleImplicitLod(const Value& handle, const Value& coords, const F32& bias,
                                        const Value& offset, const F32& lod_clamp,
                                        TextureInstInfo info) {
    const Value bias_lc{MakeLodClampPair(*this, bias, lod_clamp)};
    const Opcode op{handle.IsImmediate() ? Opcode::BoundImageSampleImplicitLod
                                         : Opcode::BindlessImageSampleImplicitLod};
    return Inst(op, Flags{info}, handle, coords, bias_lc, offset);
}

Value IREmitter::ImageSampleExplicitLod(const Value& handle, const Value& coords, const F32& lod,
                                        const Value& offset, const F32& lod_clamp,
                                        TextureInstInfo info) {
    const Value lod_lc{MakeLodClampPair(*this, lod, lod_clamp)};
    const Opcode op{handle.IsImmediate() ? Opcode::BoundImageSampleExplicitLod
                                         : Opcode::BindlessImageSampleExplicitLod};
    return Inst(op, Flags{info}, handle, coords, lod_lc, offset);
}

F32 IREmitter::ImageSampleDrefImplicitLod(const Value& handle, const Value& coords, const F32& dref,
                                          const F32& bias, const Value& offset,
                                          const F32& lod_clamp, TextureInstInfo info) {
    const Value bias_lc{MakeLodClampPair(*this, bias, lod_clamp)};
    const Opcode op{handle.IsImmediate() ? Opcode::BoundImageSampleDrefImplicitLod
                                         : Opcode::BindlessImageSampleDrefImplicitLod};
    return Inst<F32>(op, Flags{info}, handle, coords, dref, bias_lc, offset);
}

F32 IREmitter::ImageSampleDrefExplicitLod(const Value& handle, const Value& coords, const F32& dref,
                                          const F32& lod, const Value& offset, const F32& lod_clamp,
                                          TextureInstInfo info) {
    const Value lod_lc{MakeLodClampPair(*this, lod, lod_clamp)};
    const Opcode op{handle.IsImmediate() ? Opcode::BoundImageSampleDrefExplicitLod
                                         : Opcode::BindlessImageSampleDrefExplicitLod};
    return Inst<F32>(op, Flags{info}, handle, coords, dref, lod_lc, offset);
}

} // namespace Shader::IR
