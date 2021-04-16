// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include <utility>

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {
namespace {
struct AttrInfo {
    Id pointer;
    Id id;
    bool needs_cast;
};

std::optional<AttrInfo> AttrTypes(EmitContext& ctx, u32 index) {
    const AttributeType type{ctx.profile.generic_input_types.at(index)};
    switch (type) {
    case AttributeType::Float:
        return AttrInfo{ctx.input_f32, ctx.F32[1], false};
    case AttributeType::UnsignedInt:
        return AttrInfo{ctx.input_u32, ctx.U32[1], true};
    case AttributeType::SignedInt:
        return AttrInfo{ctx.input_s32, ctx.TypeInt(32, true), true};
    case AttributeType::Disabled:
        return std::nullopt;
    }
    throw InvalidArgument("Invalid attribute type {}", type);
}

template <typename... Args>
Id AttrPointer(EmitContext& ctx, Id pointer_type, Id vertex, Id base, Args&&... args) {
    switch (ctx.stage) {
    case Stage::TessellationControl:
    case Stage::TessellationEval:
    case Stage::Geometry:
        return ctx.OpAccessChain(pointer_type, base, vertex, std::forward<Args>(args)...);
    default:
        return ctx.OpAccessChain(pointer_type, base, std::forward<Args>(args)...);
    }
}

template <typename... Args>
Id OutputAccessChain(EmitContext& ctx, Id result_type, Id base, Args&&... args) {
    if (ctx.stage == Stage::TessellationControl) {
        const Id invocation_id{ctx.OpLoad(ctx.U32[1], ctx.invocation_id)};
        return ctx.OpAccessChain(result_type, base, invocation_id, std::forward<Args>(args)...);
    } else {
        return ctx.OpAccessChain(result_type, base, std::forward<Args>(args)...);
    }
}

std::optional<Id> OutputAttrPointer(EmitContext& ctx, IR::Attribute attr) {
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        const u32 element{IR::GenericAttributeElement(attr)};
        const GenericElementInfo& info{ctx.output_generics.at(index).at(element)};
        if (info.num_components == 1) {
            return info.id;
        } else {
            const u32 index_element{element - info.first_element};
            const Id index_id{ctx.Constant(ctx.U32[1], index_element)};
            return OutputAccessChain(ctx, ctx.output_f32, info.id, index_id);
        }
    }
    switch (attr) {
    case IR::Attribute::PointSize:
        return ctx.output_point_size;
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW: {
        const u32 element{static_cast<u32>(attr) % 4};
        const Id element_id{ctx.Constant(ctx.U32[1], element)};
        return OutputAccessChain(ctx, ctx.output_f32, ctx.output_position, element_id);
    }
    case IR::Attribute::ClipDistance0:
    case IR::Attribute::ClipDistance1:
    case IR::Attribute::ClipDistance2:
    case IR::Attribute::ClipDistance3:
    case IR::Attribute::ClipDistance4:
    case IR::Attribute::ClipDistance5:
    case IR::Attribute::ClipDistance6:
    case IR::Attribute::ClipDistance7: {
        const u32 base{static_cast<u32>(IR::Attribute::ClipDistance0)};
        const u32 index{static_cast<u32>(attr) - base};
        const Id clip_num{ctx.Constant(ctx.U32[1], index)};
        return OutputAccessChain(ctx, ctx.output_f32, ctx.clip_distances, clip_num);
    }
    case IR::Attribute::Layer:
        return ctx.profile.support_viewport_index_layer_non_geometry ||
                       ctx.stage == Shader::Stage::Geometry
                   ? std::optional<Id>{ctx.layer}
                   : std::nullopt;
    case IR::Attribute::ViewportIndex:
        return ctx.profile.support_viewport_index_layer_non_geometry ||
                       ctx.stage == Shader::Stage::Geometry
                   ? std::optional<Id>{ctx.viewport_index}
                   : std::nullopt;
    case IR::Attribute::ViewportMask:
        if (!ctx.profile.support_viewport_mask) {
            return std::nullopt;
        }
        return ctx.OpAccessChain(ctx.output_u32, ctx.viewport_mask, ctx.u32_zero_value);
    default:
        throw NotImplementedException("Read attribute {}", attr);
    }
}

Id GetCbuf(EmitContext& ctx, Id result_type, Id UniformDefinitions::*member_ptr, u32 element_size,
           const IR::Value& binding, const IR::Value& offset) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Constant buffer indexing");
    }
    const Id cbuf{ctx.cbufs[binding.U32()].*member_ptr};
    const Id uniform_type{ctx.uniform_types.*member_ptr};
    if (!offset.IsImmediate()) {
        Id index{ctx.Def(offset)};
        if (element_size > 1) {
            const u32 log2_element_size{static_cast<u32>(std::countr_zero(element_size))};
            const Id shift{ctx.Constant(ctx.U32[1], log2_element_size)};
            index = ctx.OpShiftRightArithmetic(ctx.U32[1], ctx.Def(offset), shift);
        }
        const Id access_chain{ctx.OpAccessChain(uniform_type, cbuf, ctx.u32_zero_value, index)};
        return ctx.OpLoad(result_type, access_chain);
    }
    if (offset.U32() % element_size != 0) {
        throw NotImplementedException("Unaligned immediate constant buffer load");
    }
    const Id imm_offset{ctx.Constant(ctx.U32[1], offset.U32() / element_size)};
    const Id access_chain{ctx.OpAccessChain(uniform_type, cbuf, ctx.u32_zero_value, imm_offset)};
    return ctx.OpLoad(result_type, access_chain);
}
} // Anonymous namespace

void EmitGetRegister(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetRegister(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetPred(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetPred(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetGotoVariable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetGotoVariable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetIndirectBranchVariable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetIndirectBranchVariable(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGetCbufU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    const Id load{GetCbuf(ctx, ctx.U8, &UniformDefinitions::U8, sizeof(u8), binding, offset)};
    return ctx.OpUConvert(ctx.U32[1], load);
}

Id EmitGetCbufS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    const Id load{GetCbuf(ctx, ctx.S8, &UniformDefinitions::S8, sizeof(s8), binding, offset)};
    return ctx.OpSConvert(ctx.U32[1], load);
}

Id EmitGetCbufU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    const Id load{GetCbuf(ctx, ctx.U16, &UniformDefinitions::U16, sizeof(u16), binding, offset)};
    return ctx.OpUConvert(ctx.U32[1], load);
}

Id EmitGetCbufS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    const Id load{GetCbuf(ctx, ctx.S16, &UniformDefinitions::S16, sizeof(s16), binding, offset)};
    return ctx.OpSConvert(ctx.U32[1], load);
}

Id EmitGetCbufU32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return GetCbuf(ctx, ctx.U32[1], &UniformDefinitions::U32, sizeof(u32), binding, offset);
}

Id EmitGetCbufF32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return GetCbuf(ctx, ctx.F32[1], &UniformDefinitions::F32, sizeof(f32), binding, offset);
}

Id EmitGetCbufU32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return GetCbuf(ctx, ctx.U32[2], &UniformDefinitions::U32x2, sizeof(u32[2]), binding, offset);
}

Id EmitGetAttribute(EmitContext& ctx, IR::Attribute attr, Id vertex) {
    const u32 element{static_cast<u32>(attr) % 4};
    const auto element_id{[&] { return ctx.Constant(ctx.U32[1], element); }};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        const std::optional<AttrInfo> type{AttrTypes(ctx, index)};
        if (!type) {
            // Attribute is disabled
            return ctx.Constant(ctx.F32[1], 0.0f);
        }
        const Id generic_id{ctx.input_generics.at(index)};
        const Id pointer{AttrPointer(ctx, type->pointer, vertex, generic_id, element_id())};
        const Id value{ctx.OpLoad(type->id, pointer)};
        return type->needs_cast ? ctx.OpBitcast(ctx.F32[1], value) : value;
    }
    switch (attr) {
    case IR::Attribute::PrimitiveId:
        return ctx.OpBitcast(ctx.F32[1], ctx.OpLoad(ctx.U32[1], ctx.primitive_id));
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        return ctx.OpLoad(
            ctx.F32[1], AttrPointer(ctx, ctx.input_f32, vertex, ctx.input_position, element_id()));
    case IR::Attribute::InstanceId:
        if (ctx.profile.support_vertex_instance_id) {
            return ctx.OpBitcast(ctx.F32[1], ctx.OpLoad(ctx.U32[1], ctx.instance_id));
        } else {
            const Id index{ctx.OpLoad(ctx.U32[1], ctx.instance_index)};
            const Id base{ctx.OpLoad(ctx.U32[1], ctx.base_instance)};
            return ctx.OpBitcast(ctx.F32[1], ctx.OpISub(ctx.U32[1], index, base));
        }
    case IR::Attribute::VertexId:
        if (ctx.profile.support_vertex_instance_id) {
            return ctx.OpBitcast(ctx.F32[1], ctx.OpLoad(ctx.U32[1], ctx.vertex_id));
        } else {
            const Id index{ctx.OpLoad(ctx.U32[1], ctx.vertex_index)};
            const Id base{ctx.OpLoad(ctx.U32[1], ctx.base_vertex)};
            return ctx.OpBitcast(ctx.F32[1], ctx.OpISub(ctx.U32[1], index, base));
        }
    case IR::Attribute::FrontFace:
        return ctx.OpSelect(ctx.U32[1], ctx.OpLoad(ctx.U1, ctx.front_face),
                            ctx.Constant(ctx.U32[1], std::numeric_limits<u32>::max()),
                            ctx.u32_zero_value);
    case IR::Attribute::PointSpriteS:
        return ctx.OpLoad(ctx.F32[1],
                          ctx.OpAccessChain(ctx.input_f32, ctx.point_coord, ctx.u32_zero_value));
    case IR::Attribute::PointSpriteT:
        return ctx.OpLoad(ctx.F32[1], ctx.OpAccessChain(ctx.input_f32, ctx.point_coord,
                                                        ctx.Constant(ctx.U32[1], 1U)));
    case IR::Attribute::TessellationEvaluationPointU:
        return ctx.OpLoad(ctx.F32[1],
                          ctx.OpAccessChain(ctx.input_f32, ctx.tess_coord, ctx.u32_zero_value));
    case IR::Attribute::TessellationEvaluationPointV:
        return ctx.OpLoad(ctx.F32[1], ctx.OpAccessChain(ctx.input_f32, ctx.tess_coord,
                                                        ctx.Constant(ctx.U32[1], 1U)));

    default:
        throw NotImplementedException("Read attribute {}", attr);
    }
}

void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, Id value, [[maybe_unused]] Id vertex) {
    const std::optional<Id> output{OutputAttrPointer(ctx, attr)};
    if (output) {
        ctx.OpStore(*output, value);
    }
}

Id EmitGetAttributeIndexed(EmitContext& ctx, Id offset, Id vertex) {
    switch (ctx.stage) {
    case Stage::TessellationControl:
    case Stage::TessellationEval:
    case Stage::Geometry:
        return ctx.OpFunctionCall(ctx.F32[1], ctx.indexed_load_func, offset, vertex);
    default:
        return ctx.OpFunctionCall(ctx.F32[1], ctx.indexed_load_func, offset);
    }
}

void EmitSetAttributeIndexed(EmitContext& ctx, Id offset, Id value, [[maybe_unused]] Id vertex) {
    ctx.OpFunctionCall(ctx.void_id, ctx.indexed_store_func, offset, value);
}

Id EmitGetPatch(EmitContext& ctx, IR::Patch patch) {
    if (!IR::IsGeneric(patch)) {
        throw NotImplementedException("Non-generic patch load");
    }
    const u32 index{IR::GenericPatchIndex(patch)};
    const Id element{ctx.Constant(ctx.U32[1], IR::GenericPatchElement(patch))};
    const Id pointer{ctx.OpAccessChain(ctx.input_f32, ctx.patches.at(index), element)};
    return ctx.OpLoad(ctx.F32[1], pointer);
}

void EmitSetPatch(EmitContext& ctx, IR::Patch patch, Id value) {
    const Id pointer{[&] {
        if (IR::IsGeneric(patch)) {
            const u32 index{IR::GenericPatchIndex(patch)};
            const Id element{ctx.Constant(ctx.U32[1], IR::GenericPatchElement(patch))};
            return ctx.OpAccessChain(ctx.output_f32, ctx.patches.at(index), element);
        }
        switch (patch) {
        case IR::Patch::TessellationLodLeft:
        case IR::Patch::TessellationLodRight:
        case IR::Patch::TessellationLodTop:
        case IR::Patch::TessellationLodBottom: {
            const u32 index{static_cast<u32>(patch) - u32(IR::Patch::TessellationLodLeft)};
            const Id index_id{ctx.Constant(ctx.U32[1], index)};
            return ctx.OpAccessChain(ctx.output_f32, ctx.output_tess_level_outer, index_id);
        }
        case IR::Patch::TessellationLodInteriorU:
            return ctx.OpAccessChain(ctx.output_f32, ctx.output_tess_level_inner,
                                     ctx.u32_zero_value);
        case IR::Patch::TessellationLodInteriorV:
            return ctx.OpAccessChain(ctx.output_f32, ctx.output_tess_level_inner,
                                     ctx.Constant(ctx.U32[1], 1u));
        default:
            throw NotImplementedException("Patch {}", patch);
        }
    }()};
    ctx.OpStore(pointer, value);
}

void EmitSetFragColor(EmitContext& ctx, u32 index, u32 component, Id value) {
    const Id component_id{ctx.Constant(ctx.U32[1], component)};
    const Id pointer{ctx.OpAccessChain(ctx.output_f32, ctx.frag_color.at(index), component_id)};
    ctx.OpStore(pointer, value);
}

void EmitSetFragDepth(EmitContext& ctx, Id value) {
    ctx.OpStore(ctx.frag_depth, value);
}

void EmitGetZFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetSFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetCFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetOFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetZFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetSFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetCFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetOFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitWorkgroupId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.workgroup_id);
}

Id EmitLocalInvocationId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.local_invocation_id);
}

Id EmitInvocationId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[1], ctx.invocation_id);
}

Id EmitIsHelperInvocation(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U1, ctx.is_helper_invocation);
}

Id EmitLoadLocal(EmitContext& ctx, Id word_offset) {
    const Id pointer{ctx.OpAccessChain(ctx.private_u32, ctx.local_memory, word_offset)};
    return ctx.OpLoad(ctx.U32[1], pointer);
}

void EmitWriteLocal(EmitContext& ctx, Id word_offset, Id value) {
    const Id pointer{ctx.OpAccessChain(ctx.private_u32, ctx.local_memory, word_offset)};
    ctx.OpStore(pointer, value);
}

} // namespace Shader::Backend::SPIRV
