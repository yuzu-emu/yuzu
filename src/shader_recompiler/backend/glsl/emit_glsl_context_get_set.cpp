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

void EmitGetCbufU32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    const auto u32_offset{offset.U32()};
    ctx.AddU32("{}=floatBitsToUint(cbuf{}[{}][{}]);", inst, binding.U32(), u32_offset / 16,
               (u32_offset / 4) % 4);
}

void EmitGetCbufF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    const auto u32_offset{offset.U32()};
    ctx.AddF32("{}=cbuf{}[{}][{}];", inst, binding.U32(), u32_offset / 16, (u32_offset / 4) % 4);
}

void EmitGetCbufU32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                      [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}
} // namespace Shader::Backend::GLSL
