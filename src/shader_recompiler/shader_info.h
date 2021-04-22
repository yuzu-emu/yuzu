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
    Buffer,
};
constexpr u32 NUM_TEXTURE_TYPES = 8;

enum class ImageFormat : u32 {
    Typeless,
    R8_UINT,
    R8_SINT,
    R16_UINT,
    R16_SINT,
    R32_UINT,
    R32G32_UINT,
    R32G32B32A32_UINT,
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

struct ConstantBufferDescriptor {
    u32 index;
    u32 count;
};

struct StorageBufferDescriptor {
    u32 cbuf_index;
    u32 cbuf_offset;
    u32 count;
    bool is_written;
};

struct TextureBufferDescriptor {
    bool has_secondary;
    u32 cbuf_index;
    u32 cbuf_offset;
    u32 secondary_cbuf_index;
    u32 secondary_cbuf_offset;
    u32 count;
    u32 size_shift;
};
using TextureBufferDescriptors = boost::container::small_vector<TextureBufferDescriptor, 6>;

struct ImageBufferDescriptor {
    ImageFormat format;
    bool is_written;
    u32 cbuf_index;
    u32 cbuf_offset;
    u32 count;
    u32 size_shift;
};
using ImageBufferDescriptors = boost::container::small_vector<ImageBufferDescriptor, 2>;

struct TextureDescriptor {
    TextureType type;
    bool is_depth;
    bool has_secondary;
    u32 cbuf_index;
    u32 cbuf_offset;
    u32 secondary_cbuf_index;
    u32 secondary_cbuf_offset;
    u32 count;
    u32 size_shift;
};
using TextureDescriptors = boost::container::small_vector<TextureDescriptor, 12>;

struct ImageDescriptor {
    TextureType type;
    ImageFormat format;
    bool is_written;
    u32 cbuf_index;
    u32 cbuf_offset;
    u32 count;
    u32 size_shift;
};
using ImageDescriptors = boost::container::small_vector<ImageDescriptor, 4>;

struct Info {
    static constexpr size_t MAX_CBUFS{18};
    static constexpr size_t MAX_SSBOS{32};

    bool uses_workgroup_id{};
    bool uses_local_invocation_id{};
    bool uses_invocation_id{};
    bool uses_sample_id{};
    bool uses_is_helper_invocation{};
    bool uses_subgroup_invocation_id{};
    std::array<bool, 30> uses_patches{};

    std::array<InputVarying, 32> input_generics{};
    bool loads_primitive_id{};
    bool loads_position{};
    bool loads_instance_id{};
    bool loads_vertex_id{};
    bool loads_front_face{};
    bool loads_point_coord{};
    bool loads_tess_coord{};
    bool loads_indexed_attributes{};

    std::array<bool, 8> stores_frag_color{};
    bool stores_sample_mask{};
    bool stores_frag_depth{};
    std::array<bool, 32> stores_generics{};
    bool stores_position{};
    bool stores_point_size{};
    bool stores_clip_distance{};
    bool stores_layer{};
    bool stores_viewport_index{};
    bool stores_viewport_mask{};
    bool stores_tess_level_outer{};
    bool stores_tess_level_inner{};
    bool stores_indexed_attributes{};

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
    bool uses_subgroup_mask{};
    bool uses_fswzadd{};
    bool uses_derivatives{};
    bool uses_typeless_image_reads{};
    bool uses_typeless_image_writes{};
    bool uses_shared_increment{};
    bool uses_shared_decrement{};
    bool uses_global_increment{};
    bool uses_global_decrement{};
    bool uses_atomic_f32_add{};
    bool uses_atomic_f16x2_add{};
    bool uses_atomic_f16x2_min{};
    bool uses_atomic_f16x2_max{};
    bool uses_atomic_f32x2_add{};
    bool uses_atomic_f32x2_min{};
    bool uses_atomic_f32x2_max{};
    bool uses_int64_bit_atomics{};
    bool uses_global_memory{};

    IR::Type used_constant_buffer_types{};
    IR::Type used_storage_buffer_types{};

    u32 constant_buffer_mask{};

    boost::container::static_vector<ConstantBufferDescriptor, MAX_CBUFS>
        constant_buffer_descriptors;
    boost::container::static_vector<StorageBufferDescriptor, MAX_SSBOS> storage_buffers_descriptors;
    TextureBufferDescriptors texture_buffer_descriptors;
    ImageBufferDescriptors image_buffer_descriptors;
    TextureDescriptors texture_descriptors;
    ImageDescriptors image_descriptors;
};

} // namespace Shader
