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

std::string_view SamplerType(TextureType type) {
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

} // namespace

EmitContext::EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_,
                         const RuntimeInfo& runtime_info_)
    : info{program.info}, profile{profile_}, runtime_info{runtime_info_} {
    SetupExtensions(header);
    stage = program.stage;
    switch (program.stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        stage_name = "vs";
        // TODO: add only what's used by the shader
        header +=
            "out gl_PerVertex {vec4 gl_Position;float gl_PointSize;float gl_ClipDistance[];};";
        break;
    case Stage::TessellationControl:
    case Stage::TessellationEval:
        stage_name = "ts";
        break;
    case Stage::Geometry:
        stage_name = "gs";
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
    const std::string_view attr_stage{stage == Stage::Fragment ? "fragment" : "vertex"};
    for (size_t index = 0; index < info.input_generics.size(); ++index) {
        const auto& generic{info.input_generics[index]};
        if (generic.used) {
            header += fmt::format("layout(location={}) {} in vec4 in_attr{};", index,
                                  InterpDecorator(generic.interpolation), index);
        }
    }
    for (size_t index = 0; index < info.stores_frag_color.size(); ++index) {
        if (!info.stores_frag_color[index]) {
            continue;
        }
        header += fmt::format("layout(location={})out vec4 frag_color{};", index, index);
    }
    for (size_t index = 0; index < info.stores_generics.size(); ++index) {
        if (info.stores_generics[index]) {
            header += fmt::format("layout(location={}) out vec4 out_attr{};", index, index);
        }
    }
    DefineConstantBuffers(bindings);
    DefineStorageBuffers(bindings);
    SetupImages(bindings);
    DefineHelperFunctions();

    header += "void main(){\n";
    if (stage == Stage::VertexA || stage == Stage::VertexB) {
        Add("gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);");
    }
}

void EmitContext::SetupExtensions(std::string&) {
    header += "#extension GL_ARB_separate_shader_objects : enable\n";
    header += "#extension GL_ARB_sparse_texture2 : enable\n";
    // header += "#extension GL_ARB_texture_cube_map_array : enable\n";
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
    for (const auto& desc : info.storage_buffers_descriptors) {
        header += fmt::format("layout(std430,binding={}) buffer ssbo_{}{{uint ssbo{}[];}};",
                              bindings.storage_buffer, bindings.storage_buffer, desc.cbuf_index);
        bindings.storage_buffer += desc.count;
    }
}

void EmitContext::DefineHelperFunctions() {
    if (info.uses_global_increment) {
        header += "uint CasIncrement(uint op_a,uint op_b){return(op_a>=op_b)?0u:(op_a+1u);}\n";
    }
    if (info.uses_global_decrement) {
        header +=
            "uint CasDecrement(uint op_a,uint op_b){return(op_a==0||op_a>op_b)?op_b:(op_a-1u);}\n";
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
        throw NotImplementedException("TextureType::Buffer");

        texture_buffer_bindings.push_back(bindings.texture);
        bindings.texture += desc.count;
    }
    texture_bindings.reserve(info.texture_descriptors.size());
    for (const auto& desc : info.texture_descriptors) {
        const auto sampler_type{SamplerType(desc.type)};
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
