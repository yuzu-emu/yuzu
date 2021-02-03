// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "shader_recompiler/frontend/ir/basic_block.h"

namespace Shader::IR {

struct Function {
    struct InplaceDelete {
        void operator()(IR::Block* block) const noexcept {
            std::destroy_at(block);
        }
    };
    using UniqueBlock = std::unique_ptr<IR::Block, InplaceDelete>;

    std::vector<UniqueBlock> blocks;
};

} // namespace Shader::IR
