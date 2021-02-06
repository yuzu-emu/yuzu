// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <boost/container/small_vector.hpp>

#include "shader_recompiler/frontend/ir/function.h"

namespace Shader::IR {

struct Program {
    boost::container::small_vector<Function, 1> functions;
};

[[nodiscard]] std::string DumpProgram(const Program& program);

} // namespace Shader::IR
