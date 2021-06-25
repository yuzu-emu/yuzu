// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
void SetZeroFlag(EmitContext& ctx, IR::Inst& inst, std::string_view result) {
    IR::Inst* const zero{inst.GetAssociatedPseudoOperation(IR::Opcode::GetZeroFromOp)};
    if (!zero) {
        return;
    }
    ctx.AddU1("{}={}==0;", *zero, result);
    zero->Invalidate();
}

void SetSignFlag(EmitContext& ctx, IR::Inst& inst, std::string_view result) {
    IR::Inst* const sign{inst.GetAssociatedPseudoOperation(IR::Opcode::GetSignFromOp)};
    if (!sign) {
        return;
    }
    ctx.AddU1("{}=int({})<0;", *sign, result);
    sign->Invalidate();
}

void BitwiseLogicalOp(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b,
                      char lop) {
    const auto result{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    ctx.Add("{}={}{}{};", result, a, lop, b);
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
}
} // Anonymous namespace

void EmitIAdd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    // Compute the overflow CC first as it requires the original operand values,
    // which may be overwritten by the result of the addition
    if (IR::Inst * overflow{inst.GetAssociatedPseudoOperation(IR::Opcode::GetOverflowFromOp)}) {
        // https://stackoverflow.com/questions/55468823/how-to-detect-integer-overflow-in-c
        constexpr u32 s32_max{static_cast<u32>(std::numeric_limits<s32>::max())};
        const auto sub_a{fmt::format("{}u-{}", s32_max, a)};
        const auto positive_result{fmt::format("int({})>int({})", b, sub_a)};
        const auto negative_result{fmt::format("int({})<int({})", b, sub_a)};
        ctx.AddU1("{}=int({})>=0?{}:{};", *overflow, a, positive_result, negative_result);
        overflow->Invalidate();
    }
    const auto result{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    if (IR::Inst* const carry{inst.GetAssociatedPseudoOperation(IR::Opcode::GetCarryFromOp)}) {
        ctx.uses_cc_carry = true;
        ctx.Add("{}=uaddCarry({},{},carry);", result, a, b);
        ctx.AddU1("{}=carry!=0;", *carry);
        carry->Invalidate();
    } else {
        ctx.Add("{}={}+{};", result, a, b);
    }
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
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
    ctx.AddU32("{}=uint({}*{});", inst, a, b);
}

void EmitINeg32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=uint(-({}));", inst, value);
}

void EmitINeg64(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU64("{}=-({});", inst, value);
}

void EmitIAbs32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=abs(int({}));", inst, value);
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
    ctx.AddU32("{}=int({})>>{};", inst, base, shift);
}

void EmitShiftRightArithmetic64(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                                std::string_view shift) {
    ctx.AddU64("{}=int64_t({})>>{};", inst, base, shift);
}

void EmitBitwiseAnd32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    BitwiseLogicalOp(ctx, inst, a, b, '&');
}

void EmitBitwiseOr32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    BitwiseLogicalOp(ctx, inst, a, b, '|');
}

void EmitBitwiseXor32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    BitwiseLogicalOp(ctx, inst, a, b, '^');
}

void EmitBitFieldInsert(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                        std::string_view insert, std::string_view offset, std::string_view count) {
    ctx.AddU32("{}=bitfieldInsert({},{},int({}),int({}));", inst, base, insert, offset, count);
}

void EmitBitFieldSExtract(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                          std::string_view offset, std::string_view count) {
    const auto result{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    ctx.Add("{}=uint(bitfieldExtract(int({}),int({}),int({})));", result, base, offset, count);
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
}

void EmitBitFieldUExtract(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                          std::string_view offset, std::string_view count) {
    const auto result{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    ctx.Add("{}=uint(bitfieldExtract(uint({}),int({}),int({})));", result, base, offset, count);
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
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

void EmitFindSMsb32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=findMSB(int({}));", inst, value);
}

void EmitFindUMsb32(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU32("{}=findMSB(uint({}));", inst, value);
}

void EmitSMin32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}=min(int({}),int({}));", inst, a, b);
}

void EmitUMin32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}=min(uint({}),uint({}));", inst, a, b);
}

void EmitSMax32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}=max(int({}),int({}));", inst, a, b);
}

void EmitUMax32(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU32("{}=max(uint({}),uint({}));", inst, a, b);
}

void EmitSClamp32(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view min,
                  std::string_view max) {
    const auto result{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    ctx.Add("{}=clamp(int({}),int({}),int({}));", result, value, min, max);
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
}

void EmitUClamp32(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view min,
                  std::string_view max) {
    const auto result{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    ctx.Add("{}=clamp(uint({}),uint({}),uint({}));", result, value, min, max);
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
}

void EmitSLessThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs) {
    ctx.AddU1("{}=int({})<int({});", inst, lhs, rhs);
}

void EmitULessThan(EmitContext& ctx, IR::Inst& inst, std::string_view lhs, std::string_view rhs) {
    ctx.AddU1("{}=uint({})<uint({});", inst, lhs, rhs);
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
