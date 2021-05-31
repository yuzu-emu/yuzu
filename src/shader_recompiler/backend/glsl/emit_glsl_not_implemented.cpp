// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

#ifdef _MSC_VER
#pragma warning(disable : 4100)
#endif

namespace Shader::Backend::GLSL {

void EmitPhi(EmitContext& ctx, IR::Inst& phi) {
    const size_t num_args{phi.NumArgs()};
    for (size_t i = 0; i < num_args; ++i) {
        ctx.var_alloc.Consume(phi.Arg(i));
    }
    if (!phi.Definition<Id>().is_valid) {
        // The phi node wasn't forward defined
        ctx.var_alloc.PhiDefine(phi, phi.Arg(0).Type());
    }
}

void EmitVoid(EmitContext& ctx) {
    // NotImplemented();
}

void EmitReference(EmitContext& ctx, const IR::Value& value) {
    ctx.var_alloc.Consume(value);
}

void EmitPhiMove(EmitContext& ctx, const IR::Value& phi_value, const IR::Value& value) {
    IR::Inst& phi{*phi_value.InstRecursive()};
    const auto phi_type{phi.Arg(0).Type()};
    if (!phi.Definition<Id>().is_valid) {
        // The phi node wasn't forward defined
        ctx.var_alloc.PhiDefine(phi, phi_type);
    }
    const auto phi_reg{ctx.var_alloc.Consume(IR::Value{&phi})};
    const auto val_reg{ctx.var_alloc.Consume(value)};
    if (phi_reg == val_reg) {
        return;
    }
    ctx.Add("{}={};", phi_reg, val_reg);
}

void EmitBranch(EmitContext& ctx, std::string_view label) {
    NotImplemented();
}

void EmitBranchConditional(EmitContext& ctx, std::string_view condition,
                           std::string_view true_label, std::string_view false_label) {
    NotImplemented();
}

void EmitLoopMerge(EmitContext& ctx, std::string_view merge_label,
                   std::string_view continue_label) {
    NotImplemented();
}

void EmitSelectionMerge(EmitContext& ctx, std::string_view merge_label) {
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

void EmitDemoteToHelperInvocation(EmitContext& ctx, std::string_view continue_label) {
    ctx.Add("discard;");
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
    ctx.Add("EmitStreamVertex(int({}));", ctx.var_alloc.Consume(stream));
}

void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream) {
    ctx.Add("EndStreamPrimitive(int({}));", ctx.var_alloc.Consume(stream));
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

void EmitGetAttributeIndexed(EmitContext& ctx, std::string_view offset, std::string_view vertex) {
    NotImplemented();
}

void EmitSetAttributeIndexed(EmitContext& ctx, std::string_view offset, std::string_view value,
                             std::string_view vertex) {
    NotImplemented();
}

void EmitGetPatch(EmitContext& ctx, IR::Patch patch) {
    NotImplemented();
}

void EmitSetPatch(EmitContext& ctx, IR::Patch patch, std::string_view value) {
    NotImplemented();
}

void EmitSetSampleMask(EmitContext& ctx, std::string_view value) {
    NotImplemented();
}

void EmitSetFragDepth(EmitContext& ctx, std::string_view value) {
    ctx.Add("gl_FragDepth={};", value);
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

void EmitWorkgroupId(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32x3("{}=gl_WorkGroupID;", inst);
}

void EmitInvocationId(EmitContext& ctx, IR::Inst& inst) {
    NotImplemented();
}

void EmitSampleId(EmitContext& ctx, IR::Inst& inst) {
    NotImplemented();
}

void EmitIsHelperInvocation(EmitContext& ctx) {
    NotImplemented();
}

void EmitYDirection(EmitContext& ctx, IR::Inst& inst) {
    ctx.uses_y_direction = true;
    ctx.AddF32("{}=gl_FrontMaterial.ambient.a;", inst);
}

void EmitUndefU1(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU1("{}=false;", inst);
}

void EmitUndefU8(EmitContext& ctx, IR::Inst& inst) {
    NotImplemented();
}

void EmitUndefU16(EmitContext& ctx, IR::Inst& inst) {
    NotImplemented();
}

void EmitUndefU32(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=0u;", inst);
}

void EmitUndefU64(EmitContext& ctx, IR::Inst& inst) {
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

void EmitLoadGlobal32(EmitContext& ctx, std::string_view address) {
    NotImplemented();
}

void EmitLoadGlobal64(EmitContext& ctx, std::string_view address) {
    NotImplemented();
}

void EmitLoadGlobal128(EmitContext& ctx, std::string_view address) {
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

void EmitWriteGlobal32(EmitContext& ctx, std::string_view address, std::string_view value) {
    NotImplemented();
}

void EmitWriteGlobal64(EmitContext& ctx, std::string_view address, std::string_view value) {
    NotImplemented();
}

void EmitWriteGlobal128(EmitContext& ctx, std::string_view address, std::string_view value) {
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
                           std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                         std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value) {
    NotImplemented();
}

void EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                               std::string_view coords, std::string_view value) {
    NotImplemented();
}

} // namespace Shader::Backend::GLSL
