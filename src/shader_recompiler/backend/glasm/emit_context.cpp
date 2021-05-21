// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Backend::GLASM {
namespace {
std::string_view InterpDecorator(Interpolation interp) {
    switch (interp) {
    case Interpolation::Smooth:
        return "";
    case Interpolation::Flat:
        return "FLAT ";
    case Interpolation::NoPerspective:
        return "NOPERSPECTIVE ";
    }
    throw InvalidArgument("Invalid interpolation {}", interp);
}
} // Anonymous namespace

EmitContext::EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_)
    : info{program.info}, profile{profile_} {
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
    stage = program.stage;
    switch (program.stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        stage_name = "vertex";
        attrib_name = "vertex";
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
        break;
    }
    const std::string_view attr_stage{stage == Stage::Fragment ? "fragment" : "vertex"};
    for (size_t index = 0; index < program.info.input_generics.size(); ++index) {
        const auto& generic{program.info.input_generics[index]};
        if (generic.used) {
            Add("{}ATTRIB in_attr{}[]={{{}.attrib[{}..{}]}};",
                InterpDecorator(generic.interpolation), index, attr_stage, index, index);
        }
    }
    if (stage == Stage::Geometry && info.loads_position) {
        Add("ATTRIB vertex_position=vertex.position;");
    }
    if (info.uses_invocation_id) {
        Add("ATTRIB primitive_invocation=primitive.invocation;");
    }
    if (info.stores_tess_level_outer) {
        Add("OUTPUT result_patch_tessouter[]={{result.patch.tessouter[0..3]}};");
    }
    if (info.stores_tess_level_inner) {
        Add("OUTPUT result_patch_tessinner[]={{result.patch.tessinner[0..1]}};");
    }
    if (info.stores_clip_distance) {
        Add("OUTPUT result_clip[]={{result.clip[0..7]}};");
    }
    for (size_t index = 0; index < info.uses_patches.size(); ++index) {
        if (!info.uses_patches[index]) {
            continue;
        }
        if (stage == Stage::TessellationControl) {
            Add("OUTPUT result_patch_attrib{}[]={{result.patch.attrib[{}..{}]}};", index, index,
                index);
        } else {
            Add("ATTRIB primitive_patch_attrib{}[]={{primitive.patch.attrib[{}..{}]}};", index,
                index, index);
        }
    }
    for (size_t index = 0; index < program.info.stores_frag_color.size(); ++index) {
        if (!program.info.stores_frag_color[index]) {
            continue;
        }
        if (index == 0) {
            Add("OUTPUT frag_color0=result.color;");
        } else {
            Add("OUTPUT frag_color{}=result.color[{}];", index, index);
        }
    }
    for (size_t index = 0; index < program.info.stores_generics.size(); ++index) {
        if (program.info.stores_generics[index]) {
            Add("OUTPUT out_attr{}[]={{result.attrib[{}..{}]}};", index, index, index);
        }
    }
    image_buffer_bindings.reserve(program.info.image_buffer_descriptors.size());
    for (const auto& desc : program.info.image_buffer_descriptors) {
        image_buffer_bindings.push_back(bindings.image);
        bindings.image += desc.count;
    }
    image_bindings.reserve(program.info.image_descriptors.size());
    for (const auto& desc : program.info.image_descriptors) {
        image_bindings.push_back(bindings.image);
        bindings.image += desc.count;
    }
    texture_buffer_bindings.reserve(program.info.texture_buffer_descriptors.size());
    for (const auto& desc : program.info.texture_buffer_descriptors) {
        texture_buffer_bindings.push_back(bindings.texture);
        bindings.texture += desc.count;
    }
    texture_bindings.reserve(program.info.texture_descriptors.size());
    for (const auto& desc : program.info.texture_descriptors) {
        texture_bindings.push_back(bindings.texture);
        bindings.texture += desc.count;
    }
}

} // namespace Shader::Backend::GLASM
