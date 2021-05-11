
// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
void EmitLoadSharedU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadSharedS8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadSharedU16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadSharedS16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadSharedU32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitLoadSharedU64([[maybe_unused]] EmitContext& ctx, IR::Inst& inst,
                       [[maybe_unused]] ScalarU32 offset) {
    ctx.LongAdd("LDS.U64 {},shared_mem[{}];", inst, offset);
}

void EmitLoadSharedU128([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteSharedU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset,
                       [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteSharedU16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset,
                        [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitWriteSharedU32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset,
                        [[maybe_unused]] ScalarU32 value) {
    ctx.Add("STS.U32 {},shared_mem[{}];", value, offset);
}

void EmitWriteSharedU64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset,
                        [[maybe_unused]] Register value) {
    ctx.Add("STS.U64 {},shared_mem[{}];", value, offset);
}

void EmitWriteSharedU128([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset,
                         [[maybe_unused]] Register value) {
    throw NotImplementedException("GLASM instruction");
}
} // namespace Shader::Backend::GLASM
