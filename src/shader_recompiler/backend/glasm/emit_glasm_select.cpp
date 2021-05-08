// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

void EmitSelectU1(EmitContext&, std::string_view, std::string_view, std::string_view) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSelectU8(EmitContext&, std::string_view, std::string_view, std::string_view) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSelectU16(EmitContext&, std::string_view, std::string_view, std::string_view) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSelectU32(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.Add("MOV.U.CC RC,{};", cond);
    ctx.Add("IF NE.x;");
    ctx.Add("MOV.U {},{};", inst, true_value);
    ctx.Add("ELSE;");
    ctx.Add("MOV.U {},{};", inst, false_value);
    ctx.Add("ENDIF;");
}

void EmitSelectU64(EmitContext&, std::string_view, std::string_view, std::string_view) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSelectF16(EmitContext&, std::string_view, std::string_view, std::string_view) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSelectF32(EmitContext&, std::string_view, std::string_view, std::string_view) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSelectF64(EmitContext&, std::string_view, std::string_view, std::string_view) {
    throw NotImplementedException("GLASM instruction");
}
} // namespace Shader::Backend::GLASM
