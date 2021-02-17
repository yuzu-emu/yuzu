// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitBitCastU16F16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBitCastU32F32(EmitContext& ctx, Id value) {
    return ctx.OpBitcast(ctx.U32[1], value);
}

void EmitBitCastU64F64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitBitCastF16U16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitBitCastF32U32(EmitContext& ctx, Id value) {
    return ctx.OpBitcast(ctx.F32[1], value);
}

void EmitBitCastF64U64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitPackUint2x32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitUnpackUint2x32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitPackFloat2x16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitUnpackFloat2x16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitPackDouble2x32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitUnpackDouble2x32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
