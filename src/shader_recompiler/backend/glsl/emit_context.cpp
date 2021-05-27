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
} // namespace

EmitContext::EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_)
    : info{program.info}, profile{profile_} {
    std::string header = "#version 450\n";
    SetupExtensions(header);
    stage = program.stage;
    switch (program.stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        stage_name = "vertex";
        attrib_name = "vertex";
        // TODO: add only what's used by the shader
        header +=
            "out gl_PerVertex {vec4 gl_Position;float gl_PointSize;float gl_ClipDistance[];};";
        break;
    case Stage::TessellationControl:
    case Stage::TessellationEval:
        stage_name = "primitive";
        attrib_name = "primitive";
        break;
    case Stage::Geometry:
        stage_name = "primitive";
        attrib_name = "vertex";
        break;
    case Stage::Fragment:
        stage_name = "fragment";
        attrib_name = "fragment";
        break;
    case Stage::Compute:
        stage_name = "invocation";
        header += fmt::format("layout(local_size_x={},local_size_y={},local_size_z={}) in;\n",
                              program.workgroup_size[0], program.workgroup_size[1],
                              program.workgroup_size[2]);
        break;
    }
    code += header;
    const std::string_view attr_stage{stage == Stage::Fragment ? "fragment" : "vertex"};
    for (size_t index = 0; index < info.input_generics.size(); ++index) {
        const auto& generic{info.input_generics[index]};
        if (generic.used) {
            Add("layout(location={}) {} in vec4 in_attr{};", index,
                InterpDecorator(generic.interpolation), index);
        }
    }
    for (size_t index = 0; index < info.stores_frag_color.size(); ++index) {
        if (!info.stores_frag_color[index]) {
            continue;
        }
        Add("layout(location={})out vec4 frag_color{};", index, index);
    }
    for (size_t index = 0; index < info.stores_generics.size(); ++index) {
        if (info.stores_generics[index]) {
            Add("layout(location={}) out vec4 out_attr{};", index, index);
        }
    }
    DefineConstantBuffers();
    DefineStorageBuffers();
    DefineHelperFunctions();
    SetupImages(bindings);
    Add("void main(){{");

    if (stage == Stage::VertexA || stage == Stage::VertexB) {
        Add("gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);");
    }
}

void EmitContext::SetupExtensions(std::string& header) {
    header += "#extension GL_ARB_separate_shader_objects : enable\n";
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
}

void EmitContext::DefineConstantBuffers() {
    if (info.constant_buffer_descriptors.empty()) {
        return;
    }
    u32 binding{};
    for (const auto& desc : info.constant_buffer_descriptors) {
        Add("layout(std140,binding={}) uniform cbuf_{}{{vec4 cbuf{}[{}];}};", binding, desc.index,
            desc.index, 4 * 1024);
        ++binding;
    }
}

void EmitContext::DefineStorageBuffers() {
    if (info.storage_buffers_descriptors.empty()) {
        return;
    }
    u32 binding{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        Add("layout(std430,binding={}) buffer ssbo_{}{{uint ssbo{}[];}};", binding, binding,
            desc.cbuf_index, desc.count);
        ++binding;
    }
}

void EmitContext::DefineHelperFunctions() {
    if (info.uses_global_increment) {
        code += "uint CasIncrement(uint op_a,uint op_b){return(op_a>=op_b)?0u:(op_a+1u);}\n";
    }
    if (info.uses_global_decrement) {
        code +=
            "uint CasDecrement(uint op_a,uint op_b){return(op_a==0||op_a>op_b)?op_b:(op_a-1u);}\n";
    }
    if (info.uses_atomic_f32_add) {
        code += "uint CasFloatAdd(uint op_a,float op_b){return "
                "floatBitsToUint(uintBitsToFloat(op_a)+op_b);}\n";
    }
    if (info.uses_atomic_f32x2_add) {
        code += "uint CasFloatAdd32x2(uint op_a,vec2 op_b){return "
                "packHalf2x16(unpackHalf2x16(op_a)+op_b);}\n";
    }
    if (info.uses_atomic_f32x2_min) {
        code += "uint CasFloatMin32x2(uint op_a,vec2 op_b){return "
                "packHalf2x16(min(unpackHalf2x16(op_a),op_b));}\n";
    }
    if (info.uses_atomic_f32x2_max) {
        code += "uint CasFloatMax32x2(uint op_a,vec2 op_b){return "
                "packHalf2x16(max(unpackHalf2x16(op_a),op_b));}\n";
    }
    if (info.uses_atomic_f16x2_add) {
        code += "uint CasFloatAdd16x2(uint op_a,f16vec2 op_b){return "
                "packFloat2x16(unpackFloat2x16(op_a)+op_b);}\n";
    }
    if (info.uses_atomic_f16x2_min) {
        code += "uint CasFloatMin16x2(uint op_a,f16vec2 op_b){return "
                "packFloat2x16(min(unpackFloat2x16(op_a),op_b));}\n";
    }
    if (info.uses_atomic_f16x2_max) {
        code += "uint CasFloatMax16x2(uint op_a,f16vec2 op_b){return "
                "packFloat2x16(max(unpackFloat2x16(op_a),op_b));}\n";
    }
    if (info.uses_atomic_s32_min) {
        code += "uint CasMinS32(uint op_a,uint op_b){return uint(min(int(op_a),int(op_b)));}";
    }
    if (info.uses_atomic_s32_max) {
        code += "uint CasMaxS32(uint op_a,uint op_b){return uint(max(int(op_a),int(op_b)));}";
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
        texture_bindings.push_back(bindings.texture);
        const auto indices{bindings.texture + desc.count};
        for (u32 index = bindings.texture; index < indices; ++index) {
            Add("layout(binding={}) uniform sampler2D tex{};", bindings.texture, index);
        }
        bindings.texture += desc.count;
    }
}

} // namespace Shader::Backend::GLSL
