// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"

namespace Shader::Maxwell {

/// Emit termination instructions and collect immediate predecessors
void EmitTerminationCode(const Flow::Block& flow_block, std::span<IR::Block* const> block_map);

} // namespace Shader::Maxwell
