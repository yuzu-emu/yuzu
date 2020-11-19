// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "common/common_types.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/surface_params.h"

namespace VideoCommon {

struct ViewParams {
    constexpr explicit ViewParams(VideoCore::Surface::SurfaceTarget target, u32 base_layer,
                                  u32 num_layers, u32 base_level, u32 num_levels)
        : target{target}, base_layer{base_layer}, num_layers{num_layers}, base_level{base_level},
          num_levels{num_levels} {}

    std::size_t Hash() const;

    auto operator<=>(const ViewParams& rhs) const = default;

    bool IsLayered() const {
        switch (target) {
        case VideoCore::Surface::SurfaceTarget::Texture1DArray:
        case VideoCore::Surface::SurfaceTarget::Texture2DArray:
        case VideoCore::Surface::SurfaceTarget::TextureCubemap:
        case VideoCore::Surface::SurfaceTarget::TextureCubeArray:
            return true;
        default:
            return false;
        }
    }

    VideoCore::Surface::SurfaceTarget target{};
    u32 base_layer{};
    u32 num_layers{};
    u32 base_level{};
    u32 num_levels{};
};

class ViewBase {
public:
    constexpr explicit ViewBase(const ViewParams& params) : params{params} {}

    constexpr const ViewParams& GetViewParams() const {
        return params;
    }

protected:
    ViewParams params;
};

} // namespace VideoCommon

namespace std {

template <>
struct hash<VideoCommon::ViewParams> {
    std::size_t operator()(const VideoCommon::ViewParams& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std
