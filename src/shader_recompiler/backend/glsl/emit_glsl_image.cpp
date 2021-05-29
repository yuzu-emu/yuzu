// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
std::string Texture(EmitContext& ctx, const IR::TextureInstInfo& info,
                    [[maybe_unused]] const IR::Value& index) {
    if (info.type == TextureType::Buffer) {
        throw NotImplementedException("TextureType::Buffer");
    } else {
        return fmt::format("tex{}", ctx.texture_bindings.at(info.descriptor_index));
    }
}

std::string CastToIntVec(std::string_view value, const IR::TextureInstInfo& info) {
    switch (info.type) {
    case TextureType::Color1D:
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
        throw NotImplementedException("Offset type {}", info.type.Value());
    }
}

IR::Inst* PrepareSparse(IR::Inst& inst) {
    const auto sparse_inst{inst.GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp)};
    if (sparse_inst) {
        sparse_inst->Invalidate();
    }
    return sparse_inst;
}
} // namespace

void EmitImageSampleImplicitLod([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] const IR::Value& index,
                                [[maybe_unused]] std::string_view coords,
                                [[maybe_unused]] std::string_view bias_lc,
                                [[maybe_unused]] const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_lod_clamp) {
        throw NotImplementedException("Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    const auto bias{info.has_bias ? fmt::format(",{}", bias_lc) : ""};
    const auto texel{ctx.reg_alloc.Define(inst, Type::F32x4)};
    const auto sparse_inst{PrepareSparse(inst)};
    if (!sparse_inst) {
        if (!offset.IsEmpty()) {
            ctx.Add("{}=textureOffset({},{},{}{});", texel, texture, coords,
                    CastToIntVec(ctx.reg_alloc.Consume(offset), info), bias);
        } else {
            ctx.Add("{}=texture({},{}{});", texel, texture, coords, bias);
        }
        return;
    }
    // TODO: Query sparseTexels extension support
    if (!offset.IsEmpty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureOffsetARB({},{},{},{}{}));",
                  *sparse_inst, texture, coords, CastToIntVec(ctx.reg_alloc.Consume(offset), info),
                  texel, bias);
    } else {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureARB({},{},{}{}));", *sparse_inst,
                  texture, coords, texel, bias);
    }
}

void EmitImageSampleExplicitLod([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] const IR::Value& index,
                                [[maybe_unused]] std::string_view coords,
                                [[maybe_unused]] std::string_view lod_lc,
                                [[maybe_unused]] const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_bias) {
        throw NotImplementedException("Bias texture samples");
    }
    if (info.has_lod_clamp) {
        throw NotImplementedException("Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    const auto texel{ctx.reg_alloc.Define(inst, Type::F32x4)};
    const auto sparse_inst{PrepareSparse(inst)};
    if (!sparse_inst) {
        if (!offset.IsEmpty()) {
            ctx.Add("{}=textureLodOffset({},{},{},{});", texel, texture, coords, lod_lc,
                    CastToIntVec(ctx.reg_alloc.Consume(offset), info));
        } else {
            ctx.Add("{}=textureLod({},{},{});", texel, texture, coords, lod_lc);
        }
        return;
    }
    if (!offset.IsEmpty()) {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTexelFetchOffsetARB({},{},int({}),{},{}));",
                  *sparse_inst, texture, CastToIntVec(coords, info), lod_lc,
                  CastToIntVec(ctx.reg_alloc.Consume(offset), info), texel);
    } else {
        ctx.AddU1("{}=sparseTexelsResidentARB(sparseTextureLodARB({},{},{},{}));", *sparse_inst,
                  texture, coords, lod_lc, texel);
    }
}

void EmitImageSampleDrefImplicitLod([[maybe_unused]] EmitContext& ctx,
                                    [[maybe_unused]] IR::Inst& inst,
                                    [[maybe_unused]] const IR::Value& index,
                                    [[maybe_unused]] std::string_view coords,
                                    [[maybe_unused]] std::string_view dref,
                                    [[maybe_unused]] std::string_view bias_lc,
                                    [[maybe_unused]] const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_bias) {
        throw NotImplementedException("Bias texture samples");
    }
    if (info.has_lod_clamp) {
        throw NotImplementedException("Lod clamp samples");
    }
    const auto bias{info.has_bias ? fmt::format(",{}", bias_lc) : ""};
    const auto texture{Texture(ctx, info, index)};
    const auto vec_cast{info.type == TextureType::ColorArrayCube ? "vec4" : "vec3"};
    ctx.AddF32("{}=texture({},{}({},{}){});", inst, texture, vec_cast, dref, coords, bias);
}

void EmitImageSampleDrefExplicitLod([[maybe_unused]] EmitContext& ctx,
                                    [[maybe_unused]] IR::Inst& inst,
                                    [[maybe_unused]] const IR::Value& index,
                                    [[maybe_unused]] std::string_view coords,
                                    [[maybe_unused]] std::string_view dref,
                                    [[maybe_unused]] std::string_view lod_lc,
                                    [[maybe_unused]] const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_bias) {
        throw NotImplementedException("Bias texture samples");
    }
    if (info.has_lod_clamp) {
        throw NotImplementedException("Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    if (info.type == TextureType::ColorArrayCube) {
        ctx.AddF32("{}=textureLod({},{},{},{});", inst, texture, coords, dref, lod_lc);
    } else {
        ctx.AddF32("{}=textureLod({},vec3({},{}),{});", inst, texture, coords, dref, lod_lc);
    }
}

void EmitImageGather([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                     [[maybe_unused]] const IR::Value& index,
                     [[maybe_unused]] std::string_view coords,
                     [[maybe_unused]] const IR::Value& offset,
                     [[maybe_unused]] const IR::Value& offset2) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageGatherDref([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] const IR::Value& index,
                         [[maybe_unused]] std::string_view coords,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] const IR::Value& offset2,
                         [[maybe_unused]] std::string_view dref) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageFetch([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] const IR::Value& index,
                    [[maybe_unused]] std::string_view coords,
                    [[maybe_unused]] std::string_view offset, [[maybe_unused]] std::string_view lod,
                    [[maybe_unused]] std::string_view ms) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageQueryDimensions([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                              [[maybe_unused]] const IR::Value& index,
                              [[maybe_unused]] std::string_view lod) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageQueryLod([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] const IR::Value& index,
                       [[maybe_unused]] std::string_view coords) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageGradient([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] const IR::Value& index,
                       [[maybe_unused]] std::string_view coords,
                       [[maybe_unused]] std::string_view derivates,
                       [[maybe_unused]] std::string_view offset,
                       [[maybe_unused]] std::string_view lod_clamp) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageRead([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] const IR::Value& index,
                   [[maybe_unused]] std::string_view coords) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageWrite([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] const IR::Value& index,
                    [[maybe_unused]] std::string_view coords,
                    [[maybe_unused]] std::string_view color) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageSampleImplicitLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageSampleExplicitLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageSampleDrefImplicitLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageSampleDrefExplicitLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageGather(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageGatherDref(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageFetch(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageQueryDimensions(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageQueryLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageGradient(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageRead(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBindlessImageWrite(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageSampleImplicitLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageSampleExplicitLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageSampleDrefImplicitLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageSampleDrefExplicitLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageGather(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageGatherDref(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageFetch(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageQueryDimensions(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageQueryLod(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageGradient(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageRead(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitBoundImageWrite(EmitContext&) {
    throw NotImplementedException("GLSL Instruction");
}

} // namespace Shader::Backend::GLSL
