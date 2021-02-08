// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitSPIRV::EmitBranch(EmitContext& ctx, IR::Inst* inst) {
    ctx.OpBranch(ctx.BlockLabel(inst->Arg(0).Label()));
}

void EmitSPIRV::EmitBranchConditional(EmitContext& ctx, IR::Inst* inst) {
    ctx.OpBranchConditional(ctx.Def(inst->Arg(0)), ctx.BlockLabel(inst->Arg(1).Label()),
                            ctx.BlockLabel(inst->Arg(2).Label()));
}

void EmitSPIRV::EmitExit(EmitContext& ctx) {
    ctx.OpReturn();
}

void EmitSPIRV::EmitReturn(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitUnreachable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
