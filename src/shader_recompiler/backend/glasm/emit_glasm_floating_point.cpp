// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
template <typename InputType>
void Compare(EmitContext& ctx, IR::Inst& inst, InputType lhs, InputType rhs, std::string_view op,
             std::string_view type, bool ordered, bool inequality = false) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("{}.{} RC.x,{},{};", op, type, lhs, rhs);
    if (ordered && inequality) {
        ctx.Add("SEQ.{} RC.y,{},{};"
                "SEQ.{} RC.z,{},{};"
                "AND.U RC.x,RC.x,RC.y;"
                "AND.U RC.x,RC.x,RC.z;"
                "SNE.S {}.x,RC.x,0;",
                type, lhs, lhs, type, rhs, rhs, ret);
    } else if (ordered) {
        ctx.Add("SNE.S {}.x,RC.x,0;", ret);
    } else {
        ctx.Add("SNE.{} RC.y,{},{};"
                "SNE.{} RC.z,{},{};"
                "OR.U RC.x,RC.x,RC.y;"
                "OR.U RC.x,RC.x,RC.z;"
                "SNE.S {}.x,RC.x,0;",
                type, lhs, lhs, type, rhs, rhs, ret);
    }
}

template <typename InputType>
void Clamp(EmitContext& ctx, Register ret, InputType value, InputType min_value,
           InputType max_value, std::string_view type) {
    // Call MAX first to properly clamp nan to min_value instead
    ctx.Add("MAX.{} RC.x,{},{};"
            "MIN.{} {}.x,RC.x,{};",
            type, min_value, value, type, ret, max_value);
}

std::string_view Precise(IR::Inst& inst) {
    const bool precise{inst.Flags<IR::FpControl>().no_contraction};
    return precise ? ".PREC" : "";
}
} // Anonymous namespace

void EmitFPAbs16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPAbs32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("MOV.F {}.x,|{}|;", inst, value);
}

void EmitFPAbs64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    ctx.LongAdd("MOV.F64 {}.x,|{}|;", inst, value);
}

void EmitFPAdd16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] Register a, [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPAdd32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b) {
    ctx.Add("ADD.F{} {}.x,{},{};", Precise(inst), ctx.reg_alloc.Define(inst), a, b);
}

void EmitFPAdd64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b) {
    ctx.Add("ADD.F64{} {}.x,{},{};", Precise(inst), ctx.reg_alloc.LongDefine(inst), a, b);
}

void EmitFPFma16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] Register a, [[maybe_unused]] Register b,
                 [[maybe_unused]] Register c) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFma32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b, ScalarF32 c) {
    ctx.Add("MAD.F{} {}.x,{},{},{};", Precise(inst), ctx.reg_alloc.Define(inst), a, b, c);
}

void EmitFPFma64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b, ScalarF64 c) {
    ctx.Add("MAD.F64{} {}.x,{},{},{};", Precise(inst), ctx.reg_alloc.LongDefine(inst), a, b, c);
}

void EmitFPMax32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b) {
    ctx.Add("MAX.F {}.x,{},{};", inst, a, b);
}

void EmitFPMax64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b) {
    ctx.LongAdd("MAX.F64 {}.x,{},{};", inst, a, b);
}

void EmitFPMin32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b) {
    ctx.Add("MIN.F {}.x,{},{};", inst, a, b);
}

void EmitFPMin64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b) {
    ctx.LongAdd("MIN.F64 {}.x,{},{};", inst, a, b);
}

void EmitFPMul16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] Register a, [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMul32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b) {
    ctx.Add("MUL.F{} {}.x,{},{};", Precise(inst), ctx.reg_alloc.Define(inst), a, b);
}

void EmitFPMul64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b) {
    ctx.Add("MUL.F64{} {}.x,{},{};", Precise(inst), ctx.reg_alloc.LongDefine(inst), a, b);
}

void EmitFPNeg16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPNeg32(EmitContext& ctx, IR::Inst& inst, ScalarRegister value) {
    ctx.Add("MOV.F {}.x,-{};", inst, value);
}

void EmitFPNeg64(EmitContext& ctx, IR::Inst& inst, Register value) {
    ctx.LongAdd("MOV.F64 {}.x,-{};", inst, value);
}

void EmitFPSin(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("SIN {}.x,{};", inst, value);
}

void EmitFPCos(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("COS {}.x,{};", inst, value);
}

void EmitFPExp2(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("EX2 {}.x,{};", inst, value);
}

void EmitFPLog2(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("LG2 {}.x,{};", inst, value);
}

void EmitFPRecip32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("RCP {}.x,{};", inst, value);
}

void EmitFPRecip64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecipSqrt32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("RSQ {}.x,{};", inst, value);
}

void EmitFPRecipSqrt64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPSqrt(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("RSQ RC.x,{};RCP {}.x,RC.x;", value, ret);
}

void EmitFPSaturate16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPSaturate32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("MOV.F.SAT {}.x,{};", inst, value);
}

void EmitFPSaturate64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPClamp16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value,
                   [[maybe_unused]] Register min_value, [[maybe_unused]] Register max_value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPClamp32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value, ScalarF32 min_value,
                   ScalarF32 max_value) {
    Clamp(ctx, ctx.reg_alloc.Define(inst), value, min_value, max_value, "F");
}

void EmitFPClamp64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value, ScalarF64 min_value,
                   ScalarF64 max_value) {
    Clamp(ctx, ctx.reg_alloc.LongDefine(inst), value, min_value, max_value, "F64");
}

void EmitFPRoundEven16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRoundEven32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("ROUND.F {}.x,{};", inst, value);
}

void EmitFPRoundEven64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    ctx.LongAdd("ROUND.F64 {}.x,{};", inst, value);
}

void EmitFPFloor16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFloor32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("FLR.F {}.x,{};", inst, value);
}

void EmitFPFloor64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    ctx.LongAdd("FLR.F64 {}.x,{};", inst, value);
}

void EmitFPCeil16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCeil32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("CEIL.F {}.x,{};", inst, value);
}

void EmitFPCeil64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    ctx.LongAdd("CEIL.F64 {}.x,{};", inst, value);
}

void EmitFPTrunc16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPTrunc32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    ctx.Add("TRUNC.F {}.x,{};", inst, value);
}

void EmitFPTrunc64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    ctx.LongAdd("TRUNC.F64 {}.x,{};", inst, value);
}

void EmitFPOrdEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                      [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SEQ", "F", true);
}

void EmitFPOrdEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SEQ", "F64", true);
}

void EmitFPUnordEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                        [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SEQ", "F", false);
}

void EmitFPUnordEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SEQ", "F64", false);
}

void EmitFPOrdNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                         [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdNotEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SNE", "F", true, true);
}

void EmitFPOrdNotEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SNE", "F64", true, true);
}

void EmitFPUnordNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                           [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordNotEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SNE", "F", false, true);
}

void EmitFPUnordNotEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SNE", "F64", false, true);
}

void EmitFPOrdLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                         [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SLT", "F", true);
}

void EmitFPOrdLessThan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SLT", "F64", true);
}

void EmitFPUnordLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                           [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SLT", "F", false);
}

void EmitFPUnordLessThan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SLT", "F64", false);
}

void EmitFPOrdGreaterThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                            [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SGT", "F", true);
}

void EmitFPOrdGreaterThan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SGT", "F64", true);
}

void EmitFPUnordGreaterThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                              [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SGT", "F", false);
}

void EmitFPUnordGreaterThan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SGT", "F64", false);
}

void EmitFPOrdLessThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                              [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SLE", "F", true);
}

void EmitFPOrdLessThanEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SLE", "F64", true);
}

void EmitFPUnordLessThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SLE", "F", false);
}

void EmitFPUnordLessThanEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SLE", "F64", false);
}

void EmitFPOrdGreaterThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                 [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SGE", "F", true);
}

void EmitFPOrdGreaterThanEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SGE", "F64", true);
}

void EmitFPUnordGreaterThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                   [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    Compare(ctx, inst, lhs, rhs, "SGE", "F", false);
}

void EmitFPUnordGreaterThanEqual64(EmitContext& ctx, IR::Inst& inst, ScalarF64 lhs, ScalarF64 rhs) {
    Compare(ctx, inst, lhs, rhs, "SGE", "F64", false);
}

void EmitFPIsNan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPIsNan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Compare(ctx, inst, value, value, "SNE", "F", true, false);
}

void EmitFPIsNan64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    Compare(ctx, inst, value, value, "SNE", "F64", true, false);
}

} // namespace Shader::Backend::GLASM
