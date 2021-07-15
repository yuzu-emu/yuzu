// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::GLSL {
namespace {
u32 CbufIndex(size_t offset) {
    return (offset / 4) % 4;
}

char Swizzle(size_t offset) {
    return "xyzw"[CbufIndex(offset)];
}

std::string_view InterpDecorator(Interpolation interp) {
    switch (interp) {
    case Interpolation::Smooth:
        return "";
    case Interpolation::Flat:
        return "flat ";
    case Interpolation::NoPerspective:
        return "noperspective ";
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
        throw NotImplementedException("Texture type: {}", type);
    }
}

std::string_view ImageType(TextureType type) {
    switch (type) {
    case TextureType::Color1D:
        return "uimage1D";
    case TextureType::ColorArray1D:
        return "uimage1DArray";
    case TextureType::Color2D:
        return "uimage2D";
    case TextureType::ColorArray2D:
        return "uimage2DArray";
    case TextureType::Color3D:
        return "uimage3D";
    case TextureType::ColorCube:
        return "uimageCube";
    case TextureType::ColorArrayCube:
        return "uimageCubeArray";
    case TextureType::Buffer:
        return "uimageBuffer";
    default:
        throw NotImplementedException("Image type: {}", type);
    }
}

std::string_view ImageFormatString(ImageFormat format) {
    switch (format) {
    case ImageFormat::Typeless:
        return "";
    case ImageFormat::R8_UINT:
        return ",r8ui";
    case ImageFormat::R8_SINT:
        return ",r8i";
    case ImageFormat::R16_UINT:
        return ",r16ui";
    case ImageFormat::R16_SINT:
        return ",r16i";
    case ImageFormat::R32_UINT:
        return ",r32ui";
    case ImageFormat::R32G32_UINT:
        return ",rg32ui";
    case ImageFormat::R32G32B32A32_UINT:
        return ",rgba32ui";
    default:
        throw NotImplementedException("Image format: {}", format);
    }
}

std::string_view ImageAccessQualifier(bool is_written, bool is_read) {
    if (is_written && !is_read) {
        return "writeonly ";
    }
    if (is_read && !is_written) {
        return "readonly ";
    }
    return "";
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

void SetupLegacyOutPerVertex(EmitContext& ctx, std::string& header) {
    if (!ctx.info.stores.Legacy()) {
        return;
    }
    if (ctx.info.stores.FixedFunctionTexture()) {
        header += "vec4 gl_TexCoord[8];";
    }
    if (ctx.info.stores.AnyComponent(IR::Attribute::ColorFrontDiffuseR)) {
        header += "vec4 gl_FrontColor;";
    }
    if (ctx.info.stores.AnyComponent(IR::Attribute::ColorFrontSpecularR)) {
        header += "vec4 gl_FrontSecondaryColor;";
    }
    if (ctx.info.stores.AnyComponent(IR::Attribute::ColorBackDiffuseR)) {
        header += "vec4 gl_BackColor;";
    }
    if (ctx.info.stores.AnyComponent(IR::Attribute::ColorBackSpecularR)) {
        header += "vec4 gl_BackSecondaryColor;";
    }
}

void SetupOutPerVertex(EmitContext& ctx, std::string& header) {
    if (!StoresPerVertexAttributes(ctx.stage)) {
        return;
    }
    if (ctx.uses_geometry_passthrough) {
        return;
    }
    header += "out gl_PerVertex{vec4 gl_Position;";
    if (ctx.info.stores[IR::Attribute::PointSize]) {
        header += "float gl_PointSize;";
    }
    if (ctx.info.stores.ClipDistances()) {
        header += "float gl_ClipDistance[];";
    }
    if (ctx.info.stores[IR::Attribute::ViewportIndex] &&
        ctx.profile.support_viewport_index_layer_non_geometry && ctx.stage != Stage::Geometry) {
        header += "int gl_ViewportIndex;";
    }
    SetupLegacyOutPerVertex(ctx, header);
    header += "};";
    if (ctx.info.stores[IR::Attribute::ViewportIndex] && ctx.stage == Stage::Geometry) {
        header += "out int gl_ViewportIndex;";
    }
}

void SetupInPerVertex(EmitContext& ctx, std::string& header) {
    // Currently only required for TessellationControl to adhere to
    // ARB_separate_shader_objects requirements
    if (ctx.stage != Stage::TessellationControl) {
        return;
    }
    const bool loads_position{ctx.info.loads.AnyComponent(IR::Attribute::PositionX)};
    const bool loads_point_size{ctx.info.loads[IR::Attribute::PointSize]};
    const bool loads_clip_distance{ctx.info.loads.ClipDistances()};
    const bool loads_per_vertex{loads_position || loads_point_size || loads_clip_distance};
    if (!loads_per_vertex) {
        return;
    }
    header += "in gl_PerVertex{";
    if (loads_position) {
        header += "vec4 gl_Position;";
    }
    if (loads_point_size) {
        header += "float gl_PointSize;";
    }
    if (loads_clip_distance) {
        header += "float gl_ClipDistance[];";
    }
    header += "}gl_in[gl_MaxPatchVertices];";
}

void SetupLegacyInPerFragment(EmitContext& ctx, std::string& header) {
    if (!ctx.info.loads.Legacy()) {
        return;
    }
    header += "in gl_PerFragment{";
    if (ctx.info.loads.FixedFunctionTexture()) {
        header += "vec4 gl_TexCoord[8];";
    }
    if (ctx.info.loads.AnyComponent(IR::Attribute::ColorFrontDiffuseR)) {
        header += "vec4 gl_Color;";
    }
    header += "};";
}

} // Anonymous namespace

EmitContext::EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_,
                         const RuntimeInfo& runtime_info_)
    : info{program.info}, profile{profile_}, runtime_info{runtime_info_}, stage{program.stage},
      uses_geometry_passthrough{program.is_geometry_passthrough &&
                                profile.support_geometry_shader_passthrough} {
    if (profile.need_fastmath_off) {
        header += "#pragma optionNV(fastmath off)\n";
    }
    SetupExtensions();
    switch (program.stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        stage_name = "vs";
        break;
    case Stage::TessellationControl:
        stage_name = "tcs";
        header += fmt::format("layout(vertices={})out;", program.invocations);
        break;
    case Stage::TessellationEval:
        stage_name = "tes";
        header += fmt::format("layout({},{},{})in;", GetTessMode(runtime_info.tess_primitive),
                              GetTessSpacing(runtime_info.tess_spacing),
                              runtime_info.tess_clockwise ? "cw" : "ccw");
        break;
    case Stage::Geometry:
        stage_name = "gs";
        header += fmt::format("layout({})in;", InputPrimitive(runtime_info.input_topology));
        if (uses_geometry_passthrough) {
            header += "layout(passthrough)in gl_PerVertex{vec4 gl_Position;};";
            break;
        } else if (program.is_geometry_passthrough &&
                   !profile.support_geometry_shader_passthrough) {
            LOG_WARNING(Shader_GLSL, "Passthrough geometry program used but not supported");
        }
        header += fmt::format(
            "layout({},max_vertices={})out;in gl_PerVertex{{vec4 gl_Position;}}gl_in[];",
            OutputPrimitive(program.output_topology), program.output_vertices);
        break;
    case Stage::Fragment:
        stage_name = "fs";
        position_name = "gl_FragCoord";
        if (runtime_info.force_early_z) {
            header += "layout(early_fragment_tests)in;";
        }
        if (info.uses_sample_id) {
            header += "in int gl_SampleID;";
        }
        if (info.stores_sample_mask) {
            header += "out int gl_SampleMask[];";
        }
        break;
    case Stage::Compute:
        stage_name = "cs";
        const u32 local_x{std::max(program.workgroup_size[0], 1u)};
        const u32 local_y{std::max(program.workgroup_size[1], 1u)};
        const u32 local_z{std::max(program.workgroup_size[2], 1u)};
        header += fmt::format("layout(local_size_x={},local_size_y={},local_size_z={}) in;",
                              local_x, local_y, local_z);
        break;
    }
    SetupOutPerVertex(*this, header);
    SetupInPerVertex(*this, header);
    SetupLegacyInPerFragment(*this, header);

    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        if (!info.loads.Generic(index) || !runtime_info.previous_stage_stores.Generic(index)) {
            continue;
        }
        const auto qualifier{uses_geometry_passthrough ? "passthrough"
                                                       : fmt::format("location={}", index)};
        header += fmt::format("layout({}){}in vec4 in_attr{}{};", qualifier,
                              InterpDecorator(info.interpolation[index]), index,
                              InputArrayDecorator(stage));
    }
    for (size_t index = 0; index < info.uses_patches.size(); ++index) {
        if (!info.uses_patches[index]) {
            continue;
        }
        const auto qualifier{stage == Stage::TessellationControl ? "out" : "in"};
        header += fmt::format("layout(location={})patch {} vec4 patch{};", index, qualifier, index);
    }
    if (stage == Stage::Fragment) {
        for (size_t index = 0; index < info.stores_frag_color.size(); ++index) {
            if (!info.stores_frag_color[index] && !profile.need_declared_frag_colors) {
                continue;
            }
            header += fmt::format("layout(location={})out vec4 frag_color{};", index, index);
        }
    }
    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        if (info.stores.Generic(index)) {
            DefineGenericOutput(index, program.invocations);
        }
    }
    DefineConstantBuffers(bindings);
    DefineStorageBuffers(bindings);
    SetupImages(bindings);
    SetupTextures(bindings);
    DefineHelperFunctions();
    DefineConstants();
}

void EmitContext::SetupExtensions() {
    header += "#extension GL_ARB_separate_shader_objects : enable\n";
    if (info.uses_shadow_lod && profile.support_gl_texture_shadow_lod) {
        header += "#extension GL_EXT_texture_shadow_lod : enable\n";
    }
    if (info.uses_int64 && profile.support_int64) {
        header += "#extension GL_ARB_gpu_shader_int64 : enable\n";
    }
    if (info.uses_int64_bit_atomics) {
        header += "#extension GL_NV_shader_atomic_int64 : enable\n";
    }
    if (info.uses_atomic_f32_add) {
        header += "#extension GL_NV_shader_atomic_float : enable\n";
    }
    if (info.uses_atomic_f16x2_add || info.uses_atomic_f16x2_min || info.uses_atomic_f16x2_max) {
        header += "#extension GL_NV_shader_atomic_fp16_vector : enable\n";
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
        header += "#extension GL_ARB_shader_ballot : enable\n"
                  "#extension GL_ARB_shader_group_vote : enable\n";
        if (!info.uses_int64 && profile.support_int64) {
            header += "#extension GL_ARB_gpu_shader_int64 : enable\n";
        }
        if (profile.support_gl_warp_intrinsics) {
            header += "#extension GL_NV_shader_thread_shuffle : enable\n";
        }
    }
    if ((info.stores[IR::Attribute::ViewportIndex] || info.stores[IR::Attribute::Layer]) &&
        profile.support_viewport_index_layer_non_geometry && stage != Stage::Geometry) {
        header += "#extension GL_ARB_shader_viewport_layer_array : enable\n";
    }
    if (info.uses_sparse_residency && profile.support_gl_sparse_textures) {
        header += "#extension GL_ARB_sparse_texture2 : enable\n";
    }
    if (info.stores[IR::Attribute::ViewportMask] && profile.support_viewport_mask) {
        header += "#extension GL_NV_viewport_array2 : enable\n";
    }
    if (info.uses_typeless_image_reads) {
        header += "#extension GL_EXT_shader_image_load_formatted : enable\n";
    }
    if (info.uses_derivatives && profile.support_gl_derivative_control) {
        header += "#extension GL_ARB_derivative_control : enable\n";
    }
    if (uses_geometry_passthrough) {
        header += "#extension GL_NV_geometry_shader_passthrough : enable\n";
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
}

void EmitContext::DefineHelperFunctions() {
    header += "\n#define ftoi floatBitsToInt\n#define ftou floatBitsToUint\n"
              "#define itof intBitsToFloat\n#define utof uintBitsToFloat\n";
    if (info.uses_global_increment || info.uses_shared_increment) {
        header += "uint CasIncrement(uint op_a,uint op_b){return op_a>=op_b?0u:(op_a+1u);}";
    }
    if (info.uses_global_decrement || info.uses_shared_decrement) {
        header += "uint CasDecrement(uint op_a,uint op_b){"
                  "return op_a==0||op_a>op_b?op_b:(op_a-1u);}";
    }
    if (info.uses_atomic_f32_add) {
        header += "uint CasFloatAdd(uint op_a,float op_b){"
                  "return ftou(utof(op_a)+op_b);}";
    }
    if (info.uses_atomic_f32x2_add) {
        header += "uint CasFloatAdd32x2(uint op_a,vec2 op_b){"
                  "return packHalf2x16(unpackHalf2x16(op_a)+op_b);}";
    }
    if (info.uses_atomic_f32x2_min) {
        header += "uint CasFloatMin32x2(uint op_a,vec2 op_b){return "
                  "packHalf2x16(min(unpackHalf2x16(op_a),op_b));}";
    }
    if (info.uses_atomic_f32x2_max) {
        header += "uint CasFloatMax32x2(uint op_a,vec2 op_b){return "
                  "packHalf2x16(max(unpackHalf2x16(op_a),op_b));}";
    }
    if (info.uses_atomic_f16x2_add) {
        header += "uint CasFloatAdd16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(unpackFloat2x16(op_a)+op_b);}";
    }
    if (info.uses_atomic_f16x2_min) {
        header += "uint CasFloatMin16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(min(unpackFloat2x16(op_a),op_b));}";
    }
    if (info.uses_atomic_f16x2_max) {
        header += "uint CasFloatMax16x2(uint op_a,f16vec2 op_b){return "
                  "packFloat2x16(max(unpackFloat2x16(op_a),op_b));}";
    }
    if (info.uses_atomic_s32_min) {
        header += "uint CasMinS32(uint op_a,uint op_b){return uint(min(int(op_a),int(op_b)));}";
    }
    if (info.uses_atomic_s32_max) {
        header += "uint CasMaxS32(uint op_a,uint op_b){return uint(max(int(op_a),int(op_b)));}";
    }
    if (info.uses_global_memory && profile.support_int64) {
        header += DefineGlobalMemoryFunctions();
    }
    if (info.loads_indexed_attributes) {
        const bool is_array{stage == Stage::Geometry};
        const auto vertex_arg{is_array ? ",uint vertex" : ""};
        std::string func{
            fmt::format("float IndexedAttrLoad(int offset{}){{int base_index=offset>>2;uint "
                        "masked_index=uint(base_index)&3u;switch(base_index>>2){{",
                        vertex_arg)};
        if (info.loads.AnyComponent(IR::Attribute::PositionX)) {
            const auto position_idx{is_array ? "gl_in[vertex]." : ""};
            func += fmt::format("case {}:return {}{}[masked_index];",
                                static_cast<u32>(IR::Attribute::PositionX) >> 2, position_idx,
                                position_name);
        }
        const u32 base_attribute_value = static_cast<u32>(IR::Attribute::Generic0X) >> 2;
        for (u32 index = 0; index < IR::NUM_GENERICS; ++index) {
            if (!info.loads.Generic(index)) {
                continue;
            }
            const auto vertex_idx{is_array ? "[vertex]" : ""};
            func += fmt::format("case {}:return in_attr{}{}[masked_index];",
                                base_attribute_value + index, index, vertex_idx);
        }
        func += "default: return 0.0;}}";
        header += func;
    }
    if (info.stores_indexed_attributes) {
        // TODO
    }
}

std::string EmitContext::DefineGlobalMemoryFunctions() {
    const auto define_body{[&](std::string& func, size_t index, std::string_view return_statement) {
        const auto& ssbo{info.storage_buffers_descriptors[index]};
        const u32 size_cbuf_offset{ssbo.cbuf_offset + 8};
        const auto ssbo_addr{fmt::format("ssbo_addr{}", index)};
        const auto cbuf{fmt::format("{}_cbuf{}", stage_name, ssbo.cbuf_index)};
        std::array<std::string, 2> addr_xy;
        std::array<std::string, 2> size_xy;
        for (size_t i = 0; i < addr_xy.size(); ++i) {
            const auto addr_loc{ssbo.cbuf_offset + 4 * i};
            const auto size_loc{size_cbuf_offset + 4 * i};
            addr_xy[i] = fmt::format("ftou({}[{}].{})", cbuf, addr_loc / 16, Swizzle(addr_loc));
            size_xy[i] = fmt::format("ftou({}[{}].{})", cbuf, size_loc / 16, Swizzle(size_loc));
        }
        const auto addr_pack{fmt::format("packUint2x32(uvec2({},{}))", addr_xy[0], addr_xy[1])};
        const auto addr_statment{fmt::format("uint64_t {}={};", ssbo_addr, addr_pack)};
        func += addr_statment;

        const auto size_vec{fmt::format("uvec2({},{})", size_xy[0], size_xy[1])};
        const auto comp_lhs{fmt::format("(addr>={})", ssbo_addr)};
        const auto comp_rhs{fmt::format("(addr<({}+uint64_t({})))", ssbo_addr, size_vec)};
        const auto comparison{fmt::format("if({}&&{}){{", comp_lhs, comp_rhs)};
        func += comparison;

        const auto ssbo_name{fmt::format("{}_ssbo{}", stage_name, index)};
        func += fmt::format(fmt::runtime(return_statement), ssbo_name, ssbo_addr);
    }};
    std::string write_func{"void WriteGlobal32(uint64_t addr,uint data){"};
    std::string write_func_64{"void WriteGlobal64(uint64_t addr,uvec2 data){"};
    std::string write_func_128{"void WriteGlobal128(uint64_t addr,uvec4 data){"};
    std::string load_func{"uint LoadGlobal32(uint64_t addr){"};
    std::string load_func_64{"uvec2 LoadGlobal64(uint64_t addr){"};
    std::string load_func_128{"uvec4 LoadGlobal128(uint64_t addr){"};
    const size_t num_buffers{info.storage_buffers_descriptors.size()};
    for (size_t index = 0; index < num_buffers; ++index) {
        if (!info.nvn_buffer_used[index]) {
            continue;
        }
        define_body(write_func, index, "{0}[uint(addr-{1})>>2]=data;return;}}");
        define_body(write_func_64, index,
                    "{0}[uint(addr-{1})>>2]=data.x;{0}[uint(addr-{1}+4)>>2]=data.y;return;}}");
        define_body(write_func_128, index,
                    "{0}[uint(addr-{1})>>2]=data.x;{0}[uint(addr-{1}+4)>>2]=data.y;{0}[uint("
                    "addr-{1}+8)>>2]=data.z;{0}[uint(addr-{1}+12)>>2]=data.w;return;}}");
        define_body(load_func, index, "return {0}[uint(addr-{1})>>2];}}");
        define_body(load_func_64, index,
                    "return uvec2({0}[uint(addr-{1})>>2],{0}[uint(addr-{1}+4)>>2]);}}");
        define_body(load_func_128, index,
                    "return uvec4({0}[uint(addr-{1})>>2],{0}[uint(addr-{1}+4)>>2],{0}["
                    "uint(addr-{1}+8)>>2],{0}[uint(addr-{1}+12)>>2]);}}");
    }
    write_func += '}';
    write_func_64 += '}';
    write_func_128 += '}';
    load_func += "return 0u;}";
    load_func_64 += "return uvec2(0);}";
    load_func_128 += "return uvec4(0);}";
    return write_func + write_func_64 + write_func_128 + load_func + load_func_64 + load_func_128;
}

void EmitContext::SetupImages(Bindings& bindings) {
    image_buffers.reserve(info.image_buffer_descriptors.size());
    for (const auto& desc : info.image_buffer_descriptors) {
        image_buffers.push_back({bindings.image, desc.count});
        const auto format{ImageFormatString(desc.format)};
        const auto qualifier{ImageAccessQualifier(desc.is_written, desc.is_read)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        header += fmt::format("layout(binding={}{}) uniform {}uimageBuffer img{}{};",
                              bindings.image, format, qualifier, bindings.image, array_decorator);
        bindings.image += desc.count;
    }
    images.reserve(info.image_descriptors.size());
    for (const auto& desc : info.image_descriptors) {
        images.push_back({bindings.image, desc.count});
        const auto format{ImageFormatString(desc.format)};
        const auto image_type{ImageType(desc.type)};
        const auto qualifier{ImageAccessQualifier(desc.is_written, desc.is_read)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        header += fmt::format("layout(binding={}{})uniform {}{} img{}{};", bindings.image, format,
                              qualifier, image_type, bindings.image, array_decorator);
        bindings.image += desc.count;
    }
}

void EmitContext::SetupTextures(Bindings& bindings) {
    texture_buffers.reserve(info.texture_buffer_descriptors.size());
    for (const auto& desc : info.texture_buffer_descriptors) {
        texture_buffers.push_back({bindings.texture, desc.count});
        const auto sampler_type{SamplerType(TextureType::Buffer, false)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        header += fmt::format("layout(binding={}) uniform {} tex{}{};", bindings.texture,
                              sampler_type, bindings.texture, array_decorator);
        bindings.texture += desc.count;
    }
    textures.reserve(info.texture_descriptors.size());
    for (const auto& desc : info.texture_descriptors) {
        textures.push_back({bindings.texture, desc.count});
        const auto sampler_type{SamplerType(desc.type, desc.is_depth)};
        const auto array_decorator{desc.count > 1 ? fmt::format("[{}]", desc.count) : ""};
        header += fmt::format("layout(binding={}) uniform {} tex{}{};", bindings.texture,
                              sampler_type, bindings.texture, array_decorator);
        bindings.texture += desc.count;
    }
}

void EmitContext::DefineConstants() {
    if (info.uses_fswzadd) {
        header += "const float FSWZ_A[]=float[4](-1.f,1.f,-1.f,0.f);"
                  "const float FSWZ_B[]=float[4](-1.f,-1.f,1.f,-1.f);";
    }
}

} // namespace Shader::Backend::GLSL
