// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

Id EmitSPIRV::EmitIAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b) {
    if (inst->HasAssociatedPseudoOperation()) {
        throw NotImplementedException("Pseudo-operations on IAdd32");
    }
    return ctx.OpIAdd(ctx.U32[1], a, b);
}

void EmitSPIRV::EmitIAdd64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitISub32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpISub(ctx.U32[1], a, b);
}

void EmitSPIRV::EmitISub64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitIMul32(EmitContext& ctx, Id a, Id b) {
    return ctx.OpIMul(ctx.U32[1], a, b);
}

void EmitSPIRV::EmitINeg32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitIAbs32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitShiftLeftLogical32(EmitContext& ctx, Id base, Id shift) {
    return ctx.OpShiftLeftLogical(ctx.U32[1], base, shift);
}

void EmitSPIRV::EmitShiftRightLogical32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitShiftRightArithmetic32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitBitwiseAnd32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitBitwiseOr32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitBitwiseXor32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitBitFieldInsert(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitBitFieldSExtract(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitBitFieldUExtract(EmitContext& ctx, Id base, Id offset, Id count) {
    return ctx.OpBitFieldUExtract(ctx.U32[1], base, offset, count);
}

Id EmitSPIRV::EmitSLessThan(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpSLessThan(ctx.U1, lhs, rhs);
}

void EmitSPIRV::EmitULessThan(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitIEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSLessThanEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitULessThanEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitSGreaterThan(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpSGreaterThan(ctx.U1, lhs, rhs);
}

void EmitSPIRV::EmitUGreaterThan(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitINotEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSGreaterThanEqual(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitUGreaterThanEqual(EmitContext& ctx, Id lhs, Id rhs) {
    return ctx.OpUGreaterThanEqual(ctx.U1, lhs, rhs);
}

void EmitSPIRV::EmitLogicalOr(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitLogicalAnd(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitLogicalXor(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitLogicalNot(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
