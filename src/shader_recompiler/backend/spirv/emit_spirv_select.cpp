// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

Id EmitSelectU1(EmitContext& ctx, Id cond, Id true_value, Id false_value) {
    return ctx.OpSelect(ctx.U1, cond, true_value, false_value);
}

Id EmitSelectU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Id cond,
                [[maybe_unused]] Id true_value, [[maybe_unused]] Id false_value) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitSelectU16(EmitContext& ctx, Id cond, Id true_value, Id false_value) {
    return ctx.OpSelect(ctx.U16, cond, true_value, false_value);
}

Id EmitSelectU32(EmitContext& ctx, Id cond, Id true_value, Id false_value) {
    return ctx.OpSelect(ctx.U32[1], cond, true_value, false_value);
}

Id EmitSelectU64(EmitContext& ctx, Id cond, Id true_value, Id false_value) {
    return ctx.OpSelect(ctx.U64, cond, true_value, false_value);
}

Id EmitSelectF16(EmitContext& ctx, Id cond, Id true_value, Id false_value) {
    return ctx.OpSelect(ctx.F16[1], cond, true_value, false_value);
}

Id EmitSelectF32(EmitContext& ctx, Id cond, Id true_value, Id false_value) {
    return ctx.OpSelect(ctx.F32[1], cond, true_value, false_value);
}

} // namespace Shader::Backend::SPIRV
