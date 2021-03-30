// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/type.h"

#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

namespace Shader {

enum class TextureType : u32 {
    Color1D,
    ColorArray1D,
    Color2D,
    ColorArray2D,
    Color3D,
    ColorCube,
    ColorArrayCube,
    Shadow1D,
    ShadowArray1D,
    Shadow2D,
    ShadowArray2D,
    Shadow3D,
    ShadowCube,
    ShadowArrayCube,
};

enum class Interpolation {
    Smooth,
    Flat,
    NoPerspective,
};

struct InputVarying {
    Interpolation interpolation{Interpolation::Smooth};
    bool used{false};
};

struct TextureDescriptor {
    TextureType type;
    u32 cbuf_index;
    u32 cbuf_offset;
    u32 count;
};
using TextureDescriptors = boost::container::small_vector<TextureDescriptor, 12>;

struct ConstantBufferDescriptor {
    u32 index;
    u32 count;
};

struct StorageBufferDescriptor {
    u32 cbuf_index;
    u32 cbuf_offset;
    u32 count;
};

struct Info {
    static constexpr size_t MAX_CBUFS{18};
    static constexpr size_t MAX_SSBOS{16};

    bool uses_workgroup_id{};
    bool uses_local_invocation_id{};
    bool uses_subgroup_invocation_id{};

    std::array<InputVarying, 32> input_generics{};
    bool loads_position{};
    bool loads_instance_id{};
    bool loads_vertex_id{};
    bool loads_front_face{};
    bool loads_point_coord{};

    std::array<bool, 8> stores_frag_color{};
    bool stores_frag_depth{};
    std::array<bool, 32> stores_generics{};
    bool stores_position{};
    bool stores_point_size{};
    bool stores_clip_distance{};

    bool uses_fp16{};
    bool uses_fp64{};
    bool uses_fp16_denorms_flush{};
    bool uses_fp16_denorms_preserve{};
    bool uses_fp32_denorms_flush{};
    bool uses_fp32_denorms_preserve{};
    bool uses_int8{};
    bool uses_int16{};
    bool uses_int64{};
    bool uses_image_1d{};
    bool uses_sampled_1d{};
    bool uses_sparse_residency{};
    bool uses_demote_to_helper_invocation{};
    bool uses_subgroup_vote{};
    bool uses_fswzadd{};

    IR::Type used_constant_buffer_types{};

    u32 constant_buffer_mask{};

    boost::container::static_vector<ConstantBufferDescriptor, MAX_CBUFS>
        constant_buffer_descriptors;
    boost::container::static_vector<StorageBufferDescriptor, MAX_SSBOS> storage_buffers_descriptors;
    TextureDescriptors texture_descriptors;
};

} // namespace Shader
