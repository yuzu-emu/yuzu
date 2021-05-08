// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

void EmitFPAbs16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPAbs32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.Add("MOV.F {},|{}|;", inst, value);
}

void EmitFPAbs64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPAdd16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPAdd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.Add("ADD.F {},{},{};", inst, a, b);
}

void EmitFPAdd64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFma16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b,
                 [[maybe_unused]] std::string_view c) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFma32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b,
                 std::string_view c) {
    ctx.Add("MAD.F {},{},{},{};", inst, a, b, c);
}

void EmitFPFma64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b,
                 [[maybe_unused]] std::string_view c) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                 [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMax64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                 [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                 [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMin64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                 [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMul16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPMul32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.Add("MUL.F {},{},{};", inst, a, b);
}

void EmitFPMul64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPNeg16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPNeg32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    if (value[0] == '-') {
        // Guard against negating a negative immediate
        ctx.Add("MOV.F {},{};", inst, value.substr(1));
    } else {
        ctx.Add("MOV.F {},-{};", inst, value);
    }
}

void EmitFPNeg64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPSin([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCos([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPExp2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPLog2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecip32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecip64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecipSqrt32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRecipSqrt64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPSqrt([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPSaturate16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPSaturate32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.Add("MOV.F.SAT {},{};", inst, value);
}

void EmitFPSaturate64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPClamp16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value,
                   [[maybe_unused]] std::string_view min_value,
                   [[maybe_unused]] std::string_view max_value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value,
                   [[maybe_unused]] std::string_view min_value,
                   [[maybe_unused]] std::string_view max_value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPClamp64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value,
                   [[maybe_unused]] std::string_view min_value,
                   [[maybe_unused]] std::string_view max_value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRoundEven16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRoundEven32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPRoundEven64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFloor16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFloor32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPFloor64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCeil16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCeil32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPCeil64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPTrunc16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPTrunc32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPTrunc64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                      std::string_view rhs) {
    const std::string ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("SEQ.F {},{},{};SNE.S {},{},0;", ret, lhs, rhs, ret, ret);
}

void EmitFPOrdEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdNotEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdNotEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordNotEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordNotEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThan32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                         std::string_view rhs) {
    const std::string ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("SLT.F {},{},{};SNE.S {},{},0;", ret, lhs, rhs, ret, ret);
}

void EmitFPOrdLessThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThan16([[maybe_unused]] EmitContext& ctx,
                            [[maybe_unused]] std::string_view lhs,
                            [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThan32([[maybe_unused]] EmitContext& ctx,
                            [[maybe_unused]] std::string_view lhs,
                            [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThan64([[maybe_unused]] EmitContext& ctx,
                            [[maybe_unused]] std::string_view lhs,
                            [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThan16([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThan32([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThan64([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThanEqual16([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdLessThanEqual32(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                              std::string_view rhs) {
    const std::string ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("SLE.F {},{},{};SNE.S {},{},0;", ret, lhs, rhs, ret, ret);
}

void EmitFPOrdLessThanEqual64([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThanEqual16([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string_view lhs,
                                [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThanEqual32([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string_view lhs,
                                [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordLessThanEqual64([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string_view lhs,
                                [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThanEqual16([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view lhs,
                                 [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThanEqual32([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view lhs,
                                 [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPOrdGreaterThanEqual64([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view lhs,
                                 [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThanEqual16([[maybe_unused]] EmitContext& ctx,
                                   [[maybe_unused]] std::string_view lhs,
                                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThanEqual32([[maybe_unused]] EmitContext& ctx,
                                   [[maybe_unused]] std::string_view lhs,
                                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFPUnordGreaterThanEqual64([[maybe_unused]] EmitContext& ctx,
                                   [[maybe_unused]] std::string_view lhs,
                                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

} // namespace Shader::Backend::GLASM
