// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
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

Id Clamp(EmitContext& ctx, Id type, Id value, Id zero, Id one) {
    if (ctx.profile.has_broken_spirv_clamp) {
        return ctx.OpFMin(type, ctx.OpFMax(type, value, zero), one);
    } else {
        return ctx.OpFClamp(type, value, zero, one);
    }
}

Id FPOrdNotEqual(EmitContext& ctx, Id lhs, Id rhs) {
    if (ctx.profile.ignore_nan_fp_comparisons) {
        const Id comp{ctx.OpFOrdEqual(ctx.U1, lhs, rhs)};
        const Id lhs_not_nan{ctx.OpLogicalNot(ctx.U1, ctx.OpIsNan(ctx.U1, lhs))};
        const Id rhs_not_nan{ctx.OpLogicalNot(ctx.U1, ctx.OpIsNan(ctx.U1, rhs))};
        return ctx.OpLogicalAnd(ctx.U1, ctx.OpLogicalAnd(ctx.U1, comp, lhs_not_nan), rhs_not_nan);
    } else {
        return ctx.OpFOrdNotEqual(ctx.U1, lhs, rhs);
    }
}

Id FPUnordCompare(Id (EmitContext::*comp_func)(Id, Id, Id), EmitContext& ctx, Id lhs, Id rhs) {
    if (ctx.profile.ignore_nan_fp_comparisons) {
        const Id lhs_nan{ctx.OpIsNan(ctx.U1, lhs)};
        const Id rhs_nan{ctx.OpIsNan(ctx.U1, rhs)};
        const Id comp{(ctx.*comp_func)(ctx.U1, lhs, rhs)};
        return ctx.OpLogicalOr(ctx.U1, ctx.OpLogicalOr(ctx.U1, comp, lhs_nan), rhs_nan);
    } else {
        return (ctx.*comp_func)(ctx.U1, lhs, rhs);
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

Id EmitFPMax32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpFMax(ctx.F32[1], a, b);
}

Id EmitFPMax64(EmitContext& ctx, Id a, Id b) {
    return ctx.OpFMax(ctx.F64[1], a, b);
}

Id EmitFPMin32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpFMin(ctx.F32[1], a, b);
}

Id EmitFPMin64(EmitContext& ctx, Id a, Id b) {
    return ctx.OpFMin(ctx.F64[1], a, b);
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

Id EmitFPSin(EmitContext& ctx, Id value) {
    return ctx.OpSin(ctx.F32[1], value);
}

Id EmitFPCos(EmitContext& ctx, Id value) {
    return ctx.OpCos(ctx.F32[1], value);
}

Id EmitFPExp2(EmitContext& ctx, Id value) {
    return ctx.OpExp2(ctx.F32[1], value);
}

Id EmitFPLog2(EmitContext& ctx, Id value) {
    return ctx.OpLog2(ctx.F32[1], value);
}

Id EmitFPRecip32(EmitContext& ctx, Id value) {
    return ctx.OpFDiv(ctx.F32[1], ctx.Const(1.0f), value);
}

Id EmitFPRecip64(EmitContext& ctx, Id value) {
    return ctx.OpFDiv(ctx.F64[1], ctx.Constant(ctx.F64[1], 1.0f), value);
}

Id EmitFPRecipSqrt32(EmitContext& ctx, Id value) {
    return ctx.OpInverseSqrt(ctx.F32[1], value);
}

Id EmitFPRecipSqrt64(EmitContext& ctx, Id value) {
    return ctx.OpInverseSqrt(ctx.F64[1], value);
}

Id EmitFPSqrt(EmitContext& ctx, Id value) {
    return ctx.OpSqrt(ctx.F32[1], value);
}

Id EmitFPSaturate16(EmitContext& ctx, Id value) {
    const Id zero{ctx.Constant(ctx.F16[1], u16{0})};
    const Id one{ctx.Constant(ctx.F16[1], u16{0x3c00})};
    return Clamp(ctx, ctx.F16[1], value, zero, one);
}

Id EmitFPSaturate32(EmitContext& ctx, Id value) {
    const Id zero{ctx.Const(f32{0.0})};
    const Id one{ctx.Const(f32{1.0})};
    return Clamp(ctx, ctx.F32[1], value, zero, one);
}

Id EmitFPSaturate64(EmitContext& ctx, Id value) {
    const Id zero{ctx.Constant(ctx.F64[1], f64{0.0})};
    const Id one{ctx.Constant(ctx.F64[1], f64{1.0})};
    return Clamp(ctx, ctx.F64[1], value, zero, one);
}

Id EmitFPClamp16(EmitContext& ctx, Id value, Id min_value, Id max_value) {
    return Clamp(ctx, ctx.F16[1], value, min_value, max_value);
}

Id EmitFPClamp32(EmitContext& ctx, Id value, Id min_value, Id max_value) {
    return Clamp(ctx, ctx.F32[1], value, min_value, max_value);
}

Id EmitFPClamp64(EmitContext& ctx, Id value, Id min_value, Id max_value) {
    return Clamp(ctx, ctx.F64[1], value, min_value, max_value);
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

Id EmitFPOrdEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdEqual(ctx.U1, lhs, rhs);
}

Id EmitFPOrdEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdEqual(ctx.U1, lhs, rhs);
}

Id EmitFPOrdEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdEqual(ctx.U1, lhs, rhs);
}

Id EmitFPUnordEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordEqual, ctx, lhs, rhs);
}

Id EmitFPUnordEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordEqual, ctx, lhs, rhs);
}

Id EmitFPUnordEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordEqual, ctx, lhs, rhs);
}

Id EmitFPOrdNotEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return FPOrdNotEqual(ctx, lhs, rhs);
}

Id EmitFPOrdNotEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return FPOrdNotEqual(ctx, lhs, rhs);
}

Id EmitFPOrdNotEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return FPOrdNotEqual(ctx, lhs, rhs);
}

Id EmitFPUnordNotEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFUnordNotEqual(ctx.U1, lhs, rhs);
}

Id EmitFPUnordNotEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFUnordNotEqual(ctx.U1, lhs, rhs);
}

Id EmitFPUnordNotEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFUnordNotEqual(ctx.U1, lhs, rhs);
}

Id EmitFPOrdLessThan16(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdLessThan(ctx.U1, lhs, rhs);
}

Id EmitFPOrdLessThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdLessThan(ctx.U1, lhs, rhs);
}

Id EmitFPOrdLessThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdLessThan(ctx.U1, lhs, rhs);
}

Id EmitFPUnordLessThan16(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordLessThan, ctx, lhs, rhs);
}

Id EmitFPUnordLessThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordLessThan, ctx, lhs, rhs);
}

Id EmitFPUnordLessThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordLessThan, ctx, lhs, rhs);
}

Id EmitFPOrdGreaterThan16(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdGreaterThan(ctx.U1, lhs, rhs);
}

Id EmitFPOrdGreaterThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdGreaterThan(ctx.U1, lhs, rhs);
}

Id EmitFPOrdGreaterThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdGreaterThan(ctx.U1, lhs, rhs);
}

Id EmitFPUnordGreaterThan16(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordGreaterThan, ctx, lhs, rhs);
}

Id EmitFPUnordGreaterThan32(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordGreaterThan, ctx, lhs, rhs);
}

Id EmitFPUnordGreaterThan64(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordGreaterThan, ctx, lhs, rhs);
}

Id EmitFPOrdLessThanEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdLessThanEqual(ctx.U1, lhs, rhs);
}

Id EmitFPOrdLessThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdLessThanEqual(ctx.U1, lhs, rhs);
}

Id EmitFPOrdLessThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdLessThanEqual(ctx.U1, lhs, rhs);
}

Id EmitFPUnordLessThanEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordLessThanEqual, ctx, lhs, rhs);
}

Id EmitFPUnordLessThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordLessThanEqual, ctx, lhs, rhs);
}

Id EmitFPUnordLessThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordLessThanEqual, ctx, lhs, rhs);
}

Id EmitFPOrdGreaterThanEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdGreaterThanEqual(ctx.U1, lhs, rhs);
}

Id EmitFPOrdGreaterThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdGreaterThanEqual(ctx.U1, lhs, rhs);
}

Id EmitFPOrdGreaterThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpFOrdGreaterThanEqual(ctx.U1, lhs, rhs);
}

Id EmitFPUnordGreaterThanEqual16(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordGreaterThanEqual, ctx, lhs, rhs);
}

Id EmitFPUnordGreaterThanEqual32(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordGreaterThanEqual, ctx, lhs, rhs);
}

Id EmitFPUnordGreaterThanEqual64(EmitContext& ctx, Id lhs, Id rhs) {
    return FPUnordCompare(&EmitContext::OpFUnordGreaterThanEqual, ctx, lhs, rhs);
}

Id EmitFPIsNan16(EmitContext& ctx, Id value) {
    return ctx.OpIsNan(ctx.U1, value);
}

Id EmitFPIsNan32(EmitContext& ctx, Id value) {
    return ctx.OpIsNan(ctx.U1, value);
}

Id EmitFPIsNan64(EmitContext& ctx, Id value) {
    return ctx.OpIsNan(ctx.U1, value);
}

} // namespace Shader::Backend::SPIRV
