// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <vector>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/engines/engine_interface.h"
#include "video_core/engines/engine_upload.h"
#include "video_core/gpu.h"
#include "video_core/textures/texture.h"

namespace Core {
class System;
}

namespace Tegra {
class MemoryManager;
}

namespace VideoCore {
class RasterizerInterface;
}

namespace Tegra::Engines {

/**
 * This Engine is known as GK104_Compute. Documentation can be found in:
 * https://github.com/envytools/envytools/blob/master/rnndb/graph/gk104_compute.xml
 * https://cgit.freedesktop.org/mesa/mesa/tree/src/gallium/drivers/nouveau/nvc0/nve4_compute.xml.h
 */

#define KEPLER_COMPUTE_REG_INDEX(field_name)                                                       \
    (offsetof(Tegra::Engines::KeplerCompute::Regs, field_name) / sizeof(u32))

class KeplerCompute final : public EngineInterface {
public:
    explicit KeplerCompute(Core::System& system, MemoryManager& memory_manager);
    ~KeplerCompute();

    /// Binds a rasterizer to this engine.
    void BindRasterizer(VideoCore::RasterizerInterface* rasterizer);

    static constexpr std::size_t NumConstBuffers = 8;

    struct Regs {
        static constexpr std::size_t NUM_REGS = 0xCF8;

        union {
            struct {
                INSERT_PADDING_WORDS_NOINIT(0x60);

                Upload::Registers upload;

                struct {
                    union {
                        BitField<0, 1, u32> linear;
                    };
                } exec_upload;

                u32 data_upload;

                INSERT_PADDING_WORDS_NOINIT(0x3F);

                struct {
                    u32 address;
                    GPUVAddr Address() const {
                        return static_cast<GPUVAddr>((static_cast<GPUVAddr>(address) << 8));
                    }
                } launch_desc_loc;

                INSERT_PADDING_WORDS_NOINIT(0x1);

                u32 launch;

                INSERT_PADDING_WORDS_NOINIT(0x4A7);

                struct {
                    u32 address_high;
                    u32 address_low;
                    u32 limit;
                    GPUVAddr Address() const {
                        return static_cast<GPUVAddr>((static_cast<GPUVAddr>(address_high) << 32) |
                                                     address_low);
                    }
                } tsc;

                INSERT_PADDING_WORDS_NOINIT(0x3);

                struct {
                    u32 address_high;
                    u32 address_low;
                    u32 limit;
                    GPUVAddr Address() const {
                        return static_cast<GPUVAddr>((static_cast<GPUVAddr>(address_high) << 32) |
                                                     address_low);
                    }
                } tic;

                INSERT_PADDING_WORDS_NOINIT(0x22);

                struct {
                    u32 address_high;
                    u32 address_low;
                    GPUVAddr Address() const {
                        return static_cast<GPUVAddr>((static_cast<GPUVAddr>(address_high) << 32) |
                                                     address_low);
                    }
                } code_loc;

                INSERT_PADDING_WORDS_NOINIT(0x3FE);

                u32 tex_cb_index;

                INSERT_PADDING_WORDS_NOINIT(0x374);
            };
            std::array<u32, NUM_REGS> reg_array;
        };
    } regs{};

    struct LaunchParams {
        static constexpr std::size_t NUM_LAUNCH_PARAMETERS = 0x40;

        INSERT_PADDING_WORDS(0x8);

        u32 program_start;

        INSERT_PADDING_WORDS(0x2);

        BitField<30, 1, u32> linked_tsc;

        BitField<0, 31, u32> grid_dim_x;
        union {
            BitField<0, 16, u32> grid_dim_y;
            BitField<16, 16, u32> grid_dim_z;
        };

        INSERT_PADDING_WORDS(0x3);

        BitField<0, 18, u32> shared_alloc;

        BitField<16, 16, u32> block_dim_x;
        union {
            BitField<0, 16, u32> block_dim_y;
            BitField<16, 16, u32> block_dim_z;
        };

        union {
            BitField<0, 8, u32> const_buffer_enable_mask;
            BitField<29, 2, u32> cache_layout;
        };

        INSERT_PADDING_WORDS(0x8);

        struct ConstBufferConfig {
            u32 address_low;
            union {
                BitField<0, 8, u32> address_high;
                BitField<15, 17, u32> size;
            };
            GPUVAddr Address() const {
                return static_cast<GPUVAddr>((static_cast<GPUVAddr>(address_high.Value()) << 32) |
                                             address_low);
            }
        };
        std::array<ConstBufferConfig, NumConstBuffers> const_buffer_config;

        union {
            BitField<0, 20, u32> local_pos_alloc;
            BitField<27, 5, u32> barrier_alloc;
        };

        union {
            BitField<0, 20, u32> local_neg_alloc;
            BitField<24, 5, u32> gpr_alloc;
        };

        union {
            BitField<0, 20, u32> local_crs_alloc;
            BitField<24, 5, u32> sass_version;
        };

        INSERT_PADDING_WORDS(0x10);
    } launch_description{};

    struct {
        u32 write_offset = 0;
        u32 copy_size = 0;
        std::vector<u8> inner_buffer;
    } state{};

    static_assert(sizeof(Regs) == Regs::NUM_REGS * sizeof(u32),
                  "KeplerCompute Regs has wrong size");

    static_assert(sizeof(LaunchParams) == LaunchParams::NUM_LAUNCH_PARAMETERS * sizeof(u32),
                  "KeplerCompute LaunchParams has wrong size");

    /// Write the value to the register identified by method.
    void CallMethod(u32 method, u32 method_argument, bool is_last_call) override;

    /// Write multiple values to the register identified by method.
    void CallMultiMethod(u32 method, const u32* base_start, u32 amount,
                         u32 methods_pending) override;

private:
    void ProcessLaunch();

    /// Retrieves information about a specific TIC entry from the TIC buffer.
    Texture::TICEntry GetTICEntry(u32 tic_index) const;

    /// Retrieves information about a specific TSC entry from the TSC buffer.
    Texture::TSCEntry GetTSCEntry(u32 tsc_index) const;

    Core::System& system;
    MemoryManager& memory_manager;
    VideoCore::RasterizerInterface* rasterizer = nullptr;
    Upload::State upload_state;
};

#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(KeplerCompute::Regs, field_name) == position * 4,                       \
                  "Field " #field_name " has invalid position")

#define ASSERT_LAUNCH_PARAM_POSITION(field_name, position)                                         \
    static_assert(offsetof(KeplerCompute::LaunchParams, field_name) == position * 4,               \
                  "Field " #field_name " has invalid position")

ASSERT_REG_POSITION(upload, 0x60);
ASSERT_REG_POSITION(exec_upload, 0x6C);
ASSERT_REG_POSITION(data_upload, 0x6D);
ASSERT_REG_POSITION(launch, 0xAF);
ASSERT_REG_POSITION(tsc, 0x557);
ASSERT_REG_POSITION(tic, 0x55D);
ASSERT_REG_POSITION(code_loc, 0x582);
ASSERT_REG_POSITION(tex_cb_index, 0x982);
ASSERT_LAUNCH_PARAM_POSITION(program_start, 0x8);
ASSERT_LAUNCH_PARAM_POSITION(grid_dim_x, 0xC);
ASSERT_LAUNCH_PARAM_POSITION(shared_alloc, 0x11);
ASSERT_LAUNCH_PARAM_POSITION(block_dim_x, 0x12);
ASSERT_LAUNCH_PARAM_POSITION(const_buffer_enable_mask, 0x14);
ASSERT_LAUNCH_PARAM_POSITION(const_buffer_config, 0x1D);

#undef ASSERT_REG_POSITION

} // namespace Tegra::Engines
