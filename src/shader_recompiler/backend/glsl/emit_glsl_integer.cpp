// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
void EmitIAdd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}={}+{};", inst, a, b);
}

void EmitIAdd64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU64("{}={}+{};", inst, a, b);
}

void EmitISub32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}={}-{};", inst, a, b);
}

void EmitISub64(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU64("{}={}-{};", inst, a, b);
}

void EmitIMul32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}={}*{};", inst, a, b);
}

void EmitINeg32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=-({});", inst, value);
}

void EmitINeg64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU64("{}=-({});", inst, value);
}

void EmitIAbs32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=abs({});", inst, value);
}

void EmitIAbs64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU64("{}=abs({});", inst, value);
}

void EmitShiftLeftLogical32(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                            std::string_view shift) {
    ctx.AddU32("{}={}<<{};", inst, base, shift);
}

void EmitShiftLeftLogical64(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                            std::string_view shift) {
    ctx.AddU64("{}={}<<{};", inst, base, shift);
}

void EmitShiftRightLogical32(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                             std::string_view shift) {
    ctx.AddU32("{}={}>>{};", inst, base, shift);
}

void EmitShiftRightLogical64(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                             std::string_view shift) {
    ctx.AddU64("{}={}>>{};", inst, base, shift);
}

void EmitShiftRightArithmetic32(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                                std::string_view shift) {
    ctx.AddS32("{}=int({})>>{};", inst, base, shift);
}

void EmitShiftRightArithmetic64(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                                std::string_view shift) {
    ctx.AddU64("{}=int64_t({})>>{};", inst, base, shift);
}

void EmitBitwiseAnd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}={}&{};", inst, a, b);
}

void EmitBitwiseOr32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}={}|{};", inst, a, b);
}

void EmitBitwiseXor32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}={}^{};", inst, a, b);
}

void EmitBitFieldInsert(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                        std::string_view insert, std::string_view offset, std::string_view count) {
    ctx.AddU32("{}=bitfieldInsert({}, {}, int({}), int({}));", inst, base, insert, offset, count);
}

void EmitBitFieldSExtract(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                          std::string_view offset, std::string_view count) {
    ctx.AddU32("{}=bitfieldExtract(int({}), int({}), int({}));", inst, base, offset, count);
}

void EmitBitFieldUExtract(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                          std::string_view offset, std::string_view count) {
    ctx.AddU32("{}=bitfieldExtract({}, int({}), int({}));", inst, base, offset, count);
}

void EmitBitReverse32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=bitfieldReverse({});", inst, value);
}

void EmitBitCount32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=bitCount({});", inst, value);
}

void EmitBitwiseNot32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
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

void EmitSMin32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}=min(int({}), int({}));", inst, a, b);
}

void EmitUMin32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}=min(uint({}), uint({}));", inst, a, b);
}

void EmitSMax32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}=max(int({}), int({}));", inst, a, b);
}

void EmitUMax32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}=max(uint({}), uint({}));", inst, a, b);
}

void EmitSClamp32(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view min,
                  std::string_view max) {
    ctx.AddU32("{}=clamp(int({}), int({}), int({}));", inst, value, min, max);
}

void EmitUClamp32(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view min,
                  std::string_view max) {
    ctx.AddU32("{}=clamp(uint({}), uint({}), uint({}));", inst, value, min, max);
}

void EmitSLessThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs) {
    ctx.AddU1("{}=int({})<int({});", inst, lhs, rhs);
}

void EmitULessThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs) {
    ctx.AddU1("{}=uint({})<uint({)};", inst, lhs, rhs);
}

void EmitIEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs) {
    ctx.AddU1("{}={}=={};", inst, lhs, rhs);
}

void EmitSLessThanEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                        std::string_view rhs) {
    ctx.AddU1("{}=int({})<=int({});", inst, lhs, rhs);
}

void EmitULessThanEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                        std::string_view rhs) {
    ctx.AddU1("{}=uint({})<=uint({});", inst, lhs, rhs);
}

void EmitSGreaterThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                      std::string_view rhs) {
    ctx.AddU1("{}=int({})>int({});", inst, lhs, rhs);
}

void EmitUGreaterThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                      std::string_view rhs) {
    ctx.AddU1("{}=uint({})>uint({});", inst, lhs, rhs);
}

void EmitINotEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs) {
    ctx.AddU1("{}={}!={};", inst, lhs, rhs);
}

void EmitSGreaterThanEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs) {
    ctx.AddU1("{}=int({})>=int({});", inst, lhs, rhs);
}

void EmitUGreaterThanEqual(EmitContext& ctx, IR::Inst& inst, std::string_view lhs,
                           std::string_view rhs) {
    ctx.AddU1("{}=uint({})>=uint({});", inst, lhs, rhs);
}
} // namespace Shader::Backend::GLSL
