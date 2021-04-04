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
namespace {
void RemoveUnreachableBlocks(IR::Program& program) {
    // Some blocks might be unreachable if a function call exists unconditionally
    // If this happens the number of blocks and post order blocks will mismatch
    if (program.blocks.size() == program.post_order_blocks.size()) {
        return;
    }
    const auto begin{std::next(program.blocks.begin())};
    const auto end{program.blocks.end()};
    const auto pred{[](IR::Block* block) { return block->ImmediatePredecessors().empty(); }};
    program.blocks.erase(std::remove_if(begin, end, pred), end);
}

void CollectInterpolationInfo(Environment& env, IR::Program& program) {
    if (program.stage != Stage::Fragment) {
        return;
    }
    const ProgramHeader& sph{env.SPH()};
    for (size_t index = 0; index < program.info.input_generics.size(); ++index) {
        std::optional<PixelImap> imap;
        for (const PixelImap value : sph.ps.GenericInputMap(static_cast<u32>(index))) {
            if (value == PixelImap::Unused) {
                continue;
            }
            if (imap && imap != value) {
                throw NotImplementedException("Per component interpolation");
            }
            imap = value;
        }
        if (!imap) {
            continue;
        }
        program.info.input_generics[index].interpolation = [&] {
            switch (*imap) {
            case PixelImap::Unused:
            case PixelImap::Perspective:
                return Interpolation::Smooth;
            case PixelImap::Constant:
                return Interpolation::Flat;
            case PixelImap::ScreenLinear:
                return Interpolation::NoPerspective;
            }
            throw NotImplementedException("Unknown interpolation {}", *imap);
        }();
    }
}
} // Anonymous namespace

IR::Program TranslateProgram(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                             Environment& env, Flow::CFG& cfg) {
    IR::Program program;
    program.blocks = VisitAST(inst_pool, block_pool, env, cfg);
    program.post_order_blocks = PostOrder(program.blocks);
    program.stage = env.ShaderStage();
    program.local_memory_size = env.LocalMemorySize();
    if (program.stage == Stage::Compute) {
        program.workgroup_size = env.WorkgroupSize();
        program.shared_memory_size = env.SharedMemorySize();
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
    CollectInterpolationInfo(env, program);
    return program;
}

} // namespace Shader::Maxwell
