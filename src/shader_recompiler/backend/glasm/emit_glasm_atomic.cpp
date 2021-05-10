// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
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
void Atom(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset,
          ValueType value, std::string_view operation, std::string_view size) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    StorageOp(ctx, binding, offset,
              fmt::format("ATOM.{}.{} {},{},DC.x;", operation, size, ret, value));
}
} // namespace

void EmitStorageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "ADD", "U32");
}

void EmitStorageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarS32 value) {
    Atom(ctx, inst, binding, offset, value, "MIN", "S32");
}

void EmitStorageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "MIN", "U32");
}

void EmitStorageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarS32 value) {
    Atom(ctx, inst, binding, offset, value, "MAX", "S32");
}

void EmitStorageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "MAX", "U32");
}

void EmitStorageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "IWRAP", "U32");
}

void EmitStorageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "DWRAP", "U32");
}

void EmitStorageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "AND", "U32");
}

void EmitStorageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "OR", "U32");
}

void EmitStorageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "XOR", "U32");
}

void EmitStorageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 ScalarU32 offset, ScalarU32 value) {
    Atom(ctx, inst, binding, offset, value, "EXCH", "U32");
}

void EmitStorageAtomicIAdd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "ADD", "U64");
}

void EmitStorageAtomicSMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "MIN", "S64");
}

void EmitStorageAtomicUMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "MIN", "U64");
}

void EmitStorageAtomicSMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "MAX", "S64");
}

void EmitStorageAtomicUMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "MAX", "U64");
}

void EmitStorageAtomicAnd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "AND", "U64");
}

void EmitStorageAtomicOr64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "OR", "U64");
}

void EmitStorageAtomicXor64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "XOR", "U64");
}

void EmitStorageAtomicExchange64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "EXCH", "U64");
}

void EmitStorageAtomicAddF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             ScalarU32 offset, ScalarF32 value) {
    Atom(ctx, inst, binding, offset, value, "ADD", "F32");
}

void EmitStorageAtomicAddF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "ADD", "F16x2");
}

void EmitStorageAtomicAddF32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] ScalarU32 offset, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitStorageAtomicMinF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "MIN", "F16x2");
}

void EmitStorageAtomicMinF32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] ScalarU32 offset, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitStorageAtomicMaxF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               ScalarU32 offset, Register value) {
    Atom(ctx, inst, binding, offset, value, "MAX", "F16x2");
}

void EmitStorageAtomicMaxF32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] ScalarU32 offset, [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicIAdd32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicSMin32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicUMin32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicSMax32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicUMax32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicInc32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicDec32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicAnd32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicOr32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicXor32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicExchange32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicIAdd64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicSMin64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicUMin64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicSMax64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicUMax64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicInc64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicDec64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicAnd64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicOr64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicXor64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicExchange64(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicAddF32(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicAddF16x2(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicAddF32x2(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicMinF16x2(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicMinF32x2(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicMaxF16x2(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGlobalAtomicMaxF32x2(EmitContext&) {
    throw NotImplementedException("GLASM instruction");
}
} // namespace Shader::Backend::GLASM
