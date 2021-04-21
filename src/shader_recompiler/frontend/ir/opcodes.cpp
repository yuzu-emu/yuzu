// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/frontend/ir/opcodes.h"

namespace Shader::IR {

std::string_view NameOf(Opcode op) {
    return Detail::META_TABLE[static_cast<size_t>(op)].name;
}

} // namespace Shader::IR
