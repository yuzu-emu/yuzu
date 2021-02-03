// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <type_traits>

#include "shader_recompiler/frontend/ir/attribute.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
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

    [[nodiscard]] U32 WorkgroupIdX();
    [[nodiscard]] U32 WorkgroupIdY();
    [[nodiscard]] U32 WorkgroupIdZ();

    [[nodiscard]] U32 LocalInvocationIdX();
    [[nodiscard]] U32 LocalInvocationIdY();
    [[nodiscard]] U32 LocalInvocationIdZ();

    [[nodiscard]] U32 LoadGlobalU8(const U64& address);
    [[nodiscard]] U32 LoadGlobalS8(const U64& address);
    [[nodiscard]] U32 LoadGlobalU16(const U64& address);
    [[nodiscard]] U32 LoadGlobalS16(const U64& address);
    [[nodiscard]] U32 LoadGlobal32(const U64& address);
    [[nodiscard]] Value LoadGlobal64(const U64& address);
    [[nodiscard]] Value LoadGlobal128(const U64& address);

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

    [[nodiscard]] UAny Select(const U1& condition, const UAny& true_value, const UAny& false_value);

    [[nodiscard]] U64 PackUint2x32(const Value& vector);
    [[nodiscard]] Value UnpackUint2x32(const U64& value);

    [[nodiscard]] U32 PackFloat2x16(const Value& vector);
    [[nodiscard]] Value UnpackFloat2x16(const U32& value);

    [[nodiscard]] U64 PackDouble2x32(const Value& vector);
    [[nodiscard]] Value UnpackDouble2x32(const U64& value);

    [[nodiscard]] U16U32U64 FPAdd(const U16U32U64& a, const U16U32U64& b, FpControl control = {});
    [[nodiscard]] U16U32U64 FPMul(const U16U32U64& a, const U16U32U64& b, FpControl control = {});
    [[nodiscard]] U16U32U64 FPFma(const U16U32U64& a, const U16U32U64& b, const U16U32U64& c,
                                  FpControl control = {});

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

    [[nodiscard]] U32U64 IAdd(const U32U64& a, const U32U64& b);
    [[nodiscard]] U32 IMul(const U32& a, const U32& b);
    [[nodiscard]] U32 INeg(const U32& value);
    [[nodiscard]] U32 IAbs(const U32& value);
    [[nodiscard]] U32 ShiftLeftLogical(const U32& base, const U32& shift);
    [[nodiscard]] U32 ShiftRightLogical(const U32& base, const U32& shift);
    [[nodiscard]] U32 ShiftRightArithmetic(const U32& base, const U32& shift);
    [[nodiscard]] U32 BitwiseAnd(const U32& a, const U32& b);
    [[nodiscard]] U32 BitwiseOr(const U32& a, const U32& b);
    [[nodiscard]] U32 BitwiseXor(const U32& a, const U32& b);
    [[nodiscard]] U32 BitFieldInsert(const U32& base, const U32& insert, const U32& offset,
                                     const U32& count);
    [[nodiscard]] U32 BitFieldExtract(const U32& base, const U32& offset, const U32& count,
                                      bool is_signed);

    [[nodiscard]] U1 ILessThan(const U32& lhs, const U32& rhs, bool is_signed);
    [[nodiscard]] U1 IEqual(const U32& lhs, const U32& rhs);
    [[nodiscard]] U1 ILessThanEqual(const U32& lhs, const U32& rhs, bool is_signed);
    [[nodiscard]] U1 IGreaterThan(const U32& lhs, const U32& rhs, bool is_signed);
    [[nodiscard]] U1 INotEqual(const U32& lhs, const U32& rhs);
    [[nodiscard]] U1 IGreaterThanEqual(const U32& lhs, const U32& rhs, bool is_signed);

    [[nodiscard]] U1 LogicalOr(const U1& a, const U1& b);
    [[nodiscard]] U1 LogicalAnd(const U1& a, const U1& b);
    [[nodiscard]] U1 LogicalXor(const U1& a, const U1& b);
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

    template <typename T>
    requires(sizeof(T) <= sizeof(u64) && std::is_trivially_copyable_v<T>) struct Flags {
        Flags() = default;
        Flags(T proxy_) : proxy{proxy_} {}

        T proxy;
    };

    template <typename T = Value, typename FlagType, typename... Args>
    T Inst(Opcode op, Flags<FlagType> flags, Args... args) {
        u64 raw_flags{};
        std::memcpy(&raw_flags, &flags.proxy, sizeof(flags.proxy));
        auto it{block.PrependNewInst(insertion_point, op, {Value{args}...}, raw_flags)};
        return T{Value{&*it}};
    }
};

} // namespace Shader::IR
