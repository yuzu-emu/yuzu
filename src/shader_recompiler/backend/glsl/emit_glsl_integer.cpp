// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
void EmitIAdd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddU32("{}={}+{};", inst, a, b);
}

void EmitIAdd64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitISub32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitISub64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitIMul32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitINeg32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    ctx.AddU32("{}=-{};", inst, value);
}

void EmitINeg64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitIAbs32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    ctx.AddU32("{}=abs({});", inst, value);
}

void EmitIAbs64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitShiftLeftLogical32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                            [[maybe_unused]] std::string_view base,
                            [[maybe_unused]] std::string_view shift) {
    ctx.AddU32("{}={}<<{};", inst, base, shift);
}

void EmitShiftLeftLogical64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                            [[maybe_unused]] std::string_view base,
                            [[maybe_unused]] std::string_view shift) {
    ctx.AddU64("{}={}<<{};", inst, base, shift);
}

void EmitShiftRightLogical32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                             [[maybe_unused]] std::string_view base,
                             [[maybe_unused]] std::string_view shift) {
    ctx.AddU32("{}={}>>{};", inst, base, shift);
}

void EmitShiftRightLogical64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                             [[maybe_unused]] std::string_view base,
                             [[maybe_unused]] std::string_view shift) {
    ctx.AddU64("{}={}>>{};", inst, base, shift);
}

void EmitShiftRightArithmetic32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] std::string_view base,
                                [[maybe_unused]] std::string_view shift) {
    ctx.AddS32("{}=int({})>>{};", inst, base, shift);
}

void EmitShiftRightArithmetic64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] std::string_view base,
                                [[maybe_unused]] std::string_view shift) {
    ctx.AddU64("{}=int64_t({})>>{};", inst, base, shift);
}

void EmitBitwiseAnd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddU32("{}={}&{};", inst, a, b);
}

void EmitBitwiseOr32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                     [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddU32("{}={}|{};", inst, a, b);
}

void EmitBitwiseXor32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddU32("{}={}^{};", inst, a, b);
}

void EmitBitFieldInsert([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                        [[maybe_unused]] std::string_view base,
                        [[maybe_unused]] std::string_view insert,
                        [[maybe_unused]] std::string_view offset,
                        [[maybe_unused]] std::string_view count) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitFieldSExtract([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] std::string_view base,
                          [[maybe_unused]] std::string_view offset,
                          [[maybe_unused]] std::string_view count) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitFieldUExtract([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] std::string_view base,
                          [[maybe_unused]] std::string_view offset,
                          [[maybe_unused]] std::string_view count) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitReverse32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitCount32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBitwiseNot32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    ctx.AddU32("{}=~{};", inst, value);
}

void EmitFindSMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitFindUMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddU32("{}=min(int({}), int({}));", inst, a, b);
}

void EmitUMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddU32("{}=min(uint({}), uint({}));", inst, a, b);
}

void EmitSMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddU32("{}=max(int({}), int({}));", inst, a, b);
}

void EmitUMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.AddU32("{}=max(uint({}), uint({}));", inst, a, b);
}

void EmitSClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value, [[maybe_unused]] std::string_view min,
                  [[maybe_unused]] std::string_view max) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitUClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value, [[maybe_unused]] std::string_view min,
                  [[maybe_unused]] std::string_view max) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitSLessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view lhs, [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}=int({})<int({});", inst, lhs, rhs);
}

void EmitULessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view lhs, [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}=uint({})<uint({)};", inst, lhs, rhs);
}

void EmitIEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view lhs, [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}={}=={};", inst, lhs, rhs);
}

void EmitSLessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                        [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}=int({})<=int({});", inst, lhs, rhs);
}

void EmitULessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                        [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}=uint({})<=uint({});", inst, lhs, rhs);
}

void EmitSGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}=int({})>int({});", inst, lhs, rhs);
}

void EmitUGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}=uint({})>uint({});", inst, lhs, rhs);
}

void EmitINotEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view lhs, [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}={}!={};", inst, lhs, rhs);
}

void EmitSGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}=int({})>=int({});", inst, lhs, rhs);
}

void EmitUGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    ctx.AddU1("{}=uint({})>=uint({});", inst, lhs, rhs);
}
} // namespace Shader::Backend::GLSL
