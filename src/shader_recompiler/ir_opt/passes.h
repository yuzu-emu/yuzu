// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/function.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Optimization {

template <typename Func>
void PostOrderInvoke(Func&& func, IR::Function& function) {
    for (const auto& block : function.post_order_blocks) {
        func(*block);
    }
}

void CollectShaderInfoPass(IR::Program& program);
void ConstantPropagationPass(IR::Block& block);
void DeadCodeEliminationPass(IR::Block& block);
void GlobalMemoryToStorageBufferPass(IR::Program& program);
void IdentityRemovalPass(IR::Function& function);
void LowerFp16ToFp32(IR::Program& program);
void SsaRewritePass(std::span<IR::Block* const> post_order_blocks);
void TexturePass(Environment& env, IR::Program& program);
void VerificationPass(const IR::Function& function);

} // namespace Shader::Optimization
