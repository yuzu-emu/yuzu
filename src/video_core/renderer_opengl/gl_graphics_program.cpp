// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/cityhash.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/renderer_opengl/gl_graphics_program.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/texture_cache/texture_cache.h"

namespace OpenGL {
namespace {
using Shader::ImageBufferDescriptor;
using Tegra::Texture::TexturePair;
using VideoCommon::ImageId;

constexpr u32 MAX_TEXTURES = 64;
constexpr u32 MAX_IMAGES = 8;

/// Translates hardware transform feedback indices
/// @param location Hardware location
/// @return Pair of ARB_transform_feedback3 token stream first and third arguments
/// @note Read https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_transform_feedback3.txt
std::pair<GLint, GLint> TransformFeedbackEnum(u8 location) {
    const u8 index = location / 4;
    if (index >= 8 && index <= 39) {
        return {GL_GENERIC_ATTRIB_NV, index - 8};
    }
    if (index >= 48 && index <= 55) {
        return {GL_TEXTURE_COORD_NV, index - 48};
    }
    switch (index) {
    case 7:
        return {GL_POSITION, 0};
    case 40:
        return {GL_PRIMARY_COLOR_NV, 0};
    case 41:
        return {GL_SECONDARY_COLOR_NV, 0};
    case 42:
        return {GL_BACK_PRIMARY_COLOR_NV, 0};
    case 43:
        return {GL_BACK_SECONDARY_COLOR_NV, 0};
    }
    UNIMPLEMENTED_MSG("index={}", index);
    return {GL_POSITION, 0};
}
} // Anonymous namespace

size_t GraphicsProgramKey::Hash() const noexcept {
    return static_cast<size_t>(Common::CityHash64(reinterpret_cast<const char*>(this), Size()));
}

bool GraphicsProgramKey::operator==(const GraphicsProgramKey& rhs) const noexcept {
    return std::memcmp(this, &rhs, Size()) == 0;
}

GraphicsProgram::GraphicsProgram(TextureCache& texture_cache_, BufferCache& buffer_cache_,
                                 Tegra::MemoryManager& gpu_memory_,
                                 Tegra::Engines::Maxwell3D& maxwell3d_,
                                 ProgramManager& program_manager_, StateTracker& state_tracker_,
                                 OGLProgram program_,
                                 std::array<OGLAssemblyProgram, 5> assembly_programs_,
                                 const std::array<const Shader::Info*, 5>& infos,
                                 const VideoCommon::TransformFeedbackState* xfb_state)
    : texture_cache{texture_cache_}, buffer_cache{buffer_cache_},
      gpu_memory{gpu_memory_}, maxwell3d{maxwell3d_}, program_manager{program_manager_},
      state_tracker{state_tracker_}, program{std::move(program_)}, assembly_programs{std::move(
                                                                       assembly_programs_)} {
    std::ranges::transform(infos, stage_infos.begin(),
                           [](const Shader::Info* info) { return info ? *info : Shader::Info{}; });

    for (size_t stage = 0; stage < 5; ++stage) {
        enabled_stages_mask |= (assembly_programs[stage].handle != 0 ? 1 : 0) << stage;
    }
    u32 num_textures{};
    u32 num_images{};
    for (size_t stage = 0; stage < base_uniform_bindings.size() - 1; ++stage) {
        const auto& info{stage_infos[stage]};
        base_uniform_bindings[stage + 1] = base_uniform_bindings[stage];
        base_storage_bindings[stage + 1] = base_storage_bindings[stage];
        for (const auto& desc : info.constant_buffer_descriptors) {
            base_uniform_bindings[stage + 1] += desc.count;
        }
        for (const auto& desc : info.storage_buffers_descriptors) {
            base_storage_bindings[stage + 1] += desc.count;
        }
        for (const auto& desc : info.texture_buffer_descriptors) {
            num_texture_buffers[stage] += desc.count;
            num_textures += desc.count;
        }
        for (const auto& desc : info.image_buffer_descriptors) {
            num_image_buffers[stage] += desc.count;
            num_images += desc.count;
        }
        for (const auto& desc : info.texture_descriptors) {
            num_textures += desc.count;
        }
        for (const auto& desc : info.image_descriptors) {
            num_images += desc.count;
        }
    }
    ASSERT(num_textures <= MAX_TEXTURES);
    ASSERT(num_images <= MAX_IMAGES);

    if (assembly_programs[0].handle != 0 && xfb_state) {
        GenerateTransformFeedbackState(*xfb_state);
    }
}

struct Spec {
    static constexpr std::array<bool, 5> enabled_stages{true, true, true, true, true};
    static constexpr bool has_storage_buffers = true;
    static constexpr bool has_texture_buffers = true;
    static constexpr bool has_image_buffers = true;
    static constexpr bool has_images = true;
};

void GraphicsProgram::Configure(bool is_indexed) {
    std::array<ImageId, MAX_TEXTURES + MAX_IMAGES> image_view_ids;
    std::array<u32, MAX_TEXTURES + MAX_IMAGES> image_view_indices;
    std::array<GLuint, MAX_TEXTURES> samplers;
    size_t image_view_index{};
    GLsizei sampler_binding{};

    texture_cache.SynchronizeGraphicsDescriptors();

    buffer_cache.runtime.SetBaseUniformBindings(base_uniform_bindings);
    buffer_cache.runtime.SetBaseStorageBindings(base_storage_bindings);

    const auto& regs{maxwell3d.regs};
    const bool via_header_index{regs.sampler_index == Maxwell::SamplerIndex::ViaHeaderIndex};
    const auto config_stage{[&](size_t stage) {
        const Shader::Info& info{stage_infos[stage]};
        buffer_cache.SetEnabledUniformBuffers(stage, info.constant_buffer_mask);
        buffer_cache.UnbindGraphicsStorageBuffers(stage);
        if constexpr (Spec::has_storage_buffers) {
            size_t ssbo_index{};
            for (const auto& desc : info.storage_buffers_descriptors) {
                ASSERT(desc.count == 1);
                buffer_cache.BindGraphicsStorageBuffer(stage, ssbo_index, desc.cbuf_index,
                                                       desc.cbuf_offset, desc.is_written);
                ++ssbo_index;
            }
        }
        const auto& cbufs{maxwell3d.state.shader_stages[stage].const_buffers};
        const auto read_handle{[&](const auto& desc, u32 index) {
            ASSERT(cbufs[desc.cbuf_index].enabled);
            const u32 index_offset{index << desc.size_shift};
            const u32 offset{desc.cbuf_offset + index_offset};
            const GPUVAddr addr{cbufs[desc.cbuf_index].address + offset};
            if constexpr (std::is_same_v<decltype(desc), const Shader::TextureDescriptor&> ||
                          std::is_same_v<decltype(desc), const Shader::TextureBufferDescriptor&>) {
                if (desc.has_secondary) {
                    ASSERT(cbufs[desc.secondary_cbuf_index].enabled);
                    const u32 second_offset{desc.secondary_cbuf_offset + index_offset};
                    const GPUVAddr separate_addr{cbufs[desc.secondary_cbuf_index].address +
                                                 second_offset};
                    const u32 lhs_raw{gpu_memory.Read<u32>(addr)};
                    const u32 rhs_raw{gpu_memory.Read<u32>(separate_addr)};
                    const u32 raw{lhs_raw | rhs_raw};
                    return TexturePair(raw, via_header_index);
                }
            }
            return TexturePair(gpu_memory.Read<u32>(addr), via_header_index);
        }};
        const auto add_image{[&](const auto& desc) {
            for (u32 index = 0; index < desc.count; ++index) {
                const auto handle{read_handle(desc, index)};
                image_view_indices[image_view_index++] = handle.first;
            }
        }};
        if constexpr (Spec::has_texture_buffers) {
            for (const auto& desc : info.texture_buffer_descriptors) {
                for (u32 index = 0; index < desc.count; ++index) {
                    const auto handle{read_handle(desc, index)};
                    image_view_indices[image_view_index++] = handle.first;
                    samplers[sampler_binding++] = 0;
                }
            }
        }
        if constexpr (Spec::has_image_buffers) {
            for (const auto& desc : info.image_buffer_descriptors) {
                add_image(desc);
            }
        }
        for (const auto& desc : info.texture_descriptors) {
            for (u32 index = 0; index < desc.count; ++index) {
                const auto handle{read_handle(desc, index)};
                image_view_indices[image_view_index++] = handle.first;

                Sampler* const sampler{texture_cache.GetGraphicsSampler(handle.second)};
                samplers[sampler_binding++] = sampler->Handle();
            }
        }
        if constexpr (Spec::has_images) {
            for (const auto& desc : info.image_descriptors) {
                add_image(desc);
            }
        }
    }};
    if constexpr (Spec::enabled_stages[0]) {
        config_stage(0);
    }
    if constexpr (Spec::enabled_stages[1]) {
        config_stage(1);
    }
    if constexpr (Spec::enabled_stages[2]) {
        config_stage(2);
    }
    if constexpr (Spec::enabled_stages[3]) {
        config_stage(3);
    }
    if constexpr (Spec::enabled_stages[4]) {
        config_stage(4);
    }
    const std::span indices_span(image_view_indices.data(), image_view_index);
    texture_cache.FillGraphicsImageViews(indices_span, image_view_ids);

    texture_cache.UpdateRenderTargets(false);
    state_tracker.BindFramebuffer(texture_cache.GetFramebuffer()->Handle());

    ImageId* texture_buffer_index{image_view_ids.data()};
    const auto bind_stage_info{[&](size_t stage) {
        size_t index{};
        const auto add_buffer{[&](const auto& desc) {
            constexpr bool is_image = std::is_same_v<decltype(desc), const ImageBufferDescriptor&>;
            for (u32 i = 0; i < desc.count; ++i) {
                bool is_written{false};
                if constexpr (is_image) {
                    is_written = desc.is_written;
                }
                ImageView& image_view{texture_cache.GetImageView(*texture_buffer_index)};
                buffer_cache.BindGraphicsTextureBuffer(stage, index, image_view.GpuAddr(),
                                                       image_view.BufferSize(), image_view.format,
                                                       is_written, is_image);
                ++index;
                ++texture_buffer_index;
            }
        }};
        const Shader::Info& info{stage_infos[stage]};
        buffer_cache.UnbindGraphicsTextureBuffers(stage);

        if constexpr (Spec::has_texture_buffers) {
            for (const auto& desc : info.texture_buffer_descriptors) {
                add_buffer(desc);
            }
        }
        if constexpr (Spec::has_image_buffers) {
            for (const auto& desc : info.image_buffer_descriptors) {
                add_buffer(desc);
            }
        }
        for (const auto& desc : info.texture_descriptors) {
            texture_buffer_index += desc.count;
        }
        if constexpr (Spec::has_images) {
            for (const auto& desc : info.image_descriptors) {
                texture_buffer_index += desc.count;
            }
        }
    }};
    if constexpr (Spec::enabled_stages[0]) {
        bind_stage_info(0);
    }
    if constexpr (Spec::enabled_stages[1]) {
        bind_stage_info(1);
    }
    if constexpr (Spec::enabled_stages[2]) {
        bind_stage_info(2);
    }
    if constexpr (Spec::enabled_stages[3]) {
        bind_stage_info(3);
    }
    if constexpr (Spec::enabled_stages[4]) {
        bind_stage_info(4);
    }
    buffer_cache.UpdateGraphicsBuffers(is_indexed);
    buffer_cache.BindHostGeometryBuffers(is_indexed);

    if (assembly_programs[0].handle != 0) {
        program_manager.BindAssemblyPrograms(assembly_programs, enabled_stages_mask);
    } else {
        program_manager.BindProgram(program.handle);
    }
    const ImageId* views_it{image_view_ids.data()};
    GLsizei texture_binding = 0;
    GLsizei image_binding = 0;
    std::array<GLuint, MAX_TEXTURES> textures;
    std::array<GLuint, MAX_IMAGES> images;
    const auto prepare_stage{[&](size_t stage) {
        buffer_cache.runtime.SetImagePointers(&textures[texture_binding], &images[image_binding]);
        buffer_cache.BindHostStageBuffers(stage);

        texture_binding += num_texture_buffers[stage];
        image_binding += num_image_buffers[stage];

        const auto& info{stage_infos[stage]};
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
    }};
    if constexpr (Spec::enabled_stages[0]) {
        prepare_stage(0);
    }
    if constexpr (Spec::enabled_stages[1]) {
        prepare_stage(1);
    }
    if constexpr (Spec::enabled_stages[2]) {
        prepare_stage(2);
    }
    if constexpr (Spec::enabled_stages[3]) {
        prepare_stage(3);
    }
    if constexpr (Spec::enabled_stages[4]) {
        prepare_stage(4);
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

void GraphicsProgram::GenerateTransformFeedbackState(
    const VideoCommon::TransformFeedbackState& xfb_state) {
    // TODO(Rodrigo): Inject SKIP_COMPONENTS*_NV when required. An unimplemented message will signal
    // when this is required.
    const auto& regs{maxwell3d.regs};

    GLint* cursor{xfb_attribs.data()};
    GLint* current_stream{xfb_streams.data()};

    for (size_t feedback = 0; feedback < Maxwell::NumTransformFeedbackBuffers; ++feedback) {
        const auto& layout = regs.tfb_layouts[feedback];
        UNIMPLEMENTED_IF_MSG(layout.stride != layout.varying_count * 4, "Stride padding");
        if (layout.varying_count == 0) {
            continue;
        }
        *current_stream = static_cast<GLint>(feedback);
        if (current_stream != xfb_streams.data()) {
            // When stepping one stream, push the expected token
            cursor[0] = GL_NEXT_BUFFER_NV;
            cursor[1] = 0;
            cursor[2] = 0;
            cursor += XFB_ENTRY_STRIDE;
        }
        ++current_stream;

        const auto& locations = regs.tfb_varying_locs[feedback];
        std::optional<u8> current_index;
        for (u32 offset = 0; offset < layout.varying_count; ++offset) {
            const u8 location = locations[offset];
            const u8 index = location / 4;

            if (current_index == index) {
                // Increase number of components of the previous attachment
                ++cursor[-2];
                continue;
            }
            current_index = index;

            std::tie(cursor[0], cursor[2]) = TransformFeedbackEnum(location);
            cursor[1] = 1;
            cursor += XFB_ENTRY_STRIDE;
        }
    }
    num_xfb_attribs = static_cast<GLsizei>((cursor - xfb_attribs.data()) / XFB_ENTRY_STRIDE);
    num_xfb_strides = static_cast<GLsizei>(current_stream - xfb_streams.data());
}

void GraphicsProgram::ConfigureTransformFeedbackImpl() const {
    glTransformFeedbackStreamAttribsNV(num_xfb_attribs, xfb_attribs.data(), num_xfb_strides,
                                       xfb_streams.data(), GL_INTERLEAVED_ATTRIBS);
}

} // namespace OpenGL
