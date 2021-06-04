// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
namespace {
constexpr char SWIZZLE[]{"xyzw"};

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
} // Anonymous namespace

void EmitGetCbufU8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                   const IR::Value& offset) {
    const auto cbuf{fmt::format("{}_cbuf{}", ctx.stage_name, binding.U32())};
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=bitfieldExtract(ftou({}[{}].{}),int({}),8);", inst, cbuf, offset.U32() / 16,
                   OffsetSwizzle(offset.U32()), (offset.U32() % 4) * 8);
        return;
    }
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    if (!ctx.profile.has_gl_component_indexing_bug) {
        ctx.AddU32("{}=bitfieldExtract(ftou({}[{}>>4][({}>>2)%4]),int(({}%4)*8),8);", inst, cbuf,
                   offset_var, offset_var, offset_var);
        return;
    }
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    const auto cbuf_offset{fmt::format("{}>>2", offset_var)};
    for (u32 swizzle = 0; swizzle < 4; ++swizzle) {
        ctx.Add("if(({}&3)=={}){}=bitfieldExtract(ftou({}[{}>>4].{}),int(({}%4)*8),8);",
                cbuf_offset, swizzle, ret, cbuf, offset_var, "xyzw"[swizzle], offset_var);
    }
}

void EmitGetCbufS8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                   const IR::Value& offset) {
    const auto cbuf{fmt::format("{}_cbuf{}", ctx.stage_name, binding.U32())};
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=bitfieldExtract(ftoi({}[{}].{}),int({}),8);", inst, cbuf, offset.U32() / 16,
                   OffsetSwizzle(offset.U32()), (offset.U32() % 4) * 8);
        return;
    }
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    if (!ctx.profile.has_gl_component_indexing_bug) {
        ctx.AddU32("{}=bitfieldExtract(ftoi({}[{}>>4][({}>>2)%4]),int(({}%4)*8),8);", inst, cbuf,
                   offset_var, offset_var, offset_var);
        return;
    }
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    const auto cbuf_offset{fmt::format("{}>>2", offset_var)};
    for (u32 swizzle = 0; swizzle < 4; ++swizzle) {
        ctx.Add("if(({}&3)=={}){}=bitfieldExtract(ftoi({}[{}>>4].{}),int(({}%4)*8),8);",
                cbuf_offset, swizzle, ret, cbuf, offset_var, "xyzw"[swizzle], offset_var);
    }
}

void EmitGetCbufU16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    const auto cbuf{fmt::format("{}_cbuf{}", ctx.stage_name, binding.U32())};
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=bitfieldExtract(ftou({}[{}].{}),int({}),16);", inst, cbuf, offset.U32() / 16,
                   OffsetSwizzle(offset.U32()), ((offset.U32() / 2) % 2) * 16);
        return;
    }
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    if (!ctx.profile.has_gl_component_indexing_bug) {
        ctx.AddU32("{}=bitfieldExtract(ftou({}[{}>>4][({}>>2)%4]),int((({}>>1)%2)*16),16);", inst,
                   cbuf, offset_var, offset_var, offset_var);
        return;
    }
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    const auto cbuf_offset{fmt::format("{}>>2", offset_var)};
    for (u32 swizzle = 0; swizzle < 4; ++swizzle) {
        ctx.Add("if(({}&3)=={}){}=bitfieldExtract(ftou({}[{}>>4].{}),int((({}>>1)%2)*16),16);",
                cbuf_offset, swizzle, ret, cbuf, offset_var, "xyzw"[swizzle], offset_var);
    }
}

void EmitGetCbufS16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    const auto cbuf{fmt::format("{}_cbuf{}", ctx.stage_name, binding.U32())};
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=bitfieldExtract(ftoi({}[{}].{}),int({}),16);", inst, cbuf, offset.U32() / 16,
                   OffsetSwizzle(offset.U32()), ((offset.U32() / 2) % 2) * 16);
        return;
    }
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    if (!ctx.profile.has_gl_component_indexing_bug) {
        ctx.AddU32("{}=bitfieldExtract(ftoi({}[{}>>4][({}>>2)%4]),int((({}>>1)%2)*16),16);", inst,
                   cbuf, offset_var, offset_var, offset_var);
        return;
    }
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    const auto cbuf_offset{fmt::format("{}>>2", offset_var)};
    for (u32 swizzle = 0; swizzle < 4; ++swizzle) {
        ctx.Add("if(({}&3)=={}){}=bitfieldExtract(ftoi({}[{}>>4].{}),int((({}>>1)%2)*16),16);",
                cbuf_offset, swizzle, ret, cbuf, offset_var, "xyzw"[swizzle], offset_var);
    }
}

void EmitGetCbufU32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    const auto cbuf{fmt::format("{}_cbuf{}", ctx.stage_name, binding.U32())};
    if (offset.IsImmediate()) {
        ctx.AddU32("{}=ftou({}[{}].{});", inst, cbuf, offset.U32() / 16,
                   OffsetSwizzle(offset.U32()));
        return;
    }
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    if (!ctx.profile.has_gl_component_indexing_bug) {
        ctx.AddU32("{}=ftou({}[{}>>4][({}>>2)%4]);", inst, cbuf, offset_var, offset_var);
        return;
    }
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    const auto cbuf_offset{fmt::format("{}>>2", offset_var)};
    for (u32 swizzle = 0; swizzle < 4; ++swizzle) {
        ctx.Add("if(({}&3)=={}){}=ftou({}[{}>>4].{});", cbuf_offset, swizzle, ret, cbuf, offset_var,
                "xyzw"[swizzle]);
    }
}

void EmitGetCbufF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset) {
    const auto cbuf{fmt::format("{}_cbuf{}", ctx.stage_name, binding.U32())};
    if (offset.IsImmediate()) {
        ctx.AddF32("{}={}[{}].{};", inst, cbuf, offset.U32() / 16, OffsetSwizzle(offset.U32()));
        return;
    }
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    if (!ctx.profile.has_gl_component_indexing_bug) {
        ctx.AddF32("{}={}[{}>>4][({}>>2)%4];", inst, cbuf, offset_var, offset_var);
        return;
    }
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::F32)};
    const auto cbuf_offset{fmt::format("{}>>2", offset_var)};
    for (u32 swizzle = 0; swizzle < 4; ++swizzle) {
        ctx.Add("if(({}&3)=={}){}={}[{}>>4].{};", cbuf_offset, swizzle, ret, cbuf, offset_var,
                "xyzw"[swizzle]);
    }
}

void EmitGetCbufU32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                      const IR::Value& offset) {
    const auto cbuf{fmt::format("{}_cbuf{}", ctx.stage_name, binding.U32())};
    if (offset.IsImmediate()) {
        ctx.AddU32x2("{}=uvec2(ftou({}[{}].{}),ftou({}[{}].{}));", inst, cbuf, offset.U32() / 16,
                     OffsetSwizzle(offset.U32()), cbuf, (offset.U32() + 4) / 16,
                     OffsetSwizzle(offset.U32() + 4));
        return;
    }
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    if (!ctx.profile.has_gl_component_indexing_bug) {
        ctx.AddU32x2("{}=uvec2(ftou({}[{}>>4][({}>>2)%4]),ftou({}[({}+4)>>4][(({}+4)>>2)%4]));",
                     inst, cbuf, offset_var, offset_var, cbuf, offset_var, offset_var);
        return;
    }
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32x2)};
    const auto cbuf_offset{fmt::format("{}>>2", offset_var)};
    for (u32 swizzle = 0; swizzle < 4; ++swizzle) {
        ctx.Add("if(({}&3)=={}){}=uvec2(ftou({}[{}>>4].{}),ftou({}[({}+4)>>4].{}));", cbuf_offset,
                swizzle, ret, cbuf, offset_var, "xyzw"[swizzle], cbuf, offset_var,
                "xyzw"[(swizzle + 1) % 4]);
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
        ctx.AddF32("{}=itof(gl_InstanceID);", inst);
        break;
    case IR::Attribute::VertexId:
        ctx.AddF32("{}=itof(gl_VertexID);", inst);
        break;
    case IR::Attribute::FrontFace:
        ctx.AddF32("{}=itof(gl_FrontFacing?-1:0);", inst);
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
                      std::string_view vertex) {
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        const u32 element{IR::GenericAttributeElement(attr)};
        const GenericElementInfo& info{ctx.output_generics.at(index).at(element)};
        const auto output_decorator{OutputVertexIndex(ctx, vertex)};
        if (info.num_components == 1) {
            ctx.Add("{}{}={};", info.name, output_decorator, value);
        } else {
            const u32 index_element{element - info.first_element};
            ctx.Add("{}{}.{}={};", info.name, output_decorator, "xyzw"[index_element], value);
        }
        return;
    }
    const u32 element{static_cast<u32>(attr) % 4};
    const char swizzle{"xyzw"[element]};
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
        if (ctx.stage != Stage::Geometry && !ctx.profile.support_gl_vertex_viewport_layer) {
            // LOG_WARNING(..., "Shader stores viewport index but device does not support viewport
            // layer extension");
            break;
        }
        ctx.Add("gl_ViewportIndex=ftoi({});", value);
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
        throw NotImplementedException("Set attribute {}", attr);
    }
}

void EmitGetPatch(EmitContext& ctx, IR::Inst& inst, IR::Patch patch) {
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
