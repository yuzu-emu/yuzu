// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/function.h"

namespace Shader::Optimization {

template <typename Func>
void Invoke(Func&& func, IR::Function& function) {
    for (const auto& block : function.blocks) {
        func(*block);
    }
}

void ConstantPropagationPass(IR::Block& block);
void DeadCodeEliminationPass(IR::Block& block);
void GlobalMemoryToStorageBufferPass(IR::Block& block);
void IdentityRemovalPass(IR::Function& function);
void SsaRewritePass(IR::Function& function);
void VerificationPass(const IR::Function& function);

} // namespace Shader::Optimization
