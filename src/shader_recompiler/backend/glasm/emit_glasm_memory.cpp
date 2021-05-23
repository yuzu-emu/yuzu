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

void GlobalStorageOp(EmitContext& ctx, Register address, std::string_view then_expr,
                     std::string_view else_expr = {}) {
    const size_t num_buffers{ctx.info.storage_buffers_descriptors.size()};
    for (size_t index = 0; index < num_buffers; ++index) {
        if (!ctx.info.nvn_buffer_used[index]) {
            continue;
        }
        const auto& ssbo{ctx.info.storage_buffers_descriptors[index]};
        ctx.Add("LDC.U64 DC.x,c[{}];"     // ssbo_addr
                "LDC.U32 RC.x,c[{}];"     // ssbo_size_u32
                "CVT.U64.U32 DC.y,RC.x;"  // ssbo_size = ssbo_size_u32
                "ADD.U64 DC.y,DC.y,DC.x;" // ssbo_end = ssbo_addr + ssbo_size
                "SGE.U64 RC.x,{}.x,DC.x;" // a = input_addr >= ssbo_addr ? -1 : 1
                "SLT.U64 RC.y,{}.x,DC.y;" // b = input_addr < ssbo_end   ? -1 : 1
                "AND.U.CC RC.x,RC.x,RC.y;"
                "IF NE.x;"                // a && b
                "SUB.U64 DC.x,{}.x,DC.x;" // offset = input_addr - ssbo_addr
                "PK64.U DC.y,c[{}];"      // host_ssbo = cbuf
                "ADD.U64 DC.x,DC.x,DC.y;" // host_addr = host_ssbo + offset
                "{}",
                "ELSE;", index, index, ssbo.cbuf_offset, ssbo.cbuf_offset + 8, address, address,
                address, index, then_expr);
    }
    if (!else_expr.empty()) {
        ctx.Add("{}", else_expr);
    }
    for (size_t index = 0; index < num_buffers; ++index) {
        ctx.Add("ENDIF;");
    }
}

template <typename ValueType>
void Write(EmitContext& ctx, const IR::Value& binding, ScalarU32 offset, ValueType value,
           std::string_view size) {
    StorageOp(ctx, binding, offset, fmt::format("STORE.{} {},DC.x;", size, value));
}

void Load(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset,
          std::string_view size) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    StorageOp(ctx, binding, offset, fmt::format("LOAD.{} {},DC.x;", size, ret),
              fmt::format("MOV.U {},{{0,0,0,0}};", ret));
}

template <typename ValueType>
void GlobalWrite(EmitContext& ctx, Register address, ValueType value, std::string_view size) {
    GlobalStorageOp(ctx, address, fmt::format("STORE.{} {},DC.x;", size, value));
}

void GlobalLoad(EmitContext& ctx, IR::Inst& inst, Register address, std::string_view size) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    GlobalStorageOp(ctx, address, fmt::format("LOAD.{} {},DC.x;", size, ret),
                    fmt::format("MOV.S {},0;", ret));
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

} // namespace Shader::Backend::GLASM
