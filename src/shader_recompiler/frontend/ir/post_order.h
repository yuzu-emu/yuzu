// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "shader_recompiler/frontend/ir/abstract_syntax_list.h"
#include "shader_recompiler/frontend/ir/basic_block.h"

namespace Shader::IR {

BlockList PostOrder(const AbstractSyntaxNode& root);

} // namespace Shader::IR
