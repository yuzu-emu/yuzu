// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"

namespace Shader::Backend::SPIRV {

Id EmitLogicalOr(EmitContext& ctx, Id a, Id b) {
    return ctx.OpLogicalOr(ctx.U1, a, b);
}

Id EmitLogicalAnd(EmitContext& ctx, Id a, Id b) {
    return ctx.OpLogicalAnd(ctx.U1, a, b);
}

Id EmitLogicalXor(EmitContext& ctx, Id a, Id b) {
    return ctx.OpLogicalNotEqual(ctx.U1, a, b);
}

Id EmitLogicalNot(EmitContext& ctx, Id value) {
    return ctx.OpLogicalNot(ctx.U1, value);
}

} // namespace Shader::Backend::SPIRV
