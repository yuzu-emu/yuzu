// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Backend::SPIRV {

class EmitSPIRV {
public:
private:
    // Microinstruction emitters
#define OPCODE(name, result_type, ...) void Emit##name(EmitContext& ctx, IR::Inst* inst);
#include "shader_recompiler/frontend/ir/opcodes.inc"
#undef OPCODE
};

} // namespace Shader::Backend::SPIRV
