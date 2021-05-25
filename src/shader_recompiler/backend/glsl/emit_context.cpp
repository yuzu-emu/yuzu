// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {

EmitContext::EmitContext(IR::Program& program, [[maybe_unused]] Bindings& bindings,
                         const Profile& profile_)
    : info{program.info}, profile{profile_} {
    std::string header = "#version 450\n";
    SetupExtensions(header);
    if (program.stage == Stage::Compute) {
        header += fmt::format("layout(local_size_x={},local_size_y={},local_size_z={}) in;\n",
                              program.workgroup_size[0], program.workgroup_size[1],
                              program.workgroup_size[2]);
    }
    code += header;

    DefineConstantBuffers();
    DefineStorageBuffers();
    DefineHelperFunctions();
    code += "void main(){\n";
}

void EmitContext::SetupExtensions(std::string& header) {
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
        Add("layout(std140,binding={}) uniform cbuf_{}{{vec4 cbuf{}[{}];}};", binding, binding,
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
    // TODO: Track this usage
    code += "uint CasMinS32(uint op_a,uint op_b){return uint(min(int(op_a),int(op_b)));}";
    code += "uint CasMaxS32(uint op_a,uint op_b){return uint(max(int(op_a),int(op_b)));}";
}

} // namespace Shader::Backend::GLSL
