// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
void EmitLoadSharedU8(EmitContext& ctx, IR::Inst& inst, std::string_view offset) {
    ctx.AddU32("{}=bitfieldExtract(smem[{}>>2],int({}%4)*8,8);", inst, offset, offset);
}

void EmitLoadSharedS8(EmitContext& ctx, IR::Inst& inst, std::string_view offset) {
    ctx.AddU32("{}=bitfieldExtract(int(smem[{}>>2]),int({}%4)*8,8);", inst, offset, offset);
}

void EmitLoadSharedU16(EmitContext& ctx, IR::Inst& inst, std::string_view offset) {
    ctx.AddU32("{}=bitfieldExtract(smem[{}>>2],int(({}>>1)%2)*16,16);", inst, offset, offset);
}

void EmitLoadSharedS16(EmitContext& ctx, IR::Inst& inst, std::string_view offset) {
    ctx.AddU32("{}=bitfieldExtract(int(smem[{}>>2]),int(({}>>1)%2)*16,16);", inst, offset, offset);
}

void EmitLoadSharedU32(EmitContext& ctx, IR::Inst& inst, std::string_view offset) {
    ctx.AddU32("{}=smem[{}>>2];", inst, offset);
}

void EmitLoadSharedU64(EmitContext& ctx, IR::Inst& inst, std::string_view offset) {
    ctx.AddU32x2("{}=uvec2(smem[{}>>2],smem[({}+4)>>2]);", inst, offset, offset);
}

void EmitLoadSharedU128(EmitContext& ctx, IR::Inst& inst, std::string_view offset) {
    ctx.AddU32x4("{}=uvec4(smem[{}>>2],smem[({}+4)>>2],smem[({}+8)>>2],smem[({}+12)>>2]);", inst,
                 offset, offset, offset, offset);
}

void EmitWriteSharedU8(EmitContext& ctx, std::string_view offset, std::string_view value) {
    ctx.Add("smem[{}>>2]=bitfieldInsert(smem[{}>>2],{},int({}%4)*8,8);", offset, offset, value,
            offset);
}

void EmitWriteSharedU16(EmitContext& ctx, std::string_view offset, std::string_view value) {
    ctx.Add("smem[{}>>2]=bitfieldInsert(smem[{}>>2],{},int(({}>>1)%2)*16,16);", offset, offset,
            value, offset);
}

void EmitWriteSharedU32(EmitContext& ctx, std::string_view offset, std::string_view value) {
    ctx.Add("smem[{}>>2]={};", offset, value);
}

void EmitWriteSharedU64(EmitContext& ctx, std::string_view offset, std::string_view value) {
    ctx.Add("smem[{}>>2]={}.x;", offset, value);
    ctx.Add("smem[({}+4)>>2]={}.y;", offset, value);
}

void EmitWriteSharedU128(EmitContext& ctx, std::string_view offset, std::string_view value) {
    ctx.Add("smem[{}>>2]={}.x;", offset, value);
    ctx.Add("smem[({}+4)>>2]={}.y;", offset, value);
    ctx.Add("smem[({}+8)>>2]={}.z;", offset, value);
    ctx.Add("smem[({}+12)>>2]={}.w;", offset, value);
}

} // namespace Shader::Backend::GLSL
