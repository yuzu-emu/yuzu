// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "shader_recompiler/backend/glasm/reg_alloc.h"

namespace Shader::IR {
enum class Attribute : u64;
enum class Patch : u64;
class Inst;
class Value;
} // namespace Shader::IR

namespace Shader::Backend::GLASM {

class EmitContext;

// Microinstruction emitters
void EmitPhi(EmitContext& ctx, IR::Inst& inst);
void EmitVoid(EmitContext& ctx);
void EmitIdentity(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitConditionRef(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitReference(EmitContext&, const IR::Value& value);
void EmitPhiMove(EmitContext& ctx, const IR::Value& phi, const IR::Value& value);
void EmitJoin(EmitContext& ctx);
void EmitDemoteToHelperInvocation(EmitContext& ctx);
void EmitBarrier(EmitContext& ctx);
void EmitWorkgroupMemoryBarrier(EmitContext& ctx);
void EmitDeviceMemoryBarrier(EmitContext& ctx);
void EmitPrologue(EmitContext& ctx);
void EmitEpilogue(EmitContext& ctx);
void EmitEmitVertex(EmitContext& ctx, ScalarS32 stream);
void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream);
void EmitGetRegister(EmitContext& ctx);
void EmitSetRegister(EmitContext& ctx);
void EmitGetPred(EmitContext& ctx);
void EmitSetPred(EmitContext& ctx);
void EmitSetGotoVariable(EmitContext& ctx);
void EmitGetGotoVariable(EmitContext& ctx);
void EmitSetIndirectBranchVariable(EmitContext& ctx);
void EmitGetIndirectBranchVariable(EmitContext& ctx);
void EmitGetCbufU8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset);
void EmitGetCbufS8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset);
void EmitGetCbufU16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset);
void EmitGetCbufS16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset);
void EmitGetCbufU32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset);
void EmitGetCbufF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset);
void EmitGetCbufU32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset);
void EmitGetAttribute(EmitContext& ctx, IR::Inst& inst, IR::Attribute attr, ScalarU32 vertex);
void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, ScalarF32 value, ScalarU32 vertex);
void EmitGetAttributeIndexed(EmitContext& ctx, IR::Inst& inst, ScalarS32 offset, ScalarU32 vertex);
void EmitSetAttributeIndexed(EmitContext& ctx, ScalarU32 offset, ScalarF32 value, ScalarU32 vertex);
void EmitGetPatch(EmitContext& ctx, IR::Inst& inst, IR::Patch patch);
void EmitSetPatch(EmitContext& ctx, IR::Patch patch, ScalarF32 value);
void EmitSetFragColor(EmitContext& ctx, u32 index, u32 component, ScalarF32 value);
void EmitSetSampleMask(EmitContext& ctx, ScalarS32 value);
void EmitSetFragDepth(EmitContext& ctx, ScalarF32 value);
void EmitGetZFlag(EmitContext& ctx);
void EmitGetSFlag(EmitContext& ctx);
void EmitGetCFlag(EmitContext& ctx);
void EmitGetOFlag(EmitContext& ctx);
void EmitSetZFlag(EmitContext& ctx);
void EmitSetSFlag(EmitContext& ctx);
void EmitSetCFlag(EmitContext& ctx);
void EmitSetOFlag(EmitContext& ctx);
void EmitWorkgroupId(EmitContext& ctx, IR::Inst& inst);
void EmitLocalInvocationId(EmitContext& ctx, IR::Inst& inst);
void EmitInvocationId(EmitContext& ctx, IR::Inst& inst);
void EmitSampleId(EmitContext& ctx, IR::Inst& inst);
void EmitIsHelperInvocation(EmitContext& ctx, IR::Inst& inst);
void EmitYDirection(EmitContext& ctx, IR::Inst& inst);
void EmitLoadLocal(EmitContext& ctx, IR::Inst& inst, ScalarU32 word_offset);
void EmitWriteLocal(EmitContext& ctx, ScalarU32 word_offset, ScalarU32 value);
void EmitUndefU1(EmitContext& ctx, IR::Inst& inst);
void EmitUndefU8(EmitContext& ctx, IR::Inst& inst);
void EmitUndefU16(EmitContext& ctx, IR::Inst& inst);
void EmitUndefU32(EmitContext& ctx, IR::Inst& inst);
void EmitUndefU64(EmitContext& ctx, IR::Inst& inst);
void EmitLoadGlobalU8(EmitContext& ctx, IR::Inst& inst, Register address);
void EmitLoadGlobalS8(EmitContext& ctx, IR::Inst& inst, Register address);
void EmitLoadGlobalU16(EmitContext& ctx, IR::Inst& inst, Register address);
void EmitLoadGlobalS16(EmitContext& ctx, IR::Inst& inst, Register address);
void EmitLoadGlobal32(EmitContext& ctx, IR::Inst& inst, Register address);
void EmitLoadGlobal64(EmitContext& ctx, IR::Inst& inst, Register address);
void EmitLoadGlobal128(EmitContext& ctx, IR::Inst& inst, Register address);
void EmitWriteGlobalU8(EmitContext& ctx, Register address, Register value);
void EmitWriteGlobalS8(EmitContext& ctx, Register address, Register value);
void EmitWriteGlobalU16(EmitContext& ctx, Register address, Register value);
void EmitWriteGlobalS16(EmitContext& ctx, Register address, Register value);
void EmitWriteGlobal32(EmitContext& ctx, Register address, ScalarU32 value);
void EmitWriteGlobal64(EmitContext& ctx, Register address, Register value);
void EmitWriteGlobal128(EmitContext& ctx, Register address, Register value);
void EmitLoadStorageU8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       ScalarU32 offset);
void EmitLoadStorageS8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       ScalarU32 offset);
void EmitLoadStorageU16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        ScalarU32 offset);
void EmitLoadStorageS16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        ScalarU32 offset);
void EmitLoadStorage32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       ScalarU32 offset);
void EmitLoadStorage64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       ScalarU32 offset);
void EmitLoadStorage128(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        ScalarU32 offset);
void EmitWriteStorageU8(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        ScalarU32 value);
void EmitWriteStorageS8(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        ScalarS32 value);
void EmitWriteStorageU16(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         ScalarU32 value);
void EmitWriteStorageS16(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         ScalarS32 value);
void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        ScalarU32 value);
void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        Register value);
void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         Register value);
void EmitLoadSharedU8(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset);
void EmitLoadSharedS8(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset);
void EmitLoadSharedU16(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset);
void EmitLoadSharedS16(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset);
void EmitLoadSharedU32(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset);
void EmitLoadSharedU64(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset);
void EmitLoadSharedU128(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset);
void EmitWriteSharedU8(EmitContext& ctx, ScalarU32 offset, ScalarU32 value);
void EmitWriteSharedU16(EmitContext& ctx, ScalarU32 offset, ScalarU32 value);
void EmitWriteSharedU32(EmitContext& ctx, ScalarU32 offset, ScalarU32 value);
void EmitWriteSharedU64(EmitContext& ctx, ScalarU32 offset, Register value);
void EmitWriteSharedU128(EmitContext& ctx, ScalarU32 offset, Register value);
void EmitCompositeConstructU32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2);
void EmitCompositeConstructU32x3(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2, const IR::Value& e3);
void EmitCompositeConstructU32x4(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2, const IR::Value& e3, const IR::Value& e4);
void EmitCompositeExtractU32x2(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index);
void EmitCompositeExtractU32x3(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index);
void EmitCompositeExtractU32x4(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index);
void EmitCompositeInsertU32x2(EmitContext& ctx, Register composite, ScalarU32 object, u32 index);
void EmitCompositeInsertU32x3(EmitContext& ctx, Register composite, ScalarU32 object, u32 index);
void EmitCompositeInsertU32x4(EmitContext& ctx, Register composite, ScalarU32 object, u32 index);
void EmitCompositeConstructF16x2(EmitContext& ctx, Register e1, Register e2);
void EmitCompositeConstructF16x3(EmitContext& ctx, Register e1, Register e2, Register e3);
void EmitCompositeConstructF16x4(EmitContext& ctx, Register e1, Register e2, Register e3,
                                 Register e4);
void EmitCompositeExtractF16x2(EmitContext& ctx, Register composite, u32 index);
void EmitCompositeExtractF16x3(EmitContext& ctx, Register composite, u32 index);
void EmitCompositeExtractF16x4(EmitContext& ctx, Register composite, u32 index);
void EmitCompositeInsertF16x2(EmitContext& ctx, Register composite, Register object, u32 index);
void EmitCompositeInsertF16x3(EmitContext& ctx, Register composite, Register object, u32 index);
void EmitCompositeInsertF16x4(EmitContext& ctx, Register composite, Register object, u32 index);
void EmitCompositeConstructF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2);
void EmitCompositeConstructF32x3(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2, const IR::Value& e3);
void EmitCompositeConstructF32x4(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2, const IR::Value& e3, const IR::Value& e4);
void EmitCompositeExtractF32x2(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index);
void EmitCompositeExtractF32x3(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index);
void EmitCompositeExtractF32x4(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index);
void EmitCompositeInsertF32x2(EmitContext& ctx, IR::Inst& inst, Register composite,
                              ScalarF32 object, u32 index);
void EmitCompositeInsertF32x3(EmitContext& ctx, IR::Inst& inst, Register composite,
                              ScalarF32 object, u32 index);
void EmitCompositeInsertF32x4(EmitContext& ctx, IR::Inst& inst, Register composite,
                              ScalarF32 object, u32 index);
void EmitCompositeConstructF64x2(EmitContext& ctx);
void EmitCompositeConstructF64x3(EmitContext& ctx);
void EmitCompositeConstructF64x4(EmitContext& ctx);
void EmitCompositeExtractF64x2(EmitContext& ctx);
void EmitCompositeExtractF64x3(EmitContext& ctx);
void EmitCompositeExtractF64x4(EmitContext& ctx);
void EmitCompositeInsertF64x2(EmitContext& ctx, Register composite, Register object, u32 index);
void EmitCompositeInsertF64x3(EmitContext& ctx, Register composite, Register object, u32 index);
void EmitCompositeInsertF64x4(EmitContext& ctx, Register composite, Register object, u32 index);
void EmitSelectU1(EmitContext& ctx, IR::Inst& inst, ScalarS32 cond, ScalarS32 true_value,
                  ScalarS32 false_value);
void EmitSelectU8(EmitContext& ctx, ScalarS32 cond, ScalarS32 true_value, ScalarS32 false_value);
void EmitSelectU16(EmitContext& ctx, ScalarS32 cond, ScalarS32 true_value, ScalarS32 false_value);
void EmitSelectU32(EmitContext& ctx, IR::Inst& inst, ScalarS32 cond, ScalarS32 true_value,
                   ScalarS32 false_value);
void EmitSelectU64(EmitContext& ctx, IR::Inst& inst, ScalarS32 cond, Register true_value,
                   Register false_value);
void EmitSelectF16(EmitContext& ctx, ScalarS32 cond, Register true_value, Register false_value);
void EmitSelectF32(EmitContext& ctx, IR::Inst& inst, ScalarS32 cond, ScalarS32 true_value,
                   ScalarS32 false_value);
void EmitSelectF64(EmitContext& ctx, ScalarS32 cond, Register true_value, Register false_value);
void EmitBitCastU16F16(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitBitCastU32F32(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitBitCastU64F64(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitBitCastF16U16(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitBitCastF32U32(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitBitCastF64U64(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitPackUint2x32(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitUnpackUint2x32(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitPackFloat2x16(EmitContext& ctx, Register value);
void EmitUnpackFloat2x16(EmitContext& ctx, Register value);
void EmitPackHalf2x16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitUnpackHalf2x16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitPackDouble2x32(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitUnpackDouble2x32(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitGetZeroFromOp(EmitContext& ctx);
void EmitGetSignFromOp(EmitContext& ctx);
void EmitGetCarryFromOp(EmitContext& ctx);
void EmitGetOverflowFromOp(EmitContext& ctx);
void EmitGetSparseFromOp(EmitContext& ctx);
void EmitGetInBoundsFromOp(EmitContext& ctx);
void EmitFPAbs16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitFPAbs32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPAbs64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitFPAdd16(EmitContext& ctx, IR::Inst& inst, Register a, Register b);
void EmitFPAdd32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b);
void EmitFPAdd64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b);
void EmitFPFma16(EmitContext& ctx, IR::Inst& inst, Register a, Register b, Register c);
void EmitFPFma32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b, ScalarF32 c);
void EmitFPFma64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b, ScalarF64 c);
void EmitFPMax32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b);
void EmitFPMax64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b);
void EmitFPMin32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b);
void EmitFPMin64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b);
void EmitFPMul16(EmitContext& ctx, IR::Inst& inst, Register a, Register b);
void EmitFPMul32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b);
void EmitFPMul64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b);
void EmitFPNeg16(EmitContext& ctx, Register value);
void EmitFPNeg32(EmitContext& ctx, IR::Inst& inst, ScalarRegister value);
void EmitFPNeg64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitFPSin(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPCos(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPExp2(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPLog2(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPRecip32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPRecip64(EmitContext& ctx, Register value);
void EmitFPRecipSqrt32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPRecipSqrt64(EmitContext& ctx, Register value);
void EmitFPSqrt(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPSaturate16(EmitContext& ctx, Register value);
void EmitFPSaturate32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPSaturate64(EmitContext& ctx, Register value);
void EmitFPClamp16(EmitContext& ctx, Register value, Register min_value, Register max_value);
void EmitFPClamp32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value, ScalarF32 min_value,
                   ScalarF32 max_value);
void EmitFPClamp64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value, ScalarF64 min_value,
                   ScalarF64 max_value);
void EmitFPRoundEven16(EmitContext& ctx, Register value);
void EmitFPRoundEven32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPRoundEven64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitFPFloor16(EmitContext& ctx, Register value);
void EmitFPFloor32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPFloor64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitFPCeil16(EmitContext& ctx, Register value);
void EmitFPCeil32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPCeil64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitFPTrunc16(EmitContext& ctx, Register value);
void EmitFPTrunc32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPTrunc64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitFPOrdEqual16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPOrdEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPOrdEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPUnordEqual16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPUnordEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPUnordEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPOrdNotEqual16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPOrdNotEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPOrdNotEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPUnordNotEqual16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPUnordNotEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPUnordNotEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPOrdLessThan16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPOrdLessThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPOrdLessThan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPUnordLessThan16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPUnordLessThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPUnordLessThan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPOrdGreaterThan16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPOrdGreaterThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPOrdGreaterThan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPUnordGreaterThan16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPUnordGreaterThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPUnordGreaterThan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPOrdLessThanEqual16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPOrdLessThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPOrdLessThanEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPUnordLessThanEqual16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPUnordLessThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPUnordLessThanEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPOrdGreaterThanEqual16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPOrdGreaterThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPOrdGreaterThanEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPUnordGreaterThanEqual16(EmitContext& ctx, Register lhs, Register rhs);
void EmitFPUnordGreaterThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs);
void EmitFPUnordGreaterThanEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs);
void EmitFPIsNan16(EmitContext& ctx, Register value);
void EmitFPIsNan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitFPIsNan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitIAdd32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitIAdd64(EmitContext& ctx, IR::Inst& inst, Register a, Register b);
void EmitISub32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitISub64(EmitContext& ctx, IR::Inst& inst, Register a, Register b);
void EmitIMul32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitINeg32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitINeg64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitIAbs32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitShiftLeftLogical32(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 shift);
void EmitShiftLeftLogical64(EmitContext& ctx, IR::Inst& inst, ScalarRegister base, ScalarU32 shift);
void EmitShiftRightLogical32(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 shift);
void EmitShiftRightLogical64(EmitContext& ctx, IR::Inst& inst, ScalarRegister base,
                             ScalarU32 shift);
void EmitShiftRightArithmetic32(EmitContext& ctx, IR::Inst& inst, ScalarS32 base, ScalarS32 shift);
void EmitShiftRightArithmetic64(EmitContext& ctx, IR::Inst& inst, ScalarRegister base,
                                ScalarS32 shift);
void EmitBitwiseAnd32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitBitwiseOr32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitBitwiseXor32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitBitFieldInsert(EmitContext& ctx, IR::Inst& inst, ScalarS32 base, ScalarS32 insert,
                        ScalarS32 offset, ScalarS32 count);
void EmitBitFieldSExtract(EmitContext& ctx, IR::Inst& inst, ScalarS32 base, ScalarS32 offset,
                          ScalarS32 count);
void EmitBitFieldUExtract(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 offset,
                          ScalarU32 count);
void EmitBitReverse32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitBitCount32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitBitwiseNot32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitFindSMsb32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitFindUMsb32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value);
void EmitSMin32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitUMin32(EmitContext& ctx, IR::Inst& inst, ScalarU32 a, ScalarU32 b);
void EmitSMax32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitUMax32(EmitContext& ctx, IR::Inst& inst, ScalarU32 a, ScalarU32 b);
void EmitSClamp32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value, ScalarS32 min, ScalarS32 max);
void EmitUClamp32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 min, ScalarU32 max);
void EmitSLessThan(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs);
void EmitULessThan(EmitContext& ctx, IR::Inst& inst, ScalarU32 lhs, ScalarU32 rhs);
void EmitIEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs);
void EmitSLessThanEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs);
void EmitULessThanEqual(EmitContext& ctx, IR::Inst& inst, ScalarU32 lhs, ScalarU32 rhs);
void EmitSGreaterThan(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs);
void EmitUGreaterThan(EmitContext& ctx, IR::Inst& inst, ScalarU32 lhs, ScalarU32 rhs);
void EmitINotEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs);
void EmitSGreaterThanEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs);
void EmitUGreaterThanEqual(EmitContext& ctx, IR::Inst& inst, ScalarU32 lhs, ScalarU32 rhs);
void EmitSharedAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarU32 value);
void EmitSharedAtomicSMin32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarS32 value);
void EmitSharedAtomicUMin32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarU32 value);
void EmitSharedAtomicSMax32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarS32 value);
void EmitSharedAtomicUMax32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarU32 value);
void EmitSharedAtomicInc32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                           ScalarU32 value);
void EmitSharedAtomicDec32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                           ScalarU32 value);
void EmitSharedAtomicAnd32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                           ScalarU32 value);
void EmitSharedAtomicOr32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                          ScalarU32 value);
void EmitSharedAtomicXor32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                           ScalarU32 value);
void EmitSharedAtomicExchange32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                                ScalarU32 value);
void EmitSharedAtomicExchange64(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                                Register value);
void EmitStorageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarS32 value);
void EmitStorageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarS32 value);
void EmitStorageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 ScalarU32 offset, ScalarU32 value);
void EmitStorageAtomicIAdd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value);
void EmitStorageAtomicSMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value);
void EmitStorageAtomicUMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value);
void EmitStorageAtomicSMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value);
void EmitStorageAtomicUMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value);
void EmitStorageAtomicAnd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, Register value);
void EmitStorageAtomicOr64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           ScalarU32 offset, Register value);
void EmitStorageAtomicXor64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, Register value);
void EmitStorageAtomicExchange64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 ScalarU32 offset, Register value);
void EmitStorageAtomicAddF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarF32 value);
void EmitStorageAtomicAddF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value);
void EmitStorageAtomicAddF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value);
void EmitStorageAtomicMinF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value);
void EmitStorageAtomicMinF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value);
void EmitStorageAtomicMaxF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value);
void EmitStorageAtomicMaxF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value);
void EmitGlobalAtomicIAdd32(EmitContext& ctx);
void EmitGlobalAtomicSMin32(EmitContext& ctx);
void EmitGlobalAtomicUMin32(EmitContext& ctx);
void EmitGlobalAtomicSMax32(EmitContext& ctx);
void EmitGlobalAtomicUMax32(EmitContext& ctx);
void EmitGlobalAtomicInc32(EmitContext& ctx);
void EmitGlobalAtomicDec32(EmitContext& ctx);
void EmitGlobalAtomicAnd32(EmitContext& ctx);
void EmitGlobalAtomicOr32(EmitContext& ctx);
void EmitGlobalAtomicXor32(EmitContext& ctx);
void EmitGlobalAtomicExchange32(EmitContext& ctx);
void EmitGlobalAtomicIAdd64(EmitContext& ctx);
void EmitGlobalAtomicSMin64(EmitContext& ctx);
void EmitGlobalAtomicUMin64(EmitContext& ctx);
void EmitGlobalAtomicSMax64(EmitContext& ctx);
void EmitGlobalAtomicUMax64(EmitContext& ctx);
void EmitGlobalAtomicInc64(EmitContext& ctx);
void EmitGlobalAtomicDec64(EmitContext& ctx);
void EmitGlobalAtomicAnd64(EmitContext& ctx);
void EmitGlobalAtomicOr64(EmitContext& ctx);
void EmitGlobalAtomicXor64(EmitContext& ctx);
void EmitGlobalAtomicExchange64(EmitContext& ctx);
void EmitGlobalAtomicAddF32(EmitContext& ctx);
void EmitGlobalAtomicAddF16x2(EmitContext& ctx);
void EmitGlobalAtomicAddF32x2(EmitContext& ctx);
void EmitGlobalAtomicMinF16x2(EmitContext& ctx);
void EmitGlobalAtomicMinF32x2(EmitContext& ctx);
void EmitGlobalAtomicMaxF16x2(EmitContext& ctx);
void EmitGlobalAtomicMaxF32x2(EmitContext& ctx);
void EmitLogicalOr(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitLogicalAnd(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitLogicalXor(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b);
void EmitLogicalNot(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitConvertS16F16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertS16F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitConvertS16F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitConvertS32F16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertS32F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitConvertS32F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitConvertS64F16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertS64F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitConvertS64F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitConvertU16F16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertU16F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitConvertU16F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitConvertU32F16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertU32F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitConvertU32F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitConvertU64F16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertU64F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitConvertU64F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitConvertU64U32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value);
void EmitConvertU32U64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF16F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitConvertF32F16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF32F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value);
void EmitConvertF64F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value);
void EmitConvertF16S8(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF16S16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF16S32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitConvertF16S64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF16U8(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF16U16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF16U32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value);
void EmitConvertF16U64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF32S8(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF32S16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF32S32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitConvertF32S64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF32U8(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF32U16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF32U32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value);
void EmitConvertF32U64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF64S8(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF64S16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF64S32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value);
void EmitConvertF64S64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF64U8(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF64U16(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitConvertF64U32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value);
void EmitConvertF64U64(EmitContext& ctx, IR::Inst& inst, Register value);
void EmitBindlessImageSampleImplicitLod(EmitContext&);
void EmitBindlessImageSampleExplicitLod(EmitContext&);
void EmitBindlessImageSampleDrefImplicitLod(EmitContext&);
void EmitBindlessImageSampleDrefExplicitLod(EmitContext&);
void EmitBindlessImageGather(EmitContext&);
void EmitBindlessImageGatherDref(EmitContext&);
void EmitBindlessImageFetch(EmitContext&);
void EmitBindlessImageQueryDimensions(EmitContext&);
void EmitBindlessImageQueryLod(EmitContext&);
void EmitBindlessImageGradient(EmitContext&);
void EmitBindlessImageRead(EmitContext&);
void EmitBindlessImageWrite(EmitContext&);
void EmitBoundImageSampleImplicitLod(EmitContext&);
void EmitBoundImageSampleExplicitLod(EmitContext&);
void EmitBoundImageSampleDrefImplicitLod(EmitContext&);
void EmitBoundImageSampleDrefExplicitLod(EmitContext&);
void EmitBoundImageGather(EmitContext&);
void EmitBoundImageGatherDref(EmitContext&);
void EmitBoundImageFetch(EmitContext&);
void EmitBoundImageQueryDimensions(EmitContext&);
void EmitBoundImageQueryLod(EmitContext&);
void EmitBoundImageGradient(EmitContext&);
void EmitBoundImageRead(EmitContext&);
void EmitBoundImageWrite(EmitContext&);
void EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                const IR::Value& coord, Register bias_lc, const IR::Value& offset);
void EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                const IR::Value& coord, ScalarF32 lod, const IR::Value& offset);
void EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    const IR::Value& coord, const IR::Value& dref,
                                    const IR::Value& bias_lc, const IR::Value& offset);
void EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    const IR::Value& coord, const IR::Value& dref,
                                    const IR::Value& lod, const IR::Value& offset);
void EmitImageGather(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                     const IR::Value& coord, const IR::Value& offset, const IR::Value& offset2);
void EmitImageGatherDref(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                         const IR::Value& coord, const IR::Value& offset, const IR::Value& offset2,
                         const IR::Value& dref);
void EmitImageFetch(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                    const IR::Value& coord, const IR::Value& offset, ScalarS32 lod, ScalarS32 ms);
void EmitImageQueryDimensions(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                              ScalarS32 lod);
void EmitImageQueryLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord);
void EmitImageGradient(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                       const IR::Value& coord, const IR::Value& derivatives,
                       const IR::Value& offset, const IR::Value& lod_clamp);
void EmitImageRead(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord);
void EmitImageWrite(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                    Register color);
void EmitBindlessImageAtomicIAdd32(EmitContext&);
void EmitBindlessImageAtomicSMin32(EmitContext&);
void EmitBindlessImageAtomicUMin32(EmitContext&);
void EmitBindlessImageAtomicSMax32(EmitContext&);
void EmitBindlessImageAtomicUMax32(EmitContext&);
void EmitBindlessImageAtomicInc32(EmitContext&);
void EmitBindlessImageAtomicDec32(EmitContext&);
void EmitBindlessImageAtomicAnd32(EmitContext&);
void EmitBindlessImageAtomicOr32(EmitContext&);
void EmitBindlessImageAtomicXor32(EmitContext&);
void EmitBindlessImageAtomicExchange32(EmitContext&);
void EmitBoundImageAtomicIAdd32(EmitContext&);
void EmitBoundImageAtomicSMin32(EmitContext&);
void EmitBoundImageAtomicUMin32(EmitContext&);
void EmitBoundImageAtomicSMax32(EmitContext&);
void EmitBoundImageAtomicUMax32(EmitContext&);
void EmitBoundImageAtomicInc32(EmitContext&);
void EmitBoundImageAtomicDec32(EmitContext&);
void EmitBoundImageAtomicAnd32(EmitContext&);
void EmitBoundImageAtomicOr32(EmitContext&);
void EmitBoundImageAtomicXor32(EmitContext&);
void EmitBoundImageAtomicExchange32(EmitContext&);
void EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarU32 value);
void EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarS32 value);
void EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarU32 value);
void EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarS32 value);
void EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarU32 value);
void EmitImageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                          ScalarU32 value);
void EmitImageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                          ScalarU32 value);
void EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                          ScalarU32 value);
void EmitImageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                         ScalarU32 value);
void EmitImageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                          ScalarU32 value);
void EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                               Register coord, ScalarU32 value);
void EmitLaneId(EmitContext& ctx, IR::Inst& inst);
void EmitVoteAll(EmitContext& ctx, IR::Inst& inst, ScalarS32 pred);
void EmitVoteAny(EmitContext& ctx, IR::Inst& inst, ScalarS32 pred);
void EmitVoteEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 pred);
void EmitSubgroupBallot(EmitContext& ctx, IR::Inst& inst, ScalarS32 pred);
void EmitSubgroupEqMask(EmitContext& ctx, IR::Inst& inst);
void EmitSubgroupLtMask(EmitContext& ctx, IR::Inst& inst);
void EmitSubgroupLeMask(EmitContext& ctx, IR::Inst& inst);
void EmitSubgroupGtMask(EmitContext& ctx, IR::Inst& inst);
void EmitSubgroupGeMask(EmitContext& ctx, IR::Inst& inst);
void EmitShuffleIndex(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                      const IR::Value& clamp, const IR::Value& segmentation_mask);
void EmitShuffleUp(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                   const IR::Value& clamp, const IR::Value& segmentation_mask);
void EmitShuffleDown(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                     const IR::Value& clamp, const IR::Value& segmentation_mask);
void EmitShuffleButterfly(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                          const IR::Value& clamp, const IR::Value& segmentation_mask);
void EmitFSwizzleAdd(EmitContext& ctx, IR::Inst& inst, ScalarF32 op_a, ScalarF32 op_b,
                     ScalarU32 swizzle);
void EmitDPdxFine(EmitContext& ctx, IR::Inst& inst, ScalarF32 op_a);
void EmitDPdyFine(EmitContext& ctx, IR::Inst& inst, ScalarF32 op_a);
void EmitDPdxCoarse(EmitContext& ctx, IR::Inst& inst, ScalarF32 op_a);
void EmitDPdyCoarse(EmitContext& ctx, IR::Inst& inst, ScalarF32 op_a);

} // namespace Shader::Backend::GLASM
