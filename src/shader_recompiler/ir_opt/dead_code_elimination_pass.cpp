// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <ranges>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

void DeadCodeEliminationPass(IR::Program& program) {
    // We iterate over the instructions in reverse order.
    // This is because removing an instruction reduces the number of uses for earlier instructions.
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions() | std::views::reverse) {
            if (!inst.HasUses() && !inst.MayHaveSideEffects()) {
                inst.Invalidate();
            }
        }
    }
}

} // namespace Shader::Optimization
