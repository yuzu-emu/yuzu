// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
static constexpr std::string_view SWIZZLE{"xyzw"};

u32 CbufIndex(u32 offset) {
    return (offset / 4) % 4;
}

char OffsetSwizzle(u32 offset) {
    return SWIZZLE[CbufIndex(offset)];
}
} // namespace

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
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=floatBitsToUint(cbuf{}[{}].{});", inst, binding.U32(), offset.U32() / 16,
                   OffsetSwizzle(offset.U32()));
    } else {
        const auto offset_var{ctx.reg_alloc.Consume(offset)};
        ctx.AddU32("{}=floatBitsToUint(cbuf{}[{}/16][({}/4)%4]);", inst, binding.U32(), offset_var,
                   offset_var);
    }
}

void EmitGetCbufF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    if (offset.IsImmediate()) {
        ctx.AddF32("{}=cbuf{}[{}].{};", inst, binding.U32(), offset.U32() / 16,
                   OffsetSwizzle(offset.U32()));
    } else {
        const auto offset_var{ctx.reg_alloc.Consume(offset)};
        ctx.AddF32("{}=cbuf{}[{}/16][({}/4)%4];", inst, binding.U32(), offset_var, offset_var);
    }
}

void EmitGetCbufU32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                      const IR::Value& offset) {
    if (offset.IsImmediate()) {
        const auto u32_offset{offset.U32()};
        const auto index{(u32_offset / 4) % 4};
        ctx.AddU32x2("{}=uvec2(floatBitsToUint(cbuf{}[{}].{}),floatBitsToUint(cbuf{}[{}].{}));",
                     inst, binding.U32(), offset.U32() / 16, OffsetSwizzle(offset.U32()),
                     binding.U32(), (offset.U32() + 1) / 16, OffsetSwizzle(offset.U32() + 1));
    } else {
        const auto offset_var{ctx.reg_alloc.Consume(offset)};
        ctx.AddU32x2("{}=uvec2(floatBitsToUint(cbuf{}[{}/16][({}/"
                     "4)%4]),floatBitsToUint(cbuf{}[({}+1)/16][(({}+1/4))%4]));",
                     inst, binding.U32(), offset_var, offset_var, binding.U32(), offset_var,
                     offset_var);
    }
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
        switch (ctx.stage) {
        case Stage::VertexA:
        case Stage::VertexB:
            ctx.AddF32("{}=gl_Position.{};", inst, swizzle);
            break;
        case Stage::Fragment:
            ctx.AddF32("{}=gl_FragCoord.{};", inst, swizzle);
            break;
        default:
            throw NotImplementedException("Get Position for stage {}", ctx.stage);
        }
        break;
    case IR::Attribute::InstanceId:
        ctx.AddS32("{}=gl_InstanceID;", inst, ctx.attrib_name);
        break;
    case IR::Attribute::VertexId:
        ctx.AddS32("{}=gl_VertexID;", inst, ctx.attrib_name);
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
