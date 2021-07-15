// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"

namespace Shader::Backend::SPIRV {
namespace {
void SetZeroFlag(EmitContext& ctx, IR::Inst* inst, Id result) {
    IR::Inst* const zero{inst->GetAssociatedPseudoOperation(IR::Opcode::GetZeroFromOp)};
    if (!zero) {
        return;
    }
    zero->SetDefinition(ctx.OpIEqual(ctx.U1, result, ctx.u32_zero_value));
    zero->Invalidate();
}

void SetSignFlag(EmitContext& ctx, IR::Inst* inst, Id result) {
    IR::Inst* const sign{inst->GetAssociatedPseudoOperation(IR::Opcode::GetSignFromOp)};
    if (!sign) {
        return;
    }
    sign->SetDefinition(ctx.OpSLessThan(ctx.U1, result, ctx.u32_zero_value));
    sign->Invalidate();
}
} // Anonymous namespace

Id EmitIAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    Id result{};
    if (IR::Inst* const carry{inst->GetAssociatedPseudoOperation(IR::Opcode::GetCarryFromOp)}) {
        const Id carry_type{ctx.TypeStruct(ctx.U32[1], ctx.U32[1])};
        const Id carry_result{ctx.OpIAddCarry(carry_type, a, b)};
        result = ctx.OpCompositeExtract(ctx.U32[1], carry_result, 0U);

        const Id carry_value{ctx.OpCompositeExtract(ctx.U32[1], carry_result, 1U)};
        carry->SetDefinition(ctx.OpINotEqual(ctx.U1, carry_value, ctx.u32_zero_value));
        carry->Invalidate();
    } else {
        result = ctx.OpIAdd(ctx.U32[1], a, b);
    }
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    if (IR::Inst * overflow{inst->GetAssociatedPseudoOperation(IR::Opcode::GetOverflowFromOp)}) {
        // https://stackoverflow.com/questions/55468823/how-to-detect-integer-overflow-in-c
        constexpr u32 s32_max{static_cast<u32>(std::numeric_limits<s32>::max())};
        const Id is_positive{ctx.OpSGreaterThanEqual(ctx.U1, a, ctx.u32_zero_value)};
        const Id sub_a{ctx.OpISub(ctx.U32[1], ctx.Const(s32_max), a)};

        const Id positive_test{ctx.OpSGreaterThan(ctx.U1, b, sub_a)};
        const Id negative_test{ctx.OpSLessThan(ctx.U1, b, sub_a)};
        const Id carry_flag{ctx.OpSelect(ctx.U1, is_positive, positive_test, negative_test)};
        overflow->SetDefinition(carry_flag);
        overflow->Invalidate();
    }
    return result;
}

Id EmitIAdd64(EmitContext& ctx, Id a, Id b) {
    return ctx.OpIAdd(ctx.U64, a, b);
}

Id EmitISub32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpISub(ctx.U32[1], a, b);
}

Id EmitISub64(EmitContext& ctx, Id a, Id b) {
    return ctx.OpISub(ctx.U64, a, b);
}

Id EmitIMul32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpIMul(ctx.U32[1], a, b);
}

Id EmitINeg32(EmitContext& ctx, Id value) {
    return ctx.OpSNegate(ctx.U32[1], value);
}

Id EmitINeg64(EmitContext& ctx, Id value) {
    return ctx.OpSNegate(ctx.U64, value);
}

Id EmitIAbs32(EmitContext& ctx, Id value) {
    return ctx.OpSAbs(ctx.U32[1], value);
}

Id EmitShiftLeftLogical32(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftLeftLogical(ctx.U32[1], base, shift);
}

Id EmitShiftLeftLogical64(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftLeftLogical(ctx.U64, base, shift);
}

Id EmitShiftRightLogical32(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftRightLogical(ctx.U32[1], base, shift);
}

Id EmitShiftRightLogical64(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftRightLogical(ctx.U64, base, shift);
}

Id EmitShiftRightArithmetic32(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftRightArithmetic(ctx.U32[1], base, shift);
}

Id EmitShiftRightArithmetic64(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftRightArithmetic(ctx.U64, base, shift);
}

Id EmitBitwiseAnd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{ctx.OpBitwiseAnd(ctx.U32[1], a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitwiseOr32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{ctx.OpBitwiseOr(ctx.U32[1], a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitwiseXor32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    const Id result{ctx.OpBitwiseXor(ctx.U32[1], a, b)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitFieldInsert(EmitContext& ctx, Id base, Id insert, Id offset, Id count) {
    return ctx.OpBitFieldInsert(ctx.U32[1], base, insert, offset, count);
}

Id EmitBitFieldSExtract(EmitContext& ctx, IR::Inst* inst, Id base, Id offset, Id count) {
    const Id result{ctx.OpBitFieldSExtract(ctx.U32[1], base, offset, count)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitFieldUExtract(EmitContext& ctx, IR::Inst* inst, Id base, Id offset, Id count) {
    const Id result{ctx.OpBitFieldUExtract(ctx.U32[1], base, offset, count)};
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitBitReverse32(EmitContext& ctx, Id value) {
    return ctx.OpBitReverse(ctx.U32[1], value);
}

Id EmitBitCount32(EmitContext& ctx, Id value) {
    return ctx.OpBitCount(ctx.U32[1], value);
}

Id EmitBitwiseNot32(EmitContext& ctx, Id value) {
    return ctx.OpNot(ctx.U32[1], value);
}

Id EmitFindSMsb32(EmitContext& ctx, Id value) {
    return ctx.OpFindSMsb(ctx.U32[1], value);
}

Id EmitFindUMsb32(EmitContext& ctx, Id value) {
    return ctx.OpFindUMsb(ctx.U32[1], value);
}

Id EmitSMin32(EmitContext& ctx, Id a, Id b) {
    const bool is_broken{ctx.profile.has_broken_signed_operations};
    if (is_broken) {
        a = ctx.OpBitcast(ctx.S32[1], a);
        b = ctx.OpBitcast(ctx.S32[1], b);
    }
    const Id result{ctx.OpSMin(ctx.U32[1], a, b)};
    return is_broken ? ctx.OpBitcast(ctx.U32[1], result) : result;
}

Id EmitUMin32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpUMin(ctx.U32[1], a, b);
}

Id EmitSMax32(EmitContext& ctx, Id a, Id b) {
    const bool is_broken{ctx.profile.has_broken_signed_operations};
    if (is_broken) {
        a = ctx.OpBitcast(ctx.S32[1], a);
        b = ctx.OpBitcast(ctx.S32[1], b);
    }
    const Id result{ctx.OpSMax(ctx.U32[1], a, b)};
    return is_broken ? ctx.OpBitcast(ctx.U32[1], result) : result;
}

Id EmitUMax32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpUMax(ctx.U32[1], a, b);
}

Id EmitSClamp32(EmitContext& ctx, IR::Inst* inst, Id value, Id min, Id max) {
    Id result{};
    if (ctx.profile.has_broken_signed_operations || ctx.profile.has_broken_spirv_clamp) {
        value = ctx.OpBitcast(ctx.S32[1], value);
        min = ctx.OpBitcast(ctx.S32[1], min);
        max = ctx.OpBitcast(ctx.S32[1], max);
        if (ctx.profile.has_broken_spirv_clamp) {
            result = ctx.OpSMax(ctx.S32[1], ctx.OpSMin(ctx.S32[1], value, max), min);
        } else {
            result = ctx.OpSClamp(ctx.S32[1], value, min, max);
        }
        result = ctx.OpBitcast(ctx.U32[1], result);
    } else {
        result = ctx.OpSClamp(ctx.U32[1], value, min, max);
    }
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitUClamp32(EmitContext& ctx, IR::Inst* inst, Id value, Id min, Id max) {
    Id result{};
    if (ctx.profile.has_broken_spirv_clamp) {
        result = ctx.OpUMax(ctx.U32[1], ctx.OpUMin(ctx.U32[1], value, max), min);
    } else {
        result = ctx.OpUClamp(ctx.U32[1], value, min, max);
    }
    SetZeroFlag(ctx, inst, result);
    SetSignFlag(ctx, inst, result);
    return result;
}

Id EmitSLessThan(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpSLessThan(ctx.U1, lhs, rhs);
}

Id EmitULessThan(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpULessThan(ctx.U1, lhs, rhs);
}

Id EmitIEqual(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpIEqual(ctx.U1, lhs, rhs);
}

Id EmitSLessThanEqual(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpSLessThanEqual(ctx.U1, lhs, rhs);
}

Id EmitULessThanEqual(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpULessThanEqual(ctx.U1, lhs, rhs);
}

Id EmitSGreaterThan(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpSGreaterThan(ctx.U1, lhs, rhs);
}

Id EmitUGreaterThan(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpUGreaterThan(ctx.U1, lhs, rhs);
}

Id EmitINotEqual(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpINotEqual(ctx.U1, lhs, rhs);
}

Id EmitSGreaterThanEqual(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpSGreaterThanEqual(ctx.U1, lhs, rhs);
}

Id EmitUGreaterThanEqual(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpUGreaterThanEqual(ctx.U1, lhs, rhs);
}

} // namespace Shader::Backend::SPIRV
