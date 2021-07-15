// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Shader {

// Try to keep entries here to a minimum
// They can accidentally change the cached information in a shader

/// Misc information about the host
struct HostTranslateInfo {
    bool support_float16{}; ///< True when the device supports 16-bit floats
    bool support_int64{};   ///< True when the device supports 64-bit integers
};

} // namespace Shader
