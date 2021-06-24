// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <optional>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Shader {

enum class OutputTopology : u32 {
    PointList = 1,
    LineStrip = 6,
    TriangleStrip = 7,
};

enum class PixelImap : u8 {
    Unused = 0,
    Constant = 1,
    Perspective = 2,
    ScreenLinear = 3,
};

// Documentation in:
// http://download.nvidia.com/open-gpu-doc/Shader-Program-Header/1/Shader-Program-Header.html
struct ProgramHeader {
    union {
        BitField<0, 5, u32> sph_type;
        BitField<5, 5, u32> version;
        BitField<10, 4, u32> shader_type;
        BitField<14, 1, u32> mrt_enable;
        BitField<15, 1, u32> kills_pixels;
        BitField<16, 1, u32> does_global_store;
        BitField<17, 4, u32> sass_version;
        BitField<21, 2, u32> reserved1;
        BitField<24, 1, u32> geometry_passthrough;
        BitField<25, 1, u32> reserved2;
        BitField<26, 1, u32> does_load_or_store;
        BitField<27, 1, u32> does_fp64;
        BitField<28, 4, u32> stream_out_mask;
    } common0;

    union {
        BitField<0, 24, u32> shader_local_memory_low_size;
        BitField<24, 8, u32> per_patch_attribute_count;
    } common1;

    union {
        BitField<0, 24, u32> shader_local_memory_high_size;
        BitField<24, 8, u32> threads_per_input_primitive;
    } common2;

    union {
        BitField<0, 24, u32> shader_local_memory_crs_size;
        BitField<24, 4, OutputTopology> output_topology;
        BitField<28, 4, u32> reserved;
    } common3;

    union {
        BitField<0, 12, u32> max_output_vertices;
        BitField<12, 8, u32> store_req_start; // NOTE: not used by geometry shaders.
        BitField<20, 4, u32> reserved;
        BitField<24, 8, u32> store_req_end; // NOTE: not used by geometry shaders.
    } common4;

    union {
        struct {
            INSERT_PADDING_BYTES_NOINIT(3); // ImapSystemValuesA

            union {
                BitField<0, 1, u8> primitive_array_id;
                BitField<1, 1, u8> rt_array_index;
                BitField<2, 1, u8> viewport_index;
                BitField<3, 1, u8> point_size;
                BitField<4, 1, u8> position_x;
                BitField<5, 1, u8> position_y;
                BitField<6, 1, u8> position_z;
                BitField<7, 1, u8> position_w;
                u8 raw;
            } imap_systemb;

            std::array<u8, 16> imap_generic_vector;

            INSERT_PADDING_BYTES_NOINIT(2); // ImapColor
            union {
                BitField<0, 8, u16> clip_distances;
                BitField<8, 1, u16> point_sprite_s;
                BitField<9, 1, u16> point_sprite_t;
                BitField<10, 1, u16> fog_coordinate;
                BitField<12, 1, u16> tessellation_eval_point_u;
                BitField<13, 1, u16> tessellation_eval_point_v;
                BitField<14, 1, u16> instance_id;
                BitField<15, 1, u16> vertex_id;
            };
            INSERT_PADDING_BYTES_NOINIT(5); // ImapFixedFncTexture[10]
            INSERT_PADDING_BYTES_NOINIT(1); // ImapReserved
            INSERT_PADDING_BYTES_NOINIT(3); // OmapSystemValuesA

            union {
                BitField<0, 1, u8> primitive_array_id;
                BitField<1, 1, u8> rt_array_index;
                BitField<2, 1, u8> viewport_index;
                BitField<3, 1, u8> point_size;
                BitField<4, 1, u8> position_x;
                BitField<5, 1, u8> position_y;
                BitField<6, 1, u8> position_z;
                BitField<7, 1, u8> position_w;
                u8 raw;
            } omap_systemb;

            std::array<u8, 16> omap_generic_vector;

            INSERT_PADDING_BYTES_NOINIT(2); // OmapColor

            union {
                BitField<0, 8, u16> clip_distances;
                BitField<8, 1, u16> point_sprite_s;
                BitField<9, 1, u16> point_sprite_t;
                BitField<10, 1, u16> fog_coordinate;
                BitField<12, 1, u16> tessellation_eval_point_u;
                BitField<13, 1, u16> tessellation_eval_point_v;
                BitField<14, 1, u16> instance_id;
                BitField<15, 1, u16> vertex_id;
            } omap_systemc;

            INSERT_PADDING_BYTES_NOINIT(5); // OmapFixedFncTexture[10]
            INSERT_PADDING_BYTES_NOINIT(1); // OmapReserved

            [[nodiscard]] std::array<bool, 4> InputGeneric(size_t index) const noexcept {
                const int data{imap_generic_vector[index >> 1] >> ((index % 2) * 4)};
                return {
                    (data & 1) != 0,
                    (data & 2) != 0,
                    (data & 4) != 0,
                    (data & 8) != 0,
                };
            }

            [[nodiscard]] std::array<bool, 4> OutputGeneric(size_t index) const noexcept {
                const int data{omap_generic_vector[index >> 1] >> ((index % 2) * 4)};
                return {
                    (data & 1) != 0,
                    (data & 2) != 0,
                    (data & 4) != 0,
                    (data & 8) != 0,
                };
            }
        } vtg;

        struct {
            INSERT_PADDING_BYTES_NOINIT(3); // ImapSystemValuesA

            union {
                BitField<0, 1, u8> primitive_array_id;
                BitField<1, 1, u8> rt_array_index;
                BitField<2, 1, u8> viewport_index;
                BitField<3, 1, u8> point_size;
                BitField<4, 1, u8> position_x;
                BitField<5, 1, u8> position_y;
                BitField<6, 1, u8> position_z;
                BitField<7, 1, u8> position_w;
                BitField<0, 4, u8> first;
                BitField<4, 4, u8> position;
                u8 raw;
            } imap_systemb;

            union {
                BitField<0, 2, PixelImap> x;
                BitField<2, 2, PixelImap> y;
                BitField<4, 2, PixelImap> z;
                BitField<6, 2, PixelImap> w;
                u8 raw;
            } imap_generic_vector[32];

            INSERT_PADDING_BYTES_NOINIT(2);  // ImapColor
            INSERT_PADDING_BYTES_NOINIT(2);  // ImapSystemValuesC
            INSERT_PADDING_BYTES_NOINIT(10); // ImapFixedFncTexture[10]
            INSERT_PADDING_BYTES_NOINIT(2);  // ImapReserved

            struct {
                u32 target;
                union {
                    BitField<0, 1, u32> sample_mask;
                    BitField<1, 1, u32> depth;
                    BitField<2, 30, u32> reserved;
                };
            } omap;

            [[nodiscard]] std::array<bool, 4> EnabledOutputComponents(u32 rt) const noexcept {
                const u32 bits{omap.target >> (rt * 4)};
                return {(bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0, (bits & 8) != 0};
            }

            [[nodiscard]] std::array<PixelImap, 4> GenericInputMap(u32 attribute) const {
                const auto& vector{imap_generic_vector[attribute]};
                return {vector.x, vector.y, vector.z, vector.w};
            }

            [[nodiscard]] bool IsGenericVectorActive(size_t index) const {
                return imap_generic_vector[index].raw != 0;
            }
        } ps;

        std::array<u32, 0xf> raw;
    };

    [[nodiscard]] u64 LocalMemorySize() const noexcept {
        return static_cast<u64>(common1.shader_local_memory_low_size) |
               (static_cast<u64>(common2.shader_local_memory_high_size) << 24);
    }
};
static_assert(sizeof(ProgramHeader) == 0x50, "Incorrect structure size");

} // namespace Shader
