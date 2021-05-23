// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <optional>
#include <vector>

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

enum class CompareFunction {
    Never,
    Less,
    Equal,
    LessThanEqual,
    Greater,
    NotEqual,
    GreaterThanEqual,
    Always,
};

enum class TessPrimitive {
    Isolines,
    Triangles,
    Quads,
};

enum class TessSpacing {
    Equal,
    FractionalOdd,
    FractionalEven,
};

struct TransformFeedbackVarying {
    u32 buffer{};
    u32 stride{};
    u32 offset{};
    u32 components{};
};

struct Profile {
    u32 supported_spirv{0x00010000};

    bool unified_descriptor_binding{};
    bool support_descriptor_aliasing{};
    bool support_int8{};
    bool support_int16{};
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
    bool support_viewport_mask{};
    bool support_typeless_image_loads{};
    bool support_demote_to_helper_invocation{};
    bool warp_size_potentially_larger_than_guest{};
    bool support_int64_atomics{};
    bool lower_left_origin_mode{};

    // FClamp is broken and OpFMax + OpFMin should be used instead
    bool has_broken_spirv_clamp{};
    // Offset image operands with an unsigned type do not work
    bool has_broken_unsigned_image_offsets{};
    // Signed instructions with unsigned data types are misinterpreted
    bool has_broken_signed_operations{};
    // Ignores SPIR-V ordered vs unordered using GLSL semantics
    bool ignore_nan_fp_comparisons{};

    std::array<AttributeType, 32> generic_input_types{};
    bool convert_depth_mode{};
    bool force_early_z{};

    TessPrimitive tess_primitive{};
    TessSpacing tess_spacing{};
    bool tess_clockwise{};

    InputTopology input_topology{};

    std::optional<float> fixed_state_point_size;
    std::optional<CompareFunction> alpha_test_func;
    float alpha_test_reference{};

    bool y_negate{};

    std::vector<TransformFeedbackVarying> xfb_varyings;
};

} // namespace Shader
