// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
void StorageOp(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
               std::string_view then_expr, std::string_view else_expr = {}) {
    // Operate on bindless SSBO, call the expression with bounds checking
    // address = c[binding].xy
    // length  = c[binding].z
    const u32 sb_binding{binding.U32()};
    ctx.Add("PK64.U DC,c[{}];"           // pointer = address
            "CVT.U64.U32 DC.z,{};"       // offset = uint64_t(offset)
            "ADD.U64 DC.x,DC.x,DC.z;"    // pointer += offset
            "SLT.U.CC RC.x,{},c[{}].z;", // cc = offset < length
            sb_binding, offset, offset, sb_binding);
    if (else_expr.empty()) {
        ctx.Add("IF NE.x;{}ENDIF;", then_expr);
    } else {
        ctx.Add("IF NE.x;{}ELSE;{}ENDIF;", then_expr, else_expr);
    }
}

template <typename ValueType>
void Store(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset, ValueType value,
           std::string_view size) {
    StorageOp(ctx, binding, offset, fmt::format("STORE.{} {},DC.x;", size, value));
}

void Load(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset,
          std::string_view size) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    StorageOp(ctx, binding, offset, fmt::format("STORE.{} {},DC.x;", size, ret),
              fmt::format("MOV.U {},{{0,0,0,0}};", ret));
}
} // Anonymous namespace

void EmitLoadGlobalU8([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadGlobalS8([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadGlobalU16([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadGlobalS16([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadGlobal32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register address) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadGlobal64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register address) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadGlobal128([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register address) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteGlobalU8([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteGlobalS8([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteGlobalU16([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteGlobalS16([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteGlobal32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register address,
                       [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteGlobal64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register address,
                       [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteGlobal128([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register address,
                        [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadStorageU8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       ScalarU32 offset) {
    Load(ctx, inst, binding, offset, "U8");
}

void EmitLoadStorageS8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       ScalarU32 offset) {
    Load(ctx, inst, binding, offset, "S8");
}

void EmitLoadStorageU16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        ScalarU32 offset) {
    Load(ctx, inst, binding, offset, "U16");
}

void EmitLoadStorageS16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        ScalarU32 offset) {
    Load(ctx, inst, binding, offset, "S16");
}

void EmitLoadStorage32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       ScalarU32 offset) {
    Load(ctx, inst, binding, offset, "U32");
}

void EmitLoadStorage64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       ScalarU32 offset) {
    Load(ctx, inst, binding, offset, "U32X2");
}

void EmitLoadStorage128(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        ScalarU32 offset) {
    Load(ctx, inst, binding, offset, "U32X4");
}

void EmitWriteStorageU8(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        ScalarU32 value) {
    Store(ctx, binding, offset, value, "U8");
}

void EmitWriteStorageS8(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        ScalarS32 value) {
    Store(ctx, binding, offset, value, "S8");
}

void EmitWriteStorageU16(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         ScalarU32 value) {
    Store(ctx, binding, offset, value, "U16");
}

void EmitWriteStorageS16(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         ScalarS32 value) {
    Store(ctx, binding, offset, value, "S16");
}

void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        ScalarU32 value) {
    Store(ctx, binding, offset, value, "U32");
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        Register value) {
    Store(ctx, binding, offset, value, "U32X2");
}

void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         Register value) {
    Store(ctx, binding, offset, value, "U32X4");
}

} // namespace Shader::Backend::GLASM
