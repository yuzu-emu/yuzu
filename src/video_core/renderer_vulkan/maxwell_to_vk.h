// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "shader_recompiler/stage.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/surface.h"
#include "video_core/textures/texture.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan::MaxwellToVK {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using PixelFormat = VideoCore::Surface::PixelFormat;

namespace Sampler {

VkFilter Filter(Tegra::Texture::TextureFilter filter);

VkSamplerMipmapMode MipmapMode(Tegra::Texture::TextureMipmapFilter mipmap_filter);

VkSamplerAddressMode WrapMode(const Device& device, Tegra::Texture::WrapMode wrap_mode,
                              Tegra::Texture::TextureFilter filter);

VkCompareOp DepthCompareFunction(Tegra::Texture::DepthCompareFunc depth_compare_func);

} // namespace Sampler

struct FormatInfo {
    VkFormat format;
    bool attachable;
    bool storage;
};

/**
 * Returns format properties supported in the host
 * @param device       Host device
 * @param format_type  Type of image the buffer will use
 * @param with_srgb    True when the format can be sRGB when converted to another format (ASTC)
 * @param pixel_format Guest pixel format to describe
 */
[[nodiscard]] FormatInfo SurfaceFormat(const Device& device, FormatType format_type, bool with_srgb,
                                       PixelFormat pixel_format);

VkShaderStageFlagBits ShaderStage(Shader::Stage stage);

VkPrimitiveTopology PrimitiveTopology(const Device& device, Maxwell::PrimitiveTopology topology);

VkFormat VertexFormat(Maxwell::VertexAttribute::Type type, Maxwell::VertexAttribute::Size size);

VkCompareOp ComparisonOp(Maxwell::ComparisonOp comparison);

VkIndexType IndexFormat(Maxwell::IndexFormat index_format);

VkStencilOp StencilOp(Maxwell::StencilOp stencil_op);

VkBlendOp BlendEquation(Maxwell::Blend::Equation equation);

VkBlendFactor BlendFactor(Maxwell::Blend::Factor factor);

VkFrontFace FrontFace(Maxwell::FrontFace front_face);

VkCullModeFlagBits CullFace(Maxwell::CullFace cull_face);

VkPolygonMode PolygonMode(Maxwell::PolygonMode polygon_mode);

VkComponentSwizzle SwizzleSource(Tegra::Texture::SwizzleSource swizzle);

VkViewportCoordinateSwizzleNV ViewportSwizzle(Maxwell::ViewportSwizzle swizzle);

VkSamplerReductionMode SamplerReduction(Tegra::Texture::SamplerReduction reduction);

VkSampleCountFlagBits MsaaMode(Tegra::Texture::MsaaMode msaa_mode);

} // namespace Vulkan::MaxwellToVK
