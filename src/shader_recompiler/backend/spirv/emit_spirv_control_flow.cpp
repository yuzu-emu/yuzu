// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

void EmitBranch(EmitContext& ctx, IR::Block* label) {
    ctx.OpBranch(label->Definition<Id>());
}

void EmitBranchConditional(EmitContext& ctx, Id condition, IR::Block* true_label,
                                      IR::Block* false_label) {
    ctx.OpBranchConditional(condition, true_label->Definition<Id>(), false_label->Definition<Id>());
}

void EmitLoopMerge(EmitContext& ctx, IR::Block* merge_label, IR::Block* continue_label) {
    ctx.OpLoopMerge(merge_label->Definition<Id>(), continue_label->Definition<Id>(),
                    spv::LoopControlMask::MaskNone);
}

void EmitSelectionMerge(EmitContext& ctx, IR::Block* merge_label) {
    ctx.OpSelectionMerge(merge_label->Definition<Id>(), spv::SelectionControlMask::MaskNone);
}

void EmitReturn(EmitContext& ctx) {
    ctx.OpReturn();
}

} // namespace Shader::Backend::SPIRV
