// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

static void ValidateTypes(const IR::Block& block) {
    for (const IR::Inst& inst : block) {
        const size_t num_args{inst.NumArgs()};
        for (size_t i = 0; i < num_args; ++i) {
            const IR::Type t1{inst.Arg(i).Type()};
            const IR::Type t2{IR::ArgTypeOf(inst.Opcode(), i)};
            if (!IR::AreTypesCompatible(t1, t2)) {
                throw LogicError("Invalid types in block:\n{}", IR::DumpBlock(block));
            }
        }
    }
}

static void ValidateUses(const IR::Block& block) {
    std::map<IR::Inst*, int> actual_uses;
    for (const IR::Inst& inst : block) {
        const size_t num_args{inst.NumArgs()};
        for (size_t i = 0; i < num_args; ++i) {
            const IR::Value arg{inst.Arg(i)};
            if (!arg.IsImmediate()) {
                ++actual_uses[arg.Inst()];
            }
        }
    }
    for (const auto [inst, uses] : actual_uses) {
        if (inst->UseCount() != uses) {
            throw LogicError("Invalid uses in block:\n{}", IR::DumpBlock(block));
        }
    }
}

void VerificationPass(const IR::Block& block) {
    ValidateTypes(block);
    ValidateUses(block);
}

} // namespace Shader::Optimization
