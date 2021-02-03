// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>

#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/frontend/maxwell/termination_code.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Maxwell {
namespace {
void TranslateCode(Environment& env, const Flow::Function& cfg_function, IR::Function& function,
                   std::span<IR::Block*> block_map, IR::Block* block_memory) {
    const size_t num_blocks{cfg_function.blocks.size()};
    function.blocks.reserve(num_blocks);

    for (const Flow::BlockId block_id : cfg_function.blocks) {
        const Flow::Block& flow_block{cfg_function.blocks_data[block_id]};

        function.blocks.emplace_back(std::construct_at(block_memory, Translate(env, flow_block)));
        block_map[flow_block.id] = function.blocks.back().get();
        ++block_memory;
    }
}

void EmitTerminationInsts(const Flow::Function& cfg_function,
                          std::span<IR::Block* const> block_map) {
    for (const Flow::BlockId block_id : cfg_function.blocks) {
        const Flow::Block& flow_block{cfg_function.blocks_data[block_id]};
        EmitTerminationCode(flow_block, block_map);
    }
}

void TranslateFunction(Environment& env, const Flow::Function& cfg_function, IR::Function& function,
                       IR::Block* block_memory) {
    std::vector<IR::Block*> block_map;
    block_map.resize(cfg_function.blocks_data.size());

    TranslateCode(env, cfg_function, function, block_map, block_memory);
    EmitTerminationInsts(cfg_function, block_map);
}
} // Anonymous namespace

Program::Program(Environment& env, const Flow::CFG& cfg) {
    functions.reserve(cfg.Functions().size());
    for (const Flow::Function& cfg_function : cfg.Functions()) {
        TranslateFunction(env, cfg_function, functions.emplace_back(),
                          block_alloc_pool.allocate(cfg_function.blocks.size()));
    }
    std::ranges::for_each(functions, Optimization::SsaRewritePass);
    for (IR::Function& function : functions) {
        Optimization::Invoke(Optimization::DeadCodeEliminationPass, function);
        Optimization::Invoke(Optimization::IdentityRemovalPass, function);
        // Optimization::Invoke(Optimization::VerificationPass, function);
    }
}

std::string DumpProgram(const Program& program) {
    size_t index{0};
    std::map<const IR::Inst*, size_t> inst_to_index;
    std::map<const IR::Block*, size_t> block_to_index;

    for (const IR::Function& function : program.functions) {
        for (const auto& block : function.blocks) {
            block_to_index.emplace(block.get(), index);
            ++index;
        }
    }
    std::string ret;
    for (const IR::Function& function : program.functions) {
        ret += fmt::format("Function\n");
        for (const auto& block : function.blocks) {
            ret += IR::DumpBlock(*block, block_to_index, inst_to_index, index) + '\n';
        }
    }
    return ret;
}

} // namespace Shader::Maxwell
