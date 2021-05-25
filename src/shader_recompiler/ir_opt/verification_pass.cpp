// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <set>

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {

static void ValidateTypes(const IR::Program& program) {
    for (const auto& block : program.blocks) {
        for (const IR::Inst& inst : *block) {
            if (inst.GetOpcode() == IR::Opcode::Phi) {
                // Skip validation on phi nodes
                continue;
            }
            const size_t num_args{inst.NumArgs()};
            for (size_t i = 0; i < num_args; ++i) {
                const IR::Type t1{inst.Arg(i).Type()};
                const IR::Type t2{IR::ArgTypeOf(inst.GetOpcode(), i)};
                if (!IR::AreTypesCompatible(t1, t2)) {
                    throw LogicError("Invalid types in block:\n{}", IR::DumpBlock(*block));
                }
            }
        }
    }
}

static void ValidateUses(const IR::Program& program) {
    std::map<IR::Inst*, int> actual_uses;
    for (const auto& block : program.blocks) {
        for (const IR::Inst& inst : *block) {
            const size_t num_args{inst.NumArgs()};
            for (size_t i = 0; i < num_args; ++i) {
                const IR::Value arg{inst.Arg(i)};
                if (!arg.IsImmediate()) {
                    ++actual_uses[arg.Inst()];
                }
            }
        }
    }
    for (const auto [inst, uses] : actual_uses) {
        if (inst->UseCount() != uses) {
            throw LogicError("Invalid uses in block: {}", IR::DumpProgram(program));
        }
    }
}

static void ValidateForwardDeclarations(const IR::Program& program) {
    std::set<const IR::Inst*> definitions;
    for (const IR::Block* const block : program.blocks) {
        for (const IR::Inst& inst : *block) {
            definitions.emplace(&inst);
            if (inst.GetOpcode() == IR::Opcode::Phi) {
                // Phi nodes can have forward declarations
                continue;
            }
            const size_t num_args{inst.NumArgs()};
            for (size_t arg = 0; arg < num_args; ++arg) {
                if (inst.Arg(arg).IsImmediate()) {
                    continue;
                }
                if (!definitions.contains(inst.Arg(arg).Inst())) {
                    throw LogicError("Forward declaration in block: {}", IR::DumpBlock(*block));
                }
            }
        }
    }
}

static void ValidatePhiNodes(const IR::Program& program) {
    for (const IR::Block* const block : program.blocks) {
        bool no_more_phis{false};
        for (const IR::Inst& inst : *block) {
            if (inst.GetOpcode() == IR::Opcode::Phi) {
                if (no_more_phis) {
                    throw LogicError("Interleaved phi nodes: {}", IR::DumpBlock(*block));
                }
            } else {
                no_more_phis = true;
            }
        }
    }
}

void VerificationPass(const IR::Program& program) {
    ValidateTypes(program);
    ValidateUses(program);
    ValidateForwardDeclarations(program);
    ValidatePhiNodes(program);
}

} // namespace Shader::Optimization
