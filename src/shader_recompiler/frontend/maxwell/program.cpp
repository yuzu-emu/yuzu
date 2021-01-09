// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>

#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/frontend/maxwell/termination_code.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"

namespace Shader::Maxwell {

Program::Function::~Function() {
    std::ranges::for_each(blocks, &std::destroy_at<IR::Block>);
}

Program::Program(Environment& env, const Flow::CFG& cfg) {
    std::vector<IR::Block*> block_map;
    functions.reserve(cfg.Functions().size());

    for (const Flow::Function& cfg_function : cfg.Functions()) {
        Function& function{functions.emplace_back()};

        const size_t num_blocks{cfg_function.blocks.size()};
        IR::Block* block_memory{block_alloc_pool.allocate(num_blocks)};
        function.blocks.reserve(num_blocks);

        block_map.resize(cfg_function.blocks_data.size());

        // Visit the instructions of all blocks
        for (const Flow::BlockId block_id : cfg_function.blocks) {
            const Flow::Block& flow_block{cfg_function.blocks_data[block_id]};

            IR::Block* const block{std::construct_at(block_memory, Translate(env, flow_block))};
            ++block_memory;
            function.blocks.push_back(block);
            block_map[flow_block.id] = block;
        }
        // Now that all blocks are defined, emit the termination instructions
        for (const Flow::BlockId block_id : cfg_function.blocks) {
            const Flow::Block& flow_block{cfg_function.blocks_data[block_id]};
            EmitTerminationCode(flow_block, block_map);
        }
    }
}

std::string DumpProgram(const Program& program) {
    size_t index{0};
    std::map<const IR::Inst*, size_t> inst_to_index;
    std::map<const IR::Block*, size_t> block_to_index;

    for (const Program::Function& function : program.functions) {
        for (const IR::Block* const block : function.blocks) {
            block_to_index.emplace(block, index);
            ++index;
        }
    }
    std::string ret;
    for (const Program::Function& function : program.functions) {
        ret += fmt::format("Function\n");
        for (const IR::Block* const block : function.blocks) {
            ret += IR::DumpBlock(*block, block_to_index, inst_to_index, index) + '\n';
        }
    }
    return ret;
}

} // namespace Shader::Maxwell
