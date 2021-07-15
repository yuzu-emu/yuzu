// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sirit/sirit.h>

#include "common/common_types.h"

namespace Shader::IR {
enum class Attribute : u64;
enum class Patch : u64;
class Inst;
class Value;
} // namespace Shader::IR

namespace Shader::Backend::SPIRV {

using Sirit::Id;

class EmitContext;

// Microinstruction emitters
Id EmitPhi(EmitContext& ctx, IR::Inst* inst);
void EmitVoid(EmitContext& ctx);
Id EmitIdentity(EmitContext& ctx, const IR::Value& value);
Id EmitConditionRef(EmitContext& ctx, const IR::Value& value);
void EmitReference(EmitContext&);
void EmitPhiMove(EmitContext&);
void EmitJoin(EmitContext& ctx);
void EmitDemoteToHelperInvocation(EmitContext& ctx);
void EmitBarrier(EmitContext& ctx);
void EmitWorkgroupMemoryBarrier(EmitContext& ctx);
void EmitDeviceMemoryBarrier(EmitContext& ctx);
void EmitPrologue(EmitContext& ctx);
void EmitEpilogue(EmitContext& ctx);
void EmitEmitVertex(EmitContext& ctx, const IR::Value& stream);
void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream);
void EmitGetRegister(EmitContext& ctx);
void EmitSetRegister(EmitContext& ctx);
void EmitGetPred(EmitContext& ctx);
void EmitSetPred(EmitContext& ctx);
void EmitSetGotoVariable(EmitContext& ctx);
void EmitGetGotoVariable(EmitContext& ctx);
void EmitSetIndirectBranchVariable(EmitContext& ctx);
void EmitGetIndirectBranchVariable(EmitContext& ctx);
Id EmitGetCbufU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitGetCbufS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitGetCbufU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitGetCbufS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitGetCbufU32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitGetCbufF32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitGetCbufU32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitGetAttribute(EmitContext& ctx, IR::Attribute attr, Id vertex);
void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, Id value, Id vertex);
Id EmitGetAttributeIndexed(EmitContext& ctx, Id offset, Id vertex);
void EmitSetAttributeIndexed(EmitContext& ctx, Id offset, Id value, Id vertex);
Id EmitGetPatch(EmitContext& ctx, IR::Patch patch);
void EmitSetPatch(EmitContext& ctx, IR::Patch patch, Id value);
void EmitSetFragColor(EmitContext& ctx, u32 index, u32 component, Id value);
void EmitSetSampleMask(EmitContext& ctx, Id value);
void EmitSetFragDepth(EmitContext& ctx, Id value);
void EmitGetZFlag(EmitContext& ctx);
void EmitGetSFlag(EmitContext& ctx);
void EmitGetCFlag(EmitContext& ctx);
void EmitGetOFlag(EmitContext& ctx);
void EmitSetZFlag(EmitContext& ctx);
void EmitSetSFlag(EmitContext& ctx);
void EmitSetCFlag(EmitContext& ctx);
void EmitSetOFlag(EmitContext& ctx);
Id EmitWorkgroupId(EmitContext& ctx);
Id EmitLocalInvocationId(EmitContext& ctx);
Id EmitInvocationId(EmitContext& ctx);
Id EmitSampleId(EmitContext& ctx);
Id EmitIsHelperInvocation(EmitContext& ctx);
Id EmitYDirection(EmitContext& ctx);
Id EmitLoadLocal(EmitContext& ctx, Id word_offset);
void EmitWriteLocal(EmitContext& ctx, Id word_offset, Id value);
Id EmitUndefU1(EmitContext& ctx);
Id EmitUndefU8(EmitContext& ctx);
Id EmitUndefU16(EmitContext& ctx);
Id EmitUndefU32(EmitContext& ctx);
Id EmitUndefU64(EmitContext& ctx);
void EmitLoadGlobalU8(EmitContext& ctx);
void EmitLoadGlobalS8(EmitContext& ctx);
void EmitLoadGlobalU16(EmitContext& ctx);
void EmitLoadGlobalS16(EmitContext& ctx);
Id EmitLoadGlobal32(EmitContext& ctx, Id address);
Id EmitLoadGlobal64(EmitContext& ctx, Id address);
Id EmitLoadGlobal128(EmitContext& ctx, Id address);
void EmitWriteGlobalU8(EmitContext& ctx);
void EmitWriteGlobalS8(EmitContext& ctx);
void EmitWriteGlobalU16(EmitContext& ctx);
void EmitWriteGlobalS16(EmitContext& ctx);
void EmitWriteGlobal32(EmitContext& ctx, Id address, Id value);
void EmitWriteGlobal64(EmitContext& ctx, Id address, Id value);
void EmitWriteGlobal128(EmitContext& ctx, Id address, Id value);
Id EmitLoadStorageU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitLoadStorageS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitLoadStorageU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitLoadStorageS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitLoadStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitLoadStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
Id EmitLoadStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
void EmitWriteStorageU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value);
void EmitWriteStorageS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value);
void EmitWriteStorageU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value);
void EmitWriteStorageS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value);
void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value);
void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value);
void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value);
Id EmitLoadSharedU8(EmitContext& ctx, Id offset);
Id EmitLoadSharedS8(EmitContext& ctx, Id offset);
Id EmitLoadSharedU16(EmitContext& ctx, Id offset);
Id EmitLoadSharedS16(EmitContext& ctx, Id offset);
Id EmitLoadSharedU32(EmitContext& ctx, Id offset);
Id EmitLoadSharedU64(EmitContext& ctx, Id offset);
Id EmitLoadSharedU128(EmitContext& ctx, Id offset);
void EmitWriteSharedU8(EmitContext& ctx, Id offset, Id value);
void EmitWriteSharedU16(EmitContext& ctx, Id offset, Id value);
void EmitWriteSharedU32(EmitContext& ctx, Id offset, Id value);
void EmitWriteSharedU64(EmitContext& ctx, Id offset, Id value);
void EmitWriteSharedU128(EmitContext& ctx, Id offset, Id value);
Id EmitCompositeConstructU32x2(EmitContext& ctx, Id e1, Id e2);
Id EmitCompositeConstructU32x3(EmitContext& ctx, Id e1, Id e2, Id e3);
Id EmitCompositeConstructU32x4(EmitContext& ctx, Id e1, Id e2, Id e3, Id e4);
Id EmitCompositeExtractU32x2(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeExtractU32x3(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeExtractU32x4(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeInsertU32x2(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeInsertU32x3(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeInsertU32x4(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeConstructF16x2(EmitContext& ctx, Id e1, Id e2);
Id EmitCompositeConstructF16x3(EmitContext& ctx, Id e1, Id e2, Id e3);
Id EmitCompositeConstructF16x4(EmitContext& ctx, Id e1, Id e2, Id e3, Id e4);
Id EmitCompositeExtractF16x2(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeExtractF16x3(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeExtractF16x4(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeInsertF16x2(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeInsertF16x3(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeInsertF16x4(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeConstructF32x2(EmitContext& ctx, Id e1, Id e2);
Id EmitCompositeConstructF32x3(EmitContext& ctx, Id e1, Id e2, Id e3);
Id EmitCompositeConstructF32x4(EmitContext& ctx, Id e1, Id e2, Id e3, Id e4);
Id EmitCompositeExtractF32x2(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeExtractF32x3(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeExtractF32x4(EmitContext& ctx, Id composite, u32 index);
Id EmitCompositeInsertF32x2(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeInsertF32x3(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeInsertF32x4(EmitContext& ctx, Id composite, Id object, u32 index);
void EmitCompositeConstructF64x2(EmitContext& ctx);
void EmitCompositeConstructF64x3(EmitContext& ctx);
void EmitCompositeConstructF64x4(EmitContext& ctx);
void EmitCompositeExtractF64x2(EmitContext& ctx);
void EmitCompositeExtractF64x3(EmitContext& ctx);
void EmitCompositeExtractF64x4(EmitContext& ctx);
Id EmitCompositeInsertF64x2(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeInsertF64x3(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitCompositeInsertF64x4(EmitContext& ctx, Id composite, Id object, u32 index);
Id EmitSelectU1(EmitContext& ctx, Id cond, Id true_value, Id false_value);
Id EmitSelectU8(EmitContext& ctx, Id cond, Id true_value, Id false_value);
Id EmitSelectU16(EmitContext& ctx, Id cond, Id true_value, Id false_value);
Id EmitSelectU32(EmitContext& ctx, Id cond, Id true_value, Id false_value);
Id EmitSelectU64(EmitContext& ctx, Id cond, Id true_value, Id false_value);
Id EmitSelectF16(EmitContext& ctx, Id cond, Id true_value, Id false_value);
Id EmitSelectF32(EmitContext& ctx, Id cond, Id true_value, Id false_value);
Id EmitSelectF64(EmitContext& ctx, Id cond, Id true_value, Id false_value);
void EmitBitCastU16F16(EmitContext& ctx);
Id EmitBitCastU32F32(EmitContext& ctx, Id value);
void EmitBitCastU64F64(EmitContext& ctx);
void EmitBitCastF16U16(EmitContext& ctx);
Id EmitBitCastF32U32(EmitContext& ctx, Id value);
void EmitBitCastF64U64(EmitContext& ctx);
Id EmitPackUint2x32(EmitContext& ctx, Id value);
Id EmitUnpackUint2x32(EmitContext& ctx, Id value);
Id EmitPackFloat2x16(EmitContext& ctx, Id value);
Id EmitUnpackFloat2x16(EmitContext& ctx, Id value);
Id EmitPackHalf2x16(EmitContext& ctx, Id value);
Id EmitUnpackHalf2x16(EmitContext& ctx, Id value);
Id EmitPackDouble2x32(EmitContext& ctx, Id value);
Id EmitUnpackDouble2x32(EmitContext& ctx, Id value);
void EmitGetZeroFromOp(EmitContext& ctx);
void EmitGetSignFromOp(EmitContext& ctx);
void EmitGetCarryFromOp(EmitContext& ctx);
void EmitGetOverflowFromOp(EmitContext& ctx);
void EmitGetSparseFromOp(EmitContext& ctx);
void EmitGetInBoundsFromOp(EmitContext& ctx);
Id EmitFPAbs16(EmitContext& ctx, Id value);
Id EmitFPAbs32(EmitContext& ctx, Id value);
Id EmitFPAbs64(EmitContext& ctx, Id value);
Id EmitFPAdd16(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitFPAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitFPAdd64(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitFPFma16(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c);
Id EmitFPFma32(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c);
Id EmitFPFma64(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c);
Id EmitFPMax32(EmitContext& ctx, Id a, Id b);
Id EmitFPMax64(EmitContext& ctx, Id a, Id b);
Id EmitFPMin32(EmitContext& ctx, Id a, Id b);
Id EmitFPMin64(EmitContext& ctx, Id a, Id b);
Id EmitFPMul16(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitFPMul32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitFPMul64(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitFPNeg16(EmitContext& ctx, Id value);
Id EmitFPNeg32(EmitContext& ctx, Id value);
Id EmitFPNeg64(EmitContext& ctx, Id value);
Id EmitFPSin(EmitContext& ctx, Id value);
Id EmitFPCos(EmitContext& ctx, Id value);
Id EmitFPExp2(EmitContext& ctx, Id value);
Id EmitFPLog2(EmitContext& ctx, Id value);
Id EmitFPRecip32(EmitContext& ctx, Id value);
Id EmitFPRecip64(EmitContext& ctx, Id value);
Id EmitFPRecipSqrt32(EmitContext& ctx, Id value);
Id EmitFPRecipSqrt64(EmitContext& ctx, Id value);
Id EmitFPSqrt(EmitContext& ctx, Id value);
Id EmitFPSaturate16(EmitContext& ctx, Id value);
Id EmitFPSaturate32(EmitContext& ctx, Id value);
Id EmitFPSaturate64(EmitContext& ctx, Id value);
Id EmitFPClamp16(EmitContext& ctx, Id value, Id min_value, Id max_value);
Id EmitFPClamp32(EmitContext& ctx, Id value, Id min_value, Id max_value);
Id EmitFPClamp64(EmitContext& ctx, Id value, Id min_value, Id max_value);
Id EmitFPRoundEven16(EmitContext& ctx, Id value);
Id EmitFPRoundEven32(EmitContext& ctx, Id value);
Id EmitFPRoundEven64(EmitContext& ctx, Id value);
Id EmitFPFloor16(EmitContext& ctx, Id value);
Id EmitFPFloor32(EmitContext& ctx, Id value);
Id EmitFPFloor64(EmitContext& ctx, Id value);
Id EmitFPCeil16(EmitContext& ctx, Id value);
Id EmitFPCeil32(EmitContext& ctx, Id value);
Id EmitFPCeil64(EmitContext& ctx, Id value);
Id EmitFPTrunc16(EmitContext& ctx, Id value);
Id EmitFPTrunc32(EmitContext& ctx, Id value);
Id EmitFPTrunc64(EmitContext& ctx, Id value);
Id EmitFPOrdEqual16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdEqual32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdEqual64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordEqual16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordEqual32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordEqual64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdNotEqual16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdNotEqual32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdNotEqual64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordNotEqual16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordNotEqual32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordNotEqual64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdLessThan16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdLessThan32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdLessThan64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordLessThan16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordLessThan32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordLessThan64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdGreaterThan16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdGreaterThan32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdGreaterThan64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordGreaterThan16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordGreaterThan32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordGreaterThan64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdLessThanEqual16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdLessThanEqual32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdLessThanEqual64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordLessThanEqual16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordLessThanEqual32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordLessThanEqual64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdGreaterThanEqual16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdGreaterThanEqual32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPOrdGreaterThanEqual64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordGreaterThanEqual16(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordGreaterThanEqual32(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPUnordGreaterThanEqual64(EmitContext& ctx, Id lhs, Id rhs);
Id EmitFPIsNan16(EmitContext& ctx, Id value);
Id EmitFPIsNan32(EmitContext& ctx, Id value);
Id EmitFPIsNan64(EmitContext& ctx, Id value);
Id EmitIAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitIAdd64(EmitContext& ctx, Id a, Id b);
Id EmitISub32(EmitContext& ctx, Id a, Id b);
Id EmitISub64(EmitContext& ctx, Id a, Id b);
Id EmitIMul32(EmitContext& ctx, Id a, Id b);
Id EmitINeg32(EmitContext& ctx, Id value);
Id EmitINeg64(EmitContext& ctx, Id value);
Id EmitIAbs32(EmitContext& ctx, Id value);
Id EmitShiftLeftLogical32(EmitContext& ctx, Id base, Id shift);
Id EmitShiftLeftLogical64(EmitContext& ctx, Id base, Id shift);
Id EmitShiftRightLogical32(EmitContext& ctx, Id base, Id shift);
Id EmitShiftRightLogical64(EmitContext& ctx, Id base, Id shift);
Id EmitShiftRightArithmetic32(EmitContext& ctx, Id base, Id shift);
Id EmitShiftRightArithmetic64(EmitContext& ctx, Id base, Id shift);
Id EmitBitwiseAnd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitBitwiseOr32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitBitwiseXor32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
Id EmitBitFieldInsert(EmitContext& ctx, Id base, Id insert, Id offset, Id count);
Id EmitBitFieldSExtract(EmitContext& ctx, IR::Inst* inst, Id base, Id offset, Id count);
Id EmitBitFieldUExtract(EmitContext& ctx, IR::Inst* inst, Id base, Id offset, Id count);
Id EmitBitReverse32(EmitContext& ctx, Id value);
Id EmitBitCount32(EmitContext& ctx, Id value);
Id EmitBitwiseNot32(EmitContext& ctx, Id value);
Id EmitFindSMsb32(EmitContext& ctx, Id value);
Id EmitFindUMsb32(EmitContext& ctx, Id value);
Id EmitSMin32(EmitContext& ctx, Id a, Id b);
Id EmitUMin32(EmitContext& ctx, Id a, Id b);
Id EmitSMax32(EmitContext& ctx, Id a, Id b);
Id EmitUMax32(EmitContext& ctx, Id a, Id b);
Id EmitSClamp32(EmitContext& ctx, IR::Inst* inst, Id value, Id min, Id max);
Id EmitUClamp32(EmitContext& ctx, IR::Inst* inst, Id value, Id min, Id max);
Id EmitSLessThan(EmitContext& ctx, Id lhs, Id rhs);
Id EmitULessThan(EmitContext& ctx, Id lhs, Id rhs);
Id EmitIEqual(EmitContext& ctx, Id lhs, Id rhs);
Id EmitSLessThanEqual(EmitContext& ctx, Id lhs, Id rhs);
Id EmitULessThanEqual(EmitContext& ctx, Id lhs, Id rhs);
Id EmitSGreaterThan(EmitContext& ctx, Id lhs, Id rhs);
Id EmitUGreaterThan(EmitContext& ctx, Id lhs, Id rhs);
Id EmitINotEqual(EmitContext& ctx, Id lhs, Id rhs);
Id EmitSGreaterThanEqual(EmitContext& ctx, Id lhs, Id rhs);
Id EmitUGreaterThanEqual(EmitContext& ctx, Id lhs, Id rhs);
Id EmitSharedAtomicIAdd32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicSMin32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicUMin32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicSMax32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicUMax32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicInc32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicDec32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicAnd32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicOr32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicXor32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicExchange32(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitSharedAtomicExchange64(EmitContext& ctx, Id pointer_offset, Id value);
Id EmitStorageAtomicIAdd32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicSMin32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicUMin32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicSMax32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicUMax32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicInc32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value);
Id EmitStorageAtomicDec32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value);
Id EmitStorageAtomicAnd32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value);
Id EmitStorageAtomicOr32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value);
Id EmitStorageAtomicXor32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value);
Id EmitStorageAtomicExchange32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               Id value);
Id EmitStorageAtomicIAdd64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicSMin64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicUMin64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicSMax64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicUMax64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicAnd64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value);
Id EmitStorageAtomicOr64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value);
Id EmitStorageAtomicXor64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value);
Id EmitStorageAtomicExchange64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               Id value);
Id EmitStorageAtomicAddF32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value);
Id EmitStorageAtomicAddF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value);
Id EmitStorageAtomicAddF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value);
Id EmitStorageAtomicMinF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value);
Id EmitStorageAtomicMinF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value);
Id EmitStorageAtomicMaxF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value);
Id EmitStorageAtomicMaxF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value);
Id EmitGlobalAtomicIAdd32(EmitContext& ctx);
Id EmitGlobalAtomicSMin32(EmitContext& ctx);
Id EmitGlobalAtomicUMin32(EmitContext& ctx);
Id EmitGlobalAtomicSMax32(EmitContext& ctx);
Id EmitGlobalAtomicUMax32(EmitContext& ctx);
Id EmitGlobalAtomicInc32(EmitContext& ctx);
Id EmitGlobalAtomicDec32(EmitContext& ctx);
Id EmitGlobalAtomicAnd32(EmitContext& ctx);
Id EmitGlobalAtomicOr32(EmitContext& ctx);
Id EmitGlobalAtomicXor32(EmitContext& ctx);
Id EmitGlobalAtomicExchange32(EmitContext& ctx);
Id EmitGlobalAtomicIAdd64(EmitContext& ctx);
Id EmitGlobalAtomicSMin64(EmitContext& ctx);
Id EmitGlobalAtomicUMin64(EmitContext& ctx);
Id EmitGlobalAtomicSMax64(EmitContext& ctx);
Id EmitGlobalAtomicUMax64(EmitContext& ctx);
Id EmitGlobalAtomicInc64(EmitContext& ctx);
Id EmitGlobalAtomicDec64(EmitContext& ctx);
Id EmitGlobalAtomicAnd64(EmitContext& ctx);
Id EmitGlobalAtomicOr64(EmitContext& ctx);
Id EmitGlobalAtomicXor64(EmitContext& ctx);
Id EmitGlobalAtomicExchange64(EmitContext& ctx);
Id EmitGlobalAtomicAddF32(EmitContext& ctx);
Id EmitGlobalAtomicAddF16x2(EmitContext& ctx);
Id EmitGlobalAtomicAddF32x2(EmitContext& ctx);
Id EmitGlobalAtomicMinF16x2(EmitContext& ctx);
Id EmitGlobalAtomicMinF32x2(EmitContext& ctx);
Id EmitGlobalAtomicMaxF16x2(EmitContext& ctx);
Id EmitGlobalAtomicMaxF32x2(EmitContext& ctx);
Id EmitLogicalOr(EmitContext& ctx, Id a, Id b);
Id EmitLogicalAnd(EmitContext& ctx, Id a, Id b);
Id EmitLogicalXor(EmitContext& ctx, Id a, Id b);
Id EmitLogicalNot(EmitContext& ctx, Id value);
Id EmitConvertS16F16(EmitContext& ctx, Id value);
Id EmitConvertS16F32(EmitContext& ctx, Id value);
Id EmitConvertS16F64(EmitContext& ctx, Id value);
Id EmitConvertS32F16(EmitContext& ctx, Id value);
Id EmitConvertS32F32(EmitContext& ctx, Id value);
Id EmitConvertS32F64(EmitContext& ctx, Id value);
Id EmitConvertS64F16(EmitContext& ctx, Id value);
Id EmitConvertS64F32(EmitContext& ctx, Id value);
Id EmitConvertS64F64(EmitContext& ctx, Id value);
Id EmitConvertU16F16(EmitContext& ctx, Id value);
Id EmitConvertU16F32(EmitContext& ctx, Id value);
Id EmitConvertU16F64(EmitContext& ctx, Id value);
Id EmitConvertU32F16(EmitContext& ctx, Id value);
Id EmitConvertU32F32(EmitContext& ctx, Id value);
Id EmitConvertU32F64(EmitContext& ctx, Id value);
Id EmitConvertU64F16(EmitContext& ctx, Id value);
Id EmitConvertU64F32(EmitContext& ctx, Id value);
Id EmitConvertU64F64(EmitContext& ctx, Id value);
Id EmitConvertU64U32(EmitContext& ctx, Id value);
Id EmitConvertU32U64(EmitContext& ctx, Id value);
Id EmitConvertF16F32(EmitContext& ctx, Id value);
Id EmitConvertF32F16(EmitContext& ctx, Id value);
Id EmitConvertF32F64(EmitContext& ctx, Id value);
Id EmitConvertF64F32(EmitContext& ctx, Id value);
Id EmitConvertF16S8(EmitContext& ctx, Id value);
Id EmitConvertF16S16(EmitContext& ctx, Id value);
Id EmitConvertF16S32(EmitContext& ctx, Id value);
Id EmitConvertF16S64(EmitContext& ctx, Id value);
Id EmitConvertF16U8(EmitContext& ctx, Id value);
Id EmitConvertF16U16(EmitContext& ctx, Id value);
Id EmitConvertF16U32(EmitContext& ctx, Id value);
Id EmitConvertF16U64(EmitContext& ctx, Id value);
Id EmitConvertF32S8(EmitContext& ctx, Id value);
Id EmitConvertF32S16(EmitContext& ctx, Id value);
Id EmitConvertF32S32(EmitContext& ctx, Id value);
Id EmitConvertF32S64(EmitContext& ctx, Id value);
Id EmitConvertF32U8(EmitContext& ctx, Id value);
Id EmitConvertF32U16(EmitContext& ctx, Id value);
Id EmitConvertF32U32(EmitContext& ctx, Id value);
Id EmitConvertF32U64(EmitContext& ctx, Id value);
Id EmitConvertF64S8(EmitContext& ctx, Id value);
Id EmitConvertF64S16(EmitContext& ctx, Id value);
Id EmitConvertF64S32(EmitContext& ctx, Id value);
Id EmitConvertF64S64(EmitContext& ctx, Id value);
Id EmitConvertF64U8(EmitContext& ctx, Id value);
Id EmitConvertF64U16(EmitContext& ctx, Id value);
Id EmitConvertF64U32(EmitContext& ctx, Id value);
Id EmitConvertF64U64(EmitContext& ctx, Id value);
Id EmitBindlessImageSampleImplicitLod(EmitContext&);
Id EmitBindlessImageSampleExplicitLod(EmitContext&);
Id EmitBindlessImageSampleDrefImplicitLod(EmitContext&);
Id EmitBindlessImageSampleDrefExplicitLod(EmitContext&);
Id EmitBindlessImageGather(EmitContext&);
Id EmitBindlessImageGatherDref(EmitContext&);
Id EmitBindlessImageFetch(EmitContext&);
Id EmitBindlessImageQueryDimensions(EmitContext&);
Id EmitBindlessImageQueryLod(EmitContext&);
Id EmitBindlessImageGradient(EmitContext&);
Id EmitBindlessImageRead(EmitContext&);
Id EmitBindlessImageWrite(EmitContext&);
Id EmitBoundImageSampleImplicitLod(EmitContext&);
Id EmitBoundImageSampleExplicitLod(EmitContext&);
Id EmitBoundImageSampleDrefImplicitLod(EmitContext&);
Id EmitBoundImageSampleDrefExplicitLod(EmitContext&);
Id EmitBoundImageGather(EmitContext&);
Id EmitBoundImageGatherDref(EmitContext&);
Id EmitBoundImageFetch(EmitContext&);
Id EmitBoundImageQueryDimensions(EmitContext&);
Id EmitBoundImageQueryLod(EmitContext&);
Id EmitBoundImageGradient(EmitContext&);
Id EmitBoundImageRead(EmitContext&);
Id EmitBoundImageWrite(EmitContext&);
Id EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                              Id bias_lc, const IR::Value& offset);
Id EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                              Id lod, const IR::Value& offset);
Id EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                  Id coords, Id dref, Id bias_lc, const IR::Value& offset);
Id EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                  Id coords, Id dref, Id lod, const IR::Value& offset);
Id EmitImageGather(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                   const IR::Value& offset, const IR::Value& offset2);
Id EmitImageGatherDref(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                       const IR::Value& offset, const IR::Value& offset2, Id dref);
Id EmitImageFetch(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords, Id offset,
                  Id lod, Id ms);
Id EmitImageQueryDimensions(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id lod);
Id EmitImageQueryLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords);
Id EmitImageGradient(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                     Id derivates, Id offset, Id lod_clamp);
Id EmitImageRead(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords);
void EmitImageWrite(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords, Id color);
Id EmitBindlessImageAtomicIAdd32(EmitContext&);
Id EmitBindlessImageAtomicSMin32(EmitContext&);
Id EmitBindlessImageAtomicUMin32(EmitContext&);
Id EmitBindlessImageAtomicSMax32(EmitContext&);
Id EmitBindlessImageAtomicUMax32(EmitContext&);
Id EmitBindlessImageAtomicInc32(EmitContext&);
Id EmitBindlessImageAtomicDec32(EmitContext&);
Id EmitBindlessImageAtomicAnd32(EmitContext&);
Id EmitBindlessImageAtomicOr32(EmitContext&);
Id EmitBindlessImageAtomicXor32(EmitContext&);
Id EmitBindlessImageAtomicExchange32(EmitContext&);
Id EmitBoundImageAtomicIAdd32(EmitContext&);
Id EmitBoundImageAtomicSMin32(EmitContext&);
Id EmitBoundImageAtomicUMin32(EmitContext&);
Id EmitBoundImageAtomicSMax32(EmitContext&);
Id EmitBoundImageAtomicUMax32(EmitContext&);
Id EmitBoundImageAtomicInc32(EmitContext&);
Id EmitBoundImageAtomicDec32(EmitContext&);
Id EmitBoundImageAtomicAnd32(EmitContext&);
Id EmitBoundImageAtomicOr32(EmitContext&);
Id EmitBoundImageAtomicXor32(EmitContext&);
Id EmitBoundImageAtomicExchange32(EmitContext&);
Id EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value);
Id EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value);
Id EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value);
Id EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value);
Id EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                         Id value);
Id EmitImageAtomicInc32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                        Id value);
Id EmitImageAtomicDec32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                        Id value);
Id EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                        Id value);
Id EmitImageAtomicOr32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                       Id value);
Id EmitImageAtomicXor32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                        Id value);
Id EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                             Id value);
Id EmitLaneId(EmitContext& ctx);
Id EmitVoteAll(EmitContext& ctx, Id pred);
Id EmitVoteAny(EmitContext& ctx, Id pred);
Id EmitVoteEqual(EmitContext& ctx, Id pred);
Id EmitSubgroupBallot(EmitContext& ctx, Id pred);
Id EmitSubgroupEqMask(EmitContext& ctx);
Id EmitSubgroupLtMask(EmitContext& ctx);
Id EmitSubgroupLeMask(EmitContext& ctx);
Id EmitSubgroupGtMask(EmitContext& ctx);
Id EmitSubgroupGeMask(EmitContext& ctx);
Id EmitShuffleIndex(EmitContext& ctx, IR::Inst* inst, Id value, Id index, Id clamp,
                    Id segmentation_mask);
Id EmitShuffleUp(EmitContext& ctx, IR::Inst* inst, Id value, Id index, Id clamp,
                 Id segmentation_mask);
Id EmitShuffleDown(EmitContext& ctx, IR::Inst* inst, Id value, Id index, Id clamp,
                   Id segmentation_mask);
Id EmitShuffleButterfly(EmitContext& ctx, IR::Inst* inst, Id value, Id index, Id clamp,
                        Id segmentation_mask);
Id EmitFSwizzleAdd(EmitContext& ctx, Id op_a, Id op_b, Id swizzle);
Id EmitDPdxFine(EmitContext& ctx, Id op_a);
Id EmitDPdyFine(EmitContext& ctx, Id op_a);
Id EmitDPdxCoarse(EmitContext& ctx, Id op_a);
Id EmitDPdyCoarse(EmitContext& ctx, Id op_a);

} // namespace Shader::Backend::SPIRV
