// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
void EmitFSwizzleAdd([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                     [[maybe_unused]] std::string_view op_a, [[maybe_unused]] std::string_view op_b,
                     [[maybe_unused]] std::string_view swizzle) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitDPdxFine(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    ctx.AddF32("{}=dFdxFine({});", inst, op_a);
}

void EmitDPdyFine(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    ctx.AddF32("{}=dFdyFine({});", inst, op_a);
}

void EmitDPdxCoarse(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    ctx.AddF32("{}=dFdxCoarse({});", inst, op_a);
}

void EmitDPdyCoarse(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    ctx.AddF32("{}=dFdyCoarse({});", inst, op_a);
}
} // namespace Shader::Backend::GLSL
