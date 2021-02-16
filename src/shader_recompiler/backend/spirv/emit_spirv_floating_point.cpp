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

void EmitSPIRV::EmitFPAbs16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPAbs32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPAbs64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitFPAdd16(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFAdd(ctx.F16[1], a, b));
}

Id EmitSPIRV::EmitFPAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFAdd(ctx.F32[1], a, b));
}

Id EmitSPIRV::EmitFPAdd64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFAdd(ctx.F64[1], a, b));
}

Id EmitSPIRV::EmitFPFma16(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c) {
    return Decorate(ctx, inst, ctx.OpFma(ctx.F16[1], a, b, c));
}

Id EmitSPIRV::EmitFPFma32(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c) {
    return Decorate(ctx, inst, ctx.OpFma(ctx.F32[1], a, b, c));
}

Id EmitSPIRV::EmitFPFma64(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c) {
    return Decorate(ctx, inst, ctx.OpFma(ctx.F64[1], a, b, c));
}

void EmitSPIRV::EmitFPMax32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPMax64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPMin32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPMin64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitFPMul16(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFMul(ctx.F16[1], a, b));
}

Id EmitSPIRV::EmitFPMul32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFMul(ctx.F32[1], a, b));
}

Id EmitSPIRV::EmitFPMul64(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    return Decorate(ctx, inst, ctx.OpFMul(ctx.F64[1], a, b));
}

void EmitSPIRV::EmitFPNeg16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPNeg32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPNeg64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPRecip32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPRecip64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPRecipSqrt32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPRecipSqrt64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPSqrt(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPSin(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPSinNotReduced(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPExp2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPExp2NotReduced(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPCos(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPCosNotReduced(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPLog2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPSaturate16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPSaturate32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPSaturate64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPRoundEven16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPRoundEven32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPRoundEven64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPFloor16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPFloor32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPFloor64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPCeil16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPCeil32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPCeil64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPTrunc16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPTrunc32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitFPTrunc64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
