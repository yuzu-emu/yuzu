// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

Id EmitConvertS16F16(EmitContext& ctx, Id value) {
    return ctx.OpUConvert(ctx.U32[1], ctx.OpConvertFToS(ctx.U16, value));
}

Id EmitConvertS16F32(EmitContext& ctx, Id value) {
    return ctx.OpUConvert(ctx.U32[1], ctx.OpConvertFToS(ctx.U16, value));
}

Id EmitConvertS16F64(EmitContext& ctx, Id value) {
    return ctx.OpUConvert(ctx.U32[1], ctx.OpConvertFToS(ctx.U16, value));
}

Id EmitConvertS32F16(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToS(ctx.U32[1], value);
}

Id EmitConvertS32F32(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToS(ctx.U32[1], value);
}

Id EmitConvertS32F64(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToS(ctx.U32[1], value);
}

Id EmitConvertS64F16(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToS(ctx.U64, value);
}

Id EmitConvertS64F32(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToS(ctx.U64, value);
}

Id EmitConvertS64F64(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToS(ctx.U64, value);
}

Id EmitConvertU16F16(EmitContext& ctx, Id value) {
    return ctx.OpUConvert(ctx.U32[1], ctx.OpConvertFToU(ctx.U16, value));
}

Id EmitConvertU16F32(EmitContext& ctx, Id value) {
    return ctx.OpUConvert(ctx.U32[1], ctx.OpConvertFToU(ctx.U16, value));
}

Id EmitConvertU16F64(EmitContext& ctx, Id value) {
    return ctx.OpUConvert(ctx.U32[1], ctx.OpConvertFToU(ctx.U16, value));
}

Id EmitConvertU32F16(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToU(ctx.U32[1], value);
}

Id EmitConvertU32F32(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToU(ctx.U32[1], value);
}

Id EmitConvertU32F64(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToU(ctx.U32[1], value);
}

Id EmitConvertU64F16(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToU(ctx.U64, value);
}

Id EmitConvertU64F32(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToU(ctx.U64, value);
}

Id EmitConvertU64F64(EmitContext& ctx, Id value) {
    return ctx.OpConvertFToU(ctx.U64, value);
}

Id EmitConvertU64U32(EmitContext& ctx, Id value) {
    return ctx.OpUConvert(ctx.U64, value);
}

Id EmitConvertU32U64(EmitContext& ctx, Id value) {
    return ctx.OpUConvert(ctx.U32[1], value);
}

} // namespace Shader::Backend::SPIRV
