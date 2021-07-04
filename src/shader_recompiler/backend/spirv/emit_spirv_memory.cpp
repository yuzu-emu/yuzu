// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <bit>

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"

namespace Shader::Backend::SPIRV {
namespace {
Id StorageIndex(EmitContext& ctx, const IR::Value& offset, size_t element_size,
                u32 index_offset = 0) {
    if (offset.IsImmediate()) {
        const u32 imm_offset{static_cast<u32>(offset.U32() / element_size) + index_offset};
        return ctx.Const(imm_offset);
    }
    const u32 shift{static_cast<u32>(std::countr_zero(element_size))};
    Id index{ctx.Def(offset)};
    if (shift != 0) {
        const Id shift_id{ctx.Const(shift)};
        index = ctx.OpShiftRightLogical(ctx.U32[1], index, shift_id);
    }
    if (index_offset != 0) {
        index = ctx.OpIAdd(ctx.U32[1], index, ctx.Const(index_offset));
    }
    return index;
}

Id StoragePointer(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                  const StorageTypeDefinition& type_def, size_t element_size,
                  Id StorageDefinitions::*member_ptr, u32 index_offset = 0) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Dynamic storage buffer indexing");
    }
    const Id ssbo{ctx.ssbos[binding.U32()].*member_ptr};
    const Id index{StorageIndex(ctx, offset, element_size, index_offset)};
    return ctx.OpAccessChain(type_def.element, ssbo, ctx.u32_zero_value, index);
}

Id LoadStorage(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset, Id result_type,
               const StorageTypeDefinition& type_def, size_t element_size,
               Id StorageDefinitions::*member_ptr, u32 index_offset = 0) {
    const Id pointer{
        StoragePointer(ctx, binding, offset, type_def, element_size, member_ptr, index_offset)};
    return ctx.OpLoad(result_type, pointer);
}

Id LoadStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                 u32 index_offset = 0) {
    return LoadStorage(ctx, binding, offset, ctx.U32[1], ctx.storage_types.U32, sizeof(u32),
                       &StorageDefinitions::U32, index_offset);
}

void WriteStorage(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset, Id value,
                  const StorageTypeDefinition& type_def, size_t element_size,
                  Id StorageDefinitions::*member_ptr, u32 index_offset = 0) {
    const Id pointer{
        StoragePointer(ctx, binding, offset, type_def, element_size, member_ptr, index_offset)};
    ctx.OpStore(pointer, value);
}

void WriteStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset, Id value,
                    u32 index_offset = 0) {
    WriteStorage(ctx, binding, offset, value, ctx.storage_types.U32, sizeof(u32),
                 &StorageDefinitions::U32, index_offset);
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

Id EmitLoadGlobal32(EmitContext& ctx, Id address) {
    if (ctx.profile.support_int64) {
        return ctx.OpFunctionCall(ctx.U32[1], ctx.load_global_func_u32, address);
    }
    LOG_WARNING(Shader_SPIRV, "Int64 not supported, ignoring memory operation");
    return ctx.Const(0u);
}

Id EmitLoadGlobal64(EmitContext& ctx, Id address) {
    if (ctx.profile.support_int64) {
        return ctx.OpFunctionCall(ctx.U32[2], ctx.load_global_func_u32x2, address);
    }
    LOG_WARNING(Shader_SPIRV, "Int64 not supported, ignoring memory operation");
    return ctx.Const(0u, 0u);
}

Id EmitLoadGlobal128(EmitContext& ctx, Id address) {
    if (ctx.profile.support_int64) {
        return ctx.OpFunctionCall(ctx.U32[4], ctx.load_global_func_u32x4, address);
    }
    LOG_WARNING(Shader_SPIRV, "Int64 not supported, ignoring memory operation");
    return ctx.Const(0u, 0u, 0u, 0u);
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

void EmitWriteGlobal32(EmitContext& ctx, Id address, Id value) {
    if (ctx.profile.support_int64) {
        ctx.OpFunctionCall(ctx.void_id, ctx.write_global_func_u32, address, value);
        return;
    }
    LOG_WARNING(Shader_SPIRV, "Int64 not supported, ignoring memory operation");
}

void EmitWriteGlobal64(EmitContext& ctx, Id address, Id value) {
    if (ctx.profile.support_int64) {
        ctx.OpFunctionCall(ctx.void_id, ctx.write_global_func_u32x2, address, value);
        return;
    }
    LOG_WARNING(Shader_SPIRV, "Int64 not supported, ignoring memory operation");
}

void EmitWriteGlobal128(EmitContext& ctx, Id address, Id value) {
    if (ctx.profile.support_int64) {
        ctx.OpFunctionCall(ctx.void_id, ctx.write_global_func_u32x4, address, value);
        return;
    }
    LOG_WARNING(Shader_SPIRV, "Int64 not supported, ignoring memory operation");
}

Id EmitLoadStorageU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    if (ctx.profile.support_int8 && ctx.profile.support_descriptor_aliasing) {
        return ctx.OpUConvert(ctx.U32[1],
                              LoadStorage(ctx, binding, offset, ctx.U8, ctx.storage_types.U8,
                                          sizeof(u8), &StorageDefinitions::U8));
    } else {
        return ctx.OpBitFieldUExtract(ctx.U32[1], LoadStorage32(ctx, binding, offset),
                                      ctx.BitOffset8(offset), ctx.Const(8u));
    }
}

Id EmitLoadStorageS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    if (ctx.profile.support_int8 && ctx.profile.support_descriptor_aliasing) {
        return ctx.OpSConvert(ctx.U32[1],
                              LoadStorage(ctx, binding, offset, ctx.S8, ctx.storage_types.S8,
                                          sizeof(s8), &StorageDefinitions::S8));
    } else {
        return ctx.OpBitFieldSExtract(ctx.U32[1], LoadStorage32(ctx, binding, offset),
                                      ctx.BitOffset8(offset), ctx.Const(8u));
    }
}

Id EmitLoadStorageU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    if (ctx.profile.support_int16 && ctx.profile.support_descriptor_aliasing) {
        return ctx.OpUConvert(ctx.U32[1],
                              LoadStorage(ctx, binding, offset, ctx.U16, ctx.storage_types.U16,
                                          sizeof(u16), &StorageDefinitions::U16));
    } else {
        return ctx.OpBitFieldUExtract(ctx.U32[1], LoadStorage32(ctx, binding, offset),
                                      ctx.BitOffset16(offset), ctx.Const(16u));
    }
}

Id EmitLoadStorageS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    if (ctx.profile.support_int16 && ctx.profile.support_descriptor_aliasing) {
        return ctx.OpSConvert(ctx.U32[1],
                              LoadStorage(ctx, binding, offset, ctx.S16, ctx.storage_types.S16,
                                          sizeof(s16), &StorageDefinitions::S16));
    } else {
        return ctx.OpBitFieldSExtract(ctx.U32[1], LoadStorage32(ctx, binding, offset),
                                      ctx.BitOffset16(offset), ctx.Const(16u));
    }
}

Id EmitLoadStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    return LoadStorage32(ctx, binding, offset);
}

Id EmitLoadStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    if (ctx.profile.support_descriptor_aliasing) {
        return LoadStorage(ctx, binding, offset, ctx.U32[2], ctx.storage_types.U32x2,
                           sizeof(u32[2]), &StorageDefinitions::U32x2);
    } else {
        return ctx.OpCompositeConstruct(ctx.U32[2], LoadStorage32(ctx, binding, offset, 0),
                                        LoadStorage32(ctx, binding, offset, 1));
    }
}

Id EmitLoadStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset) {
    if (ctx.profile.support_descriptor_aliasing) {
        return LoadStorage(ctx, binding, offset, ctx.U32[4], ctx.storage_types.U32x4,
                           sizeof(u32[4]), &StorageDefinitions::U32x4);
    } else {
        return ctx.OpCompositeConstruct(ctx.U32[4], LoadStorage32(ctx, binding, offset, 0),
                                        LoadStorage32(ctx, binding, offset, 1),
                                        LoadStorage32(ctx, binding, offset, 2),
                                        LoadStorage32(ctx, binding, offset, 3));
    }
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
    WriteStorage32(ctx, binding, offset, value);
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        Id value) {
    if (ctx.profile.support_descriptor_aliasing) {
        WriteStorage(ctx, binding, offset, value, ctx.storage_types.U32x2, sizeof(u32[2]),
                     &StorageDefinitions::U32x2);
    } else {
        for (u32 index = 0; index < 2; ++index) {
            const Id element{ctx.OpCompositeExtract(ctx.U32[1], value, index)};
            WriteStorage32(ctx, binding, offset, element, index);
        }
    }
}

void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         Id value) {
    if (ctx.profile.support_descriptor_aliasing) {
        WriteStorage(ctx, binding, offset, value, ctx.storage_types.U32x4, sizeof(u32[4]),
                     &StorageDefinitions::U32x4);
    } else {
        for (u32 index = 0; index < 4; ++index) {
            const Id element{ctx.OpCompositeExtract(ctx.U32[1], value, index)};
            WriteStorage32(ctx, binding, offset, element, index);
        }
    }
}

} // namespace Shader::Backend::SPIRV
