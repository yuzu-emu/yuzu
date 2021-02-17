// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>
#include <vector>

#include "common/common_types.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/shader_info.h"

namespace Shader {

[[nodiscard]] std::pair<Info, std::vector<u32>> RecompileSPIRV(Environment& env, u32 start_address);

} // namespace Shader
