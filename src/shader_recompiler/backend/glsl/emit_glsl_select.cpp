// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
void EmitSelectU1(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                  std::string_view true_value, std::string_view false_value) {
    ctx.AddU1("{}={}?{}:{};", inst, cond, true_value, false_value);
}

void EmitSelectU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view cond,
                  [[maybe_unused]] std::string_view true_value,
                  [[maybe_unused]] std::string_view false_value) {
    NotImplemented();
}

void EmitSelectU16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view cond,
                   [[maybe_unused]] std::string_view true_value,
                   [[maybe_unused]] std::string_view false_value) {
    NotImplemented();
}

void EmitSelectU32(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.AddU32("{}={}?{}:{};", inst, cond, true_value, false_value);
}

void EmitSelectU64(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.AddU64("{}={}?{}:{};", inst, cond, true_value, false_value);
}

void EmitSelectF16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view cond,
                   [[maybe_unused]] std::string_view true_value,
                   [[maybe_unused]] std::string_view false_value) {
    NotImplemented();
}

void EmitSelectF32(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.AddF32("{}={}?{}:{};", inst, cond, true_value, false_value);
}

void EmitSelectF64(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.AddF64("{}={}?{}:{};", inst, cond, true_value, false_value);
}

} // namespace Shader::Backend::GLSL
