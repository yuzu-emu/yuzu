// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Backend::GLASM {

EmitContext::EmitContext(IR::Program& program) {
    // FIXME: Temporary partial implementation
    u32 cbuf_index{};
    for (const auto& desc : program.info.constant_buffer_descriptors) {
        if (desc.count != 1) {
            throw NotImplementedException("Constant buffer descriptor array");
        }
        Add("CBUFFER c{}[]={{program.buffer[{}]}};", desc.index, cbuf_index);
        ++cbuf_index;
    }
    for (const auto& desc : program.info.storage_buffers_descriptors) {
        if (desc.count != 1) {
            throw NotImplementedException("Storage buffer descriptor array");
        }
    }
    if (const size_t num = program.info.storage_buffers_descriptors.size(); num > 0) {
        Add("PARAM c[{}]={{program.local[0..{}]}};", num, num - 1);
    }
    switch (program.stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        stage_name = "vertex";
        break;
    case Stage::TessellationControl:
    case Stage::TessellationEval:
    case Stage::Geometry:
        stage_name = "primitive";
        break;
    case Stage::Fragment:
        stage_name = "fragment";
        break;
    case Stage::Compute:
        stage_name = "compute";
        break;
    }
}

} // namespace Shader::Backend::GLASM
