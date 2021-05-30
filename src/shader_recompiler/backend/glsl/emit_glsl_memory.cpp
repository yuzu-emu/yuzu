// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
void EmitLoadStorageU8([[maybe_unused]] EmitContext& ctx, IR::Inst& inst,
                       [[maybe_unused]] const IR::Value& binding,
                       [[maybe_unused]] const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddU32("{}=bitfieldExtract(ssbo{}[{}/4],int({}%4)*8,8);", inst, binding.U32(), offset_var,
               offset_var);
}

void EmitLoadStorageS8([[maybe_unused]] EmitContext& ctx, IR::Inst& inst,
                       [[maybe_unused]] const IR::Value& binding,
                       [[maybe_unused]] const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddS32("{}=bitfieldExtract(int(ssbo{}[{}/4]),int({}%4)*8,8);", inst, binding.U32(),
               offset_var, offset_var);
}

void EmitLoadStorageU16([[maybe_unused]] EmitContext& ctx, IR::Inst& inst,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddU32("{}=bitfieldExtract(ssbo{}[{}/4],int(({}/2)%2)*16,16);", inst, binding.U32(),
               offset_var, offset_var);
}

void EmitLoadStorageS16([[maybe_unused]] EmitContext& ctx, IR::Inst& inst,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddS32("{}=bitfieldExtract(int(ssbo{}[{}/4]),int(({}/2)%2)*16,16);", inst, binding.U32(),
               offset_var, offset_var);
}

void EmitLoadStorage32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddU32("{}=ssbo{}[{}/4];", inst, binding.U32(), offset_var);
}

void EmitLoadStorage64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddU32x2("{}=uvec2(ssbo{}[{}/4],ssbo{}[({}+4)/4]);", inst, binding.U32(), offset_var,
                 binding.U32(), offset_var);
}

void EmitLoadStorage128(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.AddU32x4("{}=uvec4(ssbo{}[{}/4],ssbo{}[({}+4)/4],ssbo{}[({}+8)/4],ssbo{}[({}+12)/4]);",
                 inst, binding.U32(), offset_var, binding.U32(), offset_var, binding.U32(),
                 offset_var, binding.U32(), offset_var);
}

void EmitWriteStorageU8([[maybe_unused]] EmitContext& ctx,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset,
                        [[maybe_unused]] std::string_view value) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}/4]=bitfieldInsert(ssbo{}[{}/4],{},int({}%4)*8,8);", binding.U32(),
            offset_var, binding.U32(), offset_var, value, offset_var);
}

void EmitWriteStorageS8([[maybe_unused]] EmitContext& ctx,
                        [[maybe_unused]] const IR::Value& binding,
                        [[maybe_unused]] const IR::Value& offset,
                        [[maybe_unused]] std::string_view value) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}/4]=bitfieldInsert(ssbo{}[{}/4],{},int({}%4)*8,8);", binding.U32(),
            offset_var, binding.U32(), offset_var, value, offset_var);
}

void EmitWriteStorageU16([[maybe_unused]] EmitContext& ctx,
                         [[maybe_unused]] const IR::Value& binding,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] std::string_view value) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}/4]=bitfieldInsert(ssbo{}[{}/4],{},int(({}/2)%2)*16,16);", binding.U32(),
            offset_var, binding.U32(), offset_var, value, offset_var);
}

void EmitWriteStorageS16([[maybe_unused]] EmitContext& ctx,
                         [[maybe_unused]] const IR::Value& binding,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] std::string_view value) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}/4]=bitfieldInsert(ssbo{}[{}/4],{},int(({}/2)%2)*16,16);", binding.U32(),
            offset_var, binding.U32(), offset_var, value, offset_var);
}

void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}/4]={};", binding.U32(), offset_var, value);
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}/4]={}.x;", binding.U32(), offset_var, value);
    ctx.Add("ssbo{}[({}+4)/4]={}.y;", binding.U32(), offset_var, value);
}

void EmitWriteStorage128([[maybe_unused]] EmitContext& ctx,
                         [[maybe_unused]] const IR::Value& binding,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] std::string_view value) {
    const auto offset_var{ctx.reg_alloc.Consume(offset)};
    ctx.Add("ssbo{}[{}/4]={}.x;", binding.U32(), offset_var, value);
    ctx.Add("ssbo{}[({}+4)/4]={}.y;", binding.U32(), offset_var, value);
    ctx.Add("ssbo{}[({}+8)/4]={}.z;", binding.U32(), offset_var, value);
    ctx.Add("ssbo{}[({}+12)/4]={}.w;", binding.U32(), offset_var, value);
}
} // namespace Shader::Backend::GLSL
