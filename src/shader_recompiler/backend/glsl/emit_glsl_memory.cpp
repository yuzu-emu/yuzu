// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
void EmitLoadStorageU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                       [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitLoadStorageS8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                       [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitLoadStorageU16([[maybe_unused]] EmitContext& ctx,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitLoadStorageS16([[maybe_unused]] EmitContext& ctx,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitLoadStorage32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset) {
    ctx.AddU32("{}=ssbo{}_u32[{}];", inst, binding.U32(), offset.U32());
}

void EmitLoadStorage64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                       [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitLoadStorage128([[maybe_unused]] EmitContext& ctx,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitWriteStorageU8([[maybe_unused]] EmitContext& ctx,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset,
                        [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitWriteStorageS8([[maybe_unused]] EmitContext& ctx,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset,
                        [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitWriteStorageU16([[maybe_unused]] EmitContext& ctx,
                         [[maybe_unused]] const IR::Value& binding,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitWriteStorageS16([[maybe_unused]] EmitContext& ctx,
                         [[maybe_unused]] const IR::Value& binding,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    ctx.Add("ssbo{}_u32[{}]={};", binding.U32(), offset.U32(), value);
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    ctx.Add("ssbo{}_u32x2[{}]={};", binding.U32(), offset.U32(), value);
}

void EmitWriteStorage128([[maybe_unused]] EmitContext& ctx,
                         [[maybe_unused]] const IR::Value& binding,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}
} // namespace Shader::Backend::GLSL
