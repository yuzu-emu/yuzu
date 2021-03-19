// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitBranch(EmitContext& ctx, Id label) {
    ctx.OpBranch(label);
}

void EmitBranchConditional(EmitContext& ctx, Id condition, Id true_label, Id false_label) {
    ctx.OpBranchConditional(condition, true_label, false_label);
}

void EmitLoopMerge(EmitContext& ctx, Id merge_label, Id continue_label) {
    ctx.OpLoopMerge(merge_label, continue_label, spv::LoopControlMask::MaskNone);
}

void EmitSelectionMerge(EmitContext& ctx, Id merge_label) {
    ctx.OpSelectionMerge(merge_label, spv::SelectionControlMask::MaskNone);
}

void EmitReturn(EmitContext& ctx) {
    ctx.OpReturn();
}

void EmitDemoteToHelperInvocation(EmitContext& ctx, Id continue_label) {
    ctx.OpDemoteToHelperInvocationEXT();
    ctx.OpBranch(continue_label);
}

} // namespace Shader::Backend::SPIRV
