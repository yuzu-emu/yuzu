// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

Id EmitIAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    if (inst->HasAssociatedPseudoOperation()) {
        throw NotImplementedException("Pseudo-operations on IAdd32");
    }
    return ctx.OpIAdd(ctx.U32[1], a, b);
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

void EmitINeg32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitIAbs32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitShiftLeftLogical32(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftLeftLogical(ctx.U32[1], base, shift);
}

void EmitShiftRightLogical32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitShiftRightArithmetic32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitBitwiseAnd32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitBitwiseOr32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitBitwiseXor32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitBitFieldInsert(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitBitFieldSExtract(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBitFieldUExtract(EmitContext& ctx, Id base, Id offset, Id count) {
    return ctx.OpBitFieldUExtract(ctx.U32[1], base, offset, count);
}

Id EmitSLessThan(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpSLessThan(ctx.U1, lhs, rhs);
}

void EmitULessThan(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitIEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSLessThanEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitULessThanEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSGreaterThan(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpSGreaterThan(ctx.U1, lhs, rhs);
}

void EmitUGreaterThan(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitINotEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSGreaterThanEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitUGreaterThanEqual(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpUGreaterThanEqual(ctx.U1, lhs, rhs);
}

void EmitLogicalOr(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLogicalAnd(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLogicalXor(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLogicalNot(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
