// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <type_traits>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/engines/shader_type.h"
#include "video_core/guest_driver.h"
#include "video_core/textures/texture.h"

namespace Tegra::Engines {

struct SamplerDescriptor {
    union {
        u32 raw = 0;
        BitField<0, 2, Tegra::Shader::TextureType> texture_type;
        BitField<2, 3, Tegra::Texture::ComponentType> r_type;
        BitField<5, 1, u32> is_array;
        BitField<6, 1, u32> is_buffer;
        BitField<7, 1, u32> is_shadow;
        BitField<8, 3, Tegra::Texture::ComponentType> g_type;
        BitField<11, 3, Tegra::Texture::ComponentType> b_type;
        BitField<14, 3, Tegra::Texture::ComponentType> a_type;
        BitField<17, 7, Tegra::Texture::TextureFormat> format;
    };

    auto operator<=>(const SamplerDescriptor& rhs) const noexcept {
        return raw <=> rhs.raw;
    }

    static SamplerDescriptor FromTIC(const Tegra::Texture::TICEntry& tic) {
        using Tegra::Shader::TextureType;
        SamplerDescriptor result;

        result.format.Assign(tic.format.Value());
        result.r_type.Assign(tic.r_type.Value());
        result.g_type.Assign(tic.g_type.Value());
        result.b_type.Assign(tic.b_type.Value());
        result.a_type.Assign(tic.a_type.Value());

        switch (tic.texture_type.Value()) {
        case Tegra::Texture::TextureType::Texture1D:
            result.texture_type.Assign(TextureType::Texture1D);
            return result;
        case Tegra::Texture::TextureType::Texture2D:
            result.texture_type.Assign(TextureType::Texture2D);
            return result;
        case Tegra::Texture::TextureType::Texture3D:
            result.texture_type.Assign(TextureType::Texture3D);
            return result;
        case Tegra::Texture::TextureType::TextureCubemap:
            result.texture_type.Assign(TextureType::TextureCube);
            return result;
        case Tegra::Texture::TextureType::Texture1DArray:
            result.texture_type.Assign(TextureType::Texture1D);
            result.is_array.Assign(1);
            return result;
        case Tegra::Texture::TextureType::Texture2DArray:
            result.texture_type.Assign(TextureType::Texture2D);
            result.is_array.Assign(1);
            return result;
        case Tegra::Texture::TextureType::Texture1DBuffer:
            result.texture_type.Assign(TextureType::Texture1D);
            result.is_buffer.Assign(1);
            return result;
        case Tegra::Texture::TextureType::Texture2DNoMipmap:
            result.texture_type.Assign(TextureType::Texture2D);
            return result;
        case Tegra::Texture::TextureType::TextureCubeArray:
            result.texture_type.Assign(TextureType::TextureCube);
            result.is_array.Assign(1);
            return result;
        default:
            result.texture_type.Assign(TextureType::Texture2D);
            return result;
        }
    }
};
static_assert(std::is_trivially_copyable_v<SamplerDescriptor>);

class ConstBufferEngineInterface {
public:
    virtual ~ConstBufferEngineInterface() = default;
    virtual u32 AccessConstBuffer32(ShaderType stage, u64 const_buffer, u64 offset) const = 0;
    virtual SamplerDescriptor AccessBoundSampler(ShaderType stage, u64 offset) const = 0;
    virtual SamplerDescriptor AccessBindlessSampler(ShaderType stage, u64 const_buffer,
                                                    u64 offset) const = 0;
    virtual SamplerDescriptor AccessSampler(u32 handle) const = 0;
    virtual u32 GetBoundBuffer() const = 0;

    virtual VideoCore::GuestDriverProfile& AccessGuestDriverProfile() = 0;
    virtual const VideoCore::GuestDriverProfile& AccessGuestDriverProfile() const = 0;
};

} // namespace Tegra::Engines
