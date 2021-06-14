// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
namespace {
std::string Texture(EmitContext& ctx, const IR::TextureInstInfo& info, const IR::Value& index) {
    const auto def{info.type == TextureType::Buffer ? ctx.texture_buffers.at(info.descriptor_index)
                                                    : ctx.textures.at(info.descriptor_index)};
    const auto index_offset{def.count > 1 ? fmt::format("[{}]", ctx.var_alloc.Consume(index)) : ""};
    return fmt::format("tex{}{}", def.binding, index_offset);
}

std::string Image(EmitContext& ctx, const IR::TextureInstInfo& info, const IR::Value& index) {
    const auto def{info.type == TextureType::Buffer ? ctx.image_buffers.at(info.descriptor_index)
                                                    : ctx.images.at(info.descriptor_index)};
    const auto index_offset{def.count > 1 ? fmt::format("[{}]", ctx.var_alloc.Consume(index)) : ""};
    return fmt::format("img{}{}", def.binding, index_offset);
}

std::string CastToIntVec(std::string_view value, const IR::TextureInstInfo& info) {
    switch (info.type) {
    case TextureType::Color1D:
    case TextureType::Buffer:
        return fmt::format("int({})", value);
    case TextureType::ColorArray1D:
    case TextureType::Color2D:
    case TextureType::ColorArray2D:
        return fmt::format("ivec2({})", value);
    case TextureType::Color3D:
    case TextureType::ColorCube:
        return fmt::format("ivec3({})", value);
    case TextureType::ColorArrayCube:
        return fmt::format("ivec4({})", value);
    default:
        throw NotImplementedException("Integer cast for TextureType {}", info.type.Value());
    }
}

std::string CoordsCastToInt(std::string_view value, const IR::TextureInstInfo& info) {
    switch (info.type) {
    case TextureType::Color1D:
    case TextureType::Buffer:
        return fmt::format("int({})", value);
    case TextureType::ColorArray1D:
    case TextureType::Color2D:
        return fmt::format("ivec2({})", value);
    case TextureType::ColorArray2D:
    case TextureType::Color3D:
    case TextureType::ColorCube:
        return fmt::format("ivec3({})", value);
    case TextureType::ColorArrayCube:
        return fmt::format("ivec4({})", value);
    default:
        throw NotImplementedException("TexelFetchCast type {}", info.type.Value());
    }
}

bool NeedsShadowLodExt(TextureType type) {
    switch (type) {
    case TextureType::ColorArray2D:
    case TextureType::ColorCube:
    case TextureType::ColorArrayCube:
        return true;
    default:
        return false;
    }
}

std::string GetOffsetVec(EmitContext& ctx, const IR::Value& offset) {
    if (offset.IsImmediate()) {
        return fmt::format("int({})", offset.U32());
    }
    IR::Inst* const inst{offset.InstRecursive()};
    if (inst->AreAllArgsImmediates()) {
        switch (inst->GetOpcode()) {
        case IR::Opcode::CompositeConstructU32x2:
            return fmt::format("ivec2({},{})", inst->Arg(0).U32(), inst->Arg(1).U32());
        case IR::Opcode::CompositeConstructU32x3:
            return fmt::format("ivec3({},{},{})", inst->Arg(0).U32(), inst->Arg(1).U32(),
                               inst->Arg(2).U32());
        case IR::Opcode::CompositeConstructU32x4:
            return fmt::format("ivec4({},{},{},{})", inst->Arg(0).U32(), inst->Arg(1).U32(),
                               inst->Arg(2).U32(), inst->Arg(3).U32());
        default:
            break;
        }
    }
    const bool has_var_aoffi{ctx.profile.support_gl_variable_aoffi};
    if (!has_var_aoffi) {
        LOG_WARNING(Shader_GLSL, "Device does not support variable texture offsets, STUBBING");
    }
    const auto offset_str{has_var_aoffi ? ctx.var_alloc.Consume(offset) : "0"};
    switch (offset.Type()) {
    case IR::Type::U32:
        return fmt::format("int({})", offset_str);
    case IR::Type::U32x2:
        return fmt::format("ivec2({})", offset_str);
    case IR::Type::U32x3:
        return fmt::format("ivec3({})", offset_str);
    case IR::Type::U32x4:
        return fmt::format("ivec4({})", offset_str);
    default:
        throw NotImplementedException("Offset type {}", offset.Type());
    }
}

std::string PtpOffsets(const IR::Value& offset, const IR::Value& offset2) {
    const std::array values{offset.InstRecursive(), offset2.InstRecursive()};
    if (!values[0]->AreAllArgsImmediates() || !values[1]->AreAllArgsImmediates()) {
        LOG_WARNING(Shader_GLSL, "Not all arguments in PTP are immediate, STUBBING");
        return "ivec2[](ivec2(0), ivec2(1), ivec2(2), ivec2(3))";
    }
    const IR::Opcode opcode{values[0]->GetOpcode()};
    if (opcode != values[1]->GetOpcode() || opcode != IR::Opcode::CompositeConstructU32x4) {
        throw LogicError("Invalid PTP arguments");
    }
    auto read{[&](unsigned int a, unsigned int b) { return values[a]->Arg(b).U32(); }};

    return fmt::format("ivec2[](ivec2({},{}),ivec2({},{}),ivec2({},{}),ivec2({},{}))", read(0, 0),
                       read(0, 1), read(0, 2), read(0, 3), read(1, 0), read(1, 1), read(1, 2),
                       read(1, 3));
}

IR::Inst* PrepareSparse(IR::Inst& inst) {
    const auto sparse_inst{inst.GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp)};
    if (sparse_inst) {
        sparse_inst->Invalidate();
    }
    return sparse_inst;
}
} // Anonymous namespace

void EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                std::string_view coords, std::string_view bias_lc,
                                const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_lod_clamp) {
        throw NotImplementedException("EmitImageSampleImplicitLod Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    const auto bias{info.has_bias ? fmt::format(",{}", bias_lc) : ""};
    const auto texel{ctx.var_alloc.Define(inst, GlslVarType::F32x4)};
    const auto sparse_inst{PrepareSparse(inst)};
    const bool supports_sparse{ctx.profile.support_gl_sparse_textures};
    if (sparse_inst && !supports_sparse) {
        LOG_WARNING(Shader_GLSL, "Device does not support sparse texture queries. STUBBING");
        ctx.AddU1("{}=true;", *sparse_inst);
    }
    if (!sparse_inst || !supports_sparse) {
        if (!offset.IsEmpty()) {
            const auto offset_str{GetOffsetVec(ctx, offset)};
            if (ctx.stage == Stage::Fragment) {
                ctx.Add("{}=textureOffset({},{},{}{});", texel, texture, coords, offset_str, bias);
            } else {
                ctx.Add("{}=textureLodOffset({},{},0.0,{});", texel, texture, coords, offset_str);
            }
        } else {
            if (ctx.stage == Stage::Fragment) {
                ctx.Add("{}=texture({},{}{});", texel, texture, coords, bias);
            } else {
                ctx.Add("{}=textureLod({},{},0.0);", texel, texture, coords);
            }
        }
        return;
    }
    if (!offset.IsEmpty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureOffsetARB({},{},{},{}{}));",
                  *sparse_inst, texture, coords, GetOffsetVec(ctx, offset), texel, bias);
    } else {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureARB({},{},{}{}));", *sparse_inst,
                  texture, coords, texel, bias);
    }
}

void EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                std::string_view coords, std::string_view lod_lc,
                                const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_bias) {
        throw NotImplementedException("EmitImageSampleExplicitLod Bias texture samples");
    }
    if (info.has_lod_clamp) {
        throw NotImplementedException("EmitImageSampleExplicitLod Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    const auto texel{ctx.var_alloc.Define(inst, GlslVarType::F32x4)};
    const auto sparse_inst{PrepareSparse(inst)};
    const bool supports_sparse{ctx.profile.support_gl_sparse_textures};
    if (sparse_inst && !supports_sparse) {
        LOG_WARNING(Shader_GLSL, "Device does not support sparse texture queries. STUBBING");
        ctx.AddU1("{}=true;", *sparse_inst);
    }
    if (!sparse_inst || !supports_sparse) {
        if (!offset.IsEmpty()) {
            ctx.Add("{}=textureLodOffset({},{},{},{});", texel, texture, coords, lod_lc,
                    GetOffsetVec(ctx, offset));
        } else {
            ctx.Add("{}=textureLod({},{},{});", texel, texture, coords, lod_lc);
        }
        return;
    }
    if (!offset.IsEmpty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTexelFetchOffsetARB({},{},int({}),{},{}));",
                  *sparse_inst, texture, CastToIntVec(coords, info), lod_lc,
                  GetOffsetVec(ctx, offset), texel);
    } else {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureLodARB({},{},{},{}));", *sparse_inst,
                  texture, coords, lod_lc, texel);
    }
}

void EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    std::string_view coords, std::string_view dref,
                                    std::string_view bias_lc, const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{PrepareSparse(inst)};
    if (sparse_inst) {
        throw NotImplementedException("EmitImageSampleDrefImplicitLod Sparse texture samples");
    }
    if (info.has_bias) {
        throw NotImplementedException("EmitImageSampleDrefImplicitLod Bias texture samples");
    }
    if (info.has_lod_clamp) {
        throw NotImplementedException("EmitImageSampleDrefImplicitLod Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    const auto bias{info.has_bias ? fmt::format(",{}", bias_lc) : ""};
    const bool needs_shadow_ext{NeedsShadowLodExt(info.type)};
    const auto cast{needs_shadow_ext ? "vec4" : "vec3"};
    const bool use_grad{!ctx.profile.support_gl_texture_shadow_lod &&
                        ctx.stage != Stage::Fragment && needs_shadow_ext};
    if (use_grad) {
        LOG_WARNING(Shader_GLSL,
                    "Device lacks GL_EXT_texture_shadow_lod. Using textureGrad fallback");
        if (info.type == TextureType::ColorArrayCube) {
            LOG_WARNING(Shader_GLSL, "textureGrad does not support ColorArrayCube. Stubbing");
            ctx.AddF32("{}=0.0f;", inst);
            return;
        }
        const auto d_cast{info.type == TextureType::ColorArray2D ? "vec2" : "vec3"};
        ctx.AddF32("{}=textureGrad({},{}({},{}),{}(0),{}(0));", inst, texture, cast, coords, dref,
                   d_cast, d_cast);
        return;
    }
    if (!offset.IsEmpty()) {
        const auto offset_str{GetOffsetVec(ctx, offset)};
        if (ctx.stage == Stage::Fragment) {
            ctx.AddF32("{}=textureOffset({},{}({},{}),{}{});", inst, texture, cast, coords, dref,
                       offset_str, bias);
        } else {
            ctx.AddF32("{}=textureLodOffset({},{}({},{}),0.0,{});", inst, texture, cast, coords,
                       dref, offset_str);
        }
    } else {
        if (ctx.stage == Stage::Fragment) {
            if (info.type == TextureType::ColorArrayCube) {
                ctx.AddF32("{}=texture({},vec4({}),{});", inst, texture, coords, dref);
            } else {
                ctx.AddF32("{}=texture({},{}({},{}){});", inst, texture, cast, coords, dref, bias);
            }
        } else {
            ctx.AddF32("{}=textureLod({},{}({},{}),0.0);", inst, texture, cast, coords, dref);
        }
    }
}

void EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    std::string_view coords, std::string_view dref,
                                    std::string_view lod_lc, const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{PrepareSparse(inst)};
    if (sparse_inst) {
        throw NotImplementedException("EmitImageSampleDrefExplicitLod Sparse texture samples");
    }
    if (info.has_bias) {
        throw NotImplementedException("EmitImageSampleDrefExplicitLod Bias texture samples");
    }
    if (info.has_lod_clamp) {
        throw NotImplementedException("EmitImageSampleDrefExplicitLod Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    const bool needs_shadow_ext{NeedsShadowLodExt(info.type)};
    const bool use_grad{!ctx.profile.support_gl_texture_shadow_lod && needs_shadow_ext};
    const auto cast{needs_shadow_ext ? "vec4" : "vec3"};
    if (use_grad) {
        LOG_WARNING(Shader_GLSL,
                    "Device lacks GL_EXT_texture_shadow_lod. Using textureGrad fallback");
        if (info.type == TextureType::ColorArrayCube) {
            LOG_WARNING(Shader_GLSL, "textureGrad does not support ColorArrayCube. Stubbing");
            ctx.AddF32("{}=0.0f;", inst);
            return;
        }
        const auto d_cast{info.type == TextureType::ColorArray2D ? "vec2" : "vec3"};
        ctx.AddF32("{}=textureGrad({},{}({},{}),{}(0),{}(0));", inst, texture, cast, coords, dref,
                   d_cast, d_cast);
        return;
    }
    if (!offset.IsEmpty()) {
        const auto offset_str{GetOffsetVec(ctx, offset)};
        if (info.type == TextureType::ColorArrayCube) {
            ctx.AddF32("{}=textureLodOffset({},{},{},{},{});", inst, texture, coords, dref, lod_lc,
                       offset_str);
        } else {
            ctx.AddF32("{}=textureLodOffset({},{}({},{}),{},{});", inst, texture, cast, coords,
                       dref, lod_lc, offset_str);
        }
    } else {
        if (info.type == TextureType::ColorArrayCube) {
            ctx.AddF32("{}=textureLod({},{},{},{});", inst, texture, coords, dref, lod_lc);
        } else {
            ctx.AddF32("{}=textureLod({},{}({},{}),{});", inst, texture, cast, coords, dref,
                       lod_lc);
        }
    }
}

void EmitImageGather(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                     std::string_view coords, const IR::Value& offset, const IR::Value& offset2) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto texture{Texture(ctx, info, index)};
    const auto texel{ctx.var_alloc.Define(inst, GlslVarType::F32x4)};
    const auto sparse_inst{PrepareSparse(inst)};
    const bool supports_sparse{ctx.profile.support_gl_sparse_textures};
    if (sparse_inst && !supports_sparse) {
        LOG_WARNING(Shader_GLSL, "Device does not support sparse texture queries. STUBBING");
        ctx.AddU1("{}=true;", *sparse_inst);
    }
    if (!sparse_inst || !supports_sparse) {
        if (offset.IsEmpty()) {
            ctx.Add("{}=textureGather({},{},int({}));", texel, texture, coords,
                    info.gather_component);
            return;
        }
        if (offset2.IsEmpty()) {
            ctx.Add("{}=textureGatherOffset({},{},{},int({}));", texel, texture, coords,
                    GetOffsetVec(ctx, offset), info.gather_component);
            return;
        }
        // PTP
        const auto offsets{PtpOffsets(offset, offset2)};
        ctx.Add("{}=textureGatherOffsets({},{},{},int({}));", texel, texture, coords, offsets,
                info.gather_component);
        return;
    }
    if (offset.IsEmpty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureGatherARB({},{},{},int({})));",
                  *sparse_inst, texture, coords, texel, info.gather_component);
        return;
    }
    if (offset2.IsEmpty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureGatherOffsetARB({},{},{},{},int({})));",
                  *sparse_inst, texture, CastToIntVec(coords, info), GetOffsetVec(ctx, offset),
                  texel, info.gather_component);
        return;
    }
    // PTP
    const auto offsets{PtpOffsets(offset, offset2)};
    ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureGatherOffsetARB({},{},{},{},int({})));",
              *sparse_inst, texture, CastToIntVec(coords, info), offsets, texel,
              info.gather_component);
}

void EmitImageGatherDref(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                         std::string_view coords, const IR::Value& offset, const IR::Value& offset2,
                         std::string_view dref) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto texture{Texture(ctx, info, index)};
    const auto texel{ctx.var_alloc.Define(inst, GlslVarType::F32x4)};
    const auto sparse_inst{PrepareSparse(inst)};
    const bool supports_sparse{ctx.profile.support_gl_sparse_textures};
    if (sparse_inst && !supports_sparse) {
        LOG_WARNING(Shader_GLSL, "Device does not support sparse texture queries. STUBBING");
        ctx.AddU1("{}=true;", *sparse_inst);
    }
    if (!sparse_inst || !supports_sparse) {
        if (offset.IsEmpty()) {
            ctx.Add("{}=textureGather({},{},{});", texel, texture, coords, dref);
            return;
        }
        if (offset2.IsEmpty()) {
            ctx.Add("{}=textureGatherOffset({},{},{},{});", texel, texture, coords, dref,
                    GetOffsetVec(ctx, offset));
            return;
        }
        // PTP
        const auto offsets{PtpOffsets(offset, offset2)};
        ctx.Add("{}=textureGatherOffsets({},{},{},{});", texel, texture, coords, dref, offsets);
        return;
    }
    if (offset.IsEmpty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureGatherARB({},{},{},{}));", *sparse_inst,
                  texture, coords, dref, texel);
        return;
    }
    if (offset2.IsEmpty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureGatherOffsetARB({},{},{},,{},{}));",
                  *sparse_inst, texture, CastToIntVec(coords, info), dref,
                  GetOffsetVec(ctx, offset), texel);
        return;
    }
    // PTP
    const auto offsets{PtpOffsets(offset, offset2)};
    ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureGatherOffsetARB({},{},{},,{},{}));",
              *sparse_inst, texture, CastToIntVec(coords, info), dref, offsets, texel);
}

void EmitImageFetch(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                    std::string_view coords, std::string_view offset, std::string_view lod,
                    [[maybe_unused]] std::string_view ms) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_bias) {
        throw NotImplementedException("EmitImageFetch Bias texture samples");
    }
    if (info.has_lod_clamp) {
        throw NotImplementedException("EmitImageFetch Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    const auto sparse_inst{PrepareSparse(inst)};
    const auto texel{ctx.var_alloc.Define(inst, GlslVarType::F32x4)};
    const bool supports_sparse{ctx.profile.support_gl_sparse_textures};
    if (sparse_inst && !supports_sparse) {
        LOG_WARNING(Shader_GLSL, "Device does not support sparse texture queries. STUBBING");
        ctx.AddU1("{}=true;", *sparse_inst);
    }
    if (!sparse_inst || !supports_sparse) {
        if (!offset.empty()) {
            ctx.Add("{}=texelFetchOffset({},{},int({}),{});", texel, texture,
                    CoordsCastToInt(coords, info), lod, CoordsCastToInt(offset, info));
        } else {
            if (info.type == TextureType::Buffer) {
                ctx.Add("{}=texelFetch({},int({}));", texel, texture, coords);
            } else {
                ctx.Add("{}=texelFetch({},{},int({}));", texel, texture,
                        CoordsCastToInt(coords, info), lod);
            }
        }
        return;
    }
    if (!offset.empty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTexelFetchOffsetARB({},{},int({}),{},{}));",
                  *sparse_inst, texture, CastToIntVec(coords, info), lod,
                  CastToIntVec(offset, info), texel);
    } else {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTexelFetchARB({},{},int({}),{}));",
                  *sparse_inst, texture, CastToIntVec(coords, info), lod, texel);
    }
}

void EmitImageQueryDimensions(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                              std::string_view lod) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto texture{Texture(ctx, info, index)};
    switch (info.type) {
    case TextureType::Color1D:
        return ctx.AddU32x4(
            "{}=uvec4(uint(textureSize({},int({}))),0u,0u,uint(textureQueryLevels({})));", inst,
            texture, lod, texture);
    case TextureType::ColorArray1D:
    case TextureType::Color2D:
    case TextureType::ColorCube:
        return ctx.AddU32x4(
            "{}=uvec4(uvec2(textureSize({},int({}))),0u,uint(textureQueryLevels({})));", inst,
            texture, lod, texture);
    case TextureType::ColorArray2D:
    case TextureType::Color3D:
    case TextureType::ColorArrayCube:
        return ctx.AddU32x4(
            "{}=uvec4(uvec3(textureSize({},int({}))),uint(textureQueryLevels({})));", inst, texture,
            lod, texture);
    case TextureType::Buffer:
        throw NotImplementedException("EmitImageQueryDimensions Texture buffers");
    }
    throw LogicError("Unspecified image type {}", info.type.Value());
}

void EmitImageQueryLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                       std::string_view coords) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto texture{Texture(ctx, info, index)};
    return ctx.AddF32x4("{}=vec4(textureQueryLod({},{}),0.0,0.0);", inst, texture, coords);
}

void EmitImageGradient(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                       std::string_view coords, const IR::Value& derivatives,
                       const IR::Value& offset, [[maybe_unused]] const IR::Value& lod_clamp) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_lod_clamp) {
        throw NotImplementedException("EmitImageGradient Lod clamp samples");
    }
    const auto sparse_inst{PrepareSparse(inst)};
    if (sparse_inst) {
        throw NotImplementedException("EmitImageGradient Sparse");
    }
    if (!offset.IsEmpty()) {
        throw NotImplementedException("EmitImageGradient offset");
    }
    const auto texture{Texture(ctx, info, index)};
    const auto texel{ctx.var_alloc.Define(inst, GlslVarType::F32x4)};
    const bool multi_component{info.num_derivates > 1 || info.has_lod_clamp};
    const auto derivatives_vec{ctx.var_alloc.Consume(derivatives)};
    if (multi_component) {
        ctx.Add("{}=textureGrad({},{},vec2({}.xz),vec2({}.yz));", texel, texture, coords,
                derivatives_vec, derivatives_vec);
    } else {
        ctx.Add("{}=textureGrad({},{},float({}.x),float({}.y));", texel, texture, coords,
                derivatives_vec, derivatives_vec);
    }
}

void EmitImageRead(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                   std::string_view coords) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{PrepareSparse(inst)};
    if (sparse_inst) {
        throw NotImplementedException("EmitImageRead Sparse");
    }
    const auto image{Image(ctx, info, index)};
    ctx.AddU32x4("{}=uvec4(imageLoad({},{}));", inst, image, CoordsCastToInt(coords, info));
}

void EmitImageWrite(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                    std::string_view coords, std::string_view color) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.Add("imageStore({},{},{});", image, CoordsCastToInt(coords, info), color);
}

void EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicAdd({},{},{});", inst, image, CoordsCastToInt(coords, info), value);
}

void EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicMin({},{},int({}));", inst, image, CoordsCastToInt(coords, info),
               value);
}

void EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicMin({},{},uint({}));", inst, image, CoordsCastToInt(coords, info),
               value);
}

void EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicMax({},{},int({}));", inst, image, CoordsCastToInt(coords, info),
               value);
}

void EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                           std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicMax({},{},uint({}));", inst, image, CoordsCastToInt(coords, info),
               value);
}

void EmitImageAtomicInc32(EmitContext&, IR::Inst&, const IR::Value&, std::string_view,
                          std::string_view) {
    NotImplemented();
}

void EmitImageAtomicDec32(EmitContext&, IR::Inst&, const IR::Value&, std::string_view,
                          std::string_view) {
    NotImplemented();
}

void EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicAnd({},{},{});", inst, image, CoordsCastToInt(coords, info), value);
}

void EmitImageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                         std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicOr({},{},{});", inst, image, CoordsCastToInt(coords, info), value);
}

void EmitImageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                          std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicXor({},{},{});", inst, image, CoordsCastToInt(coords, info), value);
}

void EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                               std::string_view coords, std::string_view value) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto image{Image(ctx, info, index)};
    ctx.AddU32("{}=imageAtomicExchange({},{},{});", inst, image, CoordsCastToInt(coords, info),
               value);
}

void EmitBindlessImageSampleImplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageSampleExplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageSampleDrefImplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageSampleDrefExplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageGather(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageGatherDref(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageFetch(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageQueryDimensions(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageQueryLod(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageGradient(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageRead(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageWrite(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageSampleImplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageSampleExplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageSampleDrefImplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageSampleDrefExplicitLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageGather(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageGatherDref(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageFetch(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageQueryDimensions(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageQueryLod(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageGradient(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageRead(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageWrite(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicIAdd32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicSMin32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicUMin32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicSMax32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicUMax32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicInc32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicDec32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicAnd32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicOr32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicXor32(EmitContext&) {
    NotImplemented();
}

void EmitBindlessImageAtomicExchange32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicIAdd32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicSMin32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicUMin32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicSMax32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicUMax32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicInc32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicDec32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicAnd32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicOr32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicXor32(EmitContext&) {
    NotImplemented();
}

void EmitBoundImageAtomicExchange32(EmitContext&) {
    NotImplemented();
}

} // namespace Shader::Backend::GLSL
