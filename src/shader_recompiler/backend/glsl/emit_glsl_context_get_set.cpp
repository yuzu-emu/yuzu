// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
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

bool IsInputArray(Stage stage) {
    return stage == Stage::Geometry || stage == Stage::TessellationControl ||
           stage == Stage::TessellationEval;
}

std::string InputVertexIndex(EmitContext& ctx, std::string_view vertex) {
    return IsInputArray(ctx.stage) ? fmt::format("[{}]", vertex) : "";
}

bool IsOutputArray(Stage stage) {
    return stage == Stage::Geometry || stage == Stage::TessellationControl;
}

std::string OutputVertexIndex(EmitContext& ctx, std::string_view vertex) {
    switch (ctx.stage) {
    case Stage::Geometry:
        return fmt::format("[{}]", vertex);
    case Stage::TessellationControl:
        return "[gl_InvocationID]";
    default:
        return "";
    }
}
} // namespace

void EmitGetCbufU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] const IR::Value& binding,
                   [[maybe_unused]] const IR::Value& offset) {
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=bitfieldExtract(floatBitsToUint({}_cbuf{}[{}].{}),int({}),8);", inst,
                   ctx.stage_name, binding.U32(), offset.U32() / 16, OffsetSwizzle(offset.U32()),
                   (offset.U32() % 4) * 8);
    } else {
        const auto offset_var{ctx.var_alloc.Consume(offset)};
        ctx.AddU32(
            "{}=bitfieldExtract(floatBitsToUint({}_cbuf{}[{}/16][({}>>2)%4]),int(({}%4)*8),8);",
            inst, ctx.stage_name, binding.U32(), offset_var, offset_var, offset_var);
    }
}

void EmitGetCbufS8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] const IR::Value& binding,
                   [[maybe_unused]] const IR::Value& offset) {
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=bitfieldExtract(floatBitsToInt({}_cbuf{}[{}].{}),int({}),8);", inst,
                   ctx.stage_name, binding.U32(), offset.U32() / 16, OffsetSwizzle(offset.U32()),
                   (offset.U32() % 4) * 8);
    } else {
        const auto offset_var{ctx.var_alloc.Consume(offset)};
        ctx.AddU32(
            "{}=bitfieldExtract(floatBitsToInt({}_cbuf{}[{}/16][({}>>2)%4]),int(({}%4)*8),8);",
            inst, ctx.stage_name, binding.U32(), offset_var, offset_var, offset_var);
    }
}

void EmitGetCbufU16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] const IR::Value& binding,
                    [[maybe_unused]] const IR::Value& offset) {
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=bitfieldExtract(floatBitsToUint({}_cbuf{}[{}].{}),int({}),16);", inst,
                   ctx.stage_name, binding.U32(), offset.U32() / 16, OffsetSwizzle(offset.U32()),
                   ((offset.U32() / 2) % 2) * 16);
    } else {
        const auto offset_var{ctx.var_alloc.Consume(offset)};
        ctx.AddU32("{}=bitfieldExtract(floatBitsToUint({}_cbuf{}[{}/16][({}>>2)%4]),int((({}/"
                   "2)%2)*16),16);",
                   inst, ctx.stage_name, binding.U32(), offset_var, offset_var, offset_var);
    }
}

void EmitGetCbufS16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] const IR::Value& binding,
                    [[maybe_unused]] const IR::Value& offset) {
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=bitfieldExtract(floatBitsToInt({}_cbuf{}[{}].{}),int({}),16);", inst,
                   ctx.stage_name, binding.U32(), offset.U32() / 16, OffsetSwizzle(offset.U32()),
                   ((offset.U32() / 2) % 2) * 16);
    } else {
        const auto offset_var{ctx.var_alloc.Consume(offset)};
        ctx.AddU32("{}=bitfieldExtract(floatBitsToInt({}_cbuf{}[{}/16][({}>>2)%4]),int((({}/"
                   "2)%2)*16),16);",
                   inst, ctx.stage_name, binding.U32(), offset_var, offset_var, offset_var);
    }
}

void EmitGetCbufU32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=floatBitsToUint({}_cbuf{}[{}].{});", inst, ctx.stage_name, binding.U32(),
                   offset.U32() / 16, OffsetSwizzle(offset.U32()));
    } else {
        const auto offset_var{ctx.var_alloc.Consume(offset)};
        ctx.AddU32("{}=floatBitsToUint({}_cbuf{}[{}/16][({}>>2)%4]);", inst, ctx.stage_name,
                   binding.U32(), offset_var, offset_var);
    }
}

void EmitGetCbufF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    if (offset.IsImmediate()) {
        ctx.AddF32("{}={}_cbuf{}[{}].{};", inst, ctx.stage_name, binding.U32(), offset.U32() / 16,
                   OffsetSwizzle(offset.U32()));
    } else {
        const auto offset_var{ctx.var_alloc.Consume(offset)};
        ctx.AddF32("{}={}_cbuf{}[{}/16][({}>>2)%4];", inst, ctx.stage_name, binding.U32(),
                   offset_var, offset_var);
    }
}

void EmitGetCbufU32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                      const IR::Value& offset) {
    if (offset.IsImmediate()) {
        ctx.AddU32x2(
            "{}=uvec2(floatBitsToUint({}_cbuf{}[{}].{}),floatBitsToUint({}_cbuf{}[{}].{}));", inst,
            ctx.stage_name, binding.U32(), offset.U32() / 16, OffsetSwizzle(offset.U32()),
            ctx.stage_name, binding.U32(), (offset.U32() + 4) / 16,
            OffsetSwizzle(offset.U32() + 4));
    } else {
        const auto offset_var{ctx.var_alloc.Consume(offset)};
        ctx.AddU32x2("{}=uvec2(floatBitsToUint({}_cbuf{}[{}/16][({}/"
                     "4)%4]),floatBitsToUint({}_cbuf{}[({}+4)/16][(({}+4)>>2)%4]));",
                     inst, ctx.stage_name, binding.U32(), offset_var, offset_var, ctx.stage_name,
                     binding.U32(), offset_var, offset_var);
    }
}

void EmitGetAttribute(EmitContext& ctx, IR::Inst& inst, IR::Attribute attr,
                      std::string_view vertex) {
    const u32 element{static_cast<u32>(attr) % 4};
    const char swizzle{"xyzw"[element]};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        ctx.AddF32("{}=in_attr{}{}.{};", inst, index, InputVertexIndex(ctx, vertex), swizzle);
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
        case Stage::TessellationEval:
            ctx.AddF32("{}=gl_TessCoord.{};", inst, swizzle);
            break;
        case Stage::TessellationControl:
        case Stage::Geometry:
            ctx.AddF32("{}=gl_in[{}].gl_Position.{};", inst, vertex, swizzle);
            break;
        case Stage::Fragment:
            ctx.AddF32("{}=gl_FragCoord.{};", inst, swizzle);
            break;
        default:
            throw NotImplementedException("Get Position for stage {}", ctx.stage);
        }
        break;
    case IR::Attribute::PointSpriteS:
    case IR::Attribute::PointSpriteT:
        ctx.AddF32("{}=gl_PointCoord.{};", inst, swizzle);
        break;
    case IR::Attribute::InstanceId:
        ctx.AddF32("{}=intBitsToFloat(gl_InstanceID);", inst);
        break;
    case IR::Attribute::VertexId:
        ctx.AddF32("{}=intBitsToFloat(gl_VertexID);", inst);
        break;
    case IR::Attribute::FrontFace:
        ctx.AddF32("{}=intBitsToFloat(gl_FrontFacing?-1:0);", inst);
        break;
    case IR::Attribute::TessellationEvaluationPointU:
    case IR::Attribute::TessellationEvaluationPointV:
        ctx.AddF32("{}=gl_TessCoord.{};", inst, swizzle);
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
        ctx.Add("out_attr{}{}.{}={};", index, OutputVertexIndex(ctx, vertex), swizzle, value);
        return;
    }
    switch (attr) {
    case IR::Attribute::PointSize:
        ctx.Add("gl_PointSize={};", value);
        break;
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        ctx.Add("gl_Position.{}={};", swizzle, value);
        break;
    case IR::Attribute::ViewportIndex:
        ctx.Add("gl_ViewportIndex=floatBitsToInt({});", value);
        break;
    case IR::Attribute::ClipDistance0:
    case IR::Attribute::ClipDistance1:
    case IR::Attribute::ClipDistance2:
    case IR::Attribute::ClipDistance3:
    case IR::Attribute::ClipDistance4:
    case IR::Attribute::ClipDistance5:
    case IR::Attribute::ClipDistance6:
    case IR::Attribute::ClipDistance7: {
        const u32 index{static_cast<u32>(attr) - static_cast<u32>(IR::Attribute::ClipDistance0)};
        ctx.Add("gl_ClipDistance[{}]={};", index, value);
        break;
    }
    default:
        fmt::print("Set attribute {}", attr);
        throw NotImplementedException("Set attribute {}", attr);
    }
}

void EmitGetPatch([[maybe_unused]] EmitContext& ctx, IR::Inst& inst,
                  [[maybe_unused]] IR::Patch patch) {
    if (!IR::IsGeneric(patch)) {
        throw NotImplementedException("Non-generic patch load");
    }
    const u32 index{IR::GenericPatchIndex(patch)};
    const u32 element{IR::GenericPatchElement(patch)};
    const char swizzle{"xyzw"[element]};
    ctx.AddF32("{}=patch{}.{};", inst, index, swizzle);
}

void EmitSetPatch(EmitContext& ctx, IR::Patch patch, std::string_view value) {
    if (IR::IsGeneric(patch)) {
        const u32 index{IR::GenericPatchIndex(patch)};
        const u32 element{IR::GenericPatchElement(patch)};
        ctx.Add("patch{}.{}={};", index, "xyzw"[element], value);
        return;
    }
    switch (patch) {
    case IR::Patch::TessellationLodLeft:
    case IR::Patch::TessellationLodRight:
    case IR::Patch::TessellationLodTop:
    case IR::Patch::TessellationLodBottom: {
        const u32 index{static_cast<u32>(patch) - u32(IR::Patch::TessellationLodLeft)};
        ctx.Add("gl_TessLevelOuter[{}]={};", index, value);
        break;
    }
    case IR::Patch::TessellationLodInteriorU:
        ctx.Add("gl_TessLevelInner[0]={};", value);
        break;
    case IR::Patch::TessellationLodInteriorV:
        ctx.Add("gl_TessLevelInner[1]={};", value);
        break;
    default:
        throw NotImplementedException("Patch {}", patch);
    }
}

void EmitSetFragColor(EmitContext& ctx, u32 index, u32 component, std::string_view value) {
    const char swizzle{"xyzw"[component]};
    ctx.Add("frag_color{}.{}={};", index, swizzle, value);
}

void EmitLocalInvocationId(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32x3("{}=gl_LocalInvocationID;", inst);
}

void EmitLoadLocal(EmitContext& ctx, IR::Inst& inst, std::string_view word_offset) {
    ctx.AddU32("{}=lmem[{}];", inst, word_offset);
}

void EmitWriteLocal(EmitContext& ctx, std::string_view word_offset, std::string_view value) {
    ctx.Add("lmem[{}]={};", word_offset, value);
}

} // namespace Shader::Backend::GLSL
