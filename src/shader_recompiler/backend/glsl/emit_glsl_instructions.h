// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string_view>

#include "common/common_types.h"

namespace Shader::IR {
enum class Attribute : u64;
enum class Patch : u64;
class Inst;
class Value;
} // namespace Shader::IR

namespace Shader::Backend::GLSL {
class EmitContext;

#define NotImplemented() throw NotImplementedException("GLSL instruction {}", __func__)

// Microinstruction emitters
void EmitPhi(EmitContext& ctx, IR::Inst& inst);
void EmitVoid(EmitContext& ctx);
void EmitIdentity(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitConditionRef(EmitContext& ctx, IR::Inst& inst, const IR::Value& value);
void EmitReference(EmitContext& ctx, const IR::Value& value);
void EmitPhiMove(EmitContext& ctx, const IR::Value& phi, const IR::Value& value);
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
void EmitGetCbufU8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                   const IR::Value& offset);
void EmitGetCbufS8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                   const IR::Value& offset);
void EmitGetCbufU16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset);
void EmitGetCbufS16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset);
void EmitGetCbufU32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset);
void EmitGetCbufF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset);
void EmitGetCbufU32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                      const IR::Value& offset);
void EmitGetAttribute(EmitContext& ctx, IR::Inst& inst, IR::Attribute attr,
                      std::string_view vertex);
void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, std::string_view value,
                      std::string_view vertex);
void EmitGetAttributeIndexed(EmitContext& ctx, IR::Inst& inst, std::string_view offset,
                             std::string_view vertex);
void EmitSetAttributeIndexed(EmitContext& ctx, std::string_view offset, std::string_view value,
                             std::string_view vertex);
void EmitGetPatch(EmitContext& ctx, IR::Inst& inst, IR::Patch patch);
void EmitSetPatch(EmitContext& ctx, IR::Patch patch, std::string_view value);
void EmitSetFragColor(EmitContext& ctx, u32 index, u32 component, std::string_view value);
void EmitSetSampleMask(EmitContext& ctx, std::string_view value);
void EmitSetFragDepth(EmitContext& ctx, std::string_view value);
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
void EmitLoadLocal(EmitContext& ctx, IR::Inst& inst, std::string_view word_offset);
void EmitWriteLocal(EmitContext& ctx, std::string_view word_offset, std::string_view value);
void EmitUndefU1(EmitContext& ctx, IR::Inst& inst);
void EmitUndefU8(EmitContext& ctx, IR::Inst& inst);
void EmitUndefU16(EmitContext& ctx, IR::Inst& inst);
void EmitUndefU32(EmitContext& ctx, IR::Inst& inst);
void EmitUndefU64(EmitContext& ctx, IR::Inst& inst);
void EmitLoadGlobalU8(EmitContext& ctx);
void EmitLoadGlobalS8(EmitContext& ctx);
void EmitLoadGlobalU16(EmitContext& ctx);
void EmitLoadGlobalS16(EmitContext& ctx);
void EmitLoadGlobal32(EmitContext& ctx, IR::Inst& inst, std::string_view address);
void EmitLoadGlobal64(EmitContext& ctx, IR::Inst& inst, std::string_view address);
void EmitLoadGlobal128(EmitContext& ctx, IR::Inst& inst, std::string_view address);
void EmitWriteGlobalU8(EmitContext& ctx);
void EmitWriteGlobalS8(EmitContext& ctx);
void EmitWriteGlobalU16(EmitContext& ctx);
void EmitWriteGlobalS16(EmitContext& ctx);
void EmitWriteGlobal32(EmitContext& ctx, std::string_view address, std::string_view value);
void EmitWriteGlobal64(EmitContext& ctx, std::string_view address, std::string_view value);
void EmitWriteGlobal128(EmitContext& ctx, std::string_view address, std::string_view value);
void EmitLoadStorageU8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset);
void EmitLoadStorageS8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset);
void EmitLoadStorageU16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset);
void EmitLoadStorageS16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset);
void EmitLoadStorage32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset);
void EmitLoadStorage64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset);
void EmitLoadStorage128(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset);
void EmitWriteStorageU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value);
void EmitWriteStorageS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value);
void EmitWriteStorageU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         std::string_view value);
void EmitWriteStorageS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         std::string_view value);
void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value);
void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value);
void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         std::string_view value);
void EmitLoadSharedU8(EmitContext& ctx, IR::Inst& inst, std::string_view offset);
void EmitLoadSharedS8(EmitContext& ctx, IR::Inst& inst, std::string_view offset);
void EmitLoadSharedU16(EmitContext& ctx, IR::Inst& inst, std::string_view offset);
void EmitLoadSharedS16(EmitContext& ctx, IR::Inst& inst, std::string_view offset);
void EmitLoadSharedU32(EmitContext& ctx, IR::Inst& inst, std::string_view offset);
void EmitLoadSharedU64(EmitContext& ctx, IR::Inst& inst, std::string_view offset);
void EmitLoadSharedU128(EmitContext& ctx, IR::Inst& inst, std::string_view offset);
void EmitWriteSharedU8(EmitContext& ctx, std::string_view offset, std::string_view value);
void EmitWriteSharedU16(EmitContext& ctx, std::string_view offset, std::string_view value);
void EmitWriteSharedU32(EmitContext& ctx, std::string_view offset, std::string_view value);
void EmitWriteSharedU64(EmitContext& ctx, std::string_view offset, std::string_view value);
void EmitWriteSharedU128(EmitContext& ctx, std::string_view offset, std::string_view value);
void EmitCompositeConstructU32x2(EmitContext& ctx, IR::Inst& inst, std::string_view e1,
                                 std::string_view e2);
void EmitCompositeConstructU32x3(EmitContext& ctx, IR::Inst& inst, std::string_view e1,
                                 std::string_view e2, std::string_view e3);
void EmitCompositeConstructU32x4(EmitContext& ctx, IR::Inst& inst, std::string_view e1,
                                 std::string_view e2, std::string_view e3, std::string_view e4);
void EmitCompositeExtractU32x2(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                               u32 index);
void EmitCompositeExtractU32x3(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                               u32 index);
void EmitCompositeExtractU32x4(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                               u32 index);
void EmitCompositeInsertU32x2(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                              std::string_view object, u32 index);
void EmitCompositeInsertU32x3(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                              std::string_view object, u32 index);
void EmitCompositeInsertU32x4(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                              std::string_view object, u32 index);
void EmitCompositeConstructF16x2(EmitContext& ctx, std::string_view e1, std::string_view e2);
void EmitCompositeConstructF16x3(EmitContext& ctx, std::string_view e1, std::string_view e2,
                                 std::string_view e3);
void EmitCompositeConstructF16x4(EmitContext& ctx, std::string_view e1, std::string_view e2,
                                 std::string_view e3, std::string_view e4);
void EmitCompositeExtractF16x2(EmitContext& ctx, std::string_view composite, u32 index);
void EmitCompositeExtractF16x3(EmitContext& ctx, std::string_view composite, u32 index);
void EmitCompositeExtractF16x4(EmitContext& ctx, std::string_view composite, u32 index);
void EmitCompositeInsertF16x2(EmitContext& ctx, std::string_view composite, std::string_view object,
                              u32 index);
void EmitCompositeInsertF16x3(EmitContext& ctx, std::string_view composite, std::string_view object,
                              u32 index);
void EmitCompositeInsertF16x4(EmitContext& ctx, std::string_view composite, std::string_view object,
                              u32 index);
void EmitCompositeConstructF32x2(EmitContext& ctx, IR::Inst& inst, std::string_view e1,
                                 std::string_view e2);
void EmitCompositeConstructF32x3(EmitContext& ctx, IR::Inst& inst, std::string_view e1,
                                 std::string_view e2, std::string_view e3);
void EmitCompositeConstructF32x4(EmitContext& ctx, IR::Inst& inst, std::string_view e1,
                                 std::string_view e2, std::string_view e3, std::string_view e4);
void EmitCompositeExtractF32x2(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                               u32 index);
void EmitCompositeExtractF32x3(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                               u32 index);
void EmitCompositeExtractF32x4(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                               u32 index);
void EmitCompositeInsertF32x2(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                              std::string_view object, u32 index);
void EmitCompositeInsertF32x3(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                              std::string_view object, u32 index);
void EmitCompositeInsertF32x4(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                              std::string_view object, u32 index);
void EmitCompositeConstructF64x2(EmitContext& ctx);
void EmitCompositeConstructF64x3(EmitContext& ctx);
void EmitCompositeConstructF64x4(EmitContext& ctx);
void EmitCompositeExtractF64x2(EmitContext& ctx);
void EmitCompositeExtractF64x3(EmitContext& ctx);
void EmitCompositeExtractF64x4(EmitContext& ctx);
void EmitCompositeInsertF64x2(EmitContext& ctx, std::string_view composite, std::string_view object,
                              u32 index);
void EmitCompositeInsertF64x3(EmitContext& ctx, std::string_view composite, std::string_view object,
                              u32 index);
void EmitCompositeInsertF64x4(EmitContext& ctx, std::string_view composite, std::string_view object,
                              u32 index);
void EmitSelectU1(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                  std::string_view true_value, std::string_view false_value);
void EmitSelectU8(EmitContext& ctx, std::string_view cond, std::string_view true_value,
                  std::string_view false_value);
void EmitSelectU16(EmitContext& ctx, std::string_view cond, std::string_view true_value,
                   std::string_view false_value);
void EmitSelectU32(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value);
void EmitSelectU64(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value);
void EmitSelectF16(EmitContext& ctx, std::string_view cond, std::string_view true_value,
                   std::string_view false_value);
void EmitSelectF32(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value);
void EmitSelectF64(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value);
void EmitBitCastU16F16(EmitContext& ctx, IR::Inst& inst);
void EmitBitCastU32F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitBitCastU64F64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitBitCastF16U16(EmitContext& ctx, IR::Inst& inst);
void EmitBitCastF32U32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitBitCastF64U64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitPackUint2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitUnpackUint2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitPackFloat2x16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitUnpackFloat2x16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitPackHalf2x16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitUnpackHalf2x16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitPackDouble2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitUnpackDouble2x32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitGetZeroFromOp(EmitContext& ctx);
void EmitGetSignFromOp(EmitContext& ctx);
void EmitGetCarryFromOp(EmitContext& ctx);
void EmitGetOverflowFromOp(EmitContext& ctx);
void EmitGetSparseFromOp(EmitContext& ctx);
void EmitGetInBoundsFromOp(EmitContext& ctx);
void EmitFPAbs16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPAbs32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPAbs64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPAdd16(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPAdd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPAdd64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPFma16(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b,
                 std::string_view c);
void EmitFPFma32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b,
                 std::string_view c);
void EmitFPFma64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b,
                 std::string_view c);
void EmitFPMax32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPMax64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPMin32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPMin64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPMul16(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPMul32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPMul64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitFPNeg16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPNeg32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPNeg64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPSin(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPCos(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPExp2(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPLog2(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPRecip32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPRecip64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPRecipSqrt32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPRecipSqrt64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPSqrt(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPSaturate16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPSaturate32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPSaturate64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPClamp16(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                   std::string_view min_value, std::string_view max_value);
void EmitFPClamp32(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                   std::string_view min_value, std::string_view max_value);
void EmitFPClamp64(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                   std::string_view min_value, std::string_view max_value);
void EmitFPRoundEven16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPRoundEven32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPRoundEven64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPFloor16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPFloor32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPFloor64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPCeil16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPCeil32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPCeil64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPTrunc16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPTrunc32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPTrunc64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPOrdEqual16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPOrdEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs);
void EmitFPOrdEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs);
void EmitFPUnordEqual16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPUnordEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                        std::string_view rhs);
void EmitFPUnordEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                        std::string_view rhs);
void EmitFPOrdNotEqual16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPOrdNotEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs);
void EmitFPOrdNotEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs);
void EmitFPUnordNotEqual16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPUnordNotEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs);
void EmitFPUnordNotEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs);
void EmitFPOrdLessThan16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPOrdLessThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs);
void EmitFPOrdLessThan64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs);
void EmitFPUnordLessThan16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPUnordLessThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs);
void EmitFPUnordLessThan64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs);
void EmitFPOrdGreaterThan16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPOrdGreaterThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                            std::string_view rhs);
void EmitFPOrdGreaterThan64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                            std::string_view rhs);
void EmitFPUnordGreaterThan16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPUnordGreaterThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs);
void EmitFPUnordGreaterThan64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs);
void EmitFPOrdLessThanEqual16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPOrdLessThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs);
void EmitFPOrdLessThanEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs);
void EmitFPUnordLessThanEqual16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPUnordLessThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                std::string_view rhs);
void EmitFPUnordLessThanEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                std::string_view rhs);
void EmitFPOrdGreaterThanEqual16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPOrdGreaterThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                 std::string_view rhs);
void EmitFPOrdGreaterThanEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                 std::string_view rhs);
void EmitFPUnordGreaterThanEqual16(EmitContext& ctx, std::string_view lhs, std::string_view rhs);
void EmitFPUnordGreaterThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                   std::string_view rhs);
void EmitFPUnordGreaterThanEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                   std::string_view rhs);
void EmitFPIsNan16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPIsNan32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFPIsNan64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitIAdd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitIAdd64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitISub32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitISub64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitIMul32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitINeg32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitINeg64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitIAbs32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitShiftLeftLogical32(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                            std::string_view shift);
void EmitShiftLeftLogical64(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                            std::string_view shift);
void EmitShiftRightLogical32(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                             std::string_view shift);
void EmitShiftRightLogical64(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                             std::string_view shift);
void EmitShiftRightArithmetic32(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                                std::string_view shift);
void EmitShiftRightArithmetic64(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                                std::string_view shift);
void EmitBitwiseAnd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitBitwiseOr32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitBitwiseXor32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitBitFieldInsert(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                        std::string_view insert, std::string_view offset, std::string_view count);
void EmitBitFieldSExtract(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                          std::string_view offset, std::string_view count);
void EmitBitFieldUExtract(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                          std::string_view offset, std::string_view count);
void EmitBitReverse32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitBitCount32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitBitwiseNot32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFindSMsb32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitFindUMsb32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitSMin32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitUMin32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitSMax32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitUMax32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitSClamp32(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view min,
                  std::string_view max);
void EmitUClamp32(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view min,
                  std::string_view max);
void EmitSLessThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs);
void EmitULessThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs);
void EmitIEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs);
void EmitSLessThanEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                        std::string_view rhs);
void EmitULessThanEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                        std::string_view rhs);
void EmitSGreaterThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs);
void EmitUGreaterThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs);
void EmitINotEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs);
void EmitSGreaterThanEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs);
void EmitUGreaterThanEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs);
void EmitSharedAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value);
void EmitSharedAtomicSMin32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value);
void EmitSharedAtomicUMin32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value);
void EmitSharedAtomicSMax32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value);
void EmitSharedAtomicUMax32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value);
void EmitSharedAtomicInc32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                           std::string_view value);
void EmitSharedAtomicDec32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                           std::string_view value);
void EmitSharedAtomicAnd32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                           std::string_view value);
void EmitSharedAtomicOr32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                          std::string_view value);
void EmitSharedAtomicXor32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                           std::string_view value);
void EmitSharedAtomicExchange32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                                std::string_view value);
void EmitSharedAtomicExchange64(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                                std::string_view value);
void EmitStorageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value);
void EmitStorageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value);
void EmitStorageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value);
void EmitStorageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           const IR::Value& offset, std::string_view value);
void EmitStorageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value);
void EmitStorageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 const IR::Value& offset, std::string_view value);
void EmitStorageAtomicIAdd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicSMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicUMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicSMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicUMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicAnd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value);
void EmitStorageAtomicOr64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           const IR::Value& offset, std::string_view value);
void EmitStorageAtomicXor64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value);
void EmitStorageAtomicExchange64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 const IR::Value& offset, std::string_view value);
void EmitStorageAtomicAddF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value);
void EmitStorageAtomicAddF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value);
void EmitStorageAtomicAddF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value);
void EmitStorageAtomicMinF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value);
void EmitStorageAtomicMinF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value);
void EmitStorageAtomicMaxF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value);
void EmitStorageAtomicMaxF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value);
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
void EmitLogicalOr(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitLogicalAnd(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitLogicalXor(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b);
void EmitLogicalNot(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS16F16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS16F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS16F64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS32F16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS32F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS32F64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS64F16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS64F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertS64F64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU16F16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU16F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU16F64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU32F16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU32F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU32F64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU64F16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU64F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU64F64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU64U32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertU32U64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32F16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32F64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64F32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16S8(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16S16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16S32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16S64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16U8(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16U16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16U32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF16U64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32S8(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32S16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32S32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32S64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32U8(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32U16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32U32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF32U64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64S8(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64S16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64S32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64S64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64U8(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64U16(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64U32(EmitContext& ctx, IR::Inst& inst, std::string_view value);
void EmitConvertF64U64(EmitContext& ctx, IR::Inst& inst, std::string_view value);
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
                                std::string_view coords, std::string_view bias_lc,
                                const IR::Value& offset);
void EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                std::string_view coords, std::string_view lod_lc,
                                const IR::Value& offset);
void EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    std::string_view coords, std::string_view dref,
                                    std::string_view bias_lc, const IR::Value& offset);
void EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    std::string_view coords, std::string_view dref,
                                    std::string_view lod_lc, const IR::Value& offset);
void EmitImageGather(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                     std::string_view coords, const IR::Value& offset, const IR::Value& offset2);
void EmitImageGatherDref(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                         std::string_view coords, const IR::Value& offset, const IR::Value& offset2,
                         std::string_view dref);
void EmitImageFetch(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                    std::string_view coords, std::string_view offset, std::string_view lod,
                    std::string_view ms);
void EmitImageQueryDimensions(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                              std::string_view lod);
void EmitImageQueryLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                       std::string_view coords);
void EmitImageGradient(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                       std::string_view coords, const IR::Value& derivatives,
                       const IR::Value& offset, const IR::Value& lod_clamp);
void EmitImageRead(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                   std::string_view coords);
void EmitImageWrite(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                    std::string_view coords, std::string_view color);
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
void EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value);
void EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value);
void EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value);
void EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value);
void EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value);
void EmitImageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value);
void EmitImageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value);
void EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value);
void EmitImageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                         std::string_view coords, std::string_view value);
void EmitImageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value);
void EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                               std::string_view coords, std::string_view value);
void EmitLaneId(EmitContext& ctx, IR::Inst& inst);
void EmitVoteAll(EmitContext& ctx, IR::Inst& inst, std::string_view pred);
void EmitVoteAny(EmitContext& ctx, IR::Inst& inst, std::string_view pred);
void EmitVoteEqual(EmitContext& ctx, IR::Inst& inst, std::string_view pred);
void EmitSubgroupBallot(EmitContext& ctx, IR::Inst& inst, std::string_view pred);
void EmitSubgroupEqMask(EmitContext& ctx, IR::Inst& inst);
void EmitSubgroupLtMask(EmitContext& ctx, IR::Inst& inst);
void EmitSubgroupLeMask(EmitContext& ctx, IR::Inst& inst);
void EmitSubgroupGtMask(EmitContext& ctx, IR::Inst& inst);
void EmitSubgroupGeMask(EmitContext& ctx, IR::Inst& inst);
void EmitShuffleIndex(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                      std::string_view index, std::string_view clamp,
                      std::string_view segmentation_mask);
void EmitShuffleUp(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view index,
                   std::string_view clamp, std::string_view segmentation_mask);
void EmitShuffleDown(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                     std::string_view index, std::string_view clamp,
                     std::string_view segmentation_mask);
void EmitShuffleButterfly(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                          std::string_view index, std::string_view clamp,
                          std::string_view segmentation_mask);
void EmitFSwizzleAdd(EmitContext& ctx, IR::Inst& inst, std::string_view op_a, std::string_view op_b,
                     std::string_view swizzle);
void EmitDPdxFine(EmitContext& ctx, IR::Inst& inst, std::string_view op_a);
void EmitDPdyFine(EmitContext& ctx, IR::Inst& inst, std::string_view op_a);
void EmitDPdxCoarse(EmitContext& ctx, IR::Inst& inst, std::string_view op_a);
void EmitDPdyCoarse(EmitContext& ctx, IR::Inst& inst, std::string_view op_a);

} // namespace Shader::Backend::GLSL
