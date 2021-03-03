// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {
namespace {
IR::Opcode Replace(IR::Opcode op) {
    switch (op) {
    case IR::Opcode::FPAbs16:
        return IR::Opcode::FPAbs32;
    case IR::Opcode::FPAdd16:
        return IR::Opcode::FPAdd32;
    case IR::Opcode::FPCeil16:
        return IR::Opcode::FPCeil32;
    case IR::Opcode::FPFloor16:
        return IR::Opcode::FPFloor32;
    case IR::Opcode::FPFma16:
        return IR::Opcode::FPFma32;
    case IR::Opcode::FPMul16:
        return IR::Opcode::FPMul32;
    case IR::Opcode::FPNeg16:
        return IR::Opcode::FPNeg32;
    case IR::Opcode::FPRoundEven16:
        return IR::Opcode::FPRoundEven32;
    case IR::Opcode::FPSaturate16:
        return IR::Opcode::FPSaturate32;
    case IR::Opcode::FPTrunc16:
        return IR::Opcode::FPTrunc32;
    case IR::Opcode::CompositeConstructF16x2:
        return IR::Opcode::CompositeConstructF32x2;
    case IR::Opcode::CompositeConstructF16x3:
        return IR::Opcode::CompositeConstructF32x3;
    case IR::Opcode::CompositeConstructF16x4:
        return IR::Opcode::CompositeConstructF32x4;
    case IR::Opcode::CompositeExtractF16x2:
        return IR::Opcode::CompositeExtractF32x2;
    case IR::Opcode::CompositeExtractF16x3:
        return IR::Opcode::CompositeExtractF32x3;
    case IR::Opcode::CompositeExtractF16x4:
        return IR::Opcode::CompositeExtractF32x4;
    case IR::Opcode::CompositeInsertF16x2:
        return IR::Opcode::CompositeInsertF32x2;
    case IR::Opcode::CompositeInsertF16x3:
        return IR::Opcode::CompositeInsertF32x3;
    case IR::Opcode::CompositeInsertF16x4:
        return IR::Opcode::CompositeInsertF32x4;
    case IR::Opcode::ConvertS16F16:
        return IR::Opcode::ConvertS16F32;
    case IR::Opcode::ConvertS32F16:
        return IR::Opcode::ConvertS32F32;
    case IR::Opcode::ConvertS64F16:
        return IR::Opcode::ConvertS64F32;
    case IR::Opcode::ConvertU16F16:
        return IR::Opcode::ConvertU16F32;
    case IR::Opcode::ConvertU32F16:
        return IR::Opcode::ConvertU32F32;
    case IR::Opcode::ConvertU64F16:
        return IR::Opcode::ConvertU64F32;
    case IR::Opcode::PackFloat2x16:
        return IR::Opcode::PackHalf2x16;
    case IR::Opcode::UnpackFloat2x16:
        return IR::Opcode::UnpackHalf2x16;
    case IR::Opcode::ConvertF32F16:
        return IR::Opcode::Identity;
    case IR::Opcode::ConvertF16F32:
        return IR::Opcode::Identity;
    default:
        return op;
    }
}
} // Anonymous namespace

void LowerFp16ToFp32(IR::Program& program) {
    for (IR::Function& function : program.functions) {
        for (IR::Block* const block : function.blocks) {
            for (IR::Inst& inst : block->Instructions()) {
                inst.ReplaceOpcode(Replace(inst.Opcode()));
            }
        }
    }
}

} // namespace Shader::Optimization
