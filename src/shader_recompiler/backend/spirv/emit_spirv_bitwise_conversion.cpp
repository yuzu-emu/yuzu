// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitSPIRV::EmitBitCastU16F16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitBitCastU32F32(EmitContext& ctx, Id value) {
    return ctx.OpBitcast(ctx.U32[1], value);
}

void EmitSPIRV::EmitBitCastU64F64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitBitCastF16U16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitBitCastF32U32(EmitContext& ctx, Id value) {
    return ctx.OpBitcast(ctx.F32[1], value);
}

void EmitSPIRV::EmitBitCastF64U64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitPackUint2x32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitUnpackUint2x32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitPackFloat2x16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitUnpackFloat2x16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitPackDouble2x32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSPIRV::EmitUnpackDouble2x32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
