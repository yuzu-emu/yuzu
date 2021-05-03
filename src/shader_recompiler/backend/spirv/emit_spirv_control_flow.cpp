// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"

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

void EmitJoin(EmitContext&) {
    throw NotImplementedException("Join shouldn't be emitted");
}

void EmitUnreachable(EmitContext& ctx) {
    ctx.OpUnreachable();
}

void EmitDemoteToHelperInvocation(EmitContext& ctx, Id continue_label) {
    if (ctx.profile.support_demote_to_helper_invocation) {
        ctx.OpDemoteToHelperInvocationEXT();
        ctx.OpBranch(continue_label);
    } else {
        ctx.OpKill();
    }
}

} // namespace Shader::Backend::SPIRV
