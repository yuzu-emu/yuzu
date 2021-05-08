// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

void EmitIAdd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIAdd64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitISub32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitISub64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIMul32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitINeg32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitINeg64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIAbs32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIAbs64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftLeftLogical32([[maybe_unused]] EmitContext& ctx,
                            [[maybe_unused]] std::string_view base,
                            [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftLeftLogical64([[maybe_unused]] EmitContext& ctx,
                            [[maybe_unused]] std::string_view base,
                            [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightLogical32([[maybe_unused]] EmitContext& ctx,
                             [[maybe_unused]] std::string_view base,
                             [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightLogical64([[maybe_unused]] EmitContext& ctx,
                             [[maybe_unused]] std::string_view base,
                             [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightArithmetic32([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string_view base,
                                [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightArithmetic64([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string_view base,
                                [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseAnd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseOr32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                     [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseXor32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitFieldInsert([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view base,
                        [[maybe_unused]] std::string_view insert,
                        [[maybe_unused]] std::string_view offset,
                        [[maybe_unused]] std::string_view count) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitFieldSExtract([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] std::string_view base,
                          [[maybe_unused]] std::string_view offset,
                          [[maybe_unused]] std::string_view count) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitFieldUExtract([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] std::string_view base,
                          [[maybe_unused]] std::string_view offset,
                          [[maybe_unused]] std::string_view count) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitReverse32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitCount32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseNot32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFindSMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFindUMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view a,
                [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value, [[maybe_unused]] std::string_view min,
                  [[maybe_unused]] std::string_view max) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value, [[maybe_unused]] std::string_view min,
                  [[maybe_unused]] std::string_view max) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSLessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitULessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSLessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitULessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitINotEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                   [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    throw NotImplementedException("GLASM instruction");
}

} // namespace Shader::Backend::GLASM
