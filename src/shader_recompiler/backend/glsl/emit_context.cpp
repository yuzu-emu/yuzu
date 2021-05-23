// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/program.h"

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
    code += "void main(){\n";
}

void EmitContext::SetupExtensions(std::string& header) {
    if (info.uses_int64) {
        header += "#extension GL_ARB_gpu_shader_int64 : enable\n";
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
        if (True(info.used_storage_buffer_types & IR::Type::U32) ||
            True(info.used_storage_buffer_types & IR::Type::F32)) {
            Add("layout(std430,binding={}) buffer ssbo_{}_u32{{uint ssbo{}_u32[];}};", binding,
                binding, desc.cbuf_index, desc.count);
        }
        if (True(info.used_storage_buffer_types & IR::Type::U32x2) ||
            True(info.used_storage_buffer_types & IR::Type::F32x2)) {
            Add("layout(std430,binding={}) buffer ssbo_{}_u64{{uvec2 ssbo{}_u64[];}};", binding,
                binding, desc.cbuf_index, desc.count);
        }
        ++binding;
    }
}

} // namespace Shader::Backend::GLSL
