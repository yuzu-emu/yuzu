// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/frontend/maxwell/termination_code.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Maxwell {
namespace {
void TranslateCode(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                   Environment& env, const Flow::Function& cfg_function, IR::Function& function,
                   std::span<IR::Block*> block_map) {
    const size_t num_blocks{cfg_function.blocks.size()};
    function.blocks.reserve(num_blocks);

    for (const Flow::BlockId block_id : cfg_function.blocks) {
        const Flow::Block& flow_block{cfg_function.blocks_data[block_id]};

        IR::Block* const ir_block{block_pool.Create(Translate(inst_pool, env, flow_block))};
        block_map[flow_block.id] = ir_block;
        function.blocks.emplace_back(ir_block);
    }
}

void EmitTerminationInsts(const Flow::Function& cfg_function,
                          std::span<IR::Block* const> block_map) {
    for (const Flow::BlockId block_id : cfg_function.blocks) {
        const Flow::Block& flow_block{cfg_function.blocks_data[block_id]};
        EmitTerminationCode(flow_block, block_map);
    }
}

void TranslateFunction(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                       Environment& env, const Flow::Function& cfg_function,
                       IR::Function& function) {
    std::vector<IR::Block*> block_map;
    block_map.resize(cfg_function.blocks_data.size());

    TranslateCode(inst_pool, block_pool, env, cfg_function, function, block_map);
    EmitTerminationInsts(cfg_function, block_map);
}
} // Anonymous namespace

IR::Program TranslateProgram(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                             Environment& env, const Flow::CFG& cfg) {
    IR::Program program;
    auto& functions{program.functions};
    functions.reserve(cfg.Functions().size());
    for (const Flow::Function& cfg_function : cfg.Functions()) {
        TranslateFunction(inst_pool, block_pool, env, cfg_function, functions.emplace_back());
    }
    std::ranges::for_each(functions, Optimization::SsaRewritePass);
    for (IR::Function& function : functions) {
        Optimization::Invoke(Optimization::GlobalMemoryToStorageBufferPass, function);
        Optimization::Invoke(Optimization::ConstantPropagationPass, function);
        Optimization::Invoke(Optimization::DeadCodeEliminationPass, function);
        Optimization::IdentityRemovalPass(function);
        Optimization::VerificationPass(function);
    }
    //*/
    return program;
}

} // namespace Shader::Maxwell
