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
    [[nodiscard]] U64 Imm64(s64 value) const;
    [[nodiscard]] F64 Imm64(f64 value) const;

    U1 ConditionRef(const U1& value);
    void Reference(const Value& value);

    void PhiMove(IR::Inst& phi, const Value& value);

    void Prologue();
    void Epilogue();
    void DemoteToHelperInvocation();
    void EmitVertex(const U32& stream);
    void EndPrimitive(const U32& stream);

    [[nodiscard]] U32 GetReg(IR::Reg reg);
    void SetReg(IR::Reg reg, const U32& value);

    [[nodiscard]] U1 GetPred(IR::Pred pred, bool is_negated = false);
    void SetPred(IR::Pred pred, const U1& value);

    [[nodiscard]] U1 GetGotoVariable(u32 id);
    void SetGotoVariable(u32 id, const U1& value);

    [[nodiscard]] U32 GetIndirectBranchVariable();
    void SetIndirectBranchVariable(const U32& value);

    [[nodiscard]] U32 GetCbuf(const U32& binding, const U32& byte_offset);
    [[nodiscard]] Value GetCbuf(const U32& binding, const U32& byte_offset, size_t bitsize,
                                bool is_signed);
    [[nodiscard]] F32 GetFloatCbuf(const U32& binding, const U32& byte_offset);

    [[nodiscard]] U1 GetZFlag();
    [[nodiscard]] U1 GetSFlag();
    [[nodiscard]] U1 GetCFlag();
    [[nodiscard]] U1 GetOFlag();

    void SetZFlag(const U1& value);
    void SetSFlag(const U1& value);
    void SetCFlag(const U1& value);
    void SetOFlag(const U1& value);

    [[nodiscard]] U1 Condition(IR::Condition cond);
    [[nodiscard]] U1 GetFlowTestResult(FlowTest test);

    [[nodiscard]] F32 GetAttribute(IR::Attribute attribute);
    [[nodiscard]] F32 GetAttribute(IR::Attribute attribute, const U32& vertex);
    void SetAttribute(IR::Attribute attribute, const F32& value, const U32& vertex);

    [[nodiscard]] F32 GetAttributeIndexed(const U32& phys_address);
    [[nodiscard]] F32 GetAttributeIndexed(const U32& phys_address, const U32& vertex);
    void SetAttributeIndexed(const U32& phys_address, const F32& value, const U32& vertex);

    [[nodiscard]] F32 GetPatch(Patch patch);
    void SetPatch(Patch patch, const F32& value);

    void SetFragColor(u32 index, u32 component, const F32& value);
    void SetSampleMask(const U32& value);
    void SetFragDepth(const F32& value);

    [[nodiscard]] U32 WorkgroupIdX();
    [[nodiscard]] U32 WorkgroupIdY();
    [[nodiscard]] U32 WorkgroupIdZ();

    [[nodiscard]] Value LocalInvocationId();
    [[nodiscard]] U32 LocalInvocationIdX();
    [[nodiscard]] U32 LocalInvocationIdY();
    [[nodiscard]] U32 LocalInvocationIdZ();

    [[nodiscard]] U32 InvocationId();
    [[nodiscard]] U32 SampleId();
    [[nodiscard]] U1 IsHelperInvocation();
    [[nodiscard]] F32 YDirection();

    [[nodiscard]] U32 LaneId();

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

    [[nodiscard]] U32 LoadLocal(const U32& word_offset);
    void WriteLocal(const U32& word_offset, const U32& value);

    [[nodiscard]] Value LoadShared(int bit_size, bool is_signed, const U32& offset);
    void WriteShared(int bit_size, const U32& offset, const Value& value);

    [[nodiscard]] U1 GetZeroFromOp(const Value& op);
    [[nodiscard]] U1 GetSignFromOp(const Value& op);
    [[nodiscard]] U1 GetCarryFromOp(const Value& op);
    [[nodiscard]] U1 GetOverflowFromOp(const Value& op);
    [[nodiscard]] U1 GetSparseFromOp(const Value& op);
    [[nodiscard]] U1 GetInBoundsFromOp(const Value& op);

    [[nodiscard]] Value CompositeConstruct(const Value& e1, const Value& e2);
    [[nodiscard]] Value CompositeConstruct(const Value& e1, const Value& e2, const Value& e3);
    [[nodiscard]] Value CompositeConstruct(const Value& e1, const Value& e2, const Value& e3,
                                           const Value& e4);
    [[nodiscard]] Value CompositeExtract(const Value& vector, size_t element);
    [[nodiscard]] Value CompositeInsert(const Value& vector, const Value& object, size_t element);

    [[nodiscard]] Value Select(const U1& condition, const Value& true_value,
                               const Value& false_value);

    void Barrier();
    void WorkgroupMemoryBarrier();
    void DeviceMemoryBarrier();

    template <typename Dest, typename Source>
    [[nodiscard]] Dest BitCast(const Source& value);

    [[nodiscard]] U64 PackUint2x32(const Value& vector);
    [[nodiscard]] Value UnpackUint2x32(const U64& value);

    [[nodiscard]] U32 PackFloat2x16(const Value& vector);
    [[nodiscard]] Value UnpackFloat2x16(const U32& value);

    [[nodiscard]] U32 PackHalf2x16(const Value& vector);
    [[nodiscard]] Value UnpackHalf2x16(const U32& value);

    [[nodiscard]] F64 PackDouble2x32(const Value& vector);
    [[nodiscard]] Value UnpackDouble2x32(const F64& value);

    [[nodiscard]] F16F32F64 FPAdd(const F16F32F64& a, const F16F32F64& b, FpControl control = {});
    [[nodiscard]] F16F32F64 FPMul(const F16F32F64& a, const F16F32F64& b, FpControl control = {});
    [[nodiscard]] F16F32F64 FPFma(const F16F32F64& a, const F16F32F64& b, const F16F32F64& c,
                                  FpControl control = {});

    [[nodiscard]] F16F32F64 FPAbs(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPNeg(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPAbsNeg(const F16F32F64& value, bool abs, bool neg);

    [[nodiscard]] F32 FPCos(const F32& value);
    [[nodiscard]] F32 FPSin(const F32& value);
    [[nodiscard]] F32 FPExp2(const F32& value);
    [[nodiscard]] F32 FPLog2(const F32& value);
    [[nodiscard]] F32F64 FPRecip(const F32F64& value);
    [[nodiscard]] F32F64 FPRecipSqrt(const F32F64& value);
    [[nodiscard]] F32 FPSqrt(const F32& value);
    [[nodiscard]] F16F32F64 FPSaturate(const F16F32F64& value);
    [[nodiscard]] F16F32F64 FPClamp(const F16F32F64& value, const F16F32F64& min_value,
                                    const F16F32F64& max_value);
    [[nodiscard]] F16F32F64 FPRoundEven(const F16F32F64& value, FpControl control = {});
    [[nodiscard]] F16F32F64 FPFloor(const F16F32F64& value, FpControl control = {});
    [[nodiscard]] F16F32F64 FPCeil(const F16F32F64& value, FpControl control = {});
    [[nodiscard]] F16F32F64 FPTrunc(const F16F32F64& value, FpControl control = {});

    [[nodiscard]] U1 FPEqual(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control = {},
                             bool ordered = true);
    [[nodiscard]] U1 FPNotEqual(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control = {},
                                bool ordered = true);
    [[nodiscard]] U1 FPLessThan(const F16F32F64& lhs, const F16F32F64& rhs, FpControl control = {},
                                bool ordered = true);
    [[nodiscard]] U1 FPGreaterThan(const F16F32F64& lhs, const F16F32F64& rhs,
                                   FpControl control = {}, bool ordered = true);
    [[nodiscard]] U1 FPLessThanEqual(const F16F32F64& lhs, const F16F32F64& rhs,
                                     FpControl control = {}, bool ordered = true);
    [[nodiscard]] U1 FPGreaterThanEqual(const F16F32F64& lhs, const F16F32F64& rhs,
                                        FpControl control = {}, bool ordered = true);
    [[nodiscard]] U1 FPIsNan(const F16F32F64& value);
    [[nodiscard]] U1 FPOrdered(const F16F32F64& lhs, const F16F32F64& rhs);
    [[nodiscard]] U1 FPUnordered(const F16F32F64& lhs, const F16F32F64& rhs);
    [[nodiscard]] F32F64 FPMax(const F32F64& lhs, const F32F64& rhs, FpControl control = {});
    [[nodiscard]] F32F64 FPMin(const F32F64& lhs, const F32F64& rhs, FpControl control = {});

    [[nodiscard]] U32U64 IAdd(const U32U64& a, const U32U64& b);
    [[nodiscard]] U32U64 ISub(const U32U64& a, const U32U64& b);
    [[nodiscard]] U32 IMul(const U32& a, const U32& b);
    [[nodiscard]] U32U64 INeg(const U32U64& value);
    [[nodiscard]] U32 IAbs(const U32& value);
    [[nodiscard]] U32U64 ShiftLeftLogical(const U32U64& base, const U32& shift);
    [[nodiscard]] U32U64 ShiftRightLogical(const U32U64& base, const U32& shift);
    [[nodiscard]] U32U64 ShiftRightArithmetic(const U32U64& base, const U32& shift);
    [[nodiscard]] U32 BitwiseAnd(const U32& a, const U32& b);
    [[nodiscard]] U32 BitwiseOr(const U32& a, const U32& b);
    [[nodiscard]] U32 BitwiseXor(const U32& a, const U32& b);
    [[nodiscard]] U32 BitFieldInsert(const U32& base, const U32& insert, const U32& offset,
                                     const U32& count);
    [[nodiscard]] U32 BitFieldExtract(const U32& base, const U32& offset, const U32& count,
                                      bool is_signed = false);
    [[nodiscard]] U32 BitReverse(const U32& value);
    [[nodiscard]] U32 BitCount(const U32& value);
    [[nodiscard]] U32 BitwiseNot(const U32& value);

    [[nodiscard]] U32 FindSMsb(const U32& value);
    [[nodiscard]] U32 FindUMsb(const U32& value);
    [[nodiscard]] U32 SMin(const U32& a, const U32& b);
    [[nodiscard]] U32 UMin(const U32& a, const U32& b);
    [[nodiscard]] U32 IMin(const U32& a, const U32& b, bool is_signed);
    [[nodiscard]] U32 SMax(const U32& a, const U32& b);
    [[nodiscard]] U32 UMax(const U32& a, const U32& b);
    [[nodiscard]] U32 IMax(const U32& a, const U32& b, bool is_signed);
    [[nodiscard]] U32 SClamp(const U32& value, const U32& min, const U32& max);
    [[nodiscard]] U32 UClamp(const U32& value, const U32& min, const U32& max);

    [[nodiscard]] U1 ILessThan(const U32& lhs, const U32& rhs, bool is_signed);
    [[nodiscard]] U1 IEqual(const U32U64& lhs, const U32U64& rhs);
    [[nodiscard]] U1 ILessThanEqual(const U32& lhs, const U32& rhs, bool is_signed);
    [[nodiscard]] U1 IGreaterThan(const U32& lhs, const U32& rhs, bool is_signed);
    [[nodiscard]] U1 INotEqual(const U32& lhs, const U32& rhs);
    [[nodiscard]] U1 IGreaterThanEqual(const U32& lhs, const U32& rhs, bool is_signed);

    [[nodiscard]] U32 SharedAtomicIAdd(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicSMin(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicUMin(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicIMin(const U32& pointer_offset, const U32& value, bool is_signed);
    [[nodiscard]] U32 SharedAtomicSMax(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicUMax(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicIMax(const U32& pointer_offset, const U32& value, bool is_signed);
    [[nodiscard]] U32 SharedAtomicInc(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicDec(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicAnd(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicOr(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32 SharedAtomicXor(const U32& pointer_offset, const U32& value);
    [[nodiscard]] U32U64 SharedAtomicExchange(const U32& pointer_offset, const U32U64& value);

    [[nodiscard]] U32U64 GlobalAtomicIAdd(const U64& pointer_offset, const U32U64& value);
    [[nodiscard]] U32U64 GlobalAtomicSMin(const U64& pointer_offset, const U32U64& value);
    [[nodiscard]] U32U64 GlobalAtomicUMin(const U64& pointer_offset, const U32U64& value);
    [[nodiscard]] U32U64 GlobalAtomicIMin(const U64& pointer_offset, const U32U64& value,
                                          bool is_signed);
    [[nodiscard]] U32U64 GlobalAtomicSMax(const U64& pointer_offset, const U32U64& value);
    [[nodiscard]] U32U64 GlobalAtomicUMax(const U64& pointer_offset, const U32U64& value);
    [[nodiscard]] U32U64 GlobalAtomicIMax(const U64& pointer_offset, const U32U64& value,
                                          bool is_signed);
    [[nodiscard]] U32 GlobalAtomicInc(const U64& pointer_offset, const U32& value);
    [[nodiscard]] U32 GlobalAtomicDec(const U64& pointer_offset, const U32& value);
    [[nodiscard]] U32U64 GlobalAtomicAnd(const U64& pointer_offset, const U32U64& value);
    [[nodiscard]] U32U64 GlobalAtomicOr(const U64& pointer_offset, const U32U64& value);
    [[nodiscard]] U32U64 GlobalAtomicXor(const U64& pointer_offset, const U32U64& value);
    [[nodiscard]] U32U64 GlobalAtomicExchange(const U64& pointer_offset, const U32U64& value);

    [[nodiscard]] F32 GlobalAtomicF32Add(const U64& pointer_offset, const Value& value,
                                         const FpControl control = {});
    [[nodiscard]] Value GlobalAtomicF16x2Add(const U64& pointer_offset, const Value& value,
                                             const FpControl control = {});
    [[nodiscard]] Value GlobalAtomicF16x2Min(const U64& pointer_offset, const Value& value,
                                             const FpControl control = {});
    [[nodiscard]] Value GlobalAtomicF16x2Max(const U64& pointer_offset, const Value& value,
                                             const FpControl control = {});

    [[nodiscard]] U1 LogicalOr(const U1& a, const U1& b);
    [[nodiscard]] U1 LogicalAnd(const U1& a, const U1& b);
    [[nodiscard]] U1 LogicalXor(const U1& a, const U1& b);
    [[nodiscard]] U1 LogicalNot(const U1& value);

    [[nodiscard]] U32U64 ConvertFToS(size_t bitsize, const F16F32F64& value);
    [[nodiscard]] U32U64 ConvertFToU(size_t bitsize, const F16F32F64& value);
    [[nodiscard]] U32U64 ConvertFToI(size_t bitsize, bool is_signed, const F16F32F64& value);
    [[nodiscard]] F16F32F64 ConvertSToF(size_t dest_bitsize, size_t src_bitsize, const Value& value,
                                        FpControl control = {});
    [[nodiscard]] F16F32F64 ConvertUToF(size_t dest_bitsize, size_t src_bitsize, const Value& value,
                                        FpControl control = {});
    [[nodiscard]] F16F32F64 ConvertIToF(size_t dest_bitsize, size_t src_bitsize, bool is_signed,
                                        const Value& value, FpControl control = {});

    [[nodiscard]] U32U64 UConvert(size_t result_bitsize, const U32U64& value);
    [[nodiscard]] F16F32F64 FPConvert(size_t result_bitsize, const F16F32F64& value,
                                      FpControl control = {});

    [[nodiscard]] Value ImageSampleImplicitLod(const Value& handle, const Value& coords,
                                               const F32& bias, const Value& offset,
                                               const F32& lod_clamp, TextureInstInfo info);
    [[nodiscard]] Value ImageSampleExplicitLod(const Value& handle, const Value& coords,
                                               const F32& lod, const Value& offset,
                                               TextureInstInfo info);
    [[nodiscard]] F32 ImageSampleDrefImplicitLod(const Value& handle, const Value& coords,
                                                 const F32& dref, const F32& bias,
                                                 const Value& offset, const F32& lod_clamp,
                                                 TextureInstInfo info);
    [[nodiscard]] F32 ImageSampleDrefExplicitLod(const Value& handle, const Value& coords,
                                                 const F32& dref, const F32& lod,
                                                 const Value& offset, TextureInstInfo info);
    [[nodiscard]] Value ImageQueryDimension(const Value& handle, const IR::U32& lod);

    [[nodiscard]] Value ImageQueryLod(const Value& handle, const Value& coords,
                                      TextureInstInfo info);
    [[nodiscard]] Value ImageGather(const Value& handle, const Value& coords, const Value& offset,
                                    const Value& offset2, TextureInstInfo info);
    [[nodiscard]] Value ImageGatherDref(const Value& handle, const Value& coords,
                                        const Value& offset, const Value& offset2, const F32& dref,
                                        TextureInstInfo info);
    [[nodiscard]] Value ImageFetch(const Value& handle, const Value& coords, const Value& offset,
                                   const U32& lod, const U32& multisampling, TextureInstInfo info);
    [[nodiscard]] Value ImageGradient(const Value& handle, const Value& coords,
                                      const Value& derivates, const Value& offset,
                                      const F32& lod_clamp, TextureInstInfo info);
    [[nodiscard]] Value ImageRead(const Value& handle, const Value& coords, TextureInstInfo info);
    [[nodiscard]] void ImageWrite(const Value& handle, const Value& coords, const Value& color,
                                  TextureInstInfo info);

    [[nodiscard]] Value ImageAtomicIAdd(const Value& handle, const Value& coords,
                                        const Value& value, TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicSMin(const Value& handle, const Value& coords,
                                        const Value& value, TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicUMin(const Value& handle, const Value& coords,
                                        const Value& value, TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicIMin(const Value& handle, const Value& coords,
                                        const Value& value, bool is_signed, TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicSMax(const Value& handle, const Value& coords,
                                        const Value& value, TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicUMax(const Value& handle, const Value& coords,
                                        const Value& value, TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicIMax(const Value& handle, const Value& coords,
                                        const Value& value, bool is_signed, TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicInc(const Value& handle, const Value& coords, const Value& value,
                                       TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicDec(const Value& handle, const Value& coords, const Value& value,
                                       TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicAnd(const Value& handle, const Value& coords, const Value& value,
                                       TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicOr(const Value& handle, const Value& coords, const Value& value,
                                      TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicXor(const Value& handle, const Value& coords, const Value& value,
                                       TextureInstInfo info);
    [[nodiscard]] Value ImageAtomicExchange(const Value& handle, const Value& coords,
                                            const Value& value, TextureInstInfo info);
    [[nodiscard]] U1 VoteAll(const U1& value);
    [[nodiscard]] U1 VoteAny(const U1& value);
    [[nodiscard]] U1 VoteEqual(const U1& value);
    [[nodiscard]] U32 SubgroupBallot(const U1& value);
    [[nodiscard]] U32 SubgroupEqMask();
    [[nodiscard]] U32 SubgroupLtMask();
    [[nodiscard]] U32 SubgroupLeMask();
    [[nodiscard]] U32 SubgroupGtMask();
    [[nodiscard]] U32 SubgroupGeMask();
    [[nodiscard]] U32 ShuffleIndex(const IR::U32& value, const IR::U32& index, const IR::U32& clamp,
                                   const IR::U32& seg_mask);
    [[nodiscard]] U32 ShuffleUp(const IR::U32& value, const IR::U32& index, const IR::U32& clamp,
                                const IR::U32& seg_mask);
    [[nodiscard]] U32 ShuffleDown(const IR::U32& value, const IR::U32& index, const IR::U32& clamp,
                                  const IR::U32& seg_mask);
    [[nodiscard]] U32 ShuffleButterfly(const IR::U32& value, const IR::U32& index,
                                       const IR::U32& clamp, const IR::U32& seg_mask);
    [[nodiscard]] F32 FSwizzleAdd(const F32& a, const F32& b, const U32& swizzle,
                                  FpControl control = {});

    [[nodiscard]] F32 DPdxFine(const F32& a);

    [[nodiscard]] F32 DPdyFine(const F32& a);

    [[nodiscard]] F32 DPdxCoarse(const F32& a);

    [[nodiscard]] F32 DPdyCoarse(const F32& a);

private:
    IR::Block::iterator insertion_point;

    template <typename T = Value, typename... Args>
    T Inst(Opcode op, Args... args) {
        auto it{block->PrependNewInst(insertion_point, op, {Value{args}...})};
        return T{Value{&*it}};
    }

    template <typename T>
    requires(sizeof(T) <= sizeof(u32) && std::is_trivially_copyable_v<T>) struct Flags {
        Flags() = default;
        Flags(T proxy_) : proxy{proxy_} {}

        T proxy;
    };

    template <typename T = Value, typename FlagType, typename... Args>
    T Inst(Opcode op, Flags<FlagType> flags, Args... args) {
        u32 raw_flags{};
        std::memcpy(&raw_flags, &flags.proxy, sizeof(flags.proxy));
        auto it{block->PrependNewInst(insertion_point, op, {Value{args}...}, raw_flags)};
        return T{Value{&*it}};
    }
};

} // namespace Shader::IR
