// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
static constexpr std::string_view SWIZZLE{"xyzw"};

void EmitGetCbufU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                   [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufS8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                   [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufU16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufS16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetCbufU32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    const auto u32_offset{offset.U32()};
    const auto index{(u32_offset / 4) % 4};
    ctx.AddU32("{}=floatBitsToUint(cbuf{}[{}].{});", inst, binding.U32(), u32_offset / 16,
               SWIZZLE[index]);
}

void EmitGetCbufF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    const auto u32_offset{offset.U32()};
    const auto index{(u32_offset / 4) % 4};
    ctx.AddF32("{}=cbuf{}[{}].{};", inst, binding.U32(), u32_offset / 16, SWIZZLE[index]);
}

void EmitGetCbufU32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] const IR::Value& binding,
                      [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL");
}

void EmitGetAttribute(EmitContext& ctx, IR::Inst& inst, IR::Attribute attr,
                      [[maybe_unused]] std::string_view vertex) {
    const u32 element{static_cast<u32>(attr) % 4};
    const char swizzle{"xyzw"[element]};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        ctx.AddF32("{}=in_attr{}.{};", inst, index, swizzle);
        return;
    }
    switch (attr) {
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        ctx.AddF32("{}=gl_Position.{};", inst, swizzle);
        break;
    default:
        fmt::print("Get attribute {}", attr);
        throw NotImplementedException("Get attribute {}", attr);
    }
}

void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, std::string_view value,
                      [[maybe_unused]] std::string_view vertex) {
    const u32 element{static_cast<u32>(attr) % 4};
    const char swizzle{"xyzw"[element]};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        ctx.Add("out_attr{}.{}={};", index, swizzle, value);
        return;
    }
    switch (attr) {
    case IR::Attribute::PointSize:
        ctx.Add("gl_Pointsize={};", value);
        break;
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        ctx.Add("gl_Position.{}={};", swizzle, value);
        break;
    default:
        fmt::print("Set attribute {}", attr);
        throw NotImplementedException("Set attribute {}", attr);
    }
}

void EmitSetFragColor(EmitContext& ctx, u32 index, u32 component, std::string_view value) {
    const char swizzle{"xyzw"[component]};
    ctx.Add("frag_color{}.{}={};", index, swizzle, value);
}

} // namespace Shader::Backend::GLSL
