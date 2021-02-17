// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitCompositeConstructU32x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructU32x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructU32x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractU32x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitCompositeExtractU32x3(EmitContext& ctx, Id vector, u32 index) {
    return ctx.OpCompositeExtract(ctx.U32[1], vector, index);
}

void EmitCompositeExtractU32x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF16x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF16x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF16x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF16x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF16x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF16x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF32x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF32x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF32x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF32x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF32x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF32x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF64x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF64x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF64x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF64x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF64x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF64x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
