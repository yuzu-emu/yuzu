// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "common/settings.h" // for enum class Settings::ShaderBackend
#include "common/thread_worker.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/renderer_opengl/gl_graphics_pipeline.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/shader_notify.h"
#include "video_core/texture_cache/texture_cache.h"

#if defined(_MSC_VER) && defined(NDEBUG)
#define LAMBDA_FORCEINLINE [[msvc::forceinline]]
#else
#define LAMBDA_FORCEINLINE
#endif

namespace OpenGL {
namespace {
using Shader::ImageBufferDescriptor;
using Shader::ImageDescriptor;
using Shader::TextureBufferDescriptor;
using Shader::TextureDescriptor;
using Tegra::Texture::TexturePair;
using VideoCommon::ImageId;

constexpr u32 MAX_TEXTURES = 64;
constexpr u32 MAX_IMAGES = 8;

template <typename Range>
u32 AccumulateCount(const Range& range) {
    u32 num{};
    for (const auto& desc : range) {
        num += desc.count;
    }
    return num;
}

GLenum Stage(size_t stage_index) {
    switch (stage_index) {
    case 0:
        return GL_VERTEX_SHADER;
    case 1:
        return GL_TESS_CONTROL_SHADER;
    case 2:
        return GL_TESS_EVALUATION_SHADER;
    case 3:
        return GL_GEOMETRY_SHADER;
    case 4:
        return GL_FRAGMENT_SHADER;
    }
    UNREACHABLE_MSG("{}", stage_index);
    return GL_NONE;
}

GLenum AssemblyStage(size_t stage_index) {
    switch (stage_index) {
    case 0:
        return GL_VERTEX_PROGRAM_NV;
    case 1:
        return GL_TESS_CONTROL_PROGRAM_NV;
    case 2:
        return GL_TESS_EVALUATION_PROGRAM_NV;
    case 3:
        return GL_GEOMETRY_PROGRAM_NV;
    case 4:
        return GL_FRAGMENT_PROGRAM_NV;
    }
    UNREACHABLE_MSG("{}", stage_index);
    return GL_NONE;
}

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

template <typename Spec>
bool Passes(const std::array<Shader::Info, 5>& stage_infos, u32 enabled_mask) {
    for (size_t stage = 0; stage < stage_infos.size(); ++stage) {
        if (!Spec::enabled_stages[stage] && ((enabled_mask >> stage) & 1) != 0) {
            return false;
        }
        const auto& info{stage_infos[stage]};
        if constexpr (!Spec::has_storage_buffers) {
            if (!info.storage_buffers_descriptors.empty()) {
                return false;
            }
        }
        if constexpr (!Spec::has_texture_buffers) {
            if (!info.texture_buffer_descriptors.empty()) {
                return false;
            }
        }
        if constexpr (!Spec::has_image_buffers) {
            if (!info.image_buffer_descriptors.empty()) {
                return false;
            }
        }
        if constexpr (!Spec::has_images) {
            if (!info.image_descriptors.empty()) {
                return false;
            }
        }
    }
    return true;
}

using ConfigureFuncPtr = void (*)(GraphicsPipeline*, bool);

template <typename Spec, typename... Specs>
ConfigureFuncPtr FindSpec(const std::array<Shader::Info, 5>& stage_infos, u32 enabled_mask) {
    if constexpr (sizeof...(Specs) > 0) {
        if (!Passes<Spec>(stage_infos, enabled_mask)) {
            return FindSpec<Specs...>(stage_infos, enabled_mask);
        }
    }
    return GraphicsPipeline::MakeConfigureSpecFunc<Spec>();
}

struct SimpleVertexFragmentSpec {
    static constexpr std::array<bool, 5> enabled_stages{true, false, false, false, true};
    static constexpr bool has_storage_buffers = false;
    static constexpr bool has_texture_buffers = false;
    static constexpr bool has_image_buffers = false;
    static constexpr bool has_images = false;
};

struct SimpleVertexSpec {
    static constexpr std::array<bool, 5> enabled_stages{true, false, false, false, false};
    static constexpr bool has_storage_buffers = false;
    static constexpr bool has_texture_buffers = false;
    static constexpr bool has_image_buffers = false;
    static constexpr bool has_images = false;
};

struct DefaultSpec {
    static constexpr std::array<bool, 5> enabled_stages{true, true, true, true, true};
    static constexpr bool has_storage_buffers = true;
    static constexpr bool has_texture_buffers = true;
    static constexpr bool has_image_buffers = true;
    static constexpr bool has_images = true;
};

ConfigureFuncPtr ConfigureFunc(const std::array<Shader::Info, 5>& infos, u32 enabled_mask) {
    return FindSpec<SimpleVertexSpec, SimpleVertexFragmentSpec, DefaultSpec>(infos, enabled_mask);
}
} // Anonymous namespace

GraphicsPipeline::GraphicsPipeline(
    const Device& device, TextureCache& texture_cache_, BufferCache& buffer_cache_,
    Tegra::MemoryManager& gpu_memory_, Tegra::Engines::Maxwell3D& maxwell3d_,
    ProgramManager& program_manager_, StateTracker& state_tracker_, ShaderWorker* thread_worker,
    VideoCore::ShaderNotify* shader_notify, std::array<std::string, 5> sources,
    std::array<std::vector<u32>, 5> sources_spirv, const std::array<const Shader::Info*, 5>& infos,
    const GraphicsPipelineKey& key_)
    : texture_cache{texture_cache_}, buffer_cache{buffer_cache_},
      gpu_memory{gpu_memory_}, maxwell3d{maxwell3d_}, program_manager{program_manager_},
      state_tracker{state_tracker_}, key{key_} {
    if (shader_notify) {
        shader_notify->MarkShaderBuilding();
    }
    u32 num_textures{};
    u32 num_images{};
    u32 num_storage_buffers{};
    for (size_t stage = 0; stage < base_uniform_bindings.size(); ++stage) {
        auto& info{stage_infos[stage]};
        if (infos[stage]) {
            info = *infos[stage];
            enabled_stages_mask |= 1u << stage;
        }
        if (stage < 4) {
            base_uniform_bindings[stage + 1] = base_uniform_bindings[stage];
            base_storage_bindings[stage + 1] = base_storage_bindings[stage];

            base_uniform_bindings[stage + 1] += AccumulateCount(info.constant_buffer_descriptors);
            base_storage_bindings[stage + 1] += AccumulateCount(info.storage_buffers_descriptors);
        }
        enabled_uniform_buffer_masks[stage] = info.constant_buffer_mask;
        std::ranges::copy(info.constant_buffer_used_sizes, uniform_buffer_sizes[stage].begin());

        const u32 num_tex_buffer_bindings{AccumulateCount(info.texture_buffer_descriptors)};
        num_texture_buffers[stage] += num_tex_buffer_bindings;
        num_textures += num_tex_buffer_bindings;

        const u32 num_img_buffers_bindings{AccumulateCount(info.image_buffer_descriptors)};
        num_image_buffers[stage] += num_img_buffers_bindings;
        num_images += num_img_buffers_bindings;

        num_textures += AccumulateCount(info.texture_descriptors);
        num_images += AccumulateCount(info.image_descriptors);
        num_storage_buffers += AccumulateCount(info.storage_buffers_descriptors);

        writes_global_memory |= std::ranges::any_of(
            info.storage_buffers_descriptors, [](const auto& desc) { return desc.is_written; });
    }
    ASSERT(num_textures <= MAX_TEXTURES);
    ASSERT(num_images <= MAX_IMAGES);

    const bool assembly_shaders{assembly_programs[0].handle != 0};
    use_storage_buffers =
        !assembly_shaders || num_storage_buffers <= device.GetMaxGLASMStorageBufferBlocks();
    writes_global_memory &= !use_storage_buffers;
    configure_func = ConfigureFunc(stage_infos, enabled_stages_mask);

    if (key.xfb_enabled && device.UseAssemblyShaders()) {
        GenerateTransformFeedbackState();
    }
    auto func{[this, device, sources, sources_spirv,
               shader_notify](ShaderContext::Context*) mutable {
        for (size_t stage = 0; stage < 5; ++stage) {
            switch (device.GetShaderBackend()) {
            case Settings::ShaderBackend::GLSL:
                if (!sources[stage].empty()) {
                    source_programs[stage] = CreateProgram(sources[stage], Stage(stage));
                }
                break;
            case Settings::ShaderBackend::GLASM:
                if (!sources[stage].empty()) {
                    assembly_programs[stage] = CompileProgram(sources[stage], AssemblyStage(stage));
                }
                break;
            case Settings::ShaderBackend::SPIRV:
                if (!sources_spirv[stage].empty()) {
                    source_programs[stage] = CreateProgram(sources_spirv[stage], Stage(stage));
                }
                break;
            }
        }
        if (shader_notify) {
            shader_notify->MarkShaderComplete();
        }
        is_built = true;
    }};
    if (thread_worker) {
        thread_worker->QueueWork(std::move(func));
    } else {
        func(nullptr);
    }
}

template <typename Spec>
void GraphicsPipeline::ConfigureImpl(bool is_indexed) {
    std::array<ImageId, MAX_TEXTURES + MAX_IMAGES> image_view_ids;
    std::array<u32, MAX_TEXTURES + MAX_IMAGES> image_view_indices;
    std::array<GLuint, MAX_TEXTURES> samplers;
    size_t image_view_index{};
    GLsizei sampler_binding{};

    texture_cache.SynchronizeGraphicsDescriptors();

    buffer_cache.SetUniformBuffersState(enabled_uniform_buffer_masks, &uniform_buffer_sizes);
    buffer_cache.runtime.SetBaseUniformBindings(base_uniform_bindings);
    buffer_cache.runtime.SetBaseStorageBindings(base_storage_bindings);
    buffer_cache.runtime.SetEnableStorageBuffers(use_storage_buffers);

    const auto& regs{maxwell3d.regs};
    const bool via_header_index{regs.sampler_index == Maxwell::SamplerIndex::ViaHeaderIndex};
    const auto config_stage{[&](size_t stage) LAMBDA_FORCEINLINE {
        const Shader::Info& info{stage_infos[stage]};
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
            if constexpr (std::is_same_v<decltype(desc), const TextureDescriptor&> ||
                          std::is_same_v<decltype(desc), const TextureBufferDescriptor&>) {
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
    const auto bind_stage_info{[&](size_t stage) LAMBDA_FORCEINLINE {
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
        program_manager.BindSourcePrograms(source_programs);
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

        views_it += num_texture_buffers[stage];
        views_it += num_image_buffers[stage];

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
                if (desc.is_written) {
                    texture_cache.MarkModification(image_view.image_id);
                }
                images[image_binding++] = image_view.StorageView(desc.type, desc.format);
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

void GraphicsPipeline::ConfigureTransformFeedbackImpl() const {
    glTransformFeedbackStreamAttribsNV(num_xfb_attribs, xfb_attribs.data(), num_xfb_strides,
                                       xfb_streams.data(), GL_INTERLEAVED_ATTRIBS);
}

void GraphicsPipeline::GenerateTransformFeedbackState() {
    // TODO(Rodrigo): Inject SKIP_COMPONENTS*_NV when required. An unimplemented message will signal
    // when this is required.
    GLint* cursor{xfb_attribs.data()};
    GLint* current_stream{xfb_streams.data()};

    for (size_t feedback = 0; feedback < Maxwell::NumTransformFeedbackBuffers; ++feedback) {
        const auto& layout = key.xfb_state.layouts[feedback];
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

        const auto& locations = key.xfb_state.varyings[feedback];
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

} // namespace OpenGL
