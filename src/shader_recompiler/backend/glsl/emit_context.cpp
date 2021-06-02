// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
namespace {
std::string_view InterpDecorator(Interpolation interp) {
    switch (interp) {
    case Interpolation::Smooth:
        return "";
    case Interpolation::Flat:
        return "flat";
    case Interpolation::NoPerspective:
        return "noperspective";
    }
    throw InvalidArgument("Invalid interpolation {}", interp);
}

std::string_view InputArrayDecorator(Stage stage) {
    switch (stage) {
    case Stage::Geometry:
    case Stage::TessellationControl:
    case Stage::TessellationEval:
        return "[]";
    default:
        return "";
    }
}

bool StoresPerVertexAttributes(Stage stage) {
    switch (stage) {
    case Stage::VertexA:
    case Stage::VertexB:
    case Stage::Geometry:
    case Stage::TessellationEval:
        return true;
    default:
        return false;
    }
}

std::string OutputDecorator(Stage stage, u32 size) {
    switch (stage) {
    case Stage::TessellationControl:
        return fmt::format("[{}]", size);
    default:
        return "";
    }
}

std::string_view SamplerType(TextureType type, bool is_depth) {
    if (is_depth) {
        switch (type) {
        case TextureType::Color1D:
            return "sampler1DShadow";
        case TextureType::ColorArray1D:
            return "sampler1DArrayShadow";
        case TextureType::Color2D:
            return "sampler2DShadow";
        case TextureType::ColorArray2D:
            return "sampler2DArrayShadow";
        case TextureType::ColorCube:
            return "samplerCubeShadow";
        case TextureType::ColorArrayCube:
            return "samplerCubeArrayShadow";
        default:
            fmt::print("Texture type: {}", type);
            throw NotImplementedException("Texture type: {}", type);
        }
    }
    switch (type) {
    case TextureType::Color1D:
        return "sampler1D";
    case TextureType::ColorArray1D:
        return "sampler1DArray";
    case TextureType::Color2D:
        return "sampler2D";
    case TextureType::ColorArray2D:
        return "sampler2DArray";
    case TextureType::Color3D:
        return "sampler3D";
    case TextureType::ColorCube:
        return "samplerCube";
    case TextureType::ColorArrayCube:
        return "samplerCubeArray";
    case TextureType::Buffer:
        return "samplerBuffer";
    default:
        fmt::print("Texture type: {}", type);
        throw NotImplementedException("Texture type: {}", type);
    }
}

std::string_view GetTessMode(TessPrimitive primitive) {
    switch (primitive) {
    case TessPrimitive::Triangles:
        return "triangles";
    case TessPrimitive::Quads:
        return "quads";
    case TessPrimitive::Isolines:
        return "isolines";
    }
    throw InvalidArgument("Invalid tessellation primitive {}", primitive);
}

std::string_view GetTessSpacing(TessSpacing spacing) {
    switch (spacing) {
    case TessSpacing::Equal:
        return "equal_spacing";
    case TessSpacing::FractionalOdd:
        return "fractional_odd_spacing";
    case TessSpacing::FractionalEven:
        return "fractional_even_spacing";
    }
    throw InvalidArgument("Invalid tessellation spacing {}", spacing);
}

std::string_view InputPrimitive(InputTopology topology) {
    switch (topology) {
    case InputTopology::Points:
        return "points";
    case InputTopology::Lines:
        return "lines";
    case InputTopology::LinesAdjacency:
        return "lines_adjacency";
    case InputTopology::Triangles:
        return "triangles";
    case InputTopology::TrianglesAdjacency:
        return "triangles_adjacency";
    }
    throw InvalidArgument("Invalid input topology {}", topology);
}

std::string_view OutputPrimitive(OutputTopology topology) {
    switch (topology) {
    case OutputTopology::PointList:
        return "points";
    case OutputTopology::LineStrip:
        return "line_strip";
    case OutputTopology::TriangleStrip:
        return "triangle_strip";
    }
    throw InvalidArgument("Invalid output topology {}", topology);
}

void SetupOutPerVertex(EmitContext& ctx, std::string& header) {
    if (!StoresPerVertexAttributes(ctx.stage)) {
        return;
    }
    header += "out gl_PerVertex{";
    header += "vec4 gl_Position;";
    if (ctx.info.stores_point_size) {
        header += "float gl_PointSize;";
    }
    if (ctx.info.stores_clip_distance) {
        header += "float gl_ClipDistance[];";
    }
    if (ctx.info.stores_viewport_index && ctx.supports_viewport_layer &&
        ctx.stage != Stage::Geometry) {
        header += "int gl_ViewportIndex;";
    }
    header += "};\n";
    if (ctx.info.stores_viewport_index && ctx.stage == Stage::Geometry) {
        header += "out int gl_ViewportIndex;";
    }
}
} // namespace

EmitContext::EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_,
                         const RuntimeInfo& runtime_info_)
    : info{program.info}, profile{profile_}, runtime_info{runtime_info_} {
    supports_viewport_layer = profile.support_gl_vertex_viewport_layer;
    SetupExtensions(header);
    stage = program.stage;
    switch (program.stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        stage_name = "vs";
        break;
    case Stage::TessellationControl:
        stage_name = "tsc";
        header += fmt::format("layout(vertices={})out;\n", program.invocations);
        break;
    case Stage::TessellationEval:
        stage_name = "tse";
        header += fmt::format("layout({},{},{})in;\n", GetTessMode(runtime_info.tess_primitive),
                              GetTessSpacing(runtime_info.tess_spacing),
                              runtime_info.tess_clockwise ? "cw" : "ccw");
        break;
    case Stage::Geometry:
        stage_name = "gs";
        header += fmt::format("layout({})in;layout({},max_vertices={})out;\n",
                              InputPrimitive(runtime_info.input_topology),
                              OutputPrimitive(program.output_topology), program.output_vertices);
        break;
    case Stage::Fragment:
        stage_name = "fs";
        break;
    case Stage::Compute:
        stage_name = "cs";
        header += fmt::format("layout(local_size_x={},local_size_y={},local_size_z={}) in;\n",
                              program.workgroup_size[0], program.workgroup_size[1],
                              program.workgroup_size[2]);
        break;
    }
    SetupOutPerVertex(*this, header);
    for (size_t index = 0; index < info.input_generics.size(); ++index) {
        const auto& generic{info.input_generics[index]};
        if (generic.used) {
            header += fmt::format("layout(location={}){} in vec4 in_attr{}{};", index,
                                  InterpDecorator(generic.interpolation), index,
                                  InputArrayDecorator(stage));
        }
    }
    for (size_t index = 0; index < info.uses_patches.size(); ++index) {
        if (!info.uses_patches[index]) {
            continue;
        }
        if (stage == Stage::TessellationControl) {
            header += fmt::format("layout(location={})patch out vec4 patch{};", index, index);
        } else {
            header += fmt::format("layout(location={})patch in vec4 patch{};", index, index);
        }
    }
    for (size_t index = 0; index < info.stores_frag_color.size(); ++index) {
        if (!info.stores_frag_color[index]) {
            continue;
        }
        header += fmt::format("layout(location={})out vec4 frag_color{};", index, index);
    }
    for (size_t index = 0; index < info.stores_generics.size(); ++index) {
        // TODO: Properly resolve attribute issues
        if (info.stores_generics[index] || stage == Stage::VertexA || stage == Stage::VertexB) {
            DefineGenericOutput(index, program.invocations);
        }
    }
    header += "\n";
    DefineConstantBuffers(bindings);
    DefineStorageBuffers(bindings);
    SetupImages(bindings);
    DefineHelperFunctions();
}

void EmitContext::SetupExtensions(std::string&) {
    // TODO: track this usage
    header += "#extension GL_ARB_sparse_texture2 : enable\n";
    header += "#extension GL_EXT_texture_shadow_lod : enable\n";
    if (info.uses_int64) {
        header += "#extension GL_ARB_gpu_shader_int64 : enable\n";
    }
    if (info.uses_int64_bit_atomics) {
        header += "#extension GL_NV_shader_atomic_int64 : enable\n";
    }
    if (info.uses_atomic_f32_add) {
        header += "#extension GL_NV_shader_atomic_float : enable\n";
    }
    if (info.uses_atomic_f16x2_add || info.uses_atomic_f16x2_min || info.uses_atomic_f16x2_max) {
        header += "#extension NV_shader_atomic_fp16_vector : enable\n";
    }
    if (info.uses_fp16) {
        if (profile.support_gl_nv_gpu_shader_5) {
            header += "#extension GL_NV_gpu_shader5 : enable\n";
        }
        if (profile.support_gl_amd_gpu_shader_half_float) {
            header += "#extension GL_AMD_gpu_shader_half_float : enable\n";
        }
    }
    if (info.uses_subgroup_invocation_id || info.uses_subgroup_mask || info.uses_subgroup_vote ||
        info.uses_subgroup_shuffles || info.uses_fswzadd) {
        header += "#extension GL_ARB_shader_ballot : enable\n";
        header += "#extension GL_ARB_shader_group_vote : enable\n";
        if (!info.uses_int64) {
            header += "#extension GL_ARB_gpu_shader_int64 : enable\n";
        }
    }
    if (info.stores_viewport_index && supports_viewport_layer && stage != Stage::Geometry) {
        header += "#extension GL_ARB_shader_viewport_layer_array : enable\n";
    }
}

void EmitContext::DefineConstantBuffers(Bindings& bindings) {
    if (info.constant_buffer_descriptors.empty()) {
        return;
    }
    for (const auto& desc : info.constant_buffer_descriptors) {
        header += fmt::format(
            "layout(std140,binding={}) uniform {}_cbuf_{}{{vec4 {}_cbuf{}[{}];}};",
            bindings.uniform_buffer, stage_name, desc.index, stage_name, desc.index, 4 * 1024);
        bindings.uniform_buffer += desc.count;
    }
}

void EmitContext::DefineStorageBuffers(Bindings& bindings) {
    if (info.storage_buffers_descriptors.empty()) {
        return;
    }
    u32 index{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        header += fmt::format("layout(std430,binding={}) buffer {}_ssbo_{}{{uint {}_ssbo{}[];}};",
                              bindings.storage_buffer, stage_name, bindings.storage_buffer,
                              stage_name, index);
        bindings.storage_buffer += desc.count;
        index += desc.count;
    }
}

void EmitContext::DefineGenericOutput(size_t index, u32 invocations) {
    static constexpr std::string_view swizzle{"xyzw"};
    const size_t base_index{static_cast<size_t>(IR::Attribute::Generic0X) + index * 4};
    u32 element{0};
    while (element < 4) {
        std::string definition{fmt::format("layout(location={}", index)};
        const u32 remainder{4 - element};
        const TransformFeedbackVarying* xfb_varying{};
        if (!runtime_info.xfb_varyings.empty()) {
            xfb_varying = &runtime_info.xfb_varyings[base_index + element];
            xfb_varying = xfb_varying && xfb_varying->components > 0 ? xfb_varying : nullptr;
        }
        const u32 num_components{xfb_varying ? xfb_varying->components : remainder};
        if (element > 0) {
            definition += fmt::format(",component={}", element);
        }
        if (xfb_varying) {
            definition +=
                fmt::format(",xfb_buffer={},xfb_stride={},xfb_offset={}", xfb_varying->buffer,
                            xfb_varying->stride, xfb_varying->offset);
        }
        std::string name{fmt::format("out_attr{}", index)};
        if (num_components < 4 || element > 0) {
            name += fmt::format("_{}", swizzle.substr(element, num_components));
        }
        const auto type{num_components == 1 ? "float" : fmt::format("vec{}", num_components)};
        definition += fmt::format(")out {} {}{};", type, name, OutputDecorator(stage, invocations));
        header += definition;

        const GenericElementInfo element_info{
            .name = name,
            .first_element = element,
            .num_components = num_components,
        };
        std::fill_n(output_generics[index].begin() + element, num_components, element_info);
        element += num_components;
    }
    header += "\n";
}

void EmitContext::DefineHelperFunctions() {
    if (info.uses_global_increment || info.uses_shared_increment) {
        header += "uint CasIncrement(uint op_a,uint op_b){return(op_a>=op_b)?0u:(op_a+1u);}\n";
    }
    if (info.uses_global_decrement || info.uses_shared_decrement) {
        header += "uint CasDecrement(uint op_a,uint "
                  "op_b){return(op_a==0||op_a>op_b)?op_b:(op_a-1u);}\n";
    }
    if (info.uses_atomic_f32_add) {
        header += "uint CasFloatAdd(uint op_a,float op_b){return "
                  "floatBitsToUint(uintBitsToFloat(op_a)+op_b);}\n";
    }
    if (info.uses_atomic_f32x2_add) {
        header += "uint CasFloatAdd32x2(uint op_a,vec2 op_b){return "
                  "packHalf2x16(unpackHalf2x16(op_a)+op_b);}\n";
    }
    if (info.uses_atomic_f32x2_min) {
        header += "uint CasFloatMin32x2(uint op_a,vec2 op_b){return "
                  "packHalf2x16(min(unpackHalf2x16(op_a),op_b));}\n";
    }
    if (info.uses_atomic_f32x2_max) {
        header += "uint CasFloatMax32x2(uint op_a,vec2 op_b){return "
                  "packHalf2x16(max(unpackHalf2x16(op_a),op_b));}\n";
    }
    if (info.uses_atomic_f16x2_add) {
        header += "uint CasFloatAdd16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(unpackFloat2x16(op_a)+op_b);}\n";
    }
    if (info.uses_atomic_f16x2_min) {
        header += "uint CasFloatMin16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(min(unpackFloat2x16(op_a),op_b));}\n";
    }
    if (info.uses_atomic_f16x2_max) {
        header += "uint CasFloatMax16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(max(unpackFloat2x16(op_a),op_b));}\n";
    }
    if (info.uses_atomic_s32_min) {
        header += "uint CasMinS32(uint op_a,uint op_b){return uint(min(int(op_a),int(op_b)));}";
    }
    if (info.uses_atomic_s32_max) {
        header += "uint CasMaxS32(uint op_a,uint op_b){return uint(max(int(op_a),int(op_b)));}";
    }
}

void EmitContext::SetupImages(Bindings& bindings) {
    image_buffer_bindings.reserve(info.image_buffer_descriptors.size());
    for (const auto& desc : info.image_buffer_descriptors) {
        throw NotImplementedException("image_buffer_descriptors");
        image_buffer_bindings.push_back(bindings.image);
        bindings.image += desc.count;
    }
    image_bindings.reserve(info.image_descriptors.size());
    for (const auto& desc : info.image_descriptors) {
        throw NotImplementedException("image_bindings");

        image_bindings.push_back(bindings.image);
        bindings.image += desc.count;
    }
    texture_buffer_bindings.reserve(info.texture_buffer_descriptors.size());
    for (const auto& desc : info.texture_buffer_descriptors) {
        texture_buffer_bindings.push_back(bindings.texture);
        const auto sampler_type{SamplerType(TextureType::Buffer, false)};
        const auto indices{bindings.texture + desc.count};
        for (u32 index = bindings.texture; index < indices; ++index) {
            header += fmt::format("layout(binding={}) uniform {} tex{};", bindings.texture,
                                  sampler_type, index);
        }
        bindings.texture += desc.count;
    }
    texture_bindings.reserve(info.texture_descriptors.size());
    for (const auto& desc : info.texture_descriptors) {
        const auto sampler_type{SamplerType(desc.type, desc.is_depth)};
        texture_bindings.push_back(bindings.texture);
        const auto indices{bindings.texture + desc.count};
        for (u32 index = bindings.texture; index < indices; ++index) {
            header += fmt::format("layout(binding={}) uniform {} tex{};", bindings.texture,
                                  sampler_type, index);
        }
        bindings.texture += desc.count;
    }
}

} // namespace Shader::Backend::GLSL
