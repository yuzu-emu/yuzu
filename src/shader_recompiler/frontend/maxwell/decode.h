// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"

namespace Shader::Maxwell {

[[nodiscard]] Opcode Decode(u64 insn);

} // namespace Shader::Maxwell
