// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <span>

#include <boost/intrusive/list.hpp>

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/object_pool.h"

namespace Shader::Maxwell {

[[nodiscard]] IR::BlockList VisitAST(ObjectPool<IR::Inst>& inst_pool,
                                     ObjectPool<IR::Block>& block_pool, Environment& env,
                                     Flow::CFG& cfg);

} // namespace Shader::Maxwell
