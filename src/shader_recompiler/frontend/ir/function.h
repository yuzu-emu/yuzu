// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/container/small_vector.hpp>

#include "shader_recompiler/frontend/ir/basic_block.h"

namespace Shader::IR {

struct Function {
    BlockList blocks;
};

} // namespace Shader::IR
