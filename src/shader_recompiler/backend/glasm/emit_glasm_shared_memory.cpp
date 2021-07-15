
// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
void EmitLoadSharedU8(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset) {
    ctx.Add("LDS.U8 {},shared_mem[{}];", inst, offset);
}

void EmitLoadSharedS8(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset) {
    ctx.Add("LDS.S8 {},shared_mem[{}];", inst, offset);
}

void EmitLoadSharedU16(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset) {
    ctx.Add("LDS.U16 {},shared_mem[{}];", inst, offset);
}

void EmitLoadSharedS16(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset) {
    ctx.Add("LDS.S16 {},shared_mem[{}];", inst, offset);
}

void EmitLoadSharedU32(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset) {
    ctx.Add("LDS.U32 {},shared_mem[{}];", inst, offset);
}

void EmitLoadSharedU64(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset) {
    ctx.Add("LDS.U32X2 {},shared_mem[{}];", inst, offset);
}

void EmitLoadSharedU128(EmitContext& ctx, IR::Inst& inst, ScalarU32 offset) {
    ctx.Add("LDS.U32X4 {},shared_mem[{}];", inst, offset);
}

void EmitWriteSharedU8(EmitContext& ctx, ScalarU32 offset, ScalarU32 value) {
    ctx.Add("STS.U8 {},shared_mem[{}];", value, offset);
}

void EmitWriteSharedU16(EmitContext& ctx, ScalarU32 offset, ScalarU32 value) {
    ctx.Add("STS.U16 {},shared_mem[{}];", value, offset);
}

void EmitWriteSharedU32(EmitContext& ctx, ScalarU32 offset, ScalarU32 value) {
    ctx.Add("STS.U32 {},shared_mem[{}];", value, offset);
}

void EmitWriteSharedU64(EmitContext& ctx, ScalarU32 offset, Register value) {
    ctx.Add("STS.U32X2 {},shared_mem[{}];", value, offset);
}

void EmitWriteSharedU128(EmitContext& ctx, ScalarU32 offset, Register value) {
    ctx.Add("STS.U32X4 {},shared_mem[{}];", value, offset);
}
} // namespace Shader::Backend::GLASM
