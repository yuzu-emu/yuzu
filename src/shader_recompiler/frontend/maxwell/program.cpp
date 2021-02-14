// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <vector>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/post_order.h"
#include "shader_recompiler/frontend/ir/structured_control_flow.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Maxwell {
namespace {
IR::BlockList TranslateCode(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                            Environment& env, Flow::Function& cfg_function) {
    const size_t num_blocks{cfg_function.blocks.size()};
    std::vector<IR::Block*> blocks(cfg_function.blocks.size());
    std::ranges::for_each(cfg_function.blocks, [&, i = size_t{0}](auto& cfg_block) mutable {
        const u32 begin{cfg_block.begin.Offset()};
        const u32 end{cfg_block.end.Offset()};
        blocks[i] = block_pool.Create(inst_pool, begin, end);
        cfg_block.ir = blocks[i];
        ++i;
    });
    std::ranges::for_each(cfg_function.blocks, [&, i = size_t{0}](auto& cfg_block) mutable {
        IR::Block* const block{blocks[i]};
        ++i;
        if (cfg_block.end_class != Flow::EndClass::Branch) {
            block->SetReturn();
        } else if (cfg_block.cond == IR::Condition{true}) {
            block->SetBranch(cfg_block.branch_true->ir);
        } else if (cfg_block.cond == IR::Condition{false}) {
            block->SetBranch(cfg_block.branch_false->ir);
        } else {
            block->SetBranches(cfg_block.cond, cfg_block.branch_true->ir,
                               cfg_block.branch_false->ir);
        }
    });
    return IR::VisitAST(inst_pool, block_pool, blocks,
                        [&](IR::Block* block) { Translate(env, block); });
}
} // Anonymous namespace

IR::Program TranslateProgram(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                             Environment& env, Flow::CFG& cfg) {
    IR::Program program;
    auto& functions{program.functions};
    functions.reserve(cfg.Functions().size());
    for (Flow::Function& cfg_function : cfg.Functions()) {
        functions.push_back(IR::Function{
            .blocks{TranslateCode(inst_pool, block_pool, env, cfg_function)},
        });
    }

    fmt::print(stdout, "No optimizations: {}", IR::DumpProgram(program));
    for (IR::Function& function : functions) {
        function.post_order_blocks = PostOrder(function.blocks);
        Optimization::SsaRewritePass(function.post_order_blocks);
    }
    for (IR::Function& function : functions) {
        Optimization::PostOrderInvoke(Optimization::GlobalMemoryToStorageBufferPass, function);
        Optimization::PostOrderInvoke(Optimization::ConstantPropagationPass, function);
        Optimization::PostOrderInvoke(Optimization::DeadCodeEliminationPass, function);
        Optimization::IdentityRemovalPass(function);
        Optimization::VerificationPass(function);
    }
    //*/
    return program;
}

} // namespace Shader::Maxwell
