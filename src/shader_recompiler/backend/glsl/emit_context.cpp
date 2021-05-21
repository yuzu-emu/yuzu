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

void EmitContext::DefineConstantBuffers() {
    if (info.constant_buffer_descriptors.empty()) {
        return;
    }
    u32 binding{};
    for (const auto& desc : info.constant_buffer_descriptors) {
        Add("layout(std140,binding={}) uniform cbuf_{}{{uint cbuf{}[];}};", binding, binding,
            desc.index, desc.count);
        ++binding;
    }
}

void EmitContext::DefineStorageBuffers() {
    if (info.storage_buffers_descriptors.empty()) {
        return;
    }
    u32 binding{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        Add("layout(std430,binding={}) buffer buff_{}{{uint buff{}[];}};", binding, binding,
            desc.cbuf_index, desc.count);
        ++binding;
    }
}

} // namespace Shader::Backend::GLSL
