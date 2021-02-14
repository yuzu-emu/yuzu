// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/function.h"

namespace Shader::Optimization {

template <typename Func>
void PostOrderInvoke(Func&& func, IR::Function& function) {
    for (const auto& block : function.post_order_blocks) {
        func(*block);
    }
}

void ConstantPropagationPass(IR::Block& block);
void DeadCodeEliminationPass(IR::Block& block);
void GlobalMemoryToStorageBufferPass(IR::Block& block);
void IdentityRemovalPass(IR::Function& function);
void SsaRewritePass(std::span<IR::Block* const> post_order_blocks);
void VerificationPass(const IR::Function& function);

} // namespace Shader::Optimization
