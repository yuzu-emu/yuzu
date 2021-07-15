// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {

void EmitLogicalOr(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU1("{}={}||{};", inst, a, b);
}

void EmitLogicalAnd(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU1("{}={}&&{};", inst, a, b);
}

void EmitLogicalXor(EmitContext& ctx, IR::Inst& inst, std::string_view a, std::string_view b) {
    ctx.AddU1("{}={}^^{};", inst, a, b);
}

void EmitLogicalNot(EmitContext& ctx, IR::Inst& inst, std::string_view value) {
    ctx.AddU1("{}=!{};", inst, value);
}
} // namespace Shader::Backend::GLSL
