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
#include "video_core/gpu.h"

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

class AccelerateDMAInterface {
public:
    /// Write the value to the register identified by method.
    virtual bool BufferCopy(GPUVAddr src_address, GPUVAddr dest_address, u64 amount) = 0;

    virtual bool BufferClear(GPUVAddr src_address, u64 amount, u32 value) = 0;
};

/**
 * This engine is known as gk104_copy. Documentation can be found in:
 * https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/dma-copy/clb0b5.h
 * https://github.com/envytools/envytools/blob/master/rnndb/fifo/gk104_copy.xml
 */

class MaxwellDMA final : public EngineInterface {
public:
    struct PackedGPUVAddr {
        u32 upper;
        u32 lower;

        constexpr operator GPUVAddr() const noexcept {
            return (static_cast<GPUVAddr>(upper & 0xff) << 32) | lower;
        }
    };

    union BlockSize {
        BitField<0, 4, u32> width;
        BitField<4, 4, u32> height;
        BitField<8, 4, u32> depth;
        BitField<12, 4, u32> gob_height;
    };
    static_assert(sizeof(BlockSize) == 4);

    union Origin {
        BitField<0, 16, u32> x;
        BitField<16, 16, u32> y;
    };
    static_assert(sizeof(Origin) == 4);

    struct Parameters {
        BlockSize block_size;
        u32 width;
        u32 height;
        u32 depth;
        u32 layer;
        Origin origin;
    };
    static_assert(sizeof(Parameters) == 24);

    struct Semaphore {
        PackedGPUVAddr address;
        u32 payload;
    };
    static_assert(sizeof(Semaphore) == 12);

    struct RenderEnable {
        enum class Mode : u32 {
            // Note: This uses Pascal case in order to avoid the identifiers
            // FALSE and TRUE, which are reserved on Darwin.
            False = 0,
            True = 1,
            Conditional = 2,
            RenderIfEqual = 3,
            RenderIfNotEqual = 4,
        };

        PackedGPUVAddr address;
        BitField<0, 3, Mode> mode;
    };
    static_assert(sizeof(RenderEnable) == 12);

    enum class PhysModeTarget : u32 {
        LOCAL_FB = 0,
        COHERENT_SYSMEM = 1,
        NONCOHERENT_SYSMEM = 2,
    };
    using PhysMode = BitField<0, 2, PhysModeTarget>;

    union LaunchDMA {
        enum class DataTransferType : u32 {
            NONE = 0,
            PIPELINED = 1,
            NON_PIPELINED = 2,
        };

        enum class SemaphoreType : u32 {
            NONE = 0,
            RELEASE_ONE_WORD_SEMAPHORE = 1,
            RELEASE_FOUR_WORD_SEMAPHORE = 2,
        };

        enum class InterruptType : u32 {
            NONE = 0,
            BLOCKING = 1,
            NON_BLOCKING = 2,
        };

        enum class MemoryLayout : u32 {
            BLOCKLINEAR = 0,
            PITCH = 1,
        };

        enum class Type : u32 {
            VIRTUAL = 0,
            PHYSICAL = 1,
        };

        enum class SemaphoreReduction : u32 {
            IMIN = 0,
            IMAX = 1,
            IXOR = 2,
            IAND = 3,
            IOR = 4,
            IADD = 5,
            INC = 6,
            DEC = 7,
            FADD = 0xA,
        };

        enum class SemaphoreReductionSign : u32 {
            SIGNED = 0,
            UNSIGNED = 1,
        };

        enum class BypassL2 : u32 {
            USE_PTE_SETTING = 0,
            FORCE_VOLATILE = 1,
        };

        BitField<0, 2, DataTransferType> data_transfer_type;
        BitField<2, 1, u32> flush_enable;
        BitField<3, 2, SemaphoreType> semaphore_type;
        BitField<5, 2, InterruptType> interrupt_type;
        BitField<7, 1, MemoryLayout> src_memory_layout;
        BitField<8, 1, MemoryLayout> dst_memory_layout;
        BitField<9, 1, u32> multi_line_enable;
        BitField<10, 1, u32> remap_enable;
        BitField<11, 1, u32> rmwdisable;
        BitField<12, 1, Type> src_type;
        BitField<13, 1, Type> dst_type;
        BitField<14, 4, SemaphoreReduction> semaphore_reduction;
        BitField<18, 1, SemaphoreReductionSign> semaphore_reduction_sign;
        BitField<19, 1, u32> reduction_enable;
        BitField<20, 1, BypassL2> bypass_l2;
    };
    static_assert(sizeof(LaunchDMA) == 4);

    struct RemapConst {
        enum Swizzle : u32 {
            SRC_X = 0,
            SRC_Y = 1,
            SRC_Z = 2,
            SRC_W = 3,
            CONST_A = 4,
            CONST_B = 5,
            NO_WRITE = 6,
        };

        PackedGPUVAddr address;

        union {
            BitField<0, 3, Swizzle> dst_x;
            BitField<4, 3, Swizzle> dst_y;
            BitField<8, 3, Swizzle> dst_z;
            BitField<12, 3, Swizzle> dst_w;
            BitField<16, 2, u32> component_size_minus_one;
            BitField<20, 2, u32> num_src_components_minus_one;
            BitField<24, 2, u32> num_dst_components_minus_one;
        };
    };
    static_assert(sizeof(RemapConst) == 12);

    void BindRasterizer(VideoCore::RasterizerInterface* rasterizer);

    explicit MaxwellDMA(Core::System& system_, MemoryManager& memory_manager_);
    ~MaxwellDMA() override;

    /// Write the value to the register identified by method.
    void CallMethod(u32 method, u32 method_argument, bool is_last_call) override;

    /// Write multiple values to the register identified by method.
    void CallMultiMethod(u32 method, const u32* base_start, u32 amount,
                         u32 methods_pending) override;

private:
    /// Performs the copy from the source buffer to the destination buffer as configured in the
    /// registers.
    void Launch();

    void CopyPitchToPitch();

    void CopyBlockLinearToPitch();

    void CopyPitchToBlockLinear();

    void FastCopyBlockLinearToPitch();

    Core::System& system;

    MemoryManager& memory_manager;
    VideoCore::RasterizerInterface* rasterizer;

    std::vector<u8> read_buffer;
    std::vector<u8> write_buffer;

    static constexpr std::size_t NUM_REGS = 0x800;
    struct Regs {
        union {
            struct {
                u32 reserved[0x40];
                u32 nop;
                u32 reserved01[0xf];
                u32 pm_trigger;
                u32 reserved02[0x3f];
                Semaphore semaphore;
                u32 reserved03[0x2];
                RenderEnable render_enable;
                PhysMode src_phys_mode;
                PhysMode dst_phys_mode;
                u32 reserved04[0x26];
                LaunchDMA launch_dma;
                u32 reserved05[0x3f];
                PackedGPUVAddr offset_in;
                PackedGPUVAddr offset_out;
                u32 pitch_in;
                u32 pitch_out;
                u32 line_length_in;
                u32 line_count;
                u32 reserved06[0xb6];
                u32 remap_consta_value;
                u32 remap_constb_value;
                RemapConst remap_const;
                Parameters dst_params;
                u32 reserved07[0x1];
                Parameters src_params;
                u32 reserved08[0x275];
                u32 pm_trigger_end;
                u32 reserved09[0x3ba];
            };
            std::array<u32, NUM_REGS> reg_array;
        };
    } regs{};

#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(MaxwellDMA::Regs, field_name) == position * 4,                          \
                  "Field " #field_name " has invalid position")

    ASSERT_REG_POSITION(launch_dma, 0xC0);
    ASSERT_REG_POSITION(offset_in, 0x100);
    ASSERT_REG_POSITION(offset_out, 0x102);
    ASSERT_REG_POSITION(pitch_in, 0x104);
    ASSERT_REG_POSITION(pitch_out, 0x105);
    ASSERT_REG_POSITION(line_length_in, 0x106);
    ASSERT_REG_POSITION(line_count, 0x107);
    ASSERT_REG_POSITION(remap_const, 0x1C0);
    ASSERT_REG_POSITION(dst_params, 0x1C3);
    ASSERT_REG_POSITION(src_params, 0x1CA);

#undef ASSERT_REG_POSITION
};

} // namespace Tegra::Engines
