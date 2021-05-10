// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
std::string_view FpRounding(IR::FpRounding fp_rounding) {
    switch (fp_rounding) {
    case IR::FpRounding::DontCare:
        return "";
    case IR::FpRounding::RN:
        return ".ROUND";
    case IR::FpRounding::RZ:
        return ".TRUNC";
    case IR::FpRounding::RM:
        return ".FLR";
    case IR::FpRounding::RP:
        return ".CEIL";
    }
    throw InvalidArgument("Invalid floating-point rounding {}", fp_rounding);
}

template <typename InputType>
void Convert(EmitContext& ctx, IR::Inst& inst, InputType value, std::string_view dest,
             std::string_view src, bool is_long_result) {
    const std::string_view fp_rounding{FpRounding(inst.Flags<IR::FpControl>().rounding)};
    const auto ret{is_long_result ? ctx.reg_alloc.LongDefine(inst) : ctx.reg_alloc.Define(inst)};
    ctx.Add("CVT.{}.{}{} {}.x,{};", dest, src, fp_rounding, ret, value);
}
} // Anonymous namespace

void EmitConvertS16F16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "S16", "F16", false);
}

void EmitConvertS16F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Convert(ctx, inst, value, "S16", "F32", false);
}

void EmitConvertS16F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    Convert(ctx, inst, value, "S16", "F64", false);
}

void EmitConvertS32F16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "S32", "F16", false);
}

void EmitConvertS32F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Convert(ctx, inst, value, "S32", "F32", false);
}

void EmitConvertS32F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    Convert(ctx, inst, value, "S32", "F64", false);
}

void EmitConvertS64F16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "S64", "F16", true);
}

void EmitConvertS64F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Convert(ctx, inst, value, "S64", "F32", true);
}

void EmitConvertS64F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    Convert(ctx, inst, value, "S64", "F64", true);
}

void EmitConvertU16F16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "U16", "F16", false);
}

void EmitConvertU16F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Convert(ctx, inst, value, "U16", "F32", false);
}

void EmitConvertU16F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    Convert(ctx, inst, value, "U16", "F64", false);
}

void EmitConvertU32F16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "U32", "F16", false);
}

void EmitConvertU32F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Convert(ctx, inst, value, "U32", "F32", false);
}

void EmitConvertU32F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    Convert(ctx, inst, value, "U32", "F64", false);
}

void EmitConvertU64F16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "U64", "F16", true);
}

void EmitConvertU64F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Convert(ctx, inst, value, "U64", "F32", true);
}

void EmitConvertU64F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    Convert(ctx, inst, value, "U64", "F64", true);
}

void EmitConvertU64U32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value) {
    Convert(ctx, inst, value, "U64", "U32", true);
}

void EmitConvertU32U64(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "U32", "U64", false);
}

void EmitConvertF16F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Convert(ctx, inst, value, "F16", "F32", false);
}

void EmitConvertF32F16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F32", "F16", false);
}

void EmitConvertF32F64(EmitContext& ctx, IR::Inst& inst, ScalarF64 value) {
    Convert(ctx, inst, value, "F32", "F64", false);
}

void EmitConvertF64F32(EmitContext& ctx, IR::Inst& inst, ScalarF32 value) {
    Convert(ctx, inst, value, "F64", "F32", true);
}

void EmitConvertF16S8(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F16", "S8", false);
}

void EmitConvertF16S16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F16", "S16", false);
}

void EmitConvertF16S32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    Convert(ctx, inst, value, "F16", "S32", false);
}

void EmitConvertF16S64(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F16", "S64", false);
}

void EmitConvertF16U8(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F16", "U8", false);
}

void EmitConvertF16U16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F16", "U16", false);
}

void EmitConvertF16U32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value) {
    Convert(ctx, inst, value, "F16", "U32", false);
}

void EmitConvertF16U64(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F16", "U64", false);
}

void EmitConvertF32S8(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F32", "S8", false);
}

void EmitConvertF32S16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F32", "S16", false);
}

void EmitConvertF32S32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    Convert(ctx, inst, value, "F32", "S32", false);
}

void EmitConvertF32S64(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F32", "S64", false);
}

void EmitConvertF32U8(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F32", "U8", false);
}

void EmitConvertF32U16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F32", "U16", false);
}

void EmitConvertF32U32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value) {
    Convert(ctx, inst, value, "F32", "U32", false);
}

void EmitConvertF32U64(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F32", "U64", false);
}

void EmitConvertF64S8(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F64", "S8", true);
}

void EmitConvertF64S16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F64", "S16", true);
}

void EmitConvertF64S32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    Convert(ctx, inst, value, "F64", "S32", true);
}

void EmitConvertF64S64(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F64", "S64", true);
}

void EmitConvertF64U8(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F64", "U8", true);
}

void EmitConvertF64U16(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F64", "U16", true);
}

void EmitConvertF64U32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value) {
    Convert(ctx, inst, value, "F64", "U32", true);
}

void EmitConvertF64U64(EmitContext& ctx, IR::Inst& inst, Register value) {
    Convert(ctx, inst, value, "F64", "U64", true);
}

} // namespace Shader::Backend::GLASM
