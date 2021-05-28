// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/value.h"

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
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddU32("{}=ssbo{}[{}];", inst, binding.U32(), offset_var);
}

void EmitLoadStorage64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddU32x2("{}=uvec2(ssbo{}[{}],ssbo{}[{}+1]);", inst, binding.U32(), offset_var,
                 binding.U32(), offset_var);
}

void EmitLoadStorage128(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddU32x4("{}=uvec4(ssbo{}[{}],ssbo{}[{}+1],ssbo{}[{}+2],ssbo{}[{}+3]);", inst,
                 binding.U32(), offset_var, binding.U32(), offset_var, binding.U32(), offset_var,
                 binding.U32(), offset_var);
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
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}]={};", binding.U32(), offset_var, value);
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}]={}.x;", binding.U32(), offset_var, value);
    ctx.Add("ssbo{}[{}+1]={}.y;", binding.U32(), offset_var, value);
}

void EmitWriteStorage128([[maybe_unused]] EmitContext& ctx,
                         [[maybe_unused]] const IR::Value& binding,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}
} // namespace Shader::Backend::GLSL
