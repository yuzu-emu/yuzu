// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
void EmitGetCbufU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                   [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufS8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                   [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufU16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufS16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufU32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                    [[maybe_unused]] const IR::Value& offset) {
    const auto var{ctx.AllocVar()};
    ctx.Add("uint {} = c{}[{}];", var, binding.U32(), offset.U32());
}

void EmitGetCbufF32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufU32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                      [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}
} // namespace Shader::Backend::GLSL
