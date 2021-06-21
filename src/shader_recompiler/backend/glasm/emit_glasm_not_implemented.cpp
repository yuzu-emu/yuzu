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

static void DefinePhi(EmitContext& ctx, IR::Inst& phi) {
    switch (phi.Arg(0).Type()) {
    case IR::Type::U1:
    case IR::Type::U32:
    case IR::Type::F32:
        ctx.reg_alloc.Define(phi);
        break;
    case IR::Type::U64:
    case IR::Type::F64:
        ctx.reg_alloc.LongDefine(phi);
        break;
    default:
        throw NotImplementedException("Phi node type {}", phi.Type());
    }
}

void EmitPhi(EmitContext& ctx, IR::Inst& phi) {
    const size_t num_args{phi.NumArgs()};
    for (size_t i = 0; i < num_args; ++i) {
        ctx.reg_alloc.Consume(phi.Arg(i));
    }
    if (!phi.Definition<Id>().is_valid) {
        // The phi node wasn't forward defined
        DefinePhi(ctx, phi);
    }
}

void EmitVoid(EmitContext&) {}

void EmitReference(EmitContext& ctx, const IR::Value& value) {
    ctx.reg_alloc.Consume(value);
}

void EmitPhiMove(EmitContext& ctx, const IR::Value& phi_value, const IR::Value& value) {
    IR::Inst& phi{RegAlloc::AliasInst(*phi_value.Inst())};
    if (!phi.Definition<Id>().is_valid) {
        // The phi node wasn't forward defined
        DefinePhi(ctx, phi);
    }
    const Register phi_reg{ctx.reg_alloc.Consume(IR::Value{&phi})};
    const Value eval_value{ctx.reg_alloc.Consume(value)};

    if (phi_reg == eval_value) {
        return;
    }
    switch (phi.Flags<IR::Type>()) {
    case IR::Type::U1:
    case IR::Type::U32:
    case IR::Type::F32:
        ctx.Add("MOV.S {}.x,{};", phi_reg, ScalarS32{eval_value});
        break;
    case IR::Type::U64:
    case IR::Type::F64:
        ctx.Add("MOV.U64 {}.x,{};", phi_reg, ScalarRegister{eval_value});
        break;
    default:
        throw NotImplementedException("Phi node type {}", phi.Type());
    }
}

void EmitJoin(EmitContext& ctx) {
    NotImplemented();
}

void EmitDemoteToHelperInvocation(EmitContext& ctx) {
    ctx.Add("KIL TR.x;");
}

void EmitBarrier(EmitContext& ctx) {
    ctx.Add("BAR;");
}

void EmitWorkgroupMemoryBarrier(EmitContext& ctx) {
    ctx.Add("MEMBAR.CTA;");
}

void EmitDeviceMemoryBarrier(EmitContext& ctx) {
    ctx.Add("MEMBAR;");
}

void EmitPrologue(EmitContext& ctx) {
    // TODO
}

void EmitEpilogue(EmitContext& ctx) {
    // TODO
}

void EmitEmitVertex(EmitContext& ctx, ScalarS32 stream) {
    if (stream.type == Type::U32 && stream.imm_u32 == 0) {
        ctx.Add("EMIT;");
    } else {
        ctx.Add("EMITS {};", stream);
    }
}

void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream) {
    if (!stream.IsImmediate()) {
        LOG_WARNING(Shader_GLASM, "Stream is not immediate");
    }
    ctx.reg_alloc.Consume(stream);
    ctx.Add("ENDPRIM;");
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

void EmitWorkgroupId(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {},invocation.groupid;", inst);
}

void EmitLocalInvocationId(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {},invocation.localid;", inst);
}

void EmitInvocationId(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {}.x,primitive_invocation.x;", inst);
}

void EmitSampleId(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {}.x,fragment.sampleid.x;", inst);
}

void EmitIsHelperInvocation(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {}.x,fragment.helperthread.x;", inst);
}

void EmitYDirection(EmitContext& ctx, IR::Inst& inst) {
    ctx.uses_y_direction = true;
    ctx.Add("MOV.F {}.x,y_direction[0].w;", inst);
}

void EmitUndefU1(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {}.x,0;", inst);
}

void EmitUndefU8(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {}.x,0;", inst);
}

void EmitUndefU16(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {}.x,0;", inst);
}

void EmitUndefU32(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {}.x,0;", inst);
}

void EmitUndefU64(EmitContext& ctx, IR::Inst& inst) {
    ctx.LongAdd("MOV.S64 {}.x,0;", inst);
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

} // namespace Shader::Backend::GLASM
