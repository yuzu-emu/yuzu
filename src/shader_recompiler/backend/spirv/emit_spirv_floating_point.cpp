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
    switch (flags.rounding) {
    case IR::FpRounding::RN:
        break;
    case IR::FpRounding::RM:
        ctx.Decorate(op, spv::Decoration::FPRoundingMode, spv::FPRoundingMode::RTN);
        break;
    case IR::FpRounding::RP:
        ctx.Decorate(op, spv::Decoration::FPRoundingMode, spv::FPRoundingMode::RTP);
        break;
    case IR::FpRounding::RZ:
        ctx.Decorate(op, spv::Decoration::FPRoundingMode, spv::FPRoundingMode::RTZ);
        break;
    }
    if (flags.fmz_mode != IR::FmzMode::FTZ) {
        throw NotImplementedException("Denorm management not implemented");
    }
    return op;
}

} // Anonymous namespace

void EmitFPAbs16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPAbs32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPAbs64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
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

void EmitFPNeg16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPNeg32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPNeg64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
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

void EmitFPSaturate16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPSaturate32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPSaturate64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPRoundEven16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPRoundEven32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPRoundEven64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPFloor16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPFloor32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPFloor64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPCeil16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPCeil32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPCeil64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPTrunc16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPTrunc32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitFPTrunc64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
