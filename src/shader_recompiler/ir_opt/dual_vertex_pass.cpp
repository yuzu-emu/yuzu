// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <ranges>

#include "common/bit_cast.h"
#include "common/bit_util.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

void VertexATransformPass(IR::Program& program) {
    for (IR::Block* const block : program.blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            if (inst.GetOpcode() == IR::Opcode::Epilogue) {
                return inst.Invalidate();
            }
        }
    }
}

void VertexBTransformPass(IR::Program& program) {
    for (IR::Block* const block : program.blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            if (inst.GetOpcode() == IR::Opcode::Prologue) {
                return inst.Invalidate();
            }
        }
    }
}

} // namespace Shader::Optimization
