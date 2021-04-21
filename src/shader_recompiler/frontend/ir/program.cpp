// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <string>

#include <fmt/format.h>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

std::string DumpProgram(const Program& program) {
    size_t index{0};
    std::map<const IR::Inst*, size_t> inst_to_index;
    std::map<const IR::Block*, size_t> block_to_index;

    for (const IR::Block* const block : program.blocks) {
        block_to_index.emplace(block, index);
        ++index;
    }
    std::string ret;
    for (const auto& block : program.blocks) {
        ret += IR::DumpBlock(*block, block_to_index, inst_to_index, index) + '\n';
    }
    return ret;
}

} // namespace Shader::IR
