
// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
void EmitIAdd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst* inst,
                [[maybe_unused]] std::string a, [[maybe_unused]] std::string b) {
    ctx.AddU32("{}={}+{};", *inst, a, b);
}

void EmitIAdd64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string a,
                [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitISub32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string a,
                [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitISub64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string a,
                [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitIMul32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string a,
                [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitINeg32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitINeg64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitIAbs32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitIAbs64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitShiftLeftLogical32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string base,
                            [[maybe_unused]] std::string shift) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitShiftLeftLogical64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string base,
                            [[maybe_unused]] std::string shift) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitShiftRightLogical32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string base,
                             [[maybe_unused]] std::string shift) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitShiftRightLogical64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string base,
                             [[maybe_unused]] std::string shift) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitShiftRightArithmetic32([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string base,
                                [[maybe_unused]] std::string shift) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitShiftRightArithmetic64([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string base,
                                [[maybe_unused]] std::string shift) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitwiseAnd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst* inst,
                      [[maybe_unused]] std::string a, [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitwiseOr32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst* inst,
                     [[maybe_unused]] std::string a, [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitwiseXor32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst* inst,
                      [[maybe_unused]] std::string a, [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitFieldInsert([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string base,
                        [[maybe_unused]] std::string insert, [[maybe_unused]] std::string offset,
                        std::string count) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitFieldSExtract([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst* inst,
                          [[maybe_unused]] std::string base, [[maybe_unused]] std::string offset,
                          std::string count) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitFieldUExtract([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst* inst,
                          [[maybe_unused]] std::string base, [[maybe_unused]] std::string offset,
                          std::string count) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitReverse32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitCount32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitwiseNot32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitFindSMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitFindUMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string a,
                [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitUMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string a,
                [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string a,
                [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitUMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string a,
                [[maybe_unused]] std::string b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst* inst,
                  [[maybe_unused]] std::string value, [[maybe_unused]] std::string min,
                  std::string max) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitUClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst* inst,
                  [[maybe_unused]] std::string value, [[maybe_unused]] std::string min,
                  std::string max) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSLessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                   [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitULessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                   [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitIEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSLessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                        [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitULessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                        [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                      [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitUGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                      [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitINotEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                   [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                           [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitUGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string lhs,
                           [[maybe_unused]] std::string rhs) {
    throw NotImplementedException("GLSL Instruction");
}
} // namespace Shader::Backend::GLSL
