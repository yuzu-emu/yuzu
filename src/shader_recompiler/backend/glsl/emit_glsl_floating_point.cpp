// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {

void EmitFPAbs16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPAbs32([[maybe_unused]] EmitContext& ctx, IR::Inst& inst,
                 [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=abs({});", inst, value);
}

void EmitFPAbs64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPAdd16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLSL");
}

void EmitFPAdd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddF32("{}=float({})+float({});", inst, a, b);
}

void EmitFPAdd64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddF64("{}=double({})+double({});", inst, a, b);
}

void EmitFPFma16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b,
                 [[maybe_unused]] std::string_view c) {
    throw NotImplementedException("GLSL");
}

void EmitFPFma32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b,
                 [[maybe_unused]] std::string_view c) {
    ctx.AddF32("{}=fma({},{},{});", inst, a, b, c);
}

void EmitFPFma64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b,
                 [[maybe_unused]] std::string_view c) {
    ctx.AddF64("{}=fma({},{},{});", inst, a, b, c);
}

void EmitFPMax32([[maybe_unused]] EmitContext& ctx, IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddF32("{}=max({},{});", inst, a, b);
}

void EmitFPMax64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddF64("{}=max({},{});", inst, a, b);
}

void EmitFPMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddF32("{}=min({},{});", inst, a, b);
}

void EmitFPMin64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddF64("{}=min({},{});", inst, a, b);
}

void EmitFPMul16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLSL");
}

void EmitFPMul32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddF32("{}={}*{};", inst, a, b);
}

void EmitFPMul64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddF64("{}={}*{};", inst, a, b);
}

void EmitFPNeg16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPNeg32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=-{};", inst, value);
}

void EmitFPNeg64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                 [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=-{};", inst, value);
}

void EmitFPSin([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
               [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=sin({});", inst, value);
}

void EmitFPCos([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
               [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=cos({});", inst, value);
}

void EmitFPExp2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=exp2({});", inst, value);
}

void EmitFPLog2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=log2({});", inst, value);
}

void EmitFPRecip32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=1/{};", inst, value);
}

void EmitFPRecip64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=1/{};", inst, value);
}

void EmitFPRecipSqrt32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPRecipSqrt64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPSqrt([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=sqrt({});", inst, value);
}

void EmitFPSaturate16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPSaturate32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=min(max({},0.0),1.0);", inst, value);
}

void EmitFPSaturate64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=min(max({},0.0),1.0);", inst, value);
}

void EmitFPClamp16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value,
                   [[maybe_unused]] std::string_view min_value,
                   [[maybe_unused]] std::string_view max_value) {
    throw NotImplementedException("GLSL");
}

void EmitFPClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value,
                   [[maybe_unused]] std::string_view min_value,
                   [[maybe_unused]] std::string_view max_value) {
    // GLSL's clamp does not produce desirable results
    ctx.AddF32("{}=min(max({},float({})),float({}));", inst, value, min_value, max_value);
}

void EmitFPClamp64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value,
                   [[maybe_unused]] std::string_view min_value,
                   [[maybe_unused]] std::string_view max_value) {
    // GLSL's clamp does not produce desirable results
    ctx.AddF64("{}=min(max({},double({})),double({}));", inst, value, min_value, max_value);
}

void EmitFPRoundEven16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPRoundEven32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=roundEven({});", inst, value);
}

void EmitFPRoundEven64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=roundEven({});", inst, value);
}

void EmitFPFloor16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPFloor32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=floor({});", inst, value);
}

void EmitFPFloor64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=floor({});", inst, value);
}

void EmitFPCeil16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPCeil32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=ceil({});", inst, value);
}

void EmitFPCeil64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=ceil({});", inst, value);
}

void EmitFPTrunc16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL");
}

void EmitFPTrunc32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=trunc({});", inst, value);
}

void EmitFPTrunc64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=trunc({});", inst, value);
}

void EmitFPOrdEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}={}=={};", inst, lhs, rhs);
}

void EmitFPOrdEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                        [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                        [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                        [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdNotEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdNotEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordNotEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordNotEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordNotEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdLessThan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdLessThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] std::string_view lhs,
                         [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordLessThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordLessThan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordLessThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdGreaterThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                            [[maybe_unused]] std::string_view lhs,
                            [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdGreaterThan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                            [[maybe_unused]] std::string_view lhs,
                            [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdGreaterThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                            [[maybe_unused]] std::string_view lhs,
                            [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordGreaterThan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordGreaterThan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordGreaterThan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdLessThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdLessThanEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdLessThanEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                              [[maybe_unused]] std::string_view lhs,
                              [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordLessThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] std::string_view lhs,
                                [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordLessThanEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] std::string_view lhs,
                                [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordLessThanEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] std::string_view lhs,
                                [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdGreaterThanEqual16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                 [[maybe_unused]] std::string_view lhs,
                                 [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdGreaterThanEqual32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                 [[maybe_unused]] std::string_view lhs,
                                 [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPOrdGreaterThanEqual64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                 [[maybe_unused]] std::string_view lhs,
                                 [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordGreaterThanEqual16([[maybe_unused]] EmitContext& ctx,
                                   [[maybe_unused]] IR::Inst& inst,
                                   [[maybe_unused]] std::string_view lhs,
                                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordGreaterThanEqual32([[maybe_unused]] EmitContext& ctx,
                                   [[maybe_unused]] IR::Inst& inst,
                                   [[maybe_unused]] std::string_view lhs,
                                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPUnordGreaterThanEqual64([[maybe_unused]] EmitContext& ctx,
                                   [[maybe_unused]] IR::Inst& inst,
                                   [[maybe_unused]] std::string_view lhs,
                                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLSL");
}

void EmitFPIsNan16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddU1("{}=isnan({});", inst, value);
}

void EmitFPIsNan32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddU1("{}=isnan({});", inst, value);
}

void EmitFPIsNan64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view value) {
    ctx.AddU1("{}=isnan({});", inst, value);
}

} // namespace Shader::Backend::GLSL
