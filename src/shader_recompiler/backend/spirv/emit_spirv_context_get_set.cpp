// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitGetRegister(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetRegister(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetPred(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetPred(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetGotoVariable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetGotoVariable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGetCbuf(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
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

void EmitGetAttribute(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetAttribute(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetAttributeIndexed(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetAttributeIndexed(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetZFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetSFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetCFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetOFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetZFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetSFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetCFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetOFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitWorkgroupId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.workgroup_id);
}

Id EmitLocalInvocationId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.local_invocation_id);
}

} // namespace Shader::Backend::SPIRV
