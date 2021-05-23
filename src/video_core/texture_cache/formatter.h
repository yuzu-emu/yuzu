// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <fmt/format.h>

#include "video_core/surface.h"
#include "video_core/texture_cache/types.h"

template <>
struct fmt::formatter<VideoCore::Surface::PixelFormat> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(VideoCore::Surface::PixelFormat format, FormatContext& ctx) {
        using VideoCore::Surface::PixelFormat;
        const string_view name = [format] {
            switch (format) {
            case PixelFormat::A8B8G8R8_UNORM:
                return "A8B8G8R8_UNORM";
            case PixelFormat::A8B8G8R8_SNORM:
                return "A8B8G8R8_SNORM";
            case PixelFormat::A8B8G8R8_SINT:
                return "A8B8G8R8_SINT";
            case PixelFormat::A8B8G8R8_UINT:
                return "A8B8G8R8_UINT";
            case PixelFormat::R5G6B5_UNORM:
                return "R5G6B5_UNORM";
            case PixelFormat::B5G6R5_UNORM:
                return "B5G6R5_UNORM";
            case PixelFormat::A1R5G5B5_UNORM:
                return "A1R5G5B5_UNORM";
            case PixelFormat::A2B10G10R10_UNORM:
                return "A2B10G10R10_UNORM";
            case PixelFormat::A2B10G10R10_UINT:
                return "A2B10G10R10_UINT";
            case PixelFormat::A1B5G5R5_UNORM:
                return "A1B5G5R5_UNORM";
            case PixelFormat::R8_UNORM:
                return "R8_UNORM";
            case PixelFormat::R8_SNORM:
                return "R8_SNORM";
            case PixelFormat::R8_SINT:
                return "R8_SINT";
            case PixelFormat::R8_UINT:
                return "R8_UINT";
            case PixelFormat::R16G16B16A16_FLOAT:
                return "R16G16B16A16_FLOAT";
            case PixelFormat::R16G16B16A16_UNORM:
                return "R16G16B16A16_UNORM";
            case PixelFormat::R16G16B16A16_SNORM:
                return "R16G16B16A16_SNORM";
            case PixelFormat::R16G16B16A16_SINT:
                return "R16G16B16A16_SINT";
            case PixelFormat::R16G16B16A16_UINT:
                return "R16G16B16A16_UINT";
            case PixelFormat::B10G11R11_FLOAT:
                return "B10G11R11_FLOAT";
            case PixelFormat::R32G32B32A32_UINT:
                return "R32G32B32A32_UINT";
            case PixelFormat::BC1_RGBA_UNORM:
                return "BC1_RGBA_UNORM";
            case PixelFormat::BC2_UNORM:
                return "BC2_UNORM";
            case PixelFormat::BC3_UNORM:
                return "BC3_UNORM";
            case PixelFormat::BC4_UNORM:
                return "BC4_UNORM";
            case PixelFormat::BC4_SNORM:
                return "BC4_SNORM";
            case PixelFormat::BC5_UNORM:
                return "BC5_UNORM";
            case PixelFormat::BC5_SNORM:
                return "BC5_SNORM";
            case PixelFormat::BC7_UNORM:
                return "BC7_UNORM";
            case PixelFormat::BC6H_UFLOAT:
                return "BC6H_UFLOAT";
            case PixelFormat::BC6H_SFLOAT:
                return "BC6H_SFLOAT";
            case PixelFormat::ASTC_2D_4X4_UNORM:
                return "ASTC_2D_4X4_UNORM";
            case PixelFormat::B8G8R8A8_UNORM:
                return "B8G8R8A8_UNORM";
            case PixelFormat::R32G32B32A32_FLOAT:
                return "R32G32B32A32_FLOAT";
            case PixelFormat::R32G32B32A32_SINT:
                return "R32G32B32A32_SINT";
            case PixelFormat::R32G32_FLOAT:
                return "R32G32_FLOAT";
            case PixelFormat::R32G32_SINT:
                return "R32G32_SINT";
            case PixelFormat::R32_FLOAT:
                return "R32_FLOAT";
            case PixelFormat::R16_FLOAT:
                return "R16_FLOAT";
            case PixelFormat::R16_UNORM:
                return "R16_UNORM";
            case PixelFormat::R16_SNORM:
                return "R16_SNORM";
            case PixelFormat::R16_UINT:
                return "R16_UINT";
            case PixelFormat::R16_SINT:
                return "R16_SINT";
            case PixelFormat::R16G16_UNORM:
                return "R16G16_UNORM";
            case PixelFormat::R16G16_FLOAT:
                return "R16G16_FLOAT";
            case PixelFormat::R16G16_UINT:
                return "R16G16_UINT";
            case PixelFormat::R16G16_SINT:
                return "R16G16_SINT";
            case PixelFormat::R16G16_SNORM:
                return "R16G16_SNORM";
            case PixelFormat::R32G32B32_FLOAT:
                return "R32G32B32_FLOAT";
            case PixelFormat::A8B8G8R8_SRGB:
                return "A8B8G8R8_SRGB";
            case PixelFormat::R8G8_UNORM:
                return "R8G8_UNORM";
            case PixelFormat::R8G8_SNORM:
                return "R8G8_SNORM";
            case PixelFormat::R8G8_SINT:
                return "R8G8_SINT";
            case PixelFormat::R8G8_UINT:
                return "R8G8_UINT";
            case PixelFormat::R32G32_UINT:
                return "R32G32_UINT";
            case PixelFormat::R16G16B16X16_FLOAT:
                return "R16G16B16X16_FLOAT";
            case PixelFormat::R32_UINT:
                return "R32_UINT";
            case PixelFormat::R32_SINT:
                return "R32_SINT";
            case PixelFormat::ASTC_2D_8X8_UNORM:
                return "ASTC_2D_8X8_UNORM";
            case PixelFormat::ASTC_2D_8X5_UNORM:
                return "ASTC_2D_8X5_UNORM";
            case PixelFormat::ASTC_2D_5X4_UNORM:
                return "ASTC_2D_5X4_UNORM";
            case PixelFormat::B8G8R8A8_SRGB:
                return "B8G8R8A8_SRGB";
            case PixelFormat::BC1_RGBA_SRGB:
                return "BC1_RGBA_SRGB";
            case PixelFormat::BC2_SRGB:
                return "BC2_SRGB";
            case PixelFormat::BC3_SRGB:
                return "BC3_SRGB";
            case PixelFormat::BC7_SRGB:
                return "BC7_SRGB";
            case PixelFormat::A4B4G4R4_UNORM:
                return "A4B4G4R4_UNORM";
            case PixelFormat::ASTC_2D_4X4_SRGB:
                return "ASTC_2D_4X4_SRGB";
            case PixelFormat::ASTC_2D_8X8_SRGB:
                return "ASTC_2D_8X8_SRGB";
            case PixelFormat::ASTC_2D_8X5_SRGB:
                return "ASTC_2D_8X5_SRGB";
            case PixelFormat::ASTC_2D_5X4_SRGB:
                return "ASTC_2D_5X4_SRGB";
            case PixelFormat::ASTC_2D_5X5_UNORM:
                return "ASTC_2D_5X5_UNORM";
            case PixelFormat::ASTC_2D_5X5_SRGB:
                return "ASTC_2D_5X5_SRGB";
            case PixelFormat::ASTC_2D_10X8_UNORM:
                return "ASTC_2D_10X8_UNORM";
            case PixelFormat::ASTC_2D_10X8_SRGB:
                return "ASTC_2D_10X8_SRGB";
            case PixelFormat::ASTC_2D_6X6_UNORM:
                return "ASTC_2D_6X6_UNORM";
            case PixelFormat::ASTC_2D_6X6_SRGB:
                return "ASTC_2D_6X6_SRGB";
            case PixelFormat::ASTC_2D_10X10_UNORM:
                return "ASTC_2D_10X10_UNORM";
            case PixelFormat::ASTC_2D_10X10_SRGB:
                return "ASTC_2D_10X10_SRGB";
            case PixelFormat::ASTC_2D_12X12_UNORM:
                return "ASTC_2D_12X12_UNORM";
            case PixelFormat::ASTC_2D_12X12_SRGB:
                return "ASTC_2D_12X12_SRGB";
            case PixelFormat::ASTC_2D_8X6_UNORM:
                return "ASTC_2D_8X6_UNORM";
            case PixelFormat::ASTC_2D_8X6_SRGB:
                return "ASTC_2D_8X6_SRGB";
            case PixelFormat::ASTC_2D_6X5_UNORM:
                return "ASTC_2D_6X5_UNORM";
            case PixelFormat::ASTC_2D_6X5_SRGB:
                return "ASTC_2D_6X5_SRGB";
            case PixelFormat::E5B9G9R9_FLOAT:
                return "E5B9G9R9_FLOAT";
            case PixelFormat::D32_FLOAT:
                return "D32_FLOAT";
            case PixelFormat::D16_UNORM:
                return "D16_UNORM";
            case PixelFormat::D24_UNORM_S8_UINT:
                return "D24_UNORM_S8_UINT";
            case PixelFormat::S8_UINT_D24_UNORM:
                return "S8_UINT_D24_UNORM";
            case PixelFormat::D32_FLOAT_S8_UINT:
                return "D32_FLOAT_S8_UINT";
            case PixelFormat::MaxDepthStencilFormat:
            case PixelFormat::Invalid:
                return "Invalid";
            }
            return "Invalid";
        }();
        return formatter<string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<VideoCommon::ImageType> : fmt::formatter<fmt::string_view> {
    template <typename FormatContext>
    auto format(VideoCommon::ImageType type, FormatContext& ctx) {
        const string_view name = [type] {
            using VideoCommon::ImageType;
            switch (type) {
            case ImageType::e1D:
                return "1D";
            case ImageType::e2D:
                return "2D";
            case ImageType::e3D:
                return "3D";
            case ImageType::Linear:
                return "Linear";
            case ImageType::Buffer:
                return "Buffer";
            }
            return "Invalid";
        }();
        return formatter<string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<VideoCommon::Extent3D> {
    constexpr auto parse(fmt::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const VideoCommon::Extent3D& extent, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{{{}, {}, {}}}", extent.width, extent.height,
                              extent.depth);
    }
};

namespace VideoCommon {

struct ImageBase;
struct ImageViewBase;
struct RenderTargets;

[[nodiscard]] std::string Name(const ImageBase& image);

[[nodiscard]] std::string Name(const ImageViewBase& image_view);

[[nodiscard]] std::string Name(const RenderTargets& render_targets);

} // namespace VideoCommon
