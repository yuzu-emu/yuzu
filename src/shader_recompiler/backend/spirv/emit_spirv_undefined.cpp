// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

Id EmitSPIRV::EmitUndefU1(EmitContext& ctx) {
    return ctx.OpUndef(ctx.U1);
}

Id EmitSPIRV::EmitUndefU8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitUndefU16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSPIRV::EmitUndefU32(EmitContext& ctx) {
    return ctx.OpUndef(ctx.U32[1]);
}

Id EmitSPIRV::EmitUndefU64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
