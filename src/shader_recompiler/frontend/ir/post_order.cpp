// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/post_order.h"

namespace Shader::IR {

BlockList PostOrder(const BlockList& blocks) {
    boost::container::small_vector<Block*, 16> block_stack;
    boost::container::flat_set<Block*> visited;

    BlockList post_order_blocks;
    post_order_blocks.reserve(blocks.size());

    Block* const first_block{blocks.front()};
    visited.insert(first_block);
    block_stack.push_back(first_block);

    const auto visit_branch = [&](Block* block, Block* branch) {
        if (!branch) {
            return false;
        }
        if (!visited.insert(branch).second) {
            return false;
        }
        // Calling push_back twice is faster than insert on MSVC
        block_stack.push_back(block);
        block_stack.push_back(branch);
        return true;
    };
    while (!block_stack.empty()) {
        Block* const block{block_stack.back()};
        block_stack.pop_back();

        if (!visit_branch(block, block->TrueBranch()) &&
            !visit_branch(block, block->FalseBranch())) {
            post_order_blocks.push_back(block);
        }
    }
    return post_order_blocks;
}

} // namespace Shader::IR
