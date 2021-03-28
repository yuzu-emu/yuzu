// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <bit>

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {
namespace {
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

Id EmitLoadStorage(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                   u32 num_components) {
    // TODO: Support reinterpreting bindings, guaranteed to be aligned
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    std::array<Id, 4> components;
    for (u32 element = 0; element < num_components; ++element) {
        Id index{base_index};
        if (element > 0) {
            index = ctx.OpIAdd(ctx.U32[1], base_index, ctx.Constant(ctx.U32[1], element));
        }
        const Id pointer{ctx.OpAccessChain(ctx.storage_u32, ssbo, ctx.u32_zero_value, index)};
        components[element] = ctx.OpLoad(ctx.U32[1], pointer);
    }
    if (num_components == 1) {
        return components[0];
    } else {
        const std::span components_span(components.data(), num_components);
        return ctx.OpCompositeConstruct(ctx.U32[num_components], components_span);
    }
}
} // Anonymous namespace

void EmitLoadGlobalU8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadGlobalS8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadGlobalU16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadGlobalS16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadGlobal32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadGlobal64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadGlobal128(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteGlobalU8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteGlobalS8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteGlobalU16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteGlobalS16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteGlobal32(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteGlobal64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteGlobal128(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadStorageU8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadStorageS8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadStorageU16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadStorageS16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

Id EmitLoadStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return EmitLoadStorage(ctx, binding, offset, 1);
}

Id EmitLoadStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return EmitLoadStorage(ctx, binding, offset, 2);
}

Id EmitLoadStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return EmitLoadStorage(ctx, binding, offset, 4);
}

void EmitWriteStorageU8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteStorageS8(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteStorageU16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteStorageS16(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id pointer{ctx.OpAccessChain(ctx.storage_u32, ssbo, ctx.u32_zero_value, index)};
    ctx.OpStore(pointer, value);
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    // TODO: Support reinterpreting bindings, guaranteed to be aligned
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id low_index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id high_index{ctx.OpIAdd(ctx.U32[1], low_index, ctx.Constant(ctx.U32[1], 1U))};
    const Id low_pointer{ctx.OpAccessChain(ctx.storage_u32, ssbo, ctx.u32_zero_value, low_index)};
    const Id high_pointer{ctx.OpAccessChain(ctx.storage_u32, ssbo, ctx.u32_zero_value, high_index)};
    ctx.OpStore(low_pointer, ctx.OpCompositeExtract(ctx.U32[1], value, 0U));
    ctx.OpStore(high_pointer, ctx.OpCompositeExtract(ctx.U32[1], value, 1U));
}

void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    // TODO: Support reinterpreting bindings, guaranteed to be aligned
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id base_index{StorageIndex(ctx, offset, sizeof(u32))};
    for (u32 element = 0; element < 4; ++element) {
        Id index = base_index;
        if (element > 0) {
            index = ctx.OpIAdd(ctx.U32[1], base_index, ctx.Constant(ctx.U32[1], element));
        }
        const Id pointer{ctx.OpAccessChain(ctx.storage_u32, ssbo, ctx.u32_zero_value, index)};
        ctx.OpStore(pointer, ctx.OpCompositeExtract(ctx.U32[1], value, element));
    }
}

} // namespace Shader::Backend::SPIRV