// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

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
    ctx.Add("ADD.F {}.x,{},{};", inst, a, b);
}

void EmitFPAdd64(EmitContext& ctx, IR::Inst& inst, ScalarF64 a, ScalarF64 b) {
    ctx.LongAdd("ADD.F64 {}.x,{},{};", inst, a, b);
}

void EmitFPFma16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] Register a, [[maybe_unused]] Register b,
                 [[maybe_unused]] Register c) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFma32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b, ScalarF32 c) {
    ctx.Add("MAD.F {}.x,{},{},{};", inst, a, b, c);
}

void EmitFPFma64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] Register a, [[maybe_unused]] Register b,
                 [[maybe_unused]] Register c) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 a,
                 [[maybe_unused]] ScalarF32 b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMax64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register a,
                 [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 a,
                 [[maybe_unused]] ScalarF32 b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMin64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register a,
                 [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMul16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] Register a, [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMul32(EmitContext& ctx, IR::Inst& inst, ScalarF32 a, ScalarF32 b) {
    ctx.Add("MUL.F {}.x,{},{};", inst, a, b);
}

void EmitFPMul64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] Register a, [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
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

void EmitFPSin([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCos([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPExp2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPLog2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecip32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecip64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecipSqrt32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecipSqrt64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPSqrt([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
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

void EmitFPClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value,
                   [[maybe_unused]] ScalarF32 min_value, [[maybe_unused]] ScalarF32 max_value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPClamp64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value,
                   [[maybe_unused]] Register min_value, [[maybe_unused]] Register max_value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRoundEven16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRoundEven32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRoundEven64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFloor16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFloor32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFloor64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCeil16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCeil32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCeil64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPTrunc16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPTrunc32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPTrunc64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                      [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("SEQ.F {}.x,{},{};SNE.S {}.x,{},0;", ret, lhs, rhs, ret, ret);
}

void EmitFPOrdEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                      [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                        [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 lhs,
                        [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                        [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                         [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdNotEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 lhs,
                         [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdNotEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                         [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                           [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordNotEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 lhs,
                           [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordNotEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                           [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                         [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThan32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("SLT.F {}.x,{},{};SNE.S {}.x,{}.x,0;", ret, lhs, rhs, ret, ret);
}

void EmitFPOrdLessThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                         [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                           [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 lhs,
                           [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                           [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                            [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 lhs,
                            [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                            [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                              [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 lhs,
                              [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                              [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                              [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThanEqual32(EmitContext& ctx, IR::Inst& inst, ScalarF32 lhs, ScalarF32 rhs) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("SLE.F {}.x,{},{};SNE.S {}.x,{}.x,0;", ret, lhs, rhs, ret, ret);
}

void EmitFPOrdLessThanEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                              [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThanEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 lhs,
                                [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThanEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                 [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThanEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 lhs,
                                 [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThanEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                 [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                   [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThanEqual32([[maybe_unused]] EmitContext& ctx,
                                   [[maybe_unused]] ScalarF32 lhs, [[maybe_unused]] ScalarF32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThanEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register lhs,
                                   [[maybe_unused]] Register rhs) {
    throw NotImplementedException("GLASM instruction");
}

} // namespace Shader::Backend::GLASM
