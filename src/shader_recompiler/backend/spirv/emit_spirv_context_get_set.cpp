// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {
namespace {
Id InputAttrPointer(EmitContext& ctx, IR::Attribute attr) {
    const u32 element{static_cast<u32>(attr) % 4};
    const auto element_id{[&] { return ctx.Constant(ctx.U32[1], element); }};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        return ctx.OpAccessChain(ctx.input_f32, ctx.input_generics.at(index), element_id());
    }
    switch (attr) {
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        return ctx.OpAccessChain(ctx.input_f32, ctx.input_position, element_id());
    case IR::Attribute::InstanceId:
        return ctx.OpLoad(ctx.U32[1], ctx.instance_id);
    case IR::Attribute::VertexId:
        return ctx.OpLoad(ctx.U32[1], ctx.vertex_id);
    default:
        throw NotImplementedException("Read attribute {}", attr);
    }
}

Id OutputAttrPointer(EmitContext& ctx, IR::Attribute attr) {
    const u32 element{static_cast<u32>(attr) % 4};
    const auto element_id{[&] { return ctx.Constant(ctx.U32[1], element); }};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        return ctx.OpAccessChain(ctx.output_f32, ctx.output_generics.at(index), element_id());
    }
    switch (attr) {
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        return ctx.OpAccessChain(ctx.output_f32, ctx.output_position, element_id());
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
    if (!ctx.profile.support_vertex_instance_id) {
        switch (attr) {
        case IR::Attribute::InstanceId:
            return ctx.OpISub(ctx.U32[1], ctx.OpLoad(ctx.U32[1], ctx.instance_index),
                              ctx.OpLoad(ctx.U32[1], ctx.base_instance));
        case IR::Attribute::VertexId:
            return ctx.OpISub(ctx.U32[1], ctx.OpLoad(ctx.U32[1], ctx.vertex_index),
                              ctx.OpLoad(ctx.U32[1], ctx.base_vertex));
        default:
            break;
        }
    }
    return ctx.OpLoad(ctx.F32[1], InputAttrPointer(ctx, attr));
}

void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, Id value) {
    ctx.OpStore(OutputAttrPointer(ctx, attr), value);
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

Id EmitWorkgroupId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.workgroup_id);
}

Id EmitLocalInvocationId(EmitContext& ctx) {
    return ctx.OpLoad(ctx.U32[3], ctx.local_invocation_id);
}

} // namespace Shader::Backend::SPIRV
