// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <limits>

#include "common/common_types.h"
#include "core/core.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/maxwell_3d.h"

namespace Vulkan {

namespace Dirty {

enum : u8 {
    First = VideoCommon::Dirty::LastCommonEntry,

    VertexInput,
    VertexAttribute0,
    VertexAttribute31 = VertexAttribute0 + 31,
    VertexBinding0,
    VertexBinding31 = VertexBinding0 + 31,

    Viewports,
    Scissors,
    DepthBias,
    BlendConstants,
    DepthBounds,
    StencilProperties,
    LineWidth,

    CullMode,
    DepthBoundsEnable,
    DepthTestEnable,
    DepthWriteEnable,
    DepthCompareOp,
    FrontFace,
    StencilOp,
    StencilTestEnable,

    Blending,
    ViewportSwizzles,

    Last,
};
static_assert(Last <= std::numeric_limits<u8>::max());

} // namespace Dirty

class StateTracker {
    using Maxwell = Tegra::Engines::Maxwell3D::Regs;

public:
    explicit StateTracker(Tegra::GPU& gpu);

    void InvalidateCommandBufferState() {
        flags |= invalidation_flags;
        current_topology = INVALID_TOPOLOGY;
    }

    void InvalidateViewports() {
        flags[Dirty::Viewports] = true;
    }

    void InvalidateScissors() {
        flags[Dirty::Scissors] = true;
    }

    bool TouchViewports() {
        return Exchange(Dirty::Viewports, false);
    }

    bool TouchScissors() {
        return Exchange(Dirty::Scissors, false);
    }

    bool TouchDepthBias() {
        return Exchange(Dirty::DepthBias, false);
    }

    bool TouchBlendConstants() {
        return Exchange(Dirty::BlendConstants, false);
    }

    bool TouchDepthBounds() {
        return Exchange(Dirty::DepthBounds, false);
    }

    bool TouchStencilProperties() {
        return Exchange(Dirty::StencilProperties, false);
    }

    bool TouchLineWidth() const {
        return Exchange(Dirty::LineWidth, false);
    }

    bool TouchCullMode() {
        return Exchange(Dirty::CullMode, false);
    }

    bool TouchDepthBoundsTestEnable() {
        return Exchange(Dirty::DepthBoundsEnable, false);
    }

    bool TouchDepthTestEnable() {
        return Exchange(Dirty::DepthTestEnable, false);
    }

    bool TouchDepthBoundsEnable() {
        return Exchange(Dirty::DepthBoundsEnable, false);
    }

    bool TouchDepthWriteEnable() {
        return Exchange(Dirty::DepthWriteEnable, false);
    }

    bool TouchDepthCompareOp() {
        return Exchange(Dirty::DepthCompareOp, false);
    }

    bool TouchFrontFace() {
        return Exchange(Dirty::FrontFace, false);
    }

    bool TouchStencilOp() {
        return Exchange(Dirty::StencilOp, false);
    }

    bool TouchStencilTestEnable() {
        return Exchange(Dirty::StencilTestEnable, false);
    }

    bool ChangePrimitiveTopology(Maxwell::PrimitiveTopology new_topology) {
        const bool has_changed = current_topology != new_topology;
        current_topology = new_topology;
        return has_changed;
    }

private:
    static constexpr auto INVALID_TOPOLOGY = static_cast<Maxwell::PrimitiveTopology>(~0u);

    bool Exchange(std::size_t id, bool new_value) const noexcept {
        const bool is_dirty = flags[id];
        flags[id] = new_value;
        return is_dirty;
    }

    Tegra::Engines::Maxwell3D::DirtyState::Flags& flags;
    Tegra::Engines::Maxwell3D::DirtyState::Flags invalidation_flags;
    Maxwell::PrimitiveTopology current_topology = INVALID_TOPOLOGY;
};

} // namespace Vulkan
