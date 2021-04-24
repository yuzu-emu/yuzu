// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"

namespace VideoCommon::Dirty {

enum : u8 {
    NullEntry = 0,

    Descriptors,

    RenderTargets,
    RenderTargetControl,
    ColorBuffer0,
    ColorBuffer1,
    ColorBuffer2,
    ColorBuffer3,
    ColorBuffer4,
    ColorBuffer5,
    ColorBuffer6,
    ColorBuffer7,
    ZetaBuffer,

    VertexBuffers,
    VertexBuffer0,
    VertexBuffer31 = VertexBuffer0 + 31,

    IndexBuffer,

    Shaders,

    LastCommonEntry,
};

template <typename Integer>
void FillBlock(Tegra::Engines::Maxwell3D::DirtyState::Table& table, std::size_t begin,
               std::size_t num, Integer dirty_index) {
    const auto it = std::begin(table) + begin;
    std::fill(it, it + num, static_cast<u8>(dirty_index));
}

template <typename Integer1, typename Integer2>
void FillBlock(Tegra::Engines::Maxwell3D::DirtyState::Tables& tables, std::size_t begin,
               std::size_t num, Integer1 index_a, Integer2 index_b) {
    FillBlock(tables[0], begin, num, index_a);
    FillBlock(tables[1], begin, num, index_b);
}

void SetupDirtyFlags(Tegra::Engines::Maxwell3D::DirtyState::Tables& tables);

} // namespace VideoCommon::Dirty
