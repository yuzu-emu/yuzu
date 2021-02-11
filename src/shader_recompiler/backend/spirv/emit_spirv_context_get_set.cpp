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
    return ctx.Name(ctx.OpUndef(ctx.u32[1]), "unimplemented_cbuf");
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
    if (ctx.workgroup_id.value == 0) {
        ctx.workgroup_id = ctx.AddGlobalVariable(
            ctx.TypePointer(spv::StorageClass::Input, ctx.u32[3]), spv::StorageClass::Input);
        ctx.Decorate(ctx.workgroup_id, spv::Decoration::BuiltIn, spv::BuiltIn::WorkgroupId);
    }
    return ctx.OpLoad(ctx.u32[3], ctx.workgroup_id);
}

Id EmitSPIRV::EmitLocalInvocationId(EmitContext& ctx) {
    if (ctx.local_invocation_id.value == 0) {
        ctx.local_invocation_id = ctx.AddGlobalVariable(
            ctx.TypePointer(spv::StorageClass::Input, ctx.u32[3]), spv::StorageClass::Input);
        ctx.Decorate(ctx.local_invocation_id, spv::Decoration::BuiltIn,
                     spv::BuiltIn::LocalInvocationId);
    }
    return ctx.OpLoad(ctx.u32[3], ctx.local_invocation_id);
}

} // namespace Shader::Backend::SPIRV
