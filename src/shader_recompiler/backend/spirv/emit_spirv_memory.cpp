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
        return ctx.Const(imm_offset);
    }
    const u32 shift{static_cast<u32>(std::countr_zero(element_size))};
    const Id index{ctx.Def(offset)};
    if (shift == 0) {
        return index;
    }
    const Id shift_id{ctx.Const(shift)};
    return ctx.OpShiftRightLogical(ctx.U32[1], index, shift_id);
}

Id StoragePointer(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                  const StorageTypeDefinition& type_def, size_t element_size,
                  Id StorageDefinitions::*member_ptr) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    const Id ssbo{ctx.ssbos[binding.U32()].*member_ptr};
    const Id index{StorageIndex(ctx, offset, element_size)};
    return ctx.OpAccessChain(type_def.element, ssbo, ctx.u32_zero_value, index);
}

Id LoadStorage(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset, Id result_type,
               const StorageTypeDefinition& type_def, size_t element_size,
               Id StorageDefinitions::*member_ptr) {
    const Id pointer{StoragePointer(ctx, binding, offset, type_def, element_size, member_ptr)};
    return ctx.OpLoad(result_type, pointer);
}

void WriteStorage(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset, Id value,
                  const StorageTypeDefinition& type_def, size_t element_size,
                  Id StorageDefinitions::*member_ptr) {
    const Id pointer{StoragePointer(ctx, binding, offset, type_def, element_size, member_ptr)};
    ctx.OpStore(pointer, value);
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

Id EmitLoadStorageU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return ctx.OpUConvert(ctx.U32[1],
                          LoadStorage(ctx, binding, offset, ctx.U8, ctx.storage_types.U8,
                                      sizeof(u8), &StorageDefinitions::U8));
}

Id EmitLoadStorageS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return ctx.OpSConvert(ctx.U32[1],
                          LoadStorage(ctx, binding, offset, ctx.S8, ctx.storage_types.S8,
                                      sizeof(s8), &StorageDefinitions::S8));
}

Id EmitLoadStorageU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return ctx.OpUConvert(ctx.U32[1],
                          LoadStorage(ctx, binding, offset, ctx.U16, ctx.storage_types.U16,
                                      sizeof(u16), &StorageDefinitions::U16));
}

Id EmitLoadStorageS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return ctx.OpSConvert(ctx.U32[1],
                          LoadStorage(ctx, binding, offset, ctx.S16, ctx.storage_types.S16,
                                      sizeof(s16), &StorageDefinitions::S16));
}

Id EmitLoadStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return LoadStorage(ctx, binding, offset, ctx.U32[1], ctx.storage_types.U32, sizeof(u32),
                       &StorageDefinitions::U32);
}

Id EmitLoadStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return LoadStorage(ctx, binding, offset, ctx.U32[2], ctx.storage_types.U32x2, sizeof(u32[2]),
                       &StorageDefinitions::U32x2);
}

Id EmitLoadStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return LoadStorage(ctx, binding, offset, ctx.U32[4], ctx.storage_types.U32x4, sizeof(u32[4]),
                       &StorageDefinitions::U32x4);
}

void EmitWriteStorageU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value) {
    WriteStorage(ctx, binding, offset, ctx.OpSConvert(ctx.U8, value), ctx.storage_types.U8,
                 sizeof(u8), &StorageDefinitions::U8);
}

void EmitWriteStorageS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value) {
    WriteStorage(ctx, binding, offset, ctx.OpSConvert(ctx.S8, value), ctx.storage_types.S8,
                 sizeof(s8), &StorageDefinitions::S8);
}

void EmitWriteStorageU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value) {
    WriteStorage(ctx, binding, offset, ctx.OpSConvert(ctx.U16, value), ctx.storage_types.U16,
                 sizeof(u16), &StorageDefinitions::U16);
}

void EmitWriteStorageS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value) {
    WriteStorage(ctx, binding, offset, ctx.OpSConvert(ctx.S16, value), ctx.storage_types.S16,
                 sizeof(s16), &StorageDefinitions::S16);
}

void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value) {
    WriteStorage(ctx, binding, offset, value, ctx.storage_types.U32, sizeof(u32),
                 &StorageDefinitions::U32);
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value) {
    WriteStorage(ctx, binding, offset, value, ctx.storage_types.U32x2, sizeof(u32[2]),
                 &StorageDefinitions::U32x2);
}

void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value) {
    WriteStorage(ctx, binding, offset, value, ctx.storage_types.U32x4, sizeof(u32[4]),
                 &StorageDefinitions::U32x4);
}

} // namespace Shader::Backend::SPIRV
