// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/shader_info.h"

namespace Shader::Backend::GLASM {
namespace {
void GetCbuf(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding, ScalarU32 offset,
             std::string_view size) {
    if (!binding.IsImmediate()) {
        throw NotImplementedException("Indirect constant buffer loading");
    }
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (offset.type == Type::U32) {
        // Avoid reading arrays out of bounds, matching hardware's behavior
        if (offset.imm_u32 >= 0x10'000) {
            ctx.Add("MOV.S {},0;", ret);
            return;
        }
    }
    ctx.Add("LDC.{} {},c{}[{}];", size, ret, binding.U32(), offset);
}

bool IsInputArray(Stage stage) {
    return stage == Stage::Geometry || stage == Stage::TessellationControl ||
           stage == Stage::TessellationEval;
}

std::string VertexIndex(EmitContext& ctx, ScalarU32 vertex) {
    return IsInputArray(ctx.stage) ? fmt::format("[{}]", vertex) : "";
}

u32 TexCoordIndex(IR::Attribute attr) {
    return (static_cast<u32>(attr) - static_cast<u32>(IR::Attribute::FixedFncTexture0S)) / 4;
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

void EmitGetAttribute(EmitContext& ctx, IR::Inst& inst, IR::Attribute attr, ScalarU32 vertex) {
    const u32 element{static_cast<u32>(attr) % 4};
    const char swizzle{"xyzw"[element]};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        ctx.Add("MOV.F {}.x,in_attr{}{}[0].{};", inst, index, VertexIndex(ctx, vertex), swizzle);
        return;
    }
    if (attr >= IR::Attribute::FixedFncTexture0S && attr <= IR::Attribute::FixedFncTexture9Q) {
        const u32 index{TexCoordIndex(attr)};
        ctx.Add("MOV.F {}.x,{}.texcoord[{}].{};", inst, ctx.attrib_name, index, swizzle);
        return;
    }
    switch (attr) {
    case IR::Attribute::PrimitiveId:
        ctx.Add("MOV.S {}.x,primitive.id;", inst);
        break;
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        if (IsInputArray(ctx.stage)) {
            ctx.Add("MOV.F {}.x,vertex_position{}.{};", inst, VertexIndex(ctx, vertex), swizzle);
        } else {
            ctx.Add("MOV.F {}.x,{}.position.{};", inst, ctx.attrib_name, swizzle);
        }
        break;
    case IR::Attribute::ColorFrontDiffuseR:
    case IR::Attribute::ColorFrontDiffuseG:
    case IR::Attribute::ColorFrontDiffuseB:
    case IR::Attribute::ColorFrontDiffuseA:
        ctx.Add("MOV.F {}.x,{}.color.{};", inst, ctx.attrib_name, swizzle);
        break;
    case IR::Attribute::PointSpriteS:
    case IR::Attribute::PointSpriteT:
        ctx.Add("MOV.F {}.x,{}.pointcoord.{};", inst, ctx.attrib_name, swizzle);
        break;
    case IR::Attribute::TessellationEvaluationPointU:
    case IR::Attribute::TessellationEvaluationPointV:
        ctx.Add("MOV.F {}.x,vertex.tesscoord.{};", inst, swizzle);
        break;
    case IR::Attribute::InstanceId:
        ctx.Add("MOV.S {}.x,{}.instance;", inst, ctx.attrib_name);
        break;
    case IR::Attribute::VertexId:
        ctx.Add("MOV.S {}.x,{}.id;", inst, ctx.attrib_name);
        break;
    case IR::Attribute::FrontFace:
        ctx.Add("CMP.S {}.x,{}.facing.x,0,-1;", inst, ctx.attrib_name);
        break;
    default:
        throw NotImplementedException("Get attribute {}", attr);
    }
}

void EmitSetAttribute(EmitContext& ctx, IR::Attribute attr, ScalarF32 value,
                      [[maybe_unused]] ScalarU32 vertex) {
    const u32 element{static_cast<u32>(attr) % 4};
    const char swizzle{"xyzw"[element]};
    if (IR::IsGeneric(attr)) {
        const u32 index{IR::GenericAttributeIndex(attr)};
        ctx.Add("MOV.F out_attr{}[0].{},{};", index, swizzle, value);
        return;
    }
    if (attr >= IR::Attribute::FixedFncTexture0S && attr <= IR::Attribute::FixedFncTexture9R) {
        const u32 index{TexCoordIndex(attr)};
        ctx.Add("MOV.F result.texcoord[{}].{},{};", index, swizzle, value);
        return;
    }
    switch (attr) {
    case IR::Attribute::Layer:
        if (ctx.stage == Stage::Geometry || ctx.profile.support_viewport_index_layer_non_geometry) {
            ctx.Add("MOV.F result.layer.x,{};", value);
        } else {
            LOG_WARNING(Shader_GLASM,
                        "Layer stored outside of geometry shader not supported by device");
        }
        break;
    case IR::Attribute::ViewportIndex:
        if (ctx.stage == Stage::Geometry || ctx.profile.support_viewport_index_layer_non_geometry) {
            ctx.Add("MOV.F result.viewport.x,{};", value);
        } else {
            LOG_WARNING(Shader_GLASM,
                        "Viewport stored outside of geometry shader not supported by device");
        }
        break;
    case IR::Attribute::ViewportMask:
        // NV_viewport_array2 is required to access result.viewportmask, regardless of shader stage.
        if (ctx.profile.support_viewport_index_layer_non_geometry) {
            ctx.Add("MOV.F result.viewportmask[0].x,{};", value);
        } else {
            LOG_WARNING(Shader_GLASM, "Device does not support storing to ViewportMask");
        }
        break;
    case IR::Attribute::PointSize:
        ctx.Add("MOV.F result.pointsize.x,{};", value);
        break;
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        ctx.Add("MOV.F result.position.{},{};", swizzle, value);
        break;
    case IR::Attribute::ColorFrontDiffuseR:
    case IR::Attribute::ColorFrontDiffuseG:
    case IR::Attribute::ColorFrontDiffuseB:
    case IR::Attribute::ColorFrontDiffuseA:
        ctx.Add("MOV.F result.color.{},{};", swizzle, value);
        break;
    case IR::Attribute::ColorFrontSpecularR:
    case IR::Attribute::ColorFrontSpecularG:
    case IR::Attribute::ColorFrontSpecularB:
    case IR::Attribute::ColorFrontSpecularA:
        ctx.Add("MOV.F result.color.secondary.{},{};", swizzle, value);
        break;
    case IR::Attribute::ColorBackDiffuseR:
    case IR::Attribute::ColorBackDiffuseG:
    case IR::Attribute::ColorBackDiffuseB:
    case IR::Attribute::ColorBackDiffuseA:
        ctx.Add("MOV.F result.color.back.{},{};", swizzle, value);
        break;
    case IR::Attribute::ColorBackSpecularR:
    case IR::Attribute::ColorBackSpecularG:
    case IR::Attribute::ColorBackSpecularB:
    case IR::Attribute::ColorBackSpecularA:
        ctx.Add("MOV.F result.color.back.secondary.{},{};", swizzle, value);
        break;
    case IR::Attribute::FogCoordinate:
        ctx.Add("MOV.F result.fogcoord.x,{};", value);
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
        ctx.Add("MOV.F result.clip[{}].x,{};", index, value);
        break;
    }
    default:
        throw NotImplementedException("Set attribute {}", attr);
    }
}

void EmitGetAttributeIndexed(EmitContext& ctx, IR::Inst& inst, ScalarS32 offset, ScalarU32 vertex) {
    // RC.x = base_index
    // RC.y = masked_index
    // RC.z = compare_index
    ctx.Add("SHR.S RC.x,{},2;"
            "AND.S RC.y,RC.x,3;"
            "SHR.S RC.z,{},4;",
            offset, offset);

    const Register ret{ctx.reg_alloc.Define(inst)};
    u32 num_endifs{};
    const auto read{[&](u32 compare_index, const std::array<std::string, 4>& values) {
        ++num_endifs;
        ctx.Add("SEQ.S.CC RC.w,RC.z,{};" // compare_index
                "IF NE.w;"
                // X
                "SEQ.S.CC RC.w,RC.y,0;"
                "IF NE.w;"
                "MOV {}.x,{};"
                "ELSE;"
                // Y
                "SEQ.S.CC RC.w,RC.y,1;"
                "IF NE.w;"
                "MOV {}.x,{};"
                "ELSE;"
                // Z
                "SEQ.S.CC RC.w,RC.y,2;"
                "IF NE.w;"
                "MOV {}.x,{};"
                "ELSE;"
                // W
                "MOV {}.x,{};"
                "ENDIF;"
                "ENDIF;"
                "ENDIF;"
                "ELSE;",
                compare_index, ret, values[0], ret, values[1], ret, values[2], ret, values[3]);
    }};
    const auto read_swizzled{[&](u32 compare_index, std::string_view value) {
        const std::array values{fmt::format("{}.x", value), fmt::format("{}.y", value),
                                fmt::format("{}.z", value), fmt::format("{}.w", value)};
        read(compare_index, values);
    }};
    if (ctx.info.loads.AnyComponent(IR::Attribute::PositionX)) {
        const u32 index{static_cast<u32>(IR::Attribute::PositionX)};
        if (IsInputArray(ctx.stage)) {
            read_swizzled(index, fmt::format("vertex_position{}", VertexIndex(ctx, vertex)));
        } else {
            read_swizzled(index, fmt::format("{}.position", ctx.attrib_name));
        }
    }
    for (u32 index = 0; index < static_cast<u32>(IR::NUM_GENERICS); ++index) {
        if (!ctx.info.loads.Generic(index)) {
            continue;
        }
        read_swizzled(index, fmt::format("in_attr{}{}[0]", index, VertexIndex(ctx, vertex)));
    }
    for (u32 i = 0; i < num_endifs; ++i) {
        ctx.Add("ENDIF;");
    }
}

void EmitSetAttributeIndexed([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] ScalarU32 offset,
                             [[maybe_unused]] ScalarF32 value, [[maybe_unused]] ScalarU32 vertex) {
    throw NotImplementedException("GLASM instruction");
}

void EmitGetPatch(EmitContext& ctx, IR::Inst& inst, IR::Patch patch) {
    if (!IR::IsGeneric(patch)) {
        throw NotImplementedException("Non-generic patch load");
    }
    const u32 index{IR::GenericPatchIndex(patch)};
    const u32 element{IR::GenericPatchElement(patch)};
    const char swizzle{"xyzw"[element]};
    const std::string_view out{ctx.stage == Stage::TessellationControl ? ".out" : ""};
    ctx.Add("MOV.F {},primitive{}.patch.attrib[{}].{};", inst, out, index, swizzle);
}

void EmitSetPatch(EmitContext& ctx, IR::Patch patch, ScalarF32 value) {
    if (IR::IsGeneric(patch)) {
        const u32 index{IR::GenericPatchIndex(patch)};
        const u32 element{IR::GenericPatchElement(patch)};
        ctx.Add("MOV.F result.patch.attrib[{}].{},{};", index, "xyzw"[element], value);
        return;
    }
    switch (patch) {
    case IR::Patch::TessellationLodLeft:
    case IR::Patch::TessellationLodRight:
    case IR::Patch::TessellationLodTop:
    case IR::Patch::TessellationLodBottom: {
        const u32 index{static_cast<u32>(patch) - u32(IR::Patch::TessellationLodLeft)};
        ctx.Add("MOV.F result.patch.tessouter[{}].x,{};", index, value);
        break;
    }
    case IR::Patch::TessellationLodInteriorU:
        ctx.Add("MOV.F result.patch.tessinner[0].x,{};", value);
        break;
    case IR::Patch::TessellationLodInteriorV:
        ctx.Add("MOV.F result.patch.tessinner[1].x,{};", value);
        break;
    default:
        throw NotImplementedException("Patch {}", patch);
    }
}

void EmitSetFragColor(EmitContext& ctx, u32 index, u32 component, ScalarF32 value) {
    ctx.Add("MOV.F frag_color{}.{},{};", index, "xyzw"[component], value);
}

void EmitSetSampleMask(EmitContext& ctx, ScalarS32 value) {
    ctx.Add("MOV.S result.samplemask.x,{};", value);
}

void EmitSetFragDepth(EmitContext& ctx, ScalarF32 value) {
    ctx.Add("MOV.F result.depth.z,{};", value);
}

void EmitLoadLocal(EmitContext& ctx, IR::Inst& inst, ScalarU32 word_offset) {
    ctx.Add("MOV.U {},lmem[{}].x;", inst, word_offset);
}

void EmitWriteLocal(EmitContext& ctx, ScalarU32 word_offset, ScalarU32 value) {
    ctx.Add("MOV.U lmem[{}].x,{};", word_offset, value);
}

} // namespace Shader::Backend::GLASM
