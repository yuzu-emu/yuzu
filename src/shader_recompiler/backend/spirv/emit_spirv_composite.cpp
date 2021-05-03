// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"

namespace Shader::Backend::SPIRV {

Id EmitCompositeConstructU32x2(EmitContext& ctx, Id e1, Id e2) {
    return ctx.OpCompositeConstruct(ctx.U32[2], e1, e2);
}

Id EmitCompositeConstructU32x3(EmitContext& ctx, Id e1, Id e2, Id e3) {
    return ctx.OpCompositeConstruct(ctx.U32[3], e1, e2, e3);
}

Id EmitCompositeConstructU32x4(EmitContext& ctx, Id e1, Id e2, Id e3, Id e4) {
    return ctx.OpCompositeConstruct(ctx.U32[4], e1, e2, e3, e4);
}

Id EmitCompositeExtractU32x2(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.U32[1], composite, index);
}

Id EmitCompositeExtractU32x3(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.U32[1], composite, index);
}

Id EmitCompositeExtractU32x4(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.U32[1], composite, index);
}

Id EmitCompositeInsertU32x2(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.U32[2], object, composite, index);
}

Id EmitCompositeInsertU32x3(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.U32[3], object, composite, index);
}

Id EmitCompositeInsertU32x4(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.U32[4], object, composite, index);
}

Id EmitCompositeConstructF16x2(EmitContext& ctx, Id e1, Id e2) {
    return ctx.OpCompositeConstruct(ctx.F16[2], e1, e2);
}

Id EmitCompositeConstructF16x3(EmitContext& ctx, Id e1, Id e2, Id e3) {
    return ctx.OpCompositeConstruct(ctx.F16[3], e1, e2, e3);
}

Id EmitCompositeConstructF16x4(EmitContext& ctx, Id e1, Id e2, Id e3, Id e4) {
    return ctx.OpCompositeConstruct(ctx.F16[4], e1, e2, e3, e4);
}

Id EmitCompositeExtractF16x2(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.F16[1], composite, index);
}

Id EmitCompositeExtractF16x3(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.F16[1], composite, index);
}

Id EmitCompositeExtractF16x4(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.F16[1], composite, index);
}

Id EmitCompositeInsertF16x2(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F16[2], object, composite, index);
}

Id EmitCompositeInsertF16x3(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F16[3], object, composite, index);
}

Id EmitCompositeInsertF16x4(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F16[4], object, composite, index);
}

Id EmitCompositeConstructF32x2(EmitContext& ctx, Id e1, Id e2) {
    return ctx.OpCompositeConstruct(ctx.F32[2], e1, e2);
}

Id EmitCompositeConstructF32x3(EmitContext& ctx, Id e1, Id e2, Id e3) {
    return ctx.OpCompositeConstruct(ctx.F32[3], e1, e2, e3);
}

Id EmitCompositeConstructF32x4(EmitContext& ctx, Id e1, Id e2, Id e3, Id e4) {
    return ctx.OpCompositeConstruct(ctx.F32[4], e1, e2, e3, e4);
}

Id EmitCompositeExtractF32x2(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.F32[1], composite, index);
}

Id EmitCompositeExtractF32x3(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.F32[1], composite, index);
}

Id EmitCompositeExtractF32x4(EmitContext& ctx, Id composite, u32 index) {
    return ctx.OpCompositeExtract(ctx.F32[1], composite, index);
}

Id EmitCompositeInsertF32x2(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F32[2], object, composite, index);
}

Id EmitCompositeInsertF32x3(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F32[3], object, composite, index);
}

Id EmitCompositeInsertF32x4(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F32[4], object, composite, index);
}

void EmitCompositeConstructF64x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF64x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeConstructF64x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF64x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF64x3(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitCompositeExtractF64x4(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitCompositeInsertF64x2(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F64[2], object, composite, index);
}

Id EmitCompositeInsertF64x3(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F64[3], object, composite, index);
}

Id EmitCompositeInsertF64x4(EmitContext& ctx, Id composite, Id object, u32 index) {
    return ctx.OpCompositeInsert(ctx.F64[4], object, composite, index);
}

} // namespace Shader::Backend::SPIRV
