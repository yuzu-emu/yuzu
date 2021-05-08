// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/cityhash.h"
#include "video_core/renderer_opengl/gl_compute_program.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"

namespace OpenGL {

using Shader::ImageBufferDescriptor;
using Tegra::Texture::TexturePair;
using VideoCommon::ImageId;

constexpr u32 MAX_TEXTURES = 64;
constexpr u32 MAX_IMAGES = 16;

size_t ComputeProgramKey::Hash() const noexcept {
    return static_cast<size_t>(
        Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this));
}

bool ComputeProgramKey::operator==(const ComputeProgramKey& rhs) const noexcept {
    return std::memcmp(this, &rhs, sizeof *this) == 0;
}

ComputeProgram::ComputeProgram(TextureCache& texture_cache_, BufferCache& buffer_cache_,
                               Tegra::MemoryManager& gpu_memory_,
                               Tegra::Engines::KeplerCompute& kepler_compute_,
                               ProgramManager& program_manager_, const Shader::Info& info_,
                               OGLProgram source_program_, OGLAssemblyProgram assembly_program_)
    : texture_cache{texture_cache_}, buffer_cache{buffer_cache_}, gpu_memory{gpu_memory_},
      kepler_compute{kepler_compute_}, program_manager{program_manager_}, info{info_},
      source_program{std::move(source_program_)}, assembly_program{std::move(assembly_program_)} {
    for (const auto& desc : info.texture_buffer_descriptors) {
        num_texture_buffers += desc.count;
    }
    for (const auto& desc : info.image_buffer_descriptors) {
        num_image_buffers += desc.count;
    }
    u32 num_textures = num_texture_buffers;
    for (const auto& desc : info.texture_descriptors) {
        num_textures += desc.count;
    }
    ASSERT(num_textures <= MAX_TEXTURES);

    u32 num_images = num_image_buffers;
    for (const auto& desc : info.image_descriptors) {
        num_images += desc.count;
    }
    ASSERT(num_images <= MAX_IMAGES);
}

void ComputeProgram::Configure() {
    buffer_cache.SetEnabledComputeUniformBuffers(info.constant_buffer_mask);
    buffer_cache.UnbindComputeStorageBuffers();
    size_t ssbo_index{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        ASSERT(desc.count == 1);
        buffer_cache.BindComputeStorageBuffer(ssbo_index, desc.cbuf_index, desc.cbuf_offset,
                                              desc.is_written);
        ++ssbo_index;
    }
    texture_cache.SynchronizeComputeDescriptors();

    std::array<ImageViewId, MAX_TEXTURES + MAX_IMAGES> image_view_ids;
    boost::container::static_vector<u32, MAX_TEXTURES + MAX_IMAGES> image_view_indices;
    std::array<GLuint, MAX_TEXTURES> samplers;
    std::array<GLuint, MAX_TEXTURES> textures;
    std::array<GLuint, MAX_IMAGES> images;
    GLsizei sampler_binding{};
    GLsizei texture_binding{};
    GLsizei image_binding{};

    const auto& qmd{kepler_compute.launch_description};
    const auto& cbufs{qmd.const_buffer_config};
    const bool via_header_index{qmd.linked_tsc != 0};
    const auto read_handle{[&](const auto& desc, u32 index) {
        ASSERT(((qmd.const_buffer_enable_mask >> desc.cbuf_index) & 1) != 0);
        const u32 index_offset{index << desc.size_shift};
        const u32 offset{desc.cbuf_offset + index_offset};
        const GPUVAddr addr{cbufs[desc.cbuf_index].Address() + offset};
        if constexpr (std::is_same_v<decltype(desc), const Shader::TextureDescriptor&> ||
                      std::is_same_v<decltype(desc), const Shader::TextureBufferDescriptor&>) {
            if (desc.has_secondary) {
                ASSERT(((qmd.const_buffer_enable_mask >> desc.secondary_cbuf_index) & 1) != 0);
                const u32 secondary_offset{desc.secondary_cbuf_offset + index_offset};
                const GPUVAddr separate_addr{cbufs[desc.secondary_cbuf_index].Address() +
                                             secondary_offset};
                const u32 lhs_raw{gpu_memory.Read<u32>(addr)};
                const u32 rhs_raw{gpu_memory.Read<u32>(separate_addr)};
                return TexturePair(lhs_raw | rhs_raw, via_header_index);
            }
        }
        return TexturePair(gpu_memory.Read<u32>(addr), via_header_index);
    }};
    const auto add_image{[&](const auto& desc) {
        for (u32 index = 0; index < desc.count; ++index) {
            const auto handle{read_handle(desc, index)};
            image_view_indices.push_back(handle.first);
        }
    }};
    for (const auto& desc : info.texture_buffer_descriptors) {
        for (u32 index = 0; index < desc.count; ++index) {
            const auto handle{read_handle(desc, index)};
            image_view_indices.push_back(handle.first);
            samplers[sampler_binding++] = 0;
        }
    }
    std::ranges::for_each(info.image_buffer_descriptors, add_image);
    for (const auto& desc : info.texture_descriptors) {
        for (u32 index = 0; index < desc.count; ++index) {
            const auto handle{read_handle(desc, index)};
            image_view_indices.push_back(handle.first);

            Sampler* const sampler = texture_cache.GetComputeSampler(handle.second);
            samplers[sampler_binding++] = sampler->Handle();
        }
    }
    std::ranges::for_each(info.image_descriptors, add_image);

    const std::span indices_span(image_view_indices.data(), image_view_indices.size());
    texture_cache.FillComputeImageViews(indices_span, image_view_ids);

    if (assembly_program.handle != 0) {
        // FIXME: State track this
        glEnable(GL_COMPUTE_PROGRAM_NV);
        glBindProgramARB(GL_COMPUTE_PROGRAM_NV, assembly_program.handle);
        program_manager.BindProgram(0);
    } else {
        program_manager.BindProgram(source_program.handle);
    }
    buffer_cache.UnbindComputeTextureBuffers();
    size_t texbuf_index{};
    const auto add_buffer{[&](const auto& desc) {
        constexpr bool is_image = std::is_same_v<decltype(desc), const ImageBufferDescriptor&>;
        for (u32 i = 0; i < desc.count; ++i) {
            bool is_written{false};
            if constexpr (is_image) {
                is_written = desc.is_written;
            }
            ImageView& image_view{texture_cache.GetImageView(image_view_ids[texbuf_index])};
            buffer_cache.BindComputeTextureBuffer(texbuf_index, image_view.GpuAddr(),
                                                  image_view.BufferSize(), image_view.format,
                                                  is_written, is_image);
            ++texbuf_index;
        }
    }};
    std::ranges::for_each(info.texture_buffer_descriptors, add_buffer);
    std::ranges::for_each(info.image_buffer_descriptors, add_buffer);

    buffer_cache.UpdateComputeBuffers();

    buffer_cache.runtime.SetImagePointers(textures.data(), images.data());
    buffer_cache.BindHostComputeBuffers();

    const ImageId* views_it{image_view_ids.data() + num_texture_buffers + num_image_buffers};
    texture_binding += num_texture_buffers;
    image_binding += num_image_buffers;

    for (const auto& desc : info.texture_descriptors) {
        for (u32 index = 0; index < desc.count; ++index) {
            ImageView& image_view{texture_cache.GetImageView(*(views_it++))};
            textures[texture_binding++] = image_view.Handle(desc.type);
        }
    }
    for (const auto& desc : info.image_descriptors) {
        for (u32 index = 0; index < desc.count; ++index) {
            ImageView& image_view{texture_cache.GetImageView(*(views_it++))};
            images[image_binding++] = image_view.Handle(desc.type);
        }
    }
    if (texture_binding != 0) {
        ASSERT(texture_binding == sampler_binding);
        glBindTextures(0, texture_binding, textures.data());
        glBindSamplers(0, sampler_binding, samplers.data());
    }
    if (image_binding != 0) {
        glBindImageTextures(0, image_binding, images.data());
    }
}

} // namespace OpenGL
