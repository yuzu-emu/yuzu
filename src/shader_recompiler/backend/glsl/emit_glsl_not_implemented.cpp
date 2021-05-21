// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"

#ifdef _MSC_VER
#pragma warning(disable : 4100)
#endif

namespace Shader::Backend::GLSL {

static void NotImplemented() {
    throw NotImplementedException("GLSL instruction");
}

void EmitPhi(EmitContext& ctx, IR::Inst* inst) {
    NotImplemented();
}

void EmitVoid(EmitContext& ctx) {
    NotImplemented();
}

void EmitConditionRef(EmitContext& ctx, IR::Inst& inst, const IR::Value& value) {
    NotImplemented();
}

void EmitReference(EmitContext&) {
    NotImplemented();
}

void EmitPhiMove(EmitContext& ctx, const IR::Value& phi, const IR::Value& value) {
    NotImplemented();
}

void EmitBranch(EmitContext& ctx, std::string label) {
    NotImplemented();
}

void EmitBranchConditional(EmitContext& ctx, std::string condition, std::string true_label,
                           std::string false_label) {
    NotImplemented();
}

void EmitLoopMerge(EmitContext& ctx, std::string merge_label, std::string continue_label) {
    NotImplemented();
}

void EmitSelectionMerge(EmitContext& ctx, std::string merge_label) {
    NotImplemented();
}

void EmitReturn(EmitContext& ctx) {
    NotImplemented();
}

void EmitJoin(EmitContext& ctx) {
    NotImplemented();
}

void EmitUnreachable(EmitContext& ctx) {
    NotImplemented();
}

void EmitDemoteToHelperInvocation(EmitContext& ctx, std::string continue_label) {
    NotImplemented();
}

void EmitBarrier(EmitContext& ctx) {
    NotImplemented();
}

void EmitWorkgroupMemoryBarrier(EmitContext& ctx) {
    NotImplemented();
}

void EmitDeviceMemoryBarrier(EmitContext& ctx) {
    NotImplemented();
}

void EmitPrologue(EmitContext& ctx) {
    // NotImplemented();
}

void EmitEpilogue(EmitContext& ctx) {
    // NotImplemented();
}

void EmitEmitVertex(EmitContext& ctx, const IR::Value& stream) {
    NotImplemented();
}

void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream) {
    NotImplemented();
}

void EmitGetRegister(EmitContext& ctx) {
    NotImplemented();
}

void EmitSetRegister(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetPred(EmitContext& ctx) {
    NotImplemented();
}

void EmitSetPred(EmitContext& ctx) {
    NotImplemented();
}

void EmitSetGotoVariable(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetGotoVariable(EmitContext& ctx) {
    NotImplemented();
}

void EmitSetIndirectBranchVariable(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetIndirectBranchVariable(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetAttribute(EmitContext& ctx, IR::Attribute attr, std::string vertex) {
    NotImplemented();
}

void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, std::string value, std::string vertex) {
    NotImplemented();
}

void EmitGetAttributeIndexed(EmitContext& ctx, std::string offset, std::string vertex) {
    NotImplemented();
}

void EmitSetAttributeIndexed(EmitContext& ctx, std::string offset, std::string value,
                             std::string vertex) {
    NotImplemented();
}

void EmitGetPatch(EmitContext& ctx, IR::Patch patch) {
    NotImplemented();
}

void EmitSetPatch(EmitContext& ctx, IR::Patch patch, std::string value) {
    NotImplemented();
}

void EmitSetFragColor(EmitContext& ctx, u32 index, u32 component, std::string value) {
    NotImplemented();
}

void EmitSetSampleMask(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitSetFragDepth(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitGetZFlag(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetSFlag(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetCFlag(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetOFlag(EmitContext& ctx) {
    NotImplemented();
}

void EmitSetZFlag(EmitContext& ctx) {
    NotImplemented();
}

void EmitSetSFlag(EmitContext& ctx) {
    NotImplemented();
}

void EmitSetCFlag(EmitContext& ctx) {
    NotImplemented();
}

void EmitSetOFlag(EmitContext& ctx) {
    NotImplemented();
}

void EmitWorkgroupId(EmitContext& ctx) {
    NotImplemented();
}

void EmitLocalInvocationId(EmitContext& ctx) {
    NotImplemented();
}

void EmitInvocationId(EmitContext& ctx) {
    NotImplemented();
}

void EmitSampleId(EmitContext& ctx) {
    NotImplemented();
}

void EmitIsHelperInvocation(EmitContext& ctx) {
    NotImplemented();
}

void EmitYDirection(EmitContext& ctx) {
    NotImplemented();
}

void EmitLoadLocal(EmitContext& ctx, std::string word_offset) {
    NotImplemented();
}

void EmitWriteLocal(EmitContext& ctx, std::string word_offset, std::string value) {
    NotImplemented();
}

void EmitUndefU1(EmitContext& ctx) {
    NotImplemented();
}

void EmitUndefU8(EmitContext& ctx) {
    NotImplemented();
}

void EmitUndefU16(EmitContext& ctx) {
    NotImplemented();
}

void EmitUndefU32(EmitContext& ctx) {
    NotImplemented();
}

void EmitUndefU64(EmitContext& ctx) {
    NotImplemented();
}

void EmitLoadGlobalU8(EmitContext& ctx) {
    NotImplemented();
}

void EmitLoadGlobalS8(EmitContext& ctx) {
    NotImplemented();
}

void EmitLoadGlobalU16(EmitContext& ctx) {
    NotImplemented();
}

void EmitLoadGlobalS16(EmitContext& ctx) {
    NotImplemented();
}

void EmitLoadGlobal32(EmitContext& ctx, std::string address) {
    NotImplemented();
}

void EmitLoadGlobal64(EmitContext& ctx, std::string address) {
    NotImplemented();
}

void EmitLoadGlobal128(EmitContext& ctx, std::string address) {
    NotImplemented();
}

void EmitWriteGlobalU8(EmitContext& ctx) {
    NotImplemented();
}

void EmitWriteGlobalS8(EmitContext& ctx) {
    NotImplemented();
}

void EmitWriteGlobalU16(EmitContext& ctx) {
    NotImplemented();
}

void EmitWriteGlobalS16(EmitContext& ctx) {
    NotImplemented();
}

void EmitWriteGlobal32(EmitContext& ctx, std::string address, std::string value) {
    NotImplemented();
}

void EmitWriteGlobal64(EmitContext& ctx, std::string address, std::string value) {
    NotImplemented();
}

void EmitWriteGlobal128(EmitContext& ctx, std::string address, std::string value) {
    NotImplemented();
}

void EmitLoadSharedU8(EmitContext& ctx, std::string offset) {
    NotImplemented();
}

void EmitLoadSharedS8(EmitContext& ctx, std::string offset) {
    NotImplemented();
}

void EmitLoadSharedU16(EmitContext& ctx, std::string offset) {
    NotImplemented();
}

void EmitLoadSharedS16(EmitContext& ctx, std::string offset) {
    NotImplemented();
}

void EmitLoadSharedU32(EmitContext& ctx, std::string offset) {
    NotImplemented();
}

void EmitLoadSharedU64(EmitContext& ctx, std::string offset) {
    NotImplemented();
}

void EmitLoadSharedU128(EmitContext& ctx, std::string offset) {
    NotImplemented();
}

void EmitWriteSharedU8(EmitContext& ctx, std::string offset, std::string value) {
    NotImplemented();
}

void EmitWriteSharedU16(EmitContext& ctx, std::string offset, std::string value) {
    NotImplemented();
}

void EmitWriteSharedU32(EmitContext& ctx, std::string offset, std::string value) {
    NotImplemented();
}

void EmitWriteSharedU64(EmitContext& ctx, std::string offset, std::string value) {
    NotImplemented();
}

void EmitWriteSharedU128(EmitContext& ctx, std::string offset, std::string value) {
    NotImplemented();
}

void EmitCompositeConstructU32x2(EmitContext& ctx, std::string e1, std::string e2) {
    NotImplemented();
}

void EmitCompositeConstructU32x3(EmitContext& ctx, std::string e1, std::string e2, std::string e3) {
    NotImplemented();
}

void EmitCompositeConstructU32x4(EmitContext& ctx, std::string e1, std::string e2, std::string e3,
                                 std::string e4) {
    NotImplemented();
}

void EmitCompositeExtractU32x2(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeExtractU32x3(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeExtractU32x4(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeInsertU32x2(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeInsertU32x3(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeInsertU32x4(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeConstructF16x2(EmitContext& ctx, std::string e1, std::string e2) {
    NotImplemented();
}

void EmitCompositeConstructF16x3(EmitContext& ctx, std::string e1, std::string e2, std::string e3) {
    NotImplemented();
}

void EmitCompositeConstructF16x4(EmitContext& ctx, std::string e1, std::string e2, std::string e3,
                                 std::string e4) {
    NotImplemented();
}

void EmitCompositeExtractF16x2(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeExtractF16x3(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeExtractF16x4(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeInsertF16x2(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeInsertF16x3(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeInsertF16x4(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeConstructF32x2(EmitContext& ctx, std::string e1, std::string e2) {
    NotImplemented();
}

void EmitCompositeConstructF32x3(EmitContext& ctx, std::string e1, std::string e2, std::string e3) {
    NotImplemented();
}

void EmitCompositeConstructF32x4(EmitContext& ctx, std::string e1, std::string e2, std::string e3,
                                 std::string e4) {
    NotImplemented();
}

void EmitCompositeExtractF32x2(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeExtractF32x3(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeExtractF32x4(EmitContext& ctx, std::string composite, u32 index) {
    NotImplemented();
}

void EmitCompositeInsertF32x2(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeInsertF32x3(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeInsertF32x4(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeConstructF64x2(EmitContext& ctx) {
    NotImplemented();
}

void EmitCompositeConstructF64x3(EmitContext& ctx) {
    NotImplemented();
}

void EmitCompositeConstructF64x4(EmitContext& ctx) {
    NotImplemented();
}

void EmitCompositeExtractF64x2(EmitContext& ctx) {
    NotImplemented();
}

void EmitCompositeExtractF64x3(EmitContext& ctx) {
    NotImplemented();
}

void EmitCompositeExtractF64x4(EmitContext& ctx) {
    NotImplemented();
}

void EmitCompositeInsertF64x2(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeInsertF64x3(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitCompositeInsertF64x4(EmitContext& ctx, std::string composite, std::string object,
                              u32 index) {
    NotImplemented();
}

void EmitSelectU1(EmitContext& ctx, std::string cond, std::string true_value,
                  std::string false_value) {
    NotImplemented();
}

void EmitSelectU8(EmitContext& ctx, std::string cond, std::string true_value,
                  std::string false_value) {
    NotImplemented();
}

void EmitSelectU16(EmitContext& ctx, std::string cond, std::string true_value,
                   std::string false_value) {
    NotImplemented();
}

void EmitSelectU32(EmitContext& ctx, std::string cond, std::string true_value,
                   std::string false_value) {
    NotImplemented();
}

void EmitSelectU64(EmitContext& ctx, std::string cond, std::string true_value,
                   std::string false_value) {
    NotImplemented();
}

void EmitSelectF16(EmitContext& ctx, std::string cond, std::string true_value,
                   std::string false_value) {
    NotImplemented();
}

void EmitSelectF32(EmitContext& ctx, std::string cond, std::string true_value,
                   std::string false_value) {
    NotImplemented();
}

void EmitSelectF64(EmitContext& ctx, std::string cond, std::string true_value,
                   std::string false_value) {
    NotImplemented();
}

void EmitBitCastU16F16(EmitContext& ctx) {
    NotImplemented();
}

void EmitBitCastU32F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitBitCastU64F64(EmitContext& ctx) {
    NotImplemented();
}

void EmitBitCastF16U16(EmitContext& ctx) {
    NotImplemented();
}

void EmitBitCastF32U32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitBitCastF64U64(EmitContext& ctx) {
    NotImplemented();
}

void EmitPackUint2x32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitUnpackUint2x32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitPackFloat2x16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitUnpackFloat2x16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitPackHalf2x16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitUnpackHalf2x16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitPackDouble2x32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitUnpackDouble2x32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitGetZeroFromOp(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetSignFromOp(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetCarryFromOp(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetOverflowFromOp(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetSparseFromOp(EmitContext& ctx) {
    NotImplemented();
}

void EmitGetInBoundsFromOp(EmitContext& ctx) {
    NotImplemented();
}

void EmitFPAbs16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPAbs32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPAbs64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPAdd16(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPAdd32(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPAdd64(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPFma16(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b, std::string c) {
    NotImplemented();
}

void EmitFPFma32(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b, std::string c) {
    NotImplemented();
}

void EmitFPFma64(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b, std::string c) {
    NotImplemented();
}

void EmitFPMax32(EmitContext& ctx, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPMax64(EmitContext& ctx, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPMin32(EmitContext& ctx, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPMin64(EmitContext& ctx, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPMul16(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPMul32(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPMul64(EmitContext& ctx, IR::Inst* inst, std::string a, std::string b) {
    NotImplemented();
}

void EmitFPNeg16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPNeg32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPNeg64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPSin(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPCos(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPExp2(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPLog2(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPRecip32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPRecip64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPRecipSqrt32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPRecipSqrt64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPSqrt(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPSaturate16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPSaturate32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPSaturate64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPClamp16(EmitContext& ctx, std::string value, std::string min_value,
                   std::string max_value) {
    NotImplemented();
}

void EmitFPClamp32(EmitContext& ctx, std::string value, std::string min_value,
                   std::string max_value) {
    NotImplemented();
}

void EmitFPClamp64(EmitContext& ctx, std::string value, std::string min_value,
                   std::string max_value) {
    NotImplemented();
}

void EmitFPRoundEven16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPRoundEven32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPRoundEven64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPFloor16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPFloor32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPFloor64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPCeil16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPCeil32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPCeil64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPTrunc16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPTrunc32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPTrunc64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPOrdEqual16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdEqual32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdEqual64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordEqual16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordEqual32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordEqual64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdNotEqual16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdNotEqual32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdNotEqual64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordNotEqual16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordNotEqual32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordNotEqual64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdLessThan16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdLessThan32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdLessThan64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordLessThan16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordLessThan32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordLessThan64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdGreaterThan16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdGreaterThan32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdGreaterThan64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordGreaterThan16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordGreaterThan32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordGreaterThan64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdLessThanEqual16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdLessThanEqual32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdLessThanEqual64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordLessThanEqual16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordLessThanEqual32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordLessThanEqual64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdGreaterThanEqual16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdGreaterThanEqual32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPOrdGreaterThanEqual64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordGreaterThanEqual16(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordGreaterThanEqual32(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPUnordGreaterThanEqual64(EmitContext& ctx, std::string lhs, std::string rhs) {
    NotImplemented();
}

void EmitFPIsNan16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPIsNan32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitFPIsNan64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicIAdd32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicSMin32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicUMin32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicSMax32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicUMax32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicInc32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicDec32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicAnd32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicOr32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicXor32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicExchange32(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitSharedAtomicExchange64(EmitContext& ctx, std::string pointer_offset, std::string value) {
    NotImplemented();
}

void EmitStorageAtomicIAdd32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicSMin32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicUMin32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicSMax32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicUMax32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicInc32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                            std::string value) {
    NotImplemented();
}

void EmitStorageAtomicDec32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                            std::string value) {
    NotImplemented();
}

void EmitStorageAtomicAnd32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                            std::string value) {
    NotImplemented();
}

void EmitStorageAtomicOr32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           std::string value) {
    NotImplemented();
}

void EmitStorageAtomicXor32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                            std::string value) {
    NotImplemented();
}

void EmitStorageAtomicExchange32(EmitContext& ctx, const IR::Value& binding,
                                 const IR::Value& offset, std::string value) {
    NotImplemented();
}

void EmitStorageAtomicIAdd64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicSMin64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicUMin64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicSMax64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicUMax64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicAnd64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                            std::string value) {
    NotImplemented();
}

void EmitStorageAtomicOr64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           std::string value) {
    NotImplemented();
}

void EmitStorageAtomicXor64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                            std::string value) {
    NotImplemented();
}

void EmitStorageAtomicExchange64(EmitContext& ctx, const IR::Value& binding,
                                 const IR::Value& offset, std::string value) {
    NotImplemented();
}

void EmitStorageAtomicAddF32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             std::string value) {
    NotImplemented();
}

void EmitStorageAtomicAddF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               std::string value) {
    NotImplemented();
}

void EmitStorageAtomicAddF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               std::string value) {
    NotImplemented();
}

void EmitStorageAtomicMinF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               std::string value) {
    NotImplemented();
}

void EmitStorageAtomicMinF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               std::string value) {
    NotImplemented();
}

void EmitStorageAtomicMaxF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               std::string value) {
    NotImplemented();
}

void EmitStorageAtomicMaxF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               std::string value) {
    NotImplemented();
}

void EmitGlobalAtomicIAdd32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicSMin32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicUMin32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicSMax32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicUMax32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicInc32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicDec32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicAnd32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicOr32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicXor32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicExchange32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicIAdd64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicSMin64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicUMin64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicSMax64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicUMax64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicInc64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicDec64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicAnd64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicOr64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicXor64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicExchange64(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicAddF32(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicAddF16x2(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicAddF32x2(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicMinF16x2(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicMinF32x2(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicMaxF16x2(EmitContext& ctx) {
    NotImplemented();
}

void EmitGlobalAtomicMaxF32x2(EmitContext& ctx) {
    NotImplemented();
}

void EmitLogicalOr(EmitContext& ctx, std::string a, std::string b) {
    NotImplemented();
}

void EmitLogicalAnd(EmitContext& ctx, std::string a, std::string b) {
    NotImplemented();
}

void EmitLogicalXor(EmitContext& ctx, std::string a, std::string b) {
    NotImplemented();
}

void EmitLogicalNot(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS16F16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS16F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS16F64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS32F16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS32F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS32F64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS64F16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS64F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertS64F64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU16F16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU16F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU16F64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU32F16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU32F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU32F64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU64F16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU64F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU64F64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU64U32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertU32U64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32F16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32F64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64F32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16S8(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16S16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16S32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16S64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16U8(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16U16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16U32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF16U64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32S8(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32S16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32S32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32S64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32U8(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32U16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32U32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF32U64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64S8(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64S16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64S32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64S64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64U8(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64U16(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64U32(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitConvertF64U64(EmitContext& ctx, std::string value) {
    NotImplemented();
}

void EmitBindlessImageSampleImplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageSampleExplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageSampleDrefImplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageSampleDrefExplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageGather(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageGatherDref(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageFetch(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageQueryDimensions(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageQueryLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageGradient(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageRead(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageWrite(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageSampleImplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageSampleExplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageSampleDrefImplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageSampleDrefExplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageGather(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageGatherDref(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageFetch(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageQueryDimensions(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageQueryLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageGradient(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageRead(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageWrite(EmitContext&) {
    NotImplemented();
}

void EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                std::string coords, std::string bias_lc, const IR::Value& offset) {
    NotImplemented();
}

void EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                std::string coords, std::string lod_lc, const IR::Value& offset) {
    NotImplemented();
}

void EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                    std::string coords, std::string dref, std::string bias_lc,
                                    const IR::Value& offset) {
    NotImplemented();
}

void EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                    std::string coords, std::string dref, std::string lod_lc,
                                    const IR::Value& offset) {
    NotImplemented();
}

void EmitImageGather(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, std::string coords,
                     const IR::Value& offset, const IR::Value& offset2) {
    NotImplemented();
}

void EmitImageGatherDref(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                         std::string coords, const IR::Value& offset, const IR::Value& offset2,
                         std::string dref) {
    NotImplemented();
}

void EmitImageFetch(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, std::string coords,
                    std::string offset, std::string lod, std::string ms) {
    NotImplemented();
}

void EmitImageQueryDimensions(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                              std::string lod) {
    NotImplemented();
}

void EmitImageQueryLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                       std::string coords) {
    NotImplemented();
}

void EmitImageGradient(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, std::string coords,
                       std::string derivates, std::string offset, std::string lod_clamp) {
    NotImplemented();
}

void EmitImageRead(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, std::string coords) {
    NotImplemented();
}

void EmitImageWrite(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, std::string coords,
                    std::string color) {
    NotImplemented();
}

void EmitBindlessImageAtomicIAdd32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicSMin32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicUMin32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicSMax32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicUMax32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicInc32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicDec32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicAnd32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicOr32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicXor32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicExchange32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicIAdd32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicSMin32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicUMin32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicSMax32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicUMax32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicInc32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicDec32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicAnd32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicOr32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicXor32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicExchange32(EmitContext&) {
    NotImplemented();
}

void EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                           std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                           std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                           std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                           std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                           std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicInc32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                          std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicDec32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                          std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                          std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicOr32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                         std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicXor32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                          std::string coords, std::string value) {
    NotImplemented();
}

void EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                               std::string coords, std::string value) {
    NotImplemented();
}

void EmitLaneId(EmitContext& ctx) {
    NotImplemented();
}

void EmitVoteAll(EmitContext& ctx, std::string pred) {
    NotImplemented();
}

void EmitVoteAny(EmitContext& ctx, std::string pred) {
    NotImplemented();
}

void EmitVoteEqual(EmitContext& ctx, std::string pred) {
    NotImplemented();
}

void EmitSubgroupBallot(EmitContext& ctx, std::string pred) {
    NotImplemented();
}

void EmitSubgroupEqMask(EmitContext& ctx) {
    NotImplemented();
}

void EmitSubgroupLtMask(EmitContext& ctx) {
    NotImplemented();
}

void EmitSubgroupLeMask(EmitContext& ctx) {
    NotImplemented();
}

void EmitSubgroupGtMask(EmitContext& ctx) {
    NotImplemented();
}

void EmitSubgroupGeMask(EmitContext& ctx) {
    NotImplemented();
}

void EmitShuffleIndex(EmitContext& ctx, IR::Inst* inst, std::string value, std::string index,
                      std::string clamp, std::string segmentation_mask) {
    NotImplemented();
}

void EmitShuffleUp(EmitContext& ctx, IR::Inst* inst, std::string value, std::string index,
                   std::string clamp, std::string segmentation_mask) {
    NotImplemented();
}

void EmitShuffleDown(EmitContext& ctx, IR::Inst* inst, std::string value, std::string index,
                     std::string clamp, std::string segmentation_mask) {
    NotImplemented();
}

void EmitShuffleButterfly(EmitContext& ctx, IR::Inst* inst, std::string value, std::string index,
                          std::string clamp, std::string segmentation_mask) {
    NotImplemented();
}

void EmitFSwizzleAdd(EmitContext& ctx, std::string op_a, std::string op_b, std::string swizzle) {
    NotImplemented();
}

void EmitDPdxFine(EmitContext& ctx, std::string op_a) {
    NotImplemented();
}

void EmitDPdyFine(EmitContext& ctx, std::string op_a) {
    NotImplemented();
}

void EmitDPdxCoarse(EmitContext& ctx, std::string op_a) {
    NotImplemented();
}

void EmitDPdyCoarse(EmitContext& ctx, std::string op_a) {
    NotImplemented();
}

} // namespace Shader::Backend::GLSL
