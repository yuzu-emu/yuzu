// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "shader_recompiler/frontend/ir/attribute.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

class IREmitter {
public:
    explicit IREmitter(Block& block_) : block{block_}, insertion_point{block.end()} {}

    Block& block;

    [[nodiscard]] U1 Imm1(bool value) const;
    [[nodiscard]] U8 Imm8(u8 value) const;
    [[nodiscard]] U16 Imm16(u16 value) const;
    [[nodiscard]] U32 Imm32(u32 value) const;
    [[nodiscard]] U32 Imm32(s32 value) const;
    [[nodiscard]] U32 Imm32(f32 value) const;
    [[nodiscard]] U64 Imm64(u64 value) const;
    [[nodiscard]] U64 Imm64(f64 value) const;

    void Branch(IR::Block* label);
    void BranchConditional(const U1& cond, IR::Block* true_label, IR::Block* false_label);
    void Exit();
    void Return();
    void Unreachable();

    [[nodiscard]] U32 GetReg(IR::Reg reg);
    void SetReg(IR::Reg reg, const U32& value);

    [[nodiscard]] U1 GetPred(IR::Pred pred, bool is_negated = false);
    void SetPred(IR::Pred pred, const U1& value);

    [[nodiscard]] U32 GetCbuf(const U32& binding, const U32& byte_offset);

    [[nodiscard]] U1 GetZFlag();
    [[nodiscard]] U1 GetSFlag();
    [[nodiscard]] U1 GetCFlag();
    [[nodiscard]] U1 GetOFlag();

    void SetZFlag(const U1& value);
    void SetSFlag(const U1& value);
    void SetCFlag(const U1& value);
    void SetOFlag(const U1& value);

    [[nodiscard]] U32 GetAttribute(IR::Attribute attribute);
    void SetAttribute(IR::Attribute attribute, const U32& value);

    void WriteGlobalU8(const U64& address, const U32& value);
    void WriteGlobalS8(const U64& address, const U32& value);
    void WriteGlobalU16(const U64& address, const U32& value);
    void WriteGlobalS16(const U64& address, const U32& value);
    void WriteGlobal32(const U64& address, const U32& value);
    void WriteGlobal64(const U64& address, const IR::Value& vector);
    void WriteGlobal128(const U64& address, const IR::Value& vector);

    [[nodiscard]] U1 GetZeroFromOp(const Value& op);
    [[nodiscard]] U1 GetSignFromOp(const Value& op);
    [[nodiscard]] U1 GetCarryFromOp(const Value& op);
    [[nodiscard]] U1 GetOverflowFromOp(const Value& op);

    [[nodiscard]] Value CompositeConstruct(const UAny& e1, const UAny& e2);
    [[nodiscard]] Value CompositeConstruct(const UAny& e1, const UAny& e2, const UAny& e3);
    [[nodiscard]] Value CompositeConstruct(const UAny& e1, const UAny& e2, const UAny& e3,
                                           const UAny& e4);
    [[nodiscard]] UAny CompositeExtract(const Value& vector, size_t element);

    [[nodiscard]] U64 PackUint2x32(const Value& vector);
    [[nodiscard]] Value UnpackUint2x32(const U64& value);

    [[nodiscard]] U32 PackFloat2x16(const Value& vector);
    [[nodiscard]] Value UnpackFloat2x16(const U32& value);

    [[nodiscard]] U64 PackDouble2x32(const Value& vector);
    [[nodiscard]] Value UnpackDouble2x32(const U64& value);

    [[nodiscard]] U16U32U64 FPAdd(const U16U32U64& a, const U16U32U64& b);
    [[nodiscard]] U16U32U64 FPMul(const U16U32U64& a, const U16U32U64& b);

    [[nodiscard]] U16U32U64 FPAbs(const U16U32U64& value);
    [[nodiscard]] U16U32U64 FPNeg(const U16U32U64& value);
    [[nodiscard]] U16U32U64 FPAbsNeg(const U16U32U64& value, bool abs, bool neg);

    [[nodiscard]] U32 FPCosNotReduced(const U32& value);
    [[nodiscard]] U32 FPExp2NotReduced(const U32& value);
    [[nodiscard]] U32 FPLog2(const U32& value);
    [[nodiscard]] U32U64 FPRecip(const U32U64& value);
    [[nodiscard]] U32U64 FPRecipSqrt(const U32U64& value);
    [[nodiscard]] U32 FPSinNotReduced(const U32& value);
    [[nodiscard]] U32 FPSqrt(const U32& value);
    [[nodiscard]] U16U32U64 FPSaturate(const U16U32U64& value);
    [[nodiscard]] U16U32U64 FPRoundEven(const U16U32U64& value);
    [[nodiscard]] U16U32U64 FPFloor(const U16U32U64& value);
    [[nodiscard]] U16U32U64 FPCeil(const U16U32U64& value);
    [[nodiscard]] U16U32U64 FPTrunc(const U16U32U64& value);

    [[nodiscard]] U1 LogicalOr(const U1& a, const U1& b);
    [[nodiscard]] U1 LogicalAnd(const U1& a, const U1& b);
    [[nodiscard]] U1 LogicalNot(const U1& value);

    [[nodiscard]] U32U64 ConvertFToS(size_t bitsize, const U16U32U64& value);
    [[nodiscard]] U32U64 ConvertFToU(size_t bitsize, const U16U32U64& value);
    [[nodiscard]] U32U64 ConvertFToI(size_t bitsize, bool is_signed, const U16U32U64& value);

    [[nodiscard]] U32U64 ConvertU(size_t bitsize, const U32U64& value);

private:
    IR::Block::iterator insertion_point;

    template <typename T = Value, typename... Args>
    T Inst(Opcode op, Args... args) {
        auto it{block.PrependNewInst(insertion_point, op, {Value{args}...})};
        return T{Value{&*it}};
    }
};

} // namespace Shader::IR
