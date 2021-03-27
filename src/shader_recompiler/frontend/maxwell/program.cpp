// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <vector>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/post_order.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/frontend/maxwell/structured_control_flow.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Maxwell {

static void RemoveUnreachableBlocks(IR::Program& program) {
    // Some blocks might be unreachable if a function call exists unconditionally
    // If this happens the number of blocks and post order blocks will mismatch
    if (program.blocks.size() == program.post_order_blocks.size()) {
        return;
    }
    const IR::BlockList& post_order{program.post_order_blocks};
    std::erase_if(program.blocks, [&](IR::Block* block) {
        return std::ranges::find(post_order, block) == post_order.end();
    });
}

IR::Program TranslateProgram(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                             Environment& env, Flow::CFG& cfg) {
    IR::Program program;
    program.blocks = VisitAST(inst_pool, block_pool, env, cfg);
    program.post_order_blocks = PostOrder(program.blocks);
    program.stage = env.ShaderStage();
    if (program.stage == Stage::Compute) {
        program.workgroup_size = env.WorkgroupSize();
    }
    RemoveUnreachableBlocks(program);

    // Replace instructions before the SSA rewrite
    Optimization::LowerFp16ToFp32(program);

    Optimization::SsaRewritePass(program);

    Optimization::GlobalMemoryToStorageBufferPass(program);
    Optimization::TexturePass(env, program);

    Optimization::ConstantPropagationPass(program);
    Optimization::DeadCodeEliminationPass(program);
    Optimization::IdentityRemovalPass(program);
    Optimization::VerificationPass(program);
    Optimization::CollectShaderInfoPass(program);
    return program;
}

} // namespace Shader::Maxwell
