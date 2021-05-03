// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Shader::Backend {

struct Bindings {
    u32 unified{};
    u32 uniform_buffer{};
    u32 storage_buffer{};
    u32 texture{};
    u32 image{};
};

} // namespace Shader::Backend
