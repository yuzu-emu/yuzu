// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/runtime_info.h"

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

void GlobalStorageOp(EmitContext& ctx, Register address, bool pointer_based, std::string_view expr,
                     std::string_view else_expr = {}) {
    const size_t num_buffers{ctx.info.storage_buffers_descriptors.size()};
    for (size_t index = 0; index < num_buffers; ++index) {
        if (!ctx.info.nvn_buffer_used[index]) {
            continue;
        }
        const auto& ssbo{ctx.info.storage_buffers_descriptors[index]};
        ctx.Add("LDC.U64 DC.x,c{}[{}];"    // ssbo_addr
                "LDC.U32 RC.x,c{}[{}];"    // ssbo_size_u32
                "CVT.U64.U32 DC.y,RC.x;"   // ssbo_size = ssbo_size_u32
                "ADD.U64 DC.y,DC.y,DC.x;"  // ssbo_end = ssbo_addr + ssbo_size
                "SGE.U64 RC.x,{}.x,DC.x;"  // a = input_addr >= ssbo_addr ? -1 : 0
                "SLT.U64 RC.y,{}.x,DC.y;"  // b = input_addr < ssbo_end   ? -1 : 0
                "AND.U.CC RC.x,RC.x,RC.y;" // cond = a && b
                "IF NE.x;"                 // if cond
                "SUB.U64 DC.x,{}.x,DC.x;", // offset = input_addr - ssbo_addr
                ssbo.cbuf_index, ssbo.cbuf_offset, ssbo.cbuf_index, ssbo.cbuf_offset + 8, address,
                address, address);
        if (pointer_based) {
            ctx.Add("PK64.U DC.y,c[{}];"      // host_ssbo = cbuf
                    "ADD.U64 DC.x,DC.x,DC.y;" // host_addr = host_ssbo + offset
                    "{}"
                    "ELSE;",
                    index, expr);
        } else {
            ctx.Add("CVT.U32.U64 RC.x,DC.x;"
                    "{},ssbo{}[RC.x];"
                    "ELSE;",
                    expr, index);
        }
    }
    if (!else_expr.empty()) {
        ctx.Add("{}", else_expr);
    }
    const size_t num_used_buffers{ctx.info.nvn_buffer_used.count()};
    for (size_t index = 0; index < num_used_buffers; ++index) {
        ctx.Add("ENDIF;");
    }
}

template <typename ValueType>
void Write(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset, ValueType value,
           std::string_view size) {
    if (ctx.runtime_info.glasm_use_storage_buffers) {
        ctx.Add("STB.{} {},ssbo{}[{}];", size, value, binding.U32(), offset);
    } else {
        StorageOp(ctx, binding, offset, fmt::format("STORE.{} {},DC.x;", size, value));
    }
}

void Load(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset,
          std::string_view size) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (ctx.runtime_info.glasm_use_storage_buffers) {
        ctx.Add("LDB.{} {},ssbo{}[{}];", size, ret, binding.U32(), offset);
    } else {
        StorageOp(ctx, binding, offset, fmt::format("LOAD.{} {},DC.x;", size, ret),
                  fmt::format("MOV.U {},{{0,0,0,0}};", ret));
    }
}

template <typename ValueType>
void GlobalWrite(EmitContext& ctx, Register address, ValueType value, std::string_view size) {
    if (ctx.runtime_info.glasm_use_storage_buffers) {
        GlobalStorageOp(ctx, address, false, fmt::format("STB.{} {}", size, value));
    } else {
        GlobalStorageOp(ctx, address, true, fmt::format("STORE.{} {},DC.x;", size, value));
    }
}

void GlobalLoad(EmitContext& ctx, IR::Inst& inst, Register address, std::string_view size) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (ctx.runtime_info.glasm_use_storage_buffers) {
        GlobalStorageOp(ctx, address, false, fmt::format("LDB.{} {}", size, ret));
    } else {
        GlobalStorageOp(ctx, address, true, fmt::format("LOAD.{} {},DC.x;", size, ret),
                        fmt::format("MOV.S {},0;", ret));
    }
}

template <typename ValueType>
void Atom(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset,
          ValueType value, std::string_view operation, std::string_view size) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (ctx.runtime_info.glasm_use_storage_buffers) {
        ctx.Add("ATOMB.{}.{} {},{},ssbo{}[{}];", operation, size, ret, value, binding.U32(),
                offset);
    } else {
        StorageOp(ctx, binding, offset,
                  fmt::format("ATOM.{}.{} {},{},DC.x;", operation, size, ret, value));
    }
}
} // Anonymous namespace

void EmitLoadGlobalU8(EmitContext& ctx, IR::Inst& inst, Register address) {
    GlobalLoad(ctx, inst, address, "U8");
}

void EmitLoadGlobalS8(EmitContext& ctx, IR::Inst& inst, Register address) {
    GlobalLoad(ctx, inst, address, "S8");
}

void EmitLoadGlobalU16(EmitContext& ctx, IR::Inst& inst, Register address) {
    GlobalLoad(ctx, inst, address, "U16");
}

void EmitLoadGlobalS16(EmitContext& ctx, IR::Inst& inst, Register address) {
    GlobalLoad(ctx, inst, address, "S16");
}

void EmitLoadGlobal32(EmitContext& ctx, IR::Inst& inst, Register address) {
    GlobalLoad(ctx, inst, address, "U32");
}

void EmitLoadGlobal64(EmitContext& ctx, IR::Inst& inst, Register address) {
    GlobalLoad(ctx, inst, address, "U32X2");
}

void EmitLoadGlobal128(EmitContext& ctx, IR::Inst& inst, Register address) {
    GlobalLoad(ctx, inst, address, "U32X4");
}

void EmitWriteGlobalU8(EmitContext& ctx, Register address, Register value) {
    GlobalWrite(ctx, address, value, "U8");
}

void EmitWriteGlobalS8(EmitContext& ctx, Register address, Register value) {
    GlobalWrite(ctx, address, value, "S8");
}

void EmitWriteGlobalU16(EmitContext& ctx, Register address, Register value) {
    GlobalWrite(ctx, address, value, "U16");
}

void EmitWriteGlobalS16(EmitContext& ctx, Register address, Register value) {
    GlobalWrite(ctx, address, value, "S16");
}

void EmitWriteGlobal32(EmitContext& ctx, Register address, ScalarU32 value) {
    GlobalWrite(ctx, address, value, "U32");
}

void EmitWriteGlobal64(EmitContext& ctx, Register address, Register value) {
    GlobalWrite(ctx, address, value, "U32X2");
}

void EmitWriteGlobal128(EmitContext& ctx, Register address, Register value) {
    GlobalWrite(ctx, address, value, "U32X4");
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
    Write(ctx, binding, offset, value, "U8");
}

void EmitWriteStorageS8(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        ScalarS32 value) {
    Write(ctx, binding, offset, value, "S8");
}

void EmitWriteStorageU16(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         ScalarU32 value) {
    Write(ctx, binding, offset, value, "U16");
}

void EmitWriteStorageS16(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         ScalarS32 value) {
    Write(ctx, binding, offset, value, "S16");
}

void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        ScalarU32 value) {
    Write(ctx, binding, offset, value, "U32");
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                        Register value) {
    Write(ctx, binding, offset, value, "U32X2");
}

void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset,
                         Register value) {
    Write(ctx, binding, offset, value, "U32X4");
}

void EmitSharedAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarU32 value) {
    ctx.Add("ATOMS.ADD.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicSMin32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarS32 value) {
    ctx.Add("ATOMS.MIN.S32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicUMin32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarU32 value) {
    ctx.Add("ATOMS.MIN.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicSMax32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarS32 value) {
    ctx.Add("ATOMS.MAX.S32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicUMax32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                            ScalarU32 value) {
    ctx.Add("ATOMS.MAX.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicInc32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                           ScalarU32 value) {
    ctx.Add("ATOMS.IWRAP.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicDec32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                           ScalarU32 value) {
    ctx.Add("ATOMS.DWRAP.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicAnd32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                           ScalarU32 value) {
    ctx.Add("ATOMS.AND.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicOr32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                          ScalarU32 value) {
    ctx.Add("ATOMS.OR.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicXor32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                           ScalarU32 value) {
    ctx.Add("ATOMS.XOR.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicExchange32(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                                ScalarU32 value) {
    ctx.Add("ATOMS.EXCH.U32 {},{},shared_mem[{}];", inst, value, pointer_offset);
}

void EmitSharedAtomicExchange64(EmitContext& ctx, IR::Inst& inst, ScalarU32 pointer_offset,
                                Register value) {
    ctx.LongAdd("ATOMS.EXCH.U64 {}.x,{},shared_mem[{}];", inst, value, pointer_offset);
}

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
