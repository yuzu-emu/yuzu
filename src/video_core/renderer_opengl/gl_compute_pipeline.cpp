// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/cityhash.h"
#include "common/settings.h" // for enum class Settings::ShaderBackend
#include "video_core/renderer_opengl/gl_compute_pipeline.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

namespace OpenGL {

using Shader::ImageBufferDescriptor;
using Tegra::Texture::TexturePair;
using VideoCommon::ImageId;

constexpr u32 MAX_TEXTURES = 64;
constexpr u32 MAX_IMAGES = 16;

template <typename Range>
u32 AccumulateCount(const Range& range) {
    u32 num{};
    for (const auto& desc : range) {
        num += desc.count;
    }
    return num;
}

size_t ComputePipelineKey::Hash() const noexcept {
    return static_cast<size_t>(
        Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this));
}

bool ComputePipelineKey::operator==(const ComputePipelineKey& rhs) const noexcept {
    return std::memcmp(this, &rhs, sizeof *this) == 0;
}

ComputePipeline::ComputePipeline(const Device& device, TextureCache& texture_cache_,
                                 BufferCache& buffer_cache_, Tegra::MemoryManager& gpu_memory_,
                                 Tegra::Engines::KeplerCompute& kepler_compute_,
                                 ProgramManager& program_manager_, const Shader::Info& info_,
                                 std::string code, std::vector<u32> code_v)
    : texture_cache{texture_cache_}, buffer_cache{buffer_cache_}, gpu_memory{gpu_memory_},
      kepler_compute{kepler_compute_}, program_manager{program_manager_}, info{info_} {
    switch (device.GetShaderBackend()) {
    case Settings::ShaderBackend::GLSL:
        source_program.handle = glCreateProgram();
        AttachShader(GL_COMPUTE_SHADER, source_program.handle, code);
        LinkProgram(source_program.handle);
        break;
    case Settings::ShaderBackend::GLASM:
        assembly_program = CompileProgram(code, GL_COMPUTE_PROGRAM_NV);
        break;
    case Settings::ShaderBackend::SPIRV:
        source_program.handle = glCreateProgram();
        AttachShader(GL_COMPUTE_SHADER, source_program.handle, code_v);
        LinkProgram(source_program.handle);
        break;
    }
    std::copy_n(info.constant_buffer_used_sizes.begin(), uniform_buffer_sizes.size(),
                uniform_buffer_sizes.begin());

    num_texture_buffers = AccumulateCount(info.texture_buffer_descriptors);
    num_image_buffers = AccumulateCount(info.image_buffer_descriptors);

    const u32 num_textures{num_texture_buffers + AccumulateCount(info.texture_descriptors)};
    ASSERT(num_textures <= MAX_TEXTURES);

    const u32 num_images{num_image_buffers + AccumulateCount(info.image_descriptors)};
    ASSERT(num_images <= MAX_IMAGES);

    const bool is_glasm{assembly_program.handle != 0};
    const u32 num_storage_buffers{AccumulateCount(info.storage_buffers_descriptors)};
    use_storage_buffers =
        !is_glasm || num_storage_buffers < device.GetMaxGLASMStorageBufferBlocks();
    writes_global_memory = !use_storage_buffers &&
                           std::ranges::any_of(info.storage_buffers_descriptors,
                                               [](const auto& desc) { return desc.is_written; });
}

void ComputePipeline::Configure() {
    buffer_cache.SetComputeUniformBufferState(info.constant_buffer_mask, &uniform_buffer_sizes);
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
        program_manager.BindComputeAssemblyProgram(assembly_program.handle);
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

    buffer_cache.runtime.SetEnableStorageBuffers(use_storage_buffers);
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
            if (desc.is_written) {
                texture_cache.MarkModification(image_view.image_id);
            }
            images[image_binding++] = image_view.StorageView(desc.type, desc.format);
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
