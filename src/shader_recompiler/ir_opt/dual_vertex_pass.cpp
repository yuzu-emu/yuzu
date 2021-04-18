// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <ranges>
#include <tuple>
#include <type_traits>

#include "common/bit_cast.h"
#include "common/bit_util.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

void VertexATransformPass(IR::Program& program) {
    bool replaced_join{};
    bool eliminated_epilogue{};
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            switch (inst.GetOpcode()) {
            case IR::Opcode::Return:
                inst.ReplaceOpcode(IR::Opcode::Join);
                replaced_join = true;
                break;
            case IR::Opcode::Epilogue:
                inst.Invalidate();
                eliminated_epilogue = true;
                break;
            default:
                break;
            }
            if (replaced_join && eliminated_epilogue) {
                return;
            }
        }
    }
}

void VertexBTransformPass(IR::Program& program) {
    for (IR::Block* const block : program.post_order_blocks | std::views::reverse) {
        for (IR::Inst& inst : block->Instructions()) {
            if (inst.GetOpcode() == IR::Opcode::Prologue) {
                return inst.Invalidate();
            }
        }
    }
}

void DualVertexJoinPass(IR::Program& program) {
    const auto& blocks = program.blocks;
    s64 s = static_cast<s64>(blocks.size()) - 1;
    if (s < 1) {
        throw NotImplementedException("Dual Vertex Join pass failed, expected atleast 2 blocks!");
    }
    for (s64 index = 0; index < s; index++) {
        IR::Block* const current_block = blocks[index];
        IR::Block* const next_block = blocks[index + 1];
        for (IR::Inst& inst : current_block->Instructions()) {
            if (inst.GetOpcode() == IR::Opcode::Join) {
                IR::IREmitter ir{*current_block, IR::Block::InstructionList::s_iterator_to(inst)};
                ir.Branch(next_block);
                inst.Invalidate();
                // only 1 join should exist
                return;
            }
        }
    }
    throw NotImplementedException("Dual Vertex Join pass failed, no join present!");
}

} // namespace Shader::Optimization
