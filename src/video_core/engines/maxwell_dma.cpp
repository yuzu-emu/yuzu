// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/maxwell_dma.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_base.h"
#include "video_core/textures/decoders.h"

namespace Tegra::Engines {

using namespace Texture;

MaxwellDMA::MaxwellDMA(Core::System& system_, MemoryManager& memory_manager_)
    : system{system_}, memory_manager{memory_manager_} {}

MaxwellDMA::~MaxwellDMA() = default;

void MaxwellDMA::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

void MaxwellDMA::CallMethod(u32 method, u32 method_argument, bool is_last_call) {
    ASSERT_MSG(method < NUM_REGS, "Invalid MaxwellDMA register");

    regs.reg_array[method] = method_argument;

    if (method == offsetof(Regs, launch_dma) / sizeof(u32)) {
        Launch();
    }
}

void MaxwellDMA::CallMultiMethod(u32 method, const u32* base_start, u32 amount,
                                 u32 methods_pending) {
    for (size_t i = 0; i < amount; ++i) {
        CallMethod(method, base_start[i], methods_pending - static_cast<u32>(i) <= 1);
    }
}

void MaxwellDMA::Launch() {
    LOG_TRACE(Render_OpenGL, "DMA copy 0x{:x} -> 0x{:x}", static_cast<GPUVAddr>(regs.offset_in),
              static_cast<GPUVAddr>(regs.offset_out));

    // TODO(Subv): Perform more research and implement all features of this engine.
    const LaunchDMA& launch = regs.launch_dma;
    ASSERT(launch.semaphore_type == LaunchDMA::SemaphoreType::NONE);
    ASSERT(launch.interrupt_type == LaunchDMA::InterruptType::NONE);
    ASSERT(launch.data_transfer_type == LaunchDMA::DataTransferType::NON_PIPELINED);
    ASSERT(regs.dst_params.origin.x == 0);
    ASSERT(regs.dst_params.origin.y == 0);

    const bool is_src_pitch = launch.src_memory_layout == LaunchDMA::MemoryLayout::PITCH;
    const bool is_dst_pitch = launch.dst_memory_layout == LaunchDMA::MemoryLayout::PITCH;

    if (!is_src_pitch && !is_dst_pitch) {
        // If both the source and the destination are in block layout, assert.
        UNREACHABLE_MSG("Tiled->Tiled DMA transfers are not yet implemented");
        return;
    }

    if (is_src_pitch && is_dst_pitch) {
        CopyPitchToPitch();
    } else {
        ASSERT(launch.multi_line_enable == 1);

        if (!is_src_pitch && is_dst_pitch) {
            CopyBlockLinearToPitch();
        } else {
            CopyPitchToBlockLinear();
        }
    }
}

void MaxwellDMA::CopyPitchToPitch() {
    // When `multi_line_enable` bit is disabled the copy is performed as if we were copying a 1D
    // buffer of length `line_length_in`.
    // Otherwise we copy a 2D image of dimensions (line_length_in, line_count).
    auto& accelerate = rasterizer->AccessAccelerateDMA();
    if (!regs.launch_dma.multi_line_enable) {
        const bool is_buffer_clear = regs.launch_dma.remap_enable != 0 &&
                                     regs.remap_const.dst_x == RemapConst::Swizzle::CONST_A;
        // TODO: allow multisized components.
        if (is_buffer_clear) {
            ASSERT(regs.remap_const.component_size_minus_one == 3);
            std::vector<u32> tmp_buffer(regs.line_length_in, regs.remap_consta_value);
            memory_manager.WriteBlock(regs.offset_out, reinterpret_cast<u8*>(tmp_buffer.data()),
                                      regs.line_length_in * sizeof(u32));
            return;
        }
        UNIMPLEMENTED_IF(regs.launch_dma.remap_enable != 0);
        if (!accelerate.BufferCopy(regs.offset_in, regs.offset_out, regs.line_length_in)) {
            std::vector<u8> tmp_buffer(regs.line_length_in);
            memory_manager.ReadBlockUnsafe(regs.offset_in, tmp_buffer.data(), regs.line_length_in);
            memory_manager.WriteBlock(regs.offset_out, tmp_buffer.data(), regs.line_length_in);
        }
        return;
    }

    UNIMPLEMENTED_IF(regs.launch_dma.remap_enable != 0);

    // Perform a line-by-line copy.
    // We're going to take a subrect of size (line_length_in, line_count) from the source rectangle.
    // There is no need to manually flush/invalidate the regions because CopyBlock does that for us.
    for (u32 line = 0; line < regs.line_count; ++line) {
        const GPUVAddr source_line = regs.offset_in + static_cast<size_t>(line) * regs.pitch_in;
        const GPUVAddr dest_line = regs.offset_out + static_cast<size_t>(line) * regs.pitch_out;
        memory_manager.CopyBlock(dest_line, source_line, regs.line_length_in);
    }
}

void MaxwellDMA::CopyBlockLinearToPitch() {
    UNIMPLEMENTED_IF(regs.src_params.block_size.width != 0);
    UNIMPLEMENTED_IF(regs.src_params.block_size.depth != 0);
    UNIMPLEMENTED_IF(regs.src_params.layer != 0);

    // Optimized path for micro copies.
    const size_t dst_size = static_cast<size_t>(regs.pitch_out) * regs.line_count;
    if (dst_size < GOB_SIZE && regs.pitch_out <= GOB_SIZE_X &&
        regs.src_params.height > GOB_SIZE_Y) {
        FastCopyBlockLinearToPitch();
        return;
    }

    // Deswizzle the input and copy it over.
    UNIMPLEMENTED_IF(regs.launch_dma.remap_enable != 0);
    const u32 bytes_per_pixel = regs.pitch_out / regs.line_length_in;
    const Parameters& src_params = regs.src_params;
    const u32 width = src_params.width;
    const u32 height = src_params.height;
    const u32 depth = src_params.depth;
    const u32 block_height = src_params.block_size.height;
    const u32 block_depth = src_params.block_size.depth;
    const size_t src_size =
        CalculateSize(true, bytes_per_pixel, width, height, depth, block_height, block_depth);

    if (read_buffer.size() < src_size) {
        read_buffer.resize(src_size);
    }
    if (write_buffer.size() < dst_size) {
        write_buffer.resize(dst_size);
    }

    memory_manager.ReadBlock(regs.offset_in, read_buffer.data(), src_size);
    memory_manager.ReadBlock(regs.offset_out, write_buffer.data(), dst_size);

    UnswizzleSubrect(regs.line_length_in, regs.line_count, regs.pitch_out, width, bytes_per_pixel,
                     block_height, src_params.origin.x, src_params.origin.y, write_buffer.data(),
                     read_buffer.data());

    memory_manager.WriteBlock(regs.offset_out, write_buffer.data(), dst_size);
}

void MaxwellDMA::CopyPitchToBlockLinear() {
    UNIMPLEMENTED_IF_MSG(regs.dst_params.block_size.width != 0, "Block width is not one");
    UNIMPLEMENTED_IF(regs.launch_dma.remap_enable != 0);

    const auto& dst_params = regs.dst_params;
    const u32 bytes_per_pixel = regs.pitch_in / regs.line_length_in;
    const u32 width = dst_params.width;
    const u32 height = dst_params.height;
    const u32 depth = dst_params.depth;
    const u32 block_height = dst_params.block_size.height;
    const u32 block_depth = dst_params.block_size.depth;
    const size_t dst_size =
        CalculateSize(true, bytes_per_pixel, width, height, depth, block_height, block_depth);
    const size_t dst_layer_size =
        CalculateSize(true, bytes_per_pixel, width, height, 1, block_height, block_depth);

    const size_t src_size = static_cast<size_t>(regs.pitch_in) * regs.line_count;

    if (read_buffer.size() < src_size) {
        read_buffer.resize(src_size);
    }
    if (write_buffer.size() < dst_size) {
        write_buffer.resize(dst_size);
    }

    memory_manager.ReadBlock(regs.offset_in, read_buffer.data(), src_size);
    memory_manager.ReadBlock(regs.offset_out, write_buffer.data(), dst_size);

    // If the input is linear and the output is tiled, swizzle the input and copy it over.
    if (regs.dst_params.block_size.depth > 0) {
        ASSERT(dst_params.layer == 0);
        SwizzleSliceToVoxel(regs.line_length_in, regs.line_count, regs.pitch_in, width, height,
                            bytes_per_pixel, block_height, block_depth, dst_params.origin.x,
                            dst_params.origin.y, write_buffer.data(), read_buffer.data());
    } else {
        SwizzleSubrect(regs.line_length_in, regs.line_count, regs.pitch_in, width, bytes_per_pixel,
                       write_buffer.data() + dst_layer_size * dst_params.layer, read_buffer.data(),
                       block_height, dst_params.origin.x, dst_params.origin.y);
    }

    memory_manager.WriteBlock(regs.offset_out, write_buffer.data(), dst_size);
}

void MaxwellDMA::FastCopyBlockLinearToPitch() {
    const u32 bytes_per_pixel = regs.pitch_out / regs.line_length_in;
    const size_t src_size = GOB_SIZE;
    const size_t dst_size = static_cast<size_t>(regs.pitch_out) * regs.line_count;
    u32 pos_x = regs.src_params.origin.x;
    u32 pos_y = regs.src_params.origin.y;
    const u64 offset = GetGOBOffset(regs.src_params.width, regs.src_params.height, pos_x, pos_y,
                                    regs.src_params.block_size.height, bytes_per_pixel);
    const u32 x_in_gob = 64 / bytes_per_pixel;
    pos_x = pos_x % x_in_gob;
    pos_y = pos_y % 8;

    if (read_buffer.size() < src_size) {
        read_buffer.resize(src_size);
    }
    if (write_buffer.size() < dst_size) {
        write_buffer.resize(dst_size);
    }

    if (Settings::IsGPULevelExtreme()) {
        memory_manager.ReadBlock(regs.offset_in + offset, read_buffer.data(), src_size);
        memory_manager.ReadBlock(regs.offset_out, write_buffer.data(), dst_size);
    } else {
        memory_manager.ReadBlockUnsafe(regs.offset_in + offset, read_buffer.data(), src_size);
        memory_manager.ReadBlockUnsafe(regs.offset_out, write_buffer.data(), dst_size);
    }

    UnswizzleSubrect(regs.line_length_in, regs.line_count, regs.pitch_out, regs.src_params.width,
                     bytes_per_pixel, regs.src_params.block_size.height, pos_x, pos_y,
                     write_buffer.data(), read_buffer.data());

    memory_manager.WriteBlock(regs.offset_out, write_buffer.data(), dst_size);
}

} // namespace Tegra::Engines
