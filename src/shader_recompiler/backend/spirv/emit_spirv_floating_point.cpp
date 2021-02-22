// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/ir/modifiers.h"

namespace Shader::Backend::SPIRV {
namespace {
Id Decorate(EmitContext& ctx, IR::Inst* inst, Id op) {
    const auto flags{inst->Flags<IR::FpControl>()};
    if (flags.no_contraction) {
        ctx.Decorate(op, spv::Decoration::NoContraction);
    }
    return op;
}

Id Saturate(EmitContext& ctx, Id type, Id value, Id zero, Id one) {
    if (ctx.profile.has_broken_spirv_clamp) {
        return ctx.OpFMin(type, ctx.OpFMax(type, value, zero), one);
    } else {
        return ctx.OpFClamp(type, value, zero, one);
    }
}
} // Anonymous namespace

Id EmitFPAbs16(EmitContext& ctx, Id value) {
    return ctx.OpFAbs(ctx.F16[1], value);
}

Id EmitFPAbs32(EmitContext& ctx, Id value) {
    return ctx.OpFAbs(ctx.F32[1], value);
}

Id EmitFPAbs64(EmitContext& ctx, Id value) {
    return ctx.OpFAbs(ctx.F64[1], value);
}

Id EmitFPAdd16(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFAdd(ctx.F16[1], a, b));
}

Id EmitFPAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFAdd(ctx.F32[1], a, b));
}

Id EmitFPAdd64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFAdd(ctx.F64[1], a, b));
}

Id EmitFPFma16(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c) {
    return Decorate(ctx, inst, ctx.OpFma(ctx.F16[1], a, b, c));
}

Id EmitFPFma32(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c) {
    return Decorate(ctx, inst, ctx.OpFma(ctx.F32[1], a, b, c));
}

Id EmitFPFma64(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c) {
    return Decorate(ctx, inst, ctx.OpFma(ctx.F64[1], a, b, c));
}

void EmitFPMax32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPMax64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPMin32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPMin64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitFPMul16(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFMul(ctx.F16[1], a, b));
}

Id EmitFPMul32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFMul(ctx.F32[1], a, b));
}

Id EmitFPMul64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFMul(ctx.F64[1], a, b));
}

Id EmitFPNeg16(EmitContext& ctx, Id value) {
    return ctx.OpFNegate(ctx.F16[1], value);
}

Id EmitFPNeg32(EmitContext& ctx, Id value) {
    return ctx.OpFNegate(ctx.F32[1], value);
}

Id EmitFPNeg64(EmitContext& ctx, Id value) {
    return ctx.OpFNegate(ctx.F64[1], value);
}

void EmitFPRecip32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPRecip64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPRecipSqrt32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPRecipSqrt64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPSqrt(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPSin(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPSinNotReduced(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPExp2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPExp2NotReduced(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPCos(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPCosNotReduced(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPLog2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitFPSaturate16(EmitContext& ctx, Id value) {
    const Id zero{ctx.Constant(ctx.F16[1], u16{0})};
    const Id one{ctx.Constant(ctx.F16[1], u16{0x3c00})};
    return Saturate(ctx, ctx.F16[1], value, zero, one);
}

Id EmitFPSaturate32(EmitContext& ctx, Id value) {
    const Id zero{ctx.Constant(ctx.F32[1], f32{0.0})};
    const Id one{ctx.Constant(ctx.F32[1], f32{1.0})};
    return Saturate(ctx, ctx.F32[1], value, zero, one);
}

Id EmitFPSaturate64(EmitContext& ctx, Id value) {
    const Id zero{ctx.Constant(ctx.F64[1], f64{0.0})};
    const Id one{ctx.Constant(ctx.F64[1], f64{1.0})};
    return Saturate(ctx, ctx.F64[1], value, zero, one);
}

Id EmitFPRoundEven16(EmitContext& ctx, Id value) {
    return ctx.OpRoundEven(ctx.F16[1], value);
}

Id EmitFPRoundEven32(EmitContext& ctx, Id value) {
    return ctx.OpRoundEven(ctx.F32[1], value);
}

Id EmitFPRoundEven64(EmitContext& ctx, Id value) {
    return ctx.OpRoundEven(ctx.F64[1], value);
}

Id EmitFPFloor16(EmitContext& ctx, Id value) {
    return ctx.OpFloor(ctx.F16[1], value);
}

Id EmitFPFloor32(EmitContext& ctx, Id value) {
    return ctx.OpFloor(ctx.F32[1], value);
}

Id EmitFPFloor64(EmitContext& ctx, Id value) {
    return ctx.OpFloor(ctx.F64[1], value);
}

Id EmitFPCeil16(EmitContext& ctx, Id value) {
    return ctx.OpCeil(ctx.F16[1], value);
}

Id EmitFPCeil32(EmitContext& ctx, Id value) {
    return ctx.OpCeil(ctx.F32[1], value);
}

Id EmitFPCeil64(EmitContext& ctx, Id value) {
    return ctx.OpCeil(ctx.F64[1], value);
}

Id EmitFPTrunc16(EmitContext& ctx, Id value) {
    return ctx.OpTrunc(ctx.F16[1], value);
}

Id EmitFPTrunc32(EmitContext& ctx, Id value) {
    return ctx.OpTrunc(ctx.F32[1], value);
}

Id EmitFPTrunc64(EmitContext& ctx, Id value) {
    return ctx.OpTrunc(ctx.F64[1], value);
}

} // namespace Shader::Backend::SPIRV
