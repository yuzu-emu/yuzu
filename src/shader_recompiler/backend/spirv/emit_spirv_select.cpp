// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitSelect8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSelect16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSelect32(EmitContext& ctx, Id cond, Id true_value, Id false_value) {
    return ctx.OpSelect(ctx.U32[1], cond, true_value, false_value);
}

void EmitSelect64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
