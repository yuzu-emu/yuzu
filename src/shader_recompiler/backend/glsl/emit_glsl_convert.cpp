// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
void EmitConvertS16F16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertS16F32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddS32("{}=int(float({}))&0xffff;", inst, value);
}

void EmitConvertS16F64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertS32F16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertS32F32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddS32("{}=int(float({}));", inst, value);
}

void EmitConvertS32F64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddS32("{}=int(double({}));", inst, value);
}

void EmitConvertS64F16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertS64F32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddS64("{}=int64_t(double(float({})));", inst, value);
}

void EmitConvertS64F64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddS64("{}=int64_t(double({}));", inst, value);
}

void EmitConvertU16F16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertU16F32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertU16F64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertU32F16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertU32F32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddU32("{}=uint(float({}));", inst, value);
}

void EmitConvertU32F64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddU32("{}=uint(double({}));", inst, value);
}

void EmitConvertU64F16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertU64F32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddU64("{}=uint64_t(float({}));", inst, value);
}

void EmitConvertU64F64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddU64("{}=uint64_t(double({}));", inst, value);
}

void EmitConvertU64U32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddU64("{}=uint64_t(uint({}));", inst, value);
}

void EmitConvertU32U64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddU32("{}=uint(uint64_t({}));", inst, value);
}

void EmitConvertF16F32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF32F16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF32F64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=float(double({}));", inst, value);
}

void EmitConvertF64F32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=double(float({}));", inst, value);
}

void EmitConvertF16S8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF16S16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF16S32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF16S64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF16U8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF16U16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF16U32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF16U64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF32S8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF32S16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF32S32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=float(int({}));", inst, value);
}

void EmitConvertF32S64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=float(double(int64_t({})));", inst, value);
}

void EmitConvertF32U8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF32U16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=float(uint({}&0xffff));", inst, value);
}

void EmitConvertF32U32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=float(uint({}));", inst, value);
}

void EmitConvertF32U64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF32("{}=float(double(uint64_t({})));", inst, value);
}

void EmitConvertF64S8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF64S16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF64S32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=double(int({}));", inst, value);
}

void EmitConvertF64S64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=double(int64_t({}));", inst, value);
}

void EmitConvertF64U8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF64U16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitConvertF64U32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=double(uint({}));", inst, value);
}

void EmitConvertF64U64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] std::string_view value) {
    ctx.AddF64("{}=double(uint64_t({}));", inst, value);
}

} // namespace Shader::Backend::GLSL
