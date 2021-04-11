// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {
namespace {

Id GetSharedPointer(EmitContext& ctx, Id offset, u32 index_offset = 0) {
    const Id shift_id{ctx.Constant(ctx.U32[1], 2U)};
    const Id shifted_value{ctx.OpShiftRightArithmetic(ctx.U32[1], offset, shift_id)};
    const Id index{ctx.OpIAdd(ctx.U32[1], shifted_value, ctx.Constant(ctx.U32[1], index_offset))};
    return ctx.profile.support_explicit_workgroup_layout
               ? ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, ctx.u32_zero_value, index)
               : ctx.OpAccessChain(ctx.shared_u32, ctx.shared_memory_u32, index);
}

Id StorageIndex(EmitContext& ctx, const IR::Value& offset, size_t element_size) {
    if (offset.IsImmediate()) {
        const u32 imm_offset{static_cast<u32>(offset.U32() / element_size)};
        return ctx.Constant(ctx.U32[1], imm_offset);
    }
    const u32 shift{static_cast<u32>(std::countr_zero(element_size))};
    const Id index{ctx.Def(offset)};
    if (shift == 0) {
        return index;
    }
    const Id shift_id{ctx.Constant(ctx.U32[1], shift)};
    return ctx.OpShiftRightLogical(ctx.U32[1], index, shift_id);
}

Id GetStoragePointer(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                     u32 index_offset = 0) {
    // TODO: Support reinterpreting bindings, guaranteed to be aligned
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id index{ctx.OpIAdd(ctx.U32[1], base_index, ctx.Constant(ctx.U32[1], index_offset))};
    return ctx.OpAccessChain(ctx.storage_u32, ssbo, ctx.u32_zero_value, index);
}

std::pair<Id, Id> GetAtomicArgs(EmitContext& ctx) {
    const Id scope{ctx.Constant(ctx.U32[1], static_cast<u32>(spv::Scope::Device))};
    const Id semantics{ctx.u32_zero_value};
    return {scope, semantics};
}

Id LoadU64(EmitContext& ctx, Id pointer_1, Id pointer_2) {
    const Id value_1{ctx.OpLoad(ctx.U32[1], pointer_1)};
    const Id value_2{ctx.OpLoad(ctx.U32[1], pointer_2)};
    const Id original_composite{ctx.OpCompositeConstruct(ctx.U32[2], value_1, value_2)};
    return ctx.OpBitcast(ctx.U64, original_composite);
}

void StoreResult(EmitContext& ctx, Id pointer_1, Id pointer_2, Id result) {
    const Id composite{ctx.OpBitcast(ctx.U32[2], result)};
    ctx.OpStore(pointer_1, ctx.OpCompositeExtract(ctx.U32[1], composite, 0));
    ctx.OpStore(pointer_2, ctx.OpCompositeExtract(ctx.U32[1], composite, 1));
}
} // Anonymous namespace

Id EmitSharedAtomicIAdd32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicIAdd(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicSMin32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicSMin(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicUMin32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicUMin(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicSMax32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicSMax(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicUMax32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicUMax(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicInc32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id shift_id{ctx.Constant(ctx.U32[1], 2U)};
    const Id index{ctx.OpShiftRightArithmetic(ctx.U32[1], pointer_offset, shift_id)};
    return ctx.OpFunctionCall(ctx.U32[1], ctx.increment_cas_shared, index, value,
                              ctx.shared_memory_u32);
}

Id EmitSharedAtomicDec32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id shift_id{ctx.Constant(ctx.U32[1], 2U)};
    const Id index{ctx.OpShiftRightArithmetic(ctx.U32[1], pointer_offset, shift_id)};
    return ctx.OpFunctionCall(ctx.U32[1], ctx.decrement_cas_shared, index, value,
                              ctx.shared_memory_u32);
}

Id EmitSharedAtomicAnd32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicAnd(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicOr32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicOr(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicXor32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicXor(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicExchange32(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer{GetSharedPointer(ctx, pointer_offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicExchange(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitSharedAtomicExchange64(EmitContext& ctx, Id pointer_offset, Id value) {
    const Id pointer_1{GetSharedPointer(ctx, pointer_offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicExchange(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetSharedPointer(ctx, pointer_offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    StoreResult(ctx, pointer_1, pointer_2, value);
    return original_value;
}

Id EmitStorageAtomicIAdd32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicIAdd(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicSMin32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicSMin(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicUMin32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicUMin(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicSMax32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicSMax(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicUMax32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicUMax(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicInc32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    return ctx.OpFunctionCall(ctx.U32[1], ctx.increment_cas_ssbo, base_index, value, ssbo);
}

Id EmitStorageAtomicDec32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    return ctx.OpFunctionCall(ctx.U32[1], ctx.decrement_cas_ssbo, base_index, value, ssbo);
}

Id EmitStorageAtomicAnd32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicAnd(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicOr32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicOr(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicXor32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicXor(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicExchange32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               Id value) {
    const Id pointer{GetStoragePointer(ctx, binding, offset)};
    const auto [scope, semantics]{GetAtomicArgs(ctx)};
    return ctx.OpAtomicExchange(ctx.U32[1], pointer, scope, semantics, value);
}

Id EmitStorageAtomicIAdd64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicIAdd(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    const Id result{ctx.OpIAdd(ctx.U64, value, original_value)};
    StoreResult(ctx, pointer_1, pointer_2, result);
    return original_value;
}

Id EmitStorageAtomicSMin64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicSMin(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    const Id result{ctx.OpSMin(ctx.U64, value, original_value)};
    StoreResult(ctx, pointer_1, pointer_2, result);
    return original_value;
}

Id EmitStorageAtomicUMin64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicUMin(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    const Id result{ctx.OpUMin(ctx.U64, value, original_value)};
    StoreResult(ctx, pointer_1, pointer_2, result);
    return original_value;
}

Id EmitStorageAtomicSMax64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicSMax(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    const Id result{ctx.OpSMax(ctx.U64, value, original_value)};
    StoreResult(ctx, pointer_1, pointer_2, result);
    return original_value;
}

Id EmitStorageAtomicUMax64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicUMax(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    const Id result{ctx.OpUMax(ctx.U64, value, original_value)};
    StoreResult(ctx, pointer_1, pointer_2, result);
    return original_value;
}

Id EmitStorageAtomicAnd64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicAnd(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    const Id result{ctx.OpBitwiseAnd(ctx.U64, value, original_value)};
    StoreResult(ctx, pointer_1, pointer_2, result);
    return original_value;
}

Id EmitStorageAtomicOr64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicOr(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    const Id result{ctx.OpBitwiseOr(ctx.U64, value, original_value)};
    StoreResult(ctx, pointer_1, pointer_2, result);
    return original_value;
}

Id EmitStorageAtomicXor64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                          Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicXor(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    const Id result{ctx.OpBitwiseXor(ctx.U64, value, original_value)};
    StoreResult(ctx, pointer_1, pointer_2, result);
    return original_value;
}

Id EmitStorageAtomicExchange64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                               Id value) {
    const Id pointer_1{GetStoragePointer(ctx, binding, offset)};
    if (ctx.profile.support_int64_atomics) {
        const auto [scope, semantics]{GetAtomicArgs(ctx)};
        return ctx.OpAtomicExchange(ctx.U64, pointer_1, scope, semantics, value);
    }
    // LOG_WARNING(Render_Vulkan, "Int64 Atomics not supported, fallback to non-atomic");
    const Id pointer_2{GetStoragePointer(ctx, binding, offset, 1)};
    const Id original_value{LoadU64(ctx, pointer_1, pointer_2)};
    StoreResult(ctx, pointer_1, pointer_2, value);
    return original_value;
}

Id EmitStorageAtomicAddF32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                           Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    return ctx.OpFunctionCall(ctx.F32[1], ctx.f32_add_cas, base_index, value, ssbo);
}

Id EmitStorageAtomicAddF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id result{ctx.OpFunctionCall(ctx.F16[2], ctx.f16x2_add_cas, base_index, value, ssbo)};
    return ctx.OpBitcast(ctx.U32[1], result);
}

Id EmitStorageAtomicAddF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id result{ctx.OpFunctionCall(ctx.F32[2], ctx.f32x2_add_cas, base_index, value, ssbo)};
    return ctx.OpPackHalf2x16(ctx.U32[1], result);
}

Id EmitStorageAtomicMinF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id result{ctx.OpFunctionCall(ctx.F16[2], ctx.f16x2_min_cas, base_index, value, ssbo)};
    return ctx.OpBitcast(ctx.U32[1], result);
}

Id EmitStorageAtomicMinF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id result{ctx.OpFunctionCall(ctx.F32[2], ctx.f32x2_min_cas, base_index, value, ssbo)};
    return ctx.OpPackHalf2x16(ctx.U32[1], result);
}

Id EmitStorageAtomicMaxF16x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id result{ctx.OpFunctionCall(ctx.F16[2], ctx.f16x2_max_cas, base_index, value, ssbo)};
    return ctx.OpBitcast(ctx.U32[1], result);
}

Id EmitStorageAtomicMaxF32x2(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                             Id value) {
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id result{ctx.OpFunctionCall(ctx.F32[2], ctx.f32x2_max_cas, base_index, value, ssbo)};
    return ctx.OpPackHalf2x16(ctx.U32[1], result);
}

Id EmitGlobalAtomicIAdd32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicSMin32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicUMin32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicSMax32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicUMax32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicInc32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicDec32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicAnd32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicOr32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicXor32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicExchange32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicIAdd64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicSMin64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicUMin64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicSMax64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicUMax64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicInc64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicDec64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicAnd64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicOr64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicXor64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicExchange64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicAddF32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicAddF16x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicAddF32x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicMinF16x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicMinF32x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicMaxF16x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitGlobalAtomicMaxF32x2(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
