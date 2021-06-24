// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

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

bool IsInputArray(Stage stage) {
    return stage == Stage::Geometry || stage == Stage::TessellationControl ||
           stage == Stage::TessellationEval;
}
} // Anonymous namespace

EmitContext::EmitContext(IR::Program& program, Bindings& bindings, const Profile& profile_,
                         const RuntimeInfo& runtime_info_)
    : info{program.info}, profile{profile_}, runtime_info{runtime_info_} {
    // FIXME: Temporary partial implementation
    u32 cbuf_index{};
    for (const auto& desc : info.constant_buffer_descriptors) {
        if (desc.count != 1) {
            throw NotImplementedException("Constant buffer descriptor array");
        }
        Add("CBUFFER c{}[]={{program.buffer[{}]}};", desc.index, cbuf_index);
        ++cbuf_index;
    }
    u32 ssbo_index{};
    for (const auto& desc : info.storage_buffers_descriptors) {
        if (desc.count != 1) {
            throw NotImplementedException("Storage buffer descriptor array");
        }
        if (runtime_info.glasm_use_storage_buffers) {
            Add("STORAGE ssbo{}[]={{program.storage[{}]}};", ssbo_index, bindings.storage_buffer);
            ++bindings.storage_buffer;
            ++ssbo_index;
        }
    }
    if (!runtime_info.glasm_use_storage_buffers) {
        if (const size_t num = info.storage_buffers_descriptors.size(); num > 0) {
            Add("PARAM c[{}]={{program.local[0..{}]}};", num, num - 1);
        }
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
    const VaryingState loads{info.loads.mask | info.passthrough.mask};
    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        if (loads.Generic(index)) {
            Add("{}ATTRIB in_attr{}[]={{{}.attrib[{}..{}]}};",
                InterpDecorator(info.interpolation[index]), index, attr_stage, index, index);
        }
    }
    if (IsInputArray(stage) && loads.AnyComponent(IR::Attribute::PositionX)) {
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
    if (info.stores.ClipDistances()) {
        Add("OUTPUT result_clip[]={{result.clip[0..7]}};");
    }
    for (size_t index = 0; index < info.uses_patches.size(); ++index) {
        if (!info.uses_patches[index]) {
            continue;
        }
        if (stage == Stage::TessellationControl) {
            Add("OUTPUT result_patch_attrib{}[]={{result.patch.attrib[{}..{}]}};"
                "ATTRIB primitive_out_patch_attrib{}[]={{primitive.out.patch.attrib[{}..{}]}};",
                index, index, index, index, index, index);
        } else {
            Add("ATTRIB primitive_patch_attrib{}[]={{primitive.patch.attrib[{}..{}]}};", index,
                index, index);
        }
    }
    if (stage == Stage::Fragment) {
        Add("OUTPUT frag_color0=result.color;");
        for (size_t index = 1; index < info.stores_frag_color.size(); ++index) {
            Add("OUTPUT frag_color{}=result.color[{}];", index, index);
        }
    }
    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        if (info.stores.Generic(index)) {
            Add("OUTPUT out_attr{}[]={{result.attrib[{}..{}]}};", index, index, index);
        }
    }
    image_buffer_bindings.reserve(info.image_buffer_descriptors.size());
    for (const auto& desc : info.image_buffer_descriptors) {
        image_buffer_bindings.push_back(bindings.image);
        bindings.image += desc.count;
    }
    image_bindings.reserve(info.image_descriptors.size());
    for (const auto& desc : info.image_descriptors) {
        image_bindings.push_back(bindings.image);
        bindings.image += desc.count;
    }
    texture_buffer_bindings.reserve(info.texture_buffer_descriptors.size());
    for (const auto& desc : info.texture_buffer_descriptors) {
        texture_buffer_bindings.push_back(bindings.texture);
        bindings.texture += desc.count;
    }
    texture_bindings.reserve(info.texture_descriptors.size());
    for (const auto& desc : info.texture_descriptors) {
        texture_bindings.push_back(bindings.texture);
        bindings.texture += desc.count;
    }
}

} // namespace Shader::Backend::GLASM
