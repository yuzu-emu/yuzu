// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Shader {

struct Profile {
    u32 supported_spirv{0x00010000};

    bool unified_descriptor_binding{};
    bool support_descriptor_aliasing{};
    bool support_int8{};
    bool support_int16{};
    bool support_int64{};
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
    bool support_int64_atomics{};
    bool support_derivative_control{};
    bool support_geometry_shader_passthrough{};
    bool support_gl_nv_gpu_shader_5{};
    bool support_gl_amd_gpu_shader_half_float{};
    bool support_gl_texture_shadow_lod{};
    bool support_gl_warp_intrinsics{};
    bool support_gl_variable_aoffi{};
    bool support_gl_sparse_textures{};
    bool support_gl_derivative_control{};

    bool warp_size_potentially_larger_than_guest{};

    bool lower_left_origin_mode{};
    /// Fragment outputs have to be declared even if they are not written to avoid undefined values.
    /// See Ori and the Blind Forest's main menu for reference.
    bool need_declared_frag_colors{};
    /// Prevents fast math optimizations that may cause inaccuracies
    bool need_fastmath_off{};

    /// OpFClamp is broken and OpFMax + OpFMin should be used instead
    bool has_broken_spirv_clamp{};
    /// Offset image operands with an unsigned type do not work
    bool has_broken_unsigned_image_offsets{};
    /// Signed instructions with unsigned data types are misinterpreted
    bool has_broken_signed_operations{};
    /// Float controls break when fp16 is enabled
    bool has_broken_fp16_float_controls{};
    /// Dynamic vec4 indexing is broken on some OpenGL drivers
    bool has_gl_component_indexing_bug{};
    /// The precise type qualifier is broken in the fragment stage of some drivers
    bool has_gl_precise_bug{};
    /// Ignores SPIR-V ordered vs unordered using GLSL semantics
    bool ignore_nan_fp_comparisons{};
};

} // namespace Shader
