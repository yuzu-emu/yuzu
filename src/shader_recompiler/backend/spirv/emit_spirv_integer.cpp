// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

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
    if (IR::Inst* const zero{inst->GetAssociatedPseudoOperation(IR::Opcode::GetZeroFromOp)}) {
        zero->SetDefinition(ctx.OpIEqual(ctx.U1, result, ctx.u32_zero_value));
        zero->Invalidate();
    }
    if (IR::Inst* const sign{inst->GetAssociatedPseudoOperation(IR::Opcode::GetSignFromOp)}) {
        sign->SetDefinition(ctx.OpSLessThan(ctx.U1, result, ctx.u32_zero_value));
        sign->Invalidate();
    }
    if (IR::Inst * overflow{inst->GetAssociatedPseudoOperation(IR::Opcode::GetOverflowFromOp)}) {
        // https://stackoverflow.com/questions/55468823/how-to-detect-integer-overflow-in-c
        constexpr u32 s32_max{static_cast<u32>(std::numeric_limits<s32>::max())};
        const Id is_positive{ctx.OpSGreaterThanEqual(ctx.U1, a, ctx.u32_zero_value)};
        const Id sub_a{ctx.OpISub(ctx.U32[1], ctx.Constant(ctx.U32[1], s32_max), a)};

        const Id positive_test{ctx.OpSGreaterThan(ctx.U1, b, sub_a)};
        const Id negative_test{ctx.OpSLessThan(ctx.U1, b, sub_a)};
        const Id carry_flag{ctx.OpSelect(ctx.U1, is_positive, positive_test, negative_test)};
        overflow->SetDefinition(carry_flag);
        overflow->Invalidate();
    }
    return result;
}

void EmitIAdd64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitISub32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpISub(ctx.U32[1], a, b);
}

void EmitISub64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitIMul32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpIMul(ctx.U32[1], a, b);
}

Id EmitINeg32(EmitContext& ctx, Id value) {
    return ctx.OpSNegate(ctx.U32[1], value);
}

Id EmitIAbs32(EmitContext& ctx, Id value) {
    return ctx.OpSAbs(ctx.U32[1], value);
}

Id EmitShiftLeftLogical32(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftLeftLogical(ctx.U32[1], base, shift);
}

Id EmitShiftRightLogical32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpShiftRightLogical(ctx.U32[1], a, b);
}

Id EmitShiftRightArithmetic32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpShiftRightArithmetic(ctx.U32[1], a, b);
}

Id EmitBitwiseAnd32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpBitwiseAnd(ctx.U32[1], a, b);
}

Id EmitBitwiseOr32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpBitwiseOr(ctx.U32[1], a, b);
}

Id EmitBitwiseXor32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpBitwiseXor(ctx.U32[1], a, b);
}

Id EmitBitFieldInsert(EmitContext& ctx, Id base, Id insert, Id offset, Id count) {
    return ctx.OpBitFieldInsert(ctx.U32[1], base, insert, offset, count);
}

Id EmitBitFieldSExtract(EmitContext& ctx, Id base, Id offset, Id count) {
    return ctx.OpBitFieldSExtract(ctx.U32[1], base, offset, count);
}

Id EmitBitFieldUExtract(EmitContext& ctx, Id base, Id offset, Id count) {
    return ctx.OpBitFieldUExtract(ctx.U32[1], base, offset, count);
}

Id EmitBitReverse32(EmitContext& ctx, Id value) {
    return ctx.OpBitReverse(ctx.U32[1], value);
}

Id EmitBitCount32(EmitContext& ctx, Id value) {
    return ctx.OpBitCount(ctx.U32[1], value);
}

Id EmitBitwiseNot32(EmitContext& ctx, Id a) {
    return ctx.OpNot(ctx.U32[1], a);
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
