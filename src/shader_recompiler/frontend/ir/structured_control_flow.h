// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <span>

#include <boost/intrusive/list.hpp>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/object_pool.h"

namespace Shader::IR {

[[nodiscard]] BlockList VisitAST(ObjectPool<Inst>& inst_pool, ObjectPool<Block>& block_pool,
                                 std::span<Block* const> unordered_blocks,
                                 const std::function<void(Block*)>& func);

} // namespace Shader::IR
