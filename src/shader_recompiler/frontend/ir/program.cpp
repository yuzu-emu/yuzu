// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>

#include <fmt/format.h>

#include "shader_recompiler/frontend/ir/function.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::IR {

std::string DumpProgram(const Program& program) {
    size_t index{0};
    std::map<const IR::Inst*, size_t> inst_to_index;
    std::map<const IR::Block*, size_t> block_to_index;

    for (const IR::Function& function : program.functions) {
        for (const IR::Block* const block : function.blocks) {
            block_to_index.emplace(block, index);
            ++index;
        }
    }
    std::string ret;
    for (const IR::Function& function : program.functions) {
        ret += fmt::format("Function\n");
        for (const auto& block : function.blocks) {
            ret += IR::DumpBlock(*block, block_to_index, inst_to_index, index) + '\n';
        }
    }
    return ret;
}

} // namespace Shader::IR
