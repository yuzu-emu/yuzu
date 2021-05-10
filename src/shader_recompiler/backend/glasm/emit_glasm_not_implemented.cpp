// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"

#ifdef _MSC_VER
#pragma warning(disable : 4100)
#endif

namespace Shader::Backend::GLASM {

#define NotImplemented() throw NotImplementedException("GLASM instruction {}", __LINE__)

void EmitPhi(EmitContext& ctx, IR::Inst& inst) {
    NotImplemented();
}

void EmitVoid(EmitContext&) {}

void EmitBranch(EmitContext& ctx) {
    NotImplemented();
}

void EmitBranchConditional(EmitContext& ctx) {
    NotImplemented();
}

void EmitLoopMerge(EmitContext& ctx) {
    NotImplemented();
}

void EmitSelectionMerge(EmitContext& ctx) {
    NotImplemented();
}

void EmitReturn(EmitContext& ctx) {
    ctx.Add("RET;");
}

void EmitJoin(EmitContext& ctx) {
    NotImplemented();
}

void EmitUnreachable(EmitContext& ctx) {
    NotImplemented();
}

void EmitDemoteToHelperInvocation(EmitContext& ctx) {
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
    // TODO
}

void EmitEpilogue(EmitContext& ctx) {
    // TODO
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

void EmitLocalInvocationId(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {},invocation.localid;", inst);
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

void EmitLoadLocal(EmitContext& ctx, ScalarU32 word_offset) {
    NotImplemented();
}

void EmitWriteLocal(EmitContext& ctx, ScalarU32 word_offset, ScalarU32 value) {
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

void EmitLoadSharedU8(EmitContext& ctx, ScalarU32 offset) {
    NotImplemented();
}

void EmitLoadSharedS8(EmitContext& ctx, ScalarU32 offset) {
    NotImplemented();
}

void EmitLoadSharedU16(EmitContext& ctx, ScalarU32 offset) {
    NotImplemented();
}

void EmitLoadSharedS16(EmitContext& ctx, ScalarU32 offset) {
    NotImplemented();
}

void EmitLoadSharedU32(EmitContext& ctx, ScalarU32 offset) {
    NotImplemented();
}

void EmitLoadSharedU64(EmitContext& ctx, ScalarU32 offset) {
    NotImplemented();
}

void EmitLoadSharedU128(EmitContext& ctx, ScalarU32 offset) {
    NotImplemented();
}

void EmitWriteSharedU8(EmitContext& ctx, ScalarU32 offset, ScalarU32 value) {
    NotImplemented();
}

void EmitWriteSharedU16(EmitContext& ctx, ScalarU32 offset, ScalarU32 value) {
    NotImplemented();
}

void EmitWriteSharedU32(EmitContext& ctx, ScalarU32 offset, ScalarU32 value) {
    NotImplemented();
}

void EmitWriteSharedU64(EmitContext& ctx, ScalarU32 offset, Register value) {
    NotImplemented();
}

void EmitWriteSharedU128(EmitContext& ctx, ScalarU32 offset, Register value) {
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

void EmitSharedAtomicIAdd32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicSMin32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarS32 value) {
    NotImplemented();
}

void EmitSharedAtomicUMin32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicSMax32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarS32 value) {
    NotImplemented();
}

void EmitSharedAtomicUMax32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicInc32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicDec32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicAnd32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicOr32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicXor32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicExchange32(EmitContext& ctx, ScalarU32 pointer_offset, ScalarU32 value) {
    NotImplemented();
}

void EmitSharedAtomicExchange64(EmitContext& ctx, ScalarU32 pointer_offset, Register value) {
    NotImplemented();
}

void EmitLogicalOr(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("OR.S {},{},{};", inst, a, b);
}

void EmitLogicalAnd(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("AND.S {},{},{};", inst, a, b);
}

void EmitLogicalXor(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("XOR.S {},{},{};", inst, a, b);
}

void EmitLogicalNot(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("SEQ.S {},{},0;", inst, value);
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

void EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                Register coords, Register bias_lc, const IR::Value& offset) {
    NotImplemented();
}

void EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                Register coords, Register lod_lc, const IR::Value& offset) {
    NotImplemented();
}

void EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    Register coords, Register dref, Register bias_lc,
                                    const IR::Value& offset) {
    NotImplemented();
}

void EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    Register coords, Register dref, Register lod_lc,
                                    const IR::Value& offset) {
    NotImplemented();
}

void EmitImageGather(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                     const IR::Value& offset, const IR::Value& offset2) {
    NotImplemented();
}

void EmitImageGatherDref(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                         const IR::Value& offset, const IR::Value& offset2, Register dref) {
    NotImplemented();
}

void EmitImageFetch(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                    Register offset, Register lod, Register ms) {
    NotImplemented();
}

void EmitImageQueryDimensions(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                              Register lod) {
    NotImplemented();
}

void EmitImageQueryLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords) {
    NotImplemented();
}

void EmitImageGradient(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                       Register derivates, Register offset, Register lod_clamp) {
    NotImplemented();
}

void EmitImageRead(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords) {
    NotImplemented();
}

void EmitImageWrite(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                    Register color) {
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

void EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           Register coords, ScalarU32 value) {
    NotImplemented();
}

void EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           Register coords, ScalarS32 value) {
    NotImplemented();
}

void EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           Register coords, ScalarU32 value) {
    NotImplemented();
}

void EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           Register coords, ScalarS32 value) {
    NotImplemented();
}

void EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           Register coords, ScalarU32 value) {
    NotImplemented();
}

void EmitImageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                          ScalarU32 value) {
    NotImplemented();
}

void EmitImageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                          ScalarU32 value) {
    NotImplemented();
}

void EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                          ScalarU32 value) {
    NotImplemented();
}

void EmitImageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                         ScalarU32 value) {
    NotImplemented();
}

void EmitImageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coords,
                          ScalarU32 value) {
    NotImplemented();
}

void EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                               Register coords, ScalarU32 value) {
    NotImplemented();
}

} // namespace Shader::Backend::GLASM
