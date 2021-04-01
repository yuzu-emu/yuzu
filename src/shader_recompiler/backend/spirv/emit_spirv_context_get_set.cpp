// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

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

std::optional<Id> OutputAttrPointer(EmitContext& ctx, IR::Attribute attr) {
    const u32 element{static_cast<u32>(attr) % 4};
    const auto element_id{[&] { return ctx.Constant(ctx.U32[1], element); }};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        return ctx.OpAccessChain(ctx.output_f32, ctx.output_generics.at(index), element_id());
    }
    switch (attr) {
    case IR::Attribute::PointSize:
        return ctx.output_point_size;
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        return ctx.OpAccessChain(ctx.output_f32, ctx.output_position, element_id());
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
        return ctx.OpAccessChain(ctx.output_f32, ctx.clip_distances, clip_num);
    }
    case IR::Attribute::ViewportIndex:
        return ctx.ignore_viewport_layer ? std::nullopt : std::optional<Id>{ctx.viewport_index};
    default:
        throw NotImplementedException("Read attribute {}", attr);
    }
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

static Id GetCbuf(EmitContext& ctx, Id result_type, Id UniformDefinitions::*member_ptr,
                  u32 element_size, const IR::Value& binding, const IR::Value& offset) {
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

Id EmitGetCbufU64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return GetCbuf(ctx, ctx.U64, &UniformDefinitions::U64, sizeof(u64), binding, offset);
}

Id EmitGetAttribute(EmitContext& ctx, IR::Attribute attr) {
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
        const Id pointer{ctx.OpAccessChain(type->pointer, generic_id, element_id())};
        const Id value{ctx.OpLoad(type->id, pointer)};
        return type->needs_cast ? ctx.OpBitcast(ctx.F32[1], value) : value;
    }
    switch (attr) {
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        return ctx.OpLoad(ctx.F32[1],
                          ctx.OpAccessChain(ctx.input_f32, ctx.input_position, element_id()));
    case IR::Attribute::InstanceId:
        if (ctx.profile.support_vertex_instance_id) {
            return ctx.OpLoad(ctx.U32[1], ctx.instance_id);
        } else {
            return ctx.OpISub(ctx.U32[1], ctx.OpLoad(ctx.U32[1], ctx.instance_index),
                              ctx.OpLoad(ctx.U32[1], ctx.base_instance));
        }
    case IR::Attribute::VertexId:
        if (ctx.profile.support_vertex_instance_id) {
            return ctx.OpLoad(ctx.U32[1], ctx.vertex_id);
        } else {
            return ctx.OpISub(ctx.U32[1], ctx.OpLoad(ctx.U32[1], ctx.vertex_index),
                              ctx.OpLoad(ctx.U32[1], ctx.base_vertex));
        }
    case IR::Attribute::FrontFace:
        return ctx.OpSelect(ctx.U32[1], ctx.OpLoad(ctx.U1, ctx.front_face),
                            ctx.Constant(ctx.U32[1], std::numeric_limits<u32>::max()),
                            ctx.u32_zero_value);
    case IR::Attribute::PointSpriteS:
        return ctx.OpLoad(ctx.F32[1], ctx.OpAccessChain(ctx.input_f32, ctx.point_coord,
                                                        ctx.Constant(ctx.U32[1], 0U)));
    case IR::Attribute::PointSpriteT:
        return ctx.OpLoad(ctx.F32[1], ctx.OpAccessChain(ctx.input_f32, ctx.point_coord,
                                                        ctx.Constant(ctx.U32[1], 1U)));
    default:
        throw NotImplementedException("Read attribute {}", attr);
    }
}

void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, Id value) {
    auto output = OutputAttrPointer(ctx, attr);
    if (!output) {
        return;
    }
    ctx.OpStore(*output, value);
}

void EmitGetAttributeIndexed(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetAttributeIndexed(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
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

void EmitGetFCSMFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetTAFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetTRFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitGetMXFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetFCSMFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetTAFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetTRFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitSetMXFlag(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitWorkgroupId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.workgroup_id);
}

Id EmitLocalInvocationId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.local_invocation_id);
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
