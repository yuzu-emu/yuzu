// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"

namespace Shader::Backend::SPIRV {
namespace {
Id Pointer(EmitContext& ctx, Id pointer_type, Id array, Id offset, u32 shift) {
    const Id shift_id{ctx.Const(shift)};
    const Id index{ctx.OpShiftRightArithmetic(ctx.U32[1], offset, shift_id)};
    return ctx.OpAccessChain(pointer_type, array, ctx.u32_zero_value, index);
}

Id Word(EmitContext& ctx, Id offset) {
    const Id shift_id{ctx.Const(2U)};
    const Id index{ctx.OpShiftRightArithmetic(ctx.U32[1], offset, shift_id)};
    const Id pointer{ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, index)};
    return ctx.OpLoad(ctx.U32[1], pointer);
}

std::pair<Id, Id> ExtractArgs(EmitContext& ctx, Id offset, u32 mask, u32 count) {
    const Id shift{ctx.OpShiftLeftLogical(ctx.U32[1], offset, ctx.Const(3U))};
    const Id bit{ctx.OpBitwiseAnd(ctx.U32[1], shift, ctx.Const(mask))};
    const Id count_id{ctx.Const(count)};
    return {bit, count_id};
}
} // Anonymous namespace

Id EmitLoadSharedU8(EmitContext& ctx, Id offset) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{
            ctx.OpAccessChain(ctx.shared_u8, ctx.shared_memory_u8, ctx.u32_zero_value, offset)};
        return ctx.OpUConvert(ctx.U32[1], ctx.OpLoad(ctx.U8, pointer));
    } else {
        const auto [bit, count]{ExtractArgs(ctx, offset, 24, 8)};
        return ctx.OpBitFieldUExtract(ctx.U32[1], Word(ctx, offset), bit, count);
    }
}

Id EmitLoadSharedS8(EmitContext& ctx, Id offset) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{
            ctx.OpAccessChain(ctx.shared_u8, ctx.shared_memory_u8, ctx.u32_zero_value, offset)};
        return ctx.OpSConvert(ctx.U32[1], ctx.OpLoad(ctx.U8, pointer));
    } else {
        const auto [bit, count]{ExtractArgs(ctx, offset, 24, 8)};
        return ctx.OpBitFieldSExtract(ctx.U32[1], Word(ctx, offset), bit, count);
    }
}

Id EmitLoadSharedU16(EmitContext& ctx, Id offset) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{Pointer(ctx, ctx.shared_u16, ctx.shared_memory_u16, offset, 1)};
        return ctx.OpUConvert(ctx.U32[1], ctx.OpLoad(ctx.U16, pointer));
    } else {
        const auto [bit, count]{ExtractArgs(ctx, offset, 16, 16)};
        return ctx.OpBitFieldUExtract(ctx.U32[1], Word(ctx, offset), bit, count);
    }
}

Id EmitLoadSharedS16(EmitContext& ctx, Id offset) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{Pointer(ctx, ctx.shared_u16, ctx.shared_memory_u16, offset, 1)};
        return ctx.OpSConvert(ctx.U32[1], ctx.OpLoad(ctx.U16, pointer));
    } else {
        const auto [bit, count]{ExtractArgs(ctx, offset, 16, 16)};
        return ctx.OpBitFieldSExtract(ctx.U32[1], Word(ctx, offset), bit, count);
    }
}

Id EmitLoadSharedU32(EmitContext& ctx, Id offset) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{Pointer(ctx, ctx.shared_u32, ctx.shared_memory_u32, offset, 2)};
        return ctx.OpLoad(ctx.U32[1], pointer);
    } else {
        return Word(ctx, offset);
    }
}

Id EmitLoadSharedU64(EmitContext& ctx, Id offset) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{Pointer(ctx, ctx.shared_u32x2, ctx.shared_memory_u32x2, offset, 3)};
        return ctx.OpLoad(ctx.U32[2], pointer);
    } else {
        const Id shift_id{ctx.Const(2U)};
        const Id base_index{ctx.OpShiftRightArithmetic(ctx.U32[1], offset, shift_id)};
        const Id next_index{ctx.OpIAdd(ctx.U32[1], base_index, ctx.Const(1U))};
        const Id lhs_pointer{ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, base_index)};
        const Id rhs_pointer{ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, next_index)};
        return ctx.OpCompositeConstruct(ctx.U32[2], ctx.OpLoad(ctx.U32[1], lhs_pointer),
                                        ctx.OpLoad(ctx.U32[1], rhs_pointer));
    }
}

Id EmitLoadSharedU128(EmitContext& ctx, Id offset) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{Pointer(ctx, ctx.shared_u32x4, ctx.shared_memory_u32x4, offset, 4)};
        return ctx.OpLoad(ctx.U32[4], pointer);
    }
    const Id shift_id{ctx.Const(2U)};
    const Id base_index{ctx.OpShiftRightArithmetic(ctx.U32[1], offset, shift_id)};
    std::array<Id, 4> values{};
    for (u32 i = 0; i < 4; ++i) {
        const Id index{i == 0 ? base_index : ctx.OpIAdd(ctx.U32[1], base_index, ctx.Const(i))};
        const Id pointer{ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, index)};
        values[i] = ctx.OpLoad(ctx.U32[1], pointer);
    }
    return ctx.OpCompositeConstruct(ctx.U32[4], values);
}

void EmitWriteSharedU8(EmitContext& ctx, Id offset, Id value) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{
            ctx.OpAccessChain(ctx.shared_u8, ctx.shared_memory_u8, ctx.u32_zero_value, offset)};
        ctx.OpStore(pointer, ctx.OpUConvert(ctx.U8, value));
    } else {
        ctx.OpFunctionCall(ctx.void_id, ctx.shared_store_u8_func, offset, value);
    }
}

void EmitWriteSharedU16(EmitContext& ctx, Id offset, Id value) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{Pointer(ctx, ctx.shared_u16, ctx.shared_memory_u16, offset, 1)};
        ctx.OpStore(pointer, ctx.OpUConvert(ctx.U16, value));
    } else {
        ctx.OpFunctionCall(ctx.void_id, ctx.shared_store_u16_func, offset, value);
    }
}

void EmitWriteSharedU32(EmitContext& ctx, Id offset, Id value) {
    Id pointer{};
    if (ctx.profile.support_explicit_workgroup_layout) {
        pointer = Pointer(ctx, ctx.shared_u32, ctx.shared_memory_u32, offset, 2);
    } else {
        const Id shift{ctx.Const(2U)};
        const Id word_offset{ctx.OpShiftRightArithmetic(ctx.U32[1], offset, shift)};
        pointer = ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, word_offset);
    }
    ctx.OpStore(pointer, value);
}

void EmitWriteSharedU64(EmitContext& ctx, Id offset, Id value) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{Pointer(ctx, ctx.shared_u32x2, ctx.shared_memory_u32x2, offset, 3)};
        ctx.OpStore(pointer, value);
        return;
    }
    const Id shift{ctx.Const(2U)};
    const Id word_offset{ctx.OpShiftRightArithmetic(ctx.U32[1], offset, shift)};
    const Id next_offset{ctx.OpIAdd(ctx.U32[1], word_offset, ctx.Const(1U))};
    const Id lhs_pointer{ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, word_offset)};
    const Id rhs_pointer{ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, next_offset)};
    ctx.OpStore(lhs_pointer, ctx.OpCompositeExtract(ctx.U32[1], value, 0U));
    ctx.OpStore(rhs_pointer, ctx.OpCompositeExtract(ctx.U32[1], value, 1U));
}

void EmitWriteSharedU128(EmitContext& ctx, Id offset, Id value) {
    if (ctx.profile.support_explicit_workgroup_layout) {
        const Id pointer{Pointer(ctx, ctx.shared_u32x4, ctx.shared_memory_u32x4, offset, 4)};
        ctx.OpStore(pointer, value);
        return;
    }
    const Id shift{ctx.Const(2U)};
    const Id base_index{ctx.OpShiftRightArithmetic(ctx.U32[1], offset, shift)};
    for (u32 i = 0; i < 4; ++i) {
        const Id index{i == 0 ? base_index : ctx.OpIAdd(ctx.U32[1], base_index, ctx.Const(i))};
        const Id pointer{ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, index)};
        ctx.OpStore(pointer, ctx.OpCompositeExtract(ctx.U32[1], value, i));
    }
}

} // namespace Shader::Backend::SPIRV
