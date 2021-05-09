// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

void EmitIAdd32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("ADD.S {}.x,{},{};", inst, a, b);
}

void EmitIAdd64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register a,
                [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitISub32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("SUB.S {}.x,{},{};", inst, a, b);
}

void EmitISub64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register a,
                [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIMul32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("MUL.S {}.x,{},{};", inst, a, b);
}

void EmitINeg32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitINeg64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIAbs32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIAbs64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftLeftLogical32(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 shift) {
    ctx.Add("SHL.U {}.x,{},{};", inst, base, shift);
}

void EmitShiftLeftLogical64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register base,
                            [[maybe_unused]] Register shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightLogical32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 base,
                             [[maybe_unused]] ScalarU32 shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightLogical64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register base,
                             [[maybe_unused]] Register shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightArithmetic32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 base,
                                [[maybe_unused]] ScalarS32 shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightArithmetic64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register base,
                                [[maybe_unused]] Register shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseAnd32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("AND.S {}.x,{},{};", inst, a, b);
}

void EmitBitwiseOr32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("OR.S {}.x,{},{};", inst, a, b);
}

void EmitBitwiseXor32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("XOR.S {}.x,{},{};", inst, a, b);
}

void EmitBitFieldInsert([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 base,
                        [[maybe_unused]] ScalarS32 insert, [[maybe_unused]] ScalarS32 offset,
                        [[maybe_unused]] ScalarS32 count) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitFieldSExtract([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] ScalarS32 base, [[maybe_unused]] ScalarS32 offset,
                          [[maybe_unused]] ScalarS32 count) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitFieldUExtract([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] ScalarU32 base, [[maybe_unused]] ScalarU32 offset,
                          [[maybe_unused]] ScalarU32 count) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitReverse32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitCount32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseNot32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFindSMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFindUMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 a,
                [[maybe_unused]] ScalarS32 b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 a,
                [[maybe_unused]] ScalarU32 b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 a,
                [[maybe_unused]] ScalarS32 b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 a,
                [[maybe_unused]] ScalarU32 b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] ScalarS32 value, [[maybe_unused]] ScalarS32 min,
                  [[maybe_unused]] ScalarS32 max) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] ScalarU32 value, [[maybe_unused]] ScalarU32 min,
                  [[maybe_unused]] ScalarU32 max) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSLessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 lhs,
                   [[maybe_unused]] ScalarS32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitULessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 lhs,
                   [[maybe_unused]] ScalarU32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 lhs,
                [[maybe_unused]] ScalarS32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSLessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 lhs,
                        [[maybe_unused]] ScalarS32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitULessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 lhs,
                        [[maybe_unused]] ScalarU32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 lhs,
                      [[maybe_unused]] ScalarS32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 lhs,
                      [[maybe_unused]] ScalarU32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitINotEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 lhs,
                   [[maybe_unused]] ScalarS32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarS32 lhs,
                           [[maybe_unused]] ScalarS32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 lhs,
                           [[maybe_unused]] ScalarU32 rhs) {
    throw NotImplementedException("GLASM instruction");
}

} // namespace Shader::Backend::GLASM
