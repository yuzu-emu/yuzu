// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/transform_feedback.h"

namespace VideoCommon {

std::vector<Shader::TransformFeedbackVarying> MakeTransformFeedbackVaryings(
    const TransformFeedbackState& state) {
    static constexpr std::array VECTORS{
        28,  // gl_Position
        32,  // Generic 0
        36,  // Generic 1
        40,  // Generic 2
        44,  // Generic 3
        48,  // Generic 4
        52,  // Generic 5
        56,  // Generic 6
        60,  // Generic 7
        64,  // Generic 8
        68,  // Generic 9
        72,  // Generic 10
        76,  // Generic 11
        80,  // Generic 12
        84,  // Generic 13
        88,  // Generic 14
        92,  // Generic 15
        96,  // Generic 16
        100, // Generic 17
        104, // Generic 18
        108, // Generic 19
        112, // Generic 20
        116, // Generic 21
        120, // Generic 22
        124, // Generic 23
        128, // Generic 24
        132, // Generic 25
        136, // Generic 26
        140, // Generic 27
        144, // Generic 28
        148, // Generic 29
        152, // Generic 30
        156, // Generic 31
        160, // gl_FrontColor
        164, // gl_FrontSecondaryColor
        160, // gl_BackColor
        164, // gl_BackSecondaryColor
        192, // gl_TexCoord[0]
        196, // gl_TexCoord[1]
        200, // gl_TexCoord[2]
        204, // gl_TexCoord[3]
        208, // gl_TexCoord[4]
        212, // gl_TexCoord[5]
        216, // gl_TexCoord[6]
        220, // gl_TexCoord[7]
    };
    std::vector<Shader::TransformFeedbackVarying> xfb(256);
    for (size_t buffer = 0; buffer < state.layouts.size(); ++buffer) {
        const auto& locations = state.varyings[buffer];
        const auto& layout = state.layouts[buffer];
        const u32 varying_count = layout.varying_count;
        u32 highest = 0;
        for (u32 offset = 0; offset < varying_count; ++offset) {
            const u32 base_offset = offset;
            const u8 location = locations[offset];

            UNIMPLEMENTED_IF_MSG(layout.stream != 0, "Stream is not zero: {}", layout.stream);
            Shader::TransformFeedbackVarying varying{
                .buffer = static_cast<u32>(buffer),
                .stride = layout.stride,
                .offset = offset * 4,
                .components = 1,
            };
            if (std::ranges::find(VECTORS, Common::AlignDown(location, 4)) != VECTORS.end()) {
                UNIMPLEMENTED_IF_MSG(location % 4 != 0, "Unaligned TFB");

                const u8 base_index = location / 4;
                while (offset + 1 < varying_count && base_index == locations[offset + 1] / 4) {
                    ++offset;
                    ++varying.components;
                }
            }
            xfb[location] = varying;
            highest = std::max(highest, (base_offset + varying.components) * 4);
        }
        UNIMPLEMENTED_IF(highest != layout.stride);
    }
    return xfb;
}

} // namespace VideoCommon
