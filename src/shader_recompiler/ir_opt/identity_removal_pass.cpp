// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

void IdentityRemovalPass(IR::Block& block) {
    std::vector<IR::Inst*> to_invalidate;

    for (auto inst = block.begin(); inst != block.end();) {
        const size_t num_args{inst->NumArgs()};
        for (size_t i = 0; i < num_args; ++i) {
            IR::Value arg;
            while ((arg = inst->Arg(i)).IsIdentity()) {
                inst->SetArg(i, arg.Inst()->Arg(0));
            }
        }
        if (inst->Opcode() == IR::Opcode::Identity || inst->Opcode() == IR::Opcode::Void) {
            to_invalidate.push_back(&*inst);
            inst = block.Instructions().erase(inst);
        } else {
            ++inst;
        }
    }

    for (IR::Inst* const inst : to_invalidate) {
        inst->Invalidate();
    }
}

} // namespace Shader::Optimization
