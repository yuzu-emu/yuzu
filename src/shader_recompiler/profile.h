// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <optional>

#include "common/common_types.h"

namespace Shader {

enum class AttributeType : u8 {
    Float,
    SignedInt,
    UnsignedInt,
    Disabled,
};

enum class InputTopology {
    Points,
    Lines,
    LinesAdjacency,
    Triangles,
    TrianglesAdjacency,
};

struct Profile {
    u32 supported_spirv{0x00010000};

    bool unified_descriptor_binding{};
    bool support_vertex_instance_id{};
    bool support_float_controls{};
    bool support_separate_denorm_behavior{};
    bool support_separate_rounding_mode{};
    bool support_fp16_denorm_preserve{};
    bool support_fp32_denorm_preserve{};
    bool support_fp16_denorm_flush{};
    bool support_fp32_denorm_flush{};
    bool support_fp16_signed_zero_nan_preserve{};
    bool support_fp32_signed_zero_nan_preserve{};
    bool support_fp64_signed_zero_nan_preserve{};
    bool support_explicit_workgroup_layout{};
    bool support_vote{};
    bool support_viewport_index_layer_non_geometry{};
    bool support_typeless_image_loads{};
    bool warp_size_potentially_larger_than_guest{};
    bool support_int64_atomics{};

    // FClamp is broken and OpFMax + OpFMin should be used instead
    bool has_broken_spirv_clamp{};

    std::array<AttributeType, 32> generic_input_types{};
    bool convert_depth_mode{};
    bool force_early_z{};

    InputTopology input_topology{};

    std::optional<float> fixed_state_point_size;
};

} // namespace Shader
