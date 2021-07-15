// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/post_order.h"

namespace Shader::IR {

BlockList PostOrder(const AbstractSyntaxNode& root) {
    boost::container::small_vector<Block*, 16> block_stack;
    boost::container::flat_set<Block*> visited;
    BlockList post_order_blocks;

    if (root.type != AbstractSyntaxNode::Type::Block) {
        throw LogicError("First node in abstract syntax list root is not a block");
    }
    Block* const first_block{root.data.block};
    visited.insert(first_block);
    block_stack.push_back(first_block);

    while (!block_stack.empty()) {
        Block* const block{block_stack.back()};
        const auto visit{[&](Block* branch) {
            if (!visited.insert(branch).second) {
                return false;
            }
            // Calling push_back twice is faster than insert on MSVC
            block_stack.push_back(block);
            block_stack.push_back(branch);
            return true;
        }};
        block_stack.pop_back();
        if (std::ranges::none_of(block->ImmSuccessors(), visit)) {
            post_order_blocks.push_back(block);
        }
    }
    return post_order_blocks;
}

} // namespace Shader::IR
