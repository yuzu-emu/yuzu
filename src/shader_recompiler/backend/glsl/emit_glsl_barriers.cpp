// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
void EmitBarrier(EmitContext& ctx) {
    ctx.Add("barrier();");
}

void EmitWorkgroupMemoryBarrier(EmitContext& ctx) {
    ctx.Add("groupMemoryBarrier();");
}

void EmitDeviceMemoryBarrier(EmitContext& ctx) {
    ctx.Add("memoryBarrier();");
}
} // namespace Shader::Backend::GLSL
