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
    std::string header = "#version 450 core\n";
    header += "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;";
    code += header;
    DefineConstantBuffers();
    code += "void main(){";
}

void EmitContext::DefineConstantBuffers() {
    if (info.constant_buffer_descriptors.empty()) {
        return;
    }
    for (const auto& desc : info.constant_buffer_descriptors) {
        Add("uniform uint c{}[{}];", desc.index, desc.count);
    }
}

} // namespace Shader::Backend::GLSL
