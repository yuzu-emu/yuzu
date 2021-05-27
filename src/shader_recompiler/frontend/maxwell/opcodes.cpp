// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"

namespace Shader::Maxwell {
namespace {
constexpr std::array NAME_TABLE{
#define INST(name, cute, encode) cute,
#include "maxwell.inc"
#undef INST
};
} // Anonymous namespace

const char* NameOf(Opcode opcode) {
    if (static_cast<size_t>(opcode) >= NAME_TABLE.size()) {
        throw InvalidArgument("Invalid opcode with raw value {}", static_cast<int>(opcode));
    }
    return NAME_TABLE[static_cast<size_t>(opcode)];
}

} // namespace Shader::Maxwell
