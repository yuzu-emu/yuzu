// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
void GetCbuf(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset,
             std::string_view size) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Indirect constant buffer loading");
    }
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("LDC.{} {},c{}[{}];", size, ret, binding.U32(), offset);
}
} // Anonymous namespace

void EmitGetCbufU8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset) {
    GetCbuf(ctx, inst, binding, offset, "U8");
}

void EmitGetCbufS8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset) {
    GetCbuf(ctx, inst, binding, offset, "S8");
}

void EmitGetCbufU16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset) {
    GetCbuf(ctx, inst, binding, offset, "U16");
}

void EmitGetCbufS16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset) {
    GetCbuf(ctx, inst, binding, offset, "S16");
}

void EmitGetCbufU32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset) {
    GetCbuf(ctx, inst, binding, offset, "U32");
}

void EmitGetCbufF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset) {
    GetCbuf(ctx, inst, binding, offset, "F32");
}

void EmitGetCbufU32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                      ScalarU32 offset) {
    GetCbuf(ctx, inst, binding, offset, "U32X2");
}

void EmitGetAttribute(EmitContext& ctx, IR::Inst& inst, IR::Attribute attr,
                      [[maybe_unused]] ScalarU32 vertex) {
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        const u32 element{IR::GenericAttributeElement(attr)};
        ctx.Add("MOV.F {}.x,in_attr{}.{};", inst, index, "xyzw"[element]);
        return;
    }
    throw NotImplementedException("Get attribute {}", attr);
}

void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, ScalarF32 value,
                      [[maybe_unused]] ScalarU32 vertex) {
    const u32 element{static_cast<u32>(attr) % 4};
    const char swizzle{"xyzw"[element]};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        ctx.Add("MOV.F out_attr{}.{},{};", index, swizzle, value);
        return;
    }
    switch (attr) {
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        ctx.Add("MOV.F result.position.{},{};", swizzle, value);
        break;
    default:
        throw NotImplementedException("Set attribute {}", attr);
    }
}

void EmitGetAttributeIndexed([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset,
                             [[maybe_unused]] ScalarU32 vertex) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSetAttributeIndexed([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset,
                             [[maybe_unused]] ScalarF32 value, [[maybe_unused]] ScalarU32 vertex) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGetPatch([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Patch patch) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSetPatch([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Patch patch,
                  [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSetFragColor([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] u32 index,
                      [[maybe_unused]] u32 component, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSetSampleMask([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSetFragDepth([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarF32 value) {
    throw NotImplementedException("GLASM instruction");
}

} // namespace Shader::Backend::GLASM
