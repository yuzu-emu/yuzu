// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
void Compare(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs,
             std::string_view op, bool ordered) {
    ctx.AddU1("{}={}{}{}", inst, lhs, op, rhs, lhs, rhs);
    if (ordered) {
        ctx.Add("&&!isnan({})&&!isnan({});", lhs, rhs);
    } else {
        ctx.Add("||isnan({})||isnan({});", lhs, rhs);
    }
}

bool IsPrecise(const IR::Inst& inst) {
    return inst.Flags<IR::FpControl>().no_contraction;
}
} // Anonymous namespace

void EmitFPAbs16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPAbs32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=abs({});", inst, value);
}

void EmitFPAbs64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=abs({});", inst, value);
}

void EmitFPAdd16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    NotImplemented();
}

void EmitFPAdd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    if (IsPrecise(inst)) {
        ctx.AddPrecF32("{}={}+{};", inst, a, b);
    } else {
        ctx.AddF32("{}={}+{};", inst, a, b);
    }
}

void EmitFPAdd64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    if (IsPrecise(inst)) {
        ctx.AddPrecF64("{}={}+{};", inst, a, b);
    } else {
        ctx.AddF64("{}={}+{};", inst, a, b);
    }
}

void EmitFPFma16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b,
                 [[maybe_unused]] std::string_view c) {
    NotImplemented();
}

void EmitFPFma32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b,
                 std::string_view c) {
    if (IsPrecise(inst)) {
        ctx.AddPrecF32("{}=fma({},{},{});", inst, a, b, c);
    } else {
        ctx.AddF32("{}=fma({},{},{});", inst, a, b, c);
    }
}

void EmitFPFma64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b,
                 std::string_view c) {
    if (IsPrecise(inst)) {
        ctx.AddPrecF64("{}=fma({},{},{});", inst, a, b, c);
    } else {
        ctx.AddF64("{}=fma({},{},{});", inst, a, b, c);
    }
}

void EmitFPMax32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddF32("{}=max({},{});", inst, a, b);
}

void EmitFPMax64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddF64("{}=max({},{});", inst, a, b);
}

void EmitFPMin32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddF32("{}=min({},{});", inst, a, b);
}

void EmitFPMin64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddF64("{}=min({},{});", inst, a, b);
}

void EmitFPMul16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    NotImplemented();
}

void EmitFPMul32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    if (IsPrecise(inst)) {
        ctx.AddPrecF32("{}={}*{};", inst, a, b);
    } else {
        ctx.AddF32("{}={}*{};", inst, a, b);
    }
}

void EmitFPMul64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    if (IsPrecise(inst)) {
        ctx.AddPrecF64("{}={}*{};", inst, a, b);
    } else {
        ctx.AddF64("{}={}*{};", inst, a, b);
    }
}

void EmitFPNeg16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPNeg32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=-({});", inst, value);
}

void EmitFPNeg64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=-({});", inst, value);
}

void EmitFPSin(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=sin({});", inst, value);
}

void EmitFPCos(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=cos({});", inst, value);
}

void EmitFPExp2(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=exp2({});", inst, value);
}

void EmitFPLog2(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=log2({});", inst, value);
}

void EmitFPRecip32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=(1.0f)/{};", inst, value);
}

void EmitFPRecip64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=1.0/{};", inst, value);
}

void EmitFPRecipSqrt32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=inversesqrt({});", inst, value);
}

void EmitFPRecipSqrt64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPSqrt(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=sqrt({});", inst, value);
}

void EmitFPSaturate16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPSaturate32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=min(max({},0.0),1.0);", inst, value);
}

void EmitFPSaturate64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=min(max({},0.0),1.0);", inst, value);
}

void EmitFPClamp16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value,
                   [[maybe_unused]] std::string_view min_value,
                   [[maybe_unused]] std::string_view max_value) {
    NotImplemented();
}

void EmitFPClamp32(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                   std::string_view min_value, std::string_view max_value) {
    // GLSL's clamp does not produce desirable results
    ctx.AddF32("{}=min(max({},float({})),float({}));", inst, value, min_value, max_value);
}

void EmitFPClamp64(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                   std::string_view min_value, std::string_view max_value) {
    // GLSL's clamp does not produce desirable results
    ctx.AddF64("{}=min(max({},double({})),double({}));", inst, value, min_value, max_value);
}

void EmitFPRoundEven16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPRoundEven32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=roundEven({});", inst, value);
}

void EmitFPRoundEven64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=roundEven({});", inst, value);
}

void EmitFPFloor16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPFloor32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=floor({});", inst, value);
}

void EmitFPFloor64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=floor({});", inst, value);
}

void EmitFPCeil16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPCeil32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=ceil({});", inst, value);
}

void EmitFPCeil64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=ceil({});", inst, value);
}

void EmitFPTrunc16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPTrunc32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF32("{}=trunc({});", inst, value);
}

void EmitFPTrunc64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddF64("{}=trunc({});", inst, value);
}

void EmitFPOrdEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPOrdEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                      std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "==", true);
}

void EmitFPOrdEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                      std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "==", true);
}

void EmitFPUnordEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPUnordEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                        std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "==", false);
}

void EmitFPUnordEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                        std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "==", false);
}

void EmitFPOrdNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPOrdNotEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "!=", true);
}

void EmitFPOrdNotEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "!=", true);
}

void EmitFPUnordNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPUnordNotEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "!=", false);
}

void EmitFPUnordNotEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "!=", false);
}

void EmitFPOrdLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPOrdLessThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "<", true);
}

void EmitFPOrdLessThan64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "<", true);
}

void EmitFPUnordLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPUnordLessThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "<", false);
}

void EmitFPUnordLessThan64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "<", false);
}

void EmitFPOrdGreaterThan16([[maybe_unused]] EmitContext& ctx,
                            [[maybe_unused]] std::string_view lhs,
                            [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPOrdGreaterThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                            std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, ">", true);
}

void EmitFPOrdGreaterThan64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                            std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, ">", true);
}

void EmitFPUnordGreaterThan16([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPUnordGreaterThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, ">", false);
}

void EmitFPUnordGreaterThan64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, ">", false);
}

void EmitFPOrdLessThanEqual16([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPOrdLessThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "<=", true);
}

void EmitFPOrdLessThanEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "<=", true);
}

void EmitFPUnordLessThanEqual16([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string_view lhs,
                                [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPUnordLessThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "<=", false);
}

void EmitFPUnordLessThanEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, "<=", false);
}

void EmitFPOrdGreaterThanEqual16([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view lhs,
                                 [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPOrdGreaterThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                 std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, ">=", true);
}

void EmitFPOrdGreaterThanEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                 std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, ">=", true);
}

void EmitFPUnordGreaterThanEqual16([[maybe_unused]] EmitContext& ctx,
                                   [[maybe_unused]] std::string_view lhs,
                                   [[maybe_unused]] std::string_view rhs) {
    NotImplemented();
}

void EmitFPUnordGreaterThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                   std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, ">=", false);
}

void EmitFPUnordGreaterThanEqual64(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                                   std::string_view rhs) {
    Compare(ctx, inst, lhs, rhs, ">=", false);
}

void EmitFPIsNan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    NotImplemented();
}

void EmitFPIsNan32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU1("{}=isnan({});", inst, value);
}

void EmitFPIsNan64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU1("{}=isnan({});", inst, value);
}

} // namespace Shader::Backend::GLSL
