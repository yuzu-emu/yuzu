// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitSPIRV::EmitGetRegister(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetRegister(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitGetPred(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetPred(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetGotoVariable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitGetGotoVariable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitGetCbuf(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Constant buffer indexing");
    }
    if (!offset.IsImmediate()) {
        throw NotImplementedException("Variable constant buffer offset");
    }
    const Id imm_offset{ctx.Constant(ctx.U32[1], offset.U32() / 4)};
    const Id cbuf{ctx.cbufs[binding.U32()]};
    const Id access_chain{ctx.OpAccessChain(ctx.uniform_u32, cbuf, ctx.u32_zero_value, imm_offset)};
    return ctx.OpLoad(ctx.U32[1], access_chain);
}

void EmitSPIRV::EmitGetAttribute(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetAttribute(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitGetAttributeIndexed(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetAttributeIndexed(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitGetZFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitGetSFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitGetCFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitGetOFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetZFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetSFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetCFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitSetOFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitWorkgroupId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.workgroup_id);
}

Id EmitSPIRV::EmitLocalInvocationId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.local_invocation_id);
}

} // namespace Shader::Backend::SPIRV
