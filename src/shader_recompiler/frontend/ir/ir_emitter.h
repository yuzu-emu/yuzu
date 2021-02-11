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
    explicit IREmitter(Block& block_) : block{&block_}, insertion_point{block->end()} {}
    explicit IREmitter(Block& block_, Block::iterator insertion_point_)
        : block{&block_}, insertion_point{insertion_point_} {}

    Block* block;

    [[nodiscard]] U1 Imm1(bool value) const;
    [[nodiscard]] U8 Imm8(u8 value) const;
    [[nodiscard]] U16 Imm16(u16 value) const;
    [[nodiscard]] U32 Imm32(u32 value) const;
    [[nodiscard]] U32 Imm32(s32 value) const;
    [[nodiscard]] F32 Imm32(f32 value) const;
    [[nodiscard]] U64 Imm64(u64 value) const;
    [[nodiscard]] F64 Imm64(f64 value) const;

    void Branch(Block* label);
    void BranchConditional(const U1& condition, Block* true_label, Block* false_label);
    void LoopMerge(Block* merge_block, Block* continue_target);
    void SelectionMerge(Block* merge_block);
    void Return();

    [[nodiscard]] U32 GetReg(IR::Reg reg);
    void SetReg(IR::Reg reg, const U32& value);

    [[nodiscard]] U1 GetPred(IR::Pred pred, bool is_negated = false);
    void SetPred(IR::Pred pred, const U1& value);

    [[nodiscard]] U1 GetGotoVariable(u32 id);
    void SetGotoVariable(u32 id, const U1& value);

    [[nodiscard]] U32 GetCbuf(const U32& binding, const U32& byte_offset);

    [[nodiscard]] U1 GetZFlag();
    [[nodiscard]] U1 GetSFlag();
    [[nodiscard]] U1 GetCFlag();
    [[nodiscard]] U1 GetOFlag();

    void SetZFlag(const U1& value);
    void SetSFlag(const U1& value);
    void SetCFlag(const U1& value);
    void SetOFlag(const U1& value);

    [[nodiscard]] U1 Condition(IR::Condition cond);

    [[nodiscard]] F32 GetAttribute(IR::Attribute attribute);
    void SetAttribute(IR::Attribute attribute, const F32& value);

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

    [[nodiscard]] Value CompositeConstruct(const Value& e1, const Value& e2);
    [[nodiscard]] Value CompositeConstruct(const Value& e1, const Value& e2, const Value& e3);
    [[nodiscard]] Value CompositeConstruct(const Value& e1, const Value& e2, const Value& e3,
                                           const Value& e4);
    [[nodiscard]] Value CompositeExtract(const Value& vector, size_t element);

    [[nodiscard]] UAny Select(const U1& condition, const UAny& true_value, const UAny& false_value);

    template <typename Dest, typename Source>
    [[nodiscard]] Dest BitCast(const Source& value);

    [[nodiscard]] U64 PackUint2x32(const Value& vector);
    [[nodiscard]] Value UnpackUint2x32(const U64& value);

    [[nodiscard]] U32 PackFloat2x16(const Value& vector);
    [[nodiscard]] Value UnpackFloat2x16(const U32& value);

    [[nodiscard]] F64 PackDouble2x32(const Value& vector);
    [[nodiscard]] Value UnpackDouble2x32(const F64& value);

    [[nodiscard]] F16F32F64 FPAdd(const F16F32F64& a, const F16F32F64& b, FpControl control = {});
    [[nodiscard]] F16F32F64 FPMul(const F16F32F64& a, const F16F32F64& b, FpControl control = {});
    [[nodiscard]] F16F32F64 FPFma(const F16F32F64& a, const F16F32F64& b, const F16F32F64& c,
                                  FpControl control = {});

    [[nodiscard]] F16F32F64 FPAbs(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPNeg(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPAbsNeg(const F16F32F64& value, bool abs, bool neg);

    [[nodiscard]] F32 FPCosNotReduced(const F32& value);
    [[nodiscard]] F32 FPExp2NotReduced(const F32& value);
    [[nodiscard]] F32 FPLog2(const F32& value);
    [[nodiscard]] F32F64 FPRecip(const F32F64& value);
    [[nodiscard]] F32F64 FPRecipSqrt(const F32F64& value);
    [[nodiscard]] F32 FPSinNotReduced(const F32& value);
    [[nodiscard]] F32 FPSqrt(const F32& value);
    [[nodiscard]] F16F32F64 FPSaturate(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPRoundEven(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPFloor(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPCeil(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPTrunc(const F16F32F64& value);

    [[nodiscard]] U32U64 IAdd(const U32U64& a, const U32U64& b);
    [[nodiscard]] U32U64 ISub(const U32U64& a, const U32U64& b);
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

    [[nodiscard]] U32U64 ConvertFToS(size_t bitsize, const F16F32F64& value);
    [[nodiscard]] U32U64 ConvertFToU(size_t bitsize, const F16F32F64& value);
    [[nodiscard]] U32U64 ConvertFToI(size_t bitsize, bool is_signed, const F16F32F64& value);

    [[nodiscard]] U32U64 ConvertU(size_t result_bitsize, const U32U64& value);

private:
    IR::Block::iterator insertion_point;

    template <typename T = Value, typename... Args>
    T Inst(Opcode op, Args... args) {
        auto it{block->PrependNewInst(insertion_point, op, {Value{args}...})};
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
        auto it{block->PrependNewInst(insertion_point, op, {Value{args}...}, raw_flags)};
        return T{Value{&*it}};
    }
};

} // namespace Shader::IR
