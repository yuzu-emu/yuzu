// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <bit>

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {

static Id StorageIndex(EmitContext& ctx, const IR::Value& offset, size_t element_size) {
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

Id EmitLoadStorage32(EmitContext& ctx, const IR::Value& binding,
                                const IR::Value& offset) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id pointer{ctx.OpAccessChain(ctx.storage_u32, ssbo, ctx.u32_zero_value, index)};
    return ctx.OpLoad(ctx.U32[1], pointer);
}

void EmitLoadStorage64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitLoadStorage128(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
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

void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding,
                                   const IR::Value& offset, Id value) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    const Id ssbo{ctx.ssbos[binding.U32()]};
    const Id index{StorageIndex(ctx, offset, sizeof(u32))};
    const Id pointer{ctx.OpAccessChain(ctx.storage_u32, ssbo, ctx.u32_zero_value, index)};
    ctx.OpStore(pointer, value);
}

void EmitWriteStorage64(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

void EmitWriteStorage128(EmitContext&) {
    throw NotImplementedException("SPIR-V Instruction");
}

} // namespace Shader::Backend::SPIRV
