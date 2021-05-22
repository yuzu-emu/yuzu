// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
void EmitCompositeConstructU32x2(EmitContext& ctx, IR::Inst& inst, std::string_view e1,
                                 std::string_view e2) {
    ctx.AddU32x2("{}=uvec2({},{});", inst, e1, e2);
}

void EmitCompositeConstructU32x3([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view e1,
                                 [[maybe_unused]] std::string_view e2,
                                 [[maybe_unused]] std::string_view e3) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructU32x4([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view e1,
                                 [[maybe_unused]] std::string_view e2,
                                 [[maybe_unused]] std::string_view e3,
                                 [[maybe_unused]] std::string_view e4) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractU32x2(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                               u32 index) {
    ctx.AddU32("{}={}[{}];", inst, composite, index);
}

void EmitCompositeExtractU32x3([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] std::string_view composite,
                               [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractU32x4([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] std::string_view composite,
                               [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertU32x2([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertU32x3([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertU32x4([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructF16x2([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view e1,
                                 [[maybe_unused]] std::string_view e2) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructF16x3([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view e1,
                                 [[maybe_unused]] std::string_view e2,
                                 [[maybe_unused]] std::string_view e3) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructF16x4([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view e1,
                                 [[maybe_unused]] std::string_view e2,
                                 [[maybe_unused]] std::string_view e3,
                                 [[maybe_unused]] std::string_view e4) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractF16x2([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] std::string_view composite,
                               [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractF16x3([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] std::string_view composite,
                               [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractF16x4([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] std::string_view composite,
                               [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF16x2([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF16x3([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF16x4([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructF32x2(EmitContext& ctx, IR::Inst& inst, std::string_view e1,
                                 std::string_view e2) {
    ctx.AddF32x2("{}=uvec2({},{});", inst, e1, e2);
}

void EmitCompositeConstructF32x3([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view e1,
                                 [[maybe_unused]] std::string_view e2,
                                 [[maybe_unused]] std::string_view e3) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructF32x4([[maybe_unused]] EmitContext& ctx,
                                 [[maybe_unused]] std::string_view e1,
                                 [[maybe_unused]] std::string_view e2,
                                 [[maybe_unused]] std::string_view e3,
                                 [[maybe_unused]] std::string_view e4) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractF32x2(EmitContext& ctx, IR::Inst& inst, std::string_view composite,
                               u32 index) {
    ctx.AddF32("{}={}[{}];", inst, composite, index);
}

void EmitCompositeExtractF32x3([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] std::string_view composite,
                               [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractF32x4([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] std::string_view composite,
                               [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF32x2([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF32x3([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF32x4([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructF64x2([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructF64x3([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeConstructF64x4([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractF64x2([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractF64x3([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeExtractF64x4([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF64x2([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF64x3([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitCompositeInsertF64x4([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] std::string_view composite,
                              [[maybe_unused]] std::string_view object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLSL Instruction");
}
} // namespace Shader::Backend::GLSL
