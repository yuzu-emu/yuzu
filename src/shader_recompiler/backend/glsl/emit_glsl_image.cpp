// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
std::string Texture(EmitContext& ctx, IR::TextureInstInfo info,
                    [[maybe_unused]] const IR::Value& index) {
    if (info.type == TextureType::Buffer) {
        throw NotImplementedException("TextureType::Buffer");
    } else {
        return fmt::format("tex{}", ctx.texture_bindings.at(info.descriptor_index));
    }
}
} // namespace

void EmitImageSampleImplicitLod([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] const IR::Value& index,
                                [[maybe_unused]] std::string_view coords,
                                [[maybe_unused]] std::string_view bias_lc,
                                [[maybe_unused]] const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    if (info.has_bias) {
        throw NotImplementedException("Bias texture samples");
    }
    if (info.has_lod_clamp) {
        throw NotImplementedException("Lod clamp samples");
    }
    const auto texture{Texture(ctx, info, index)};
    if (!offset.IsEmpty()) {
        ctx.AddF32x4("{}=textureOffset({},{},ivec2({}));", inst, texture, coords,
                     ctx.reg_alloc.Consume(offset));
    } else {
        ctx.AddF32x4("{}=texture({},{});", inst, texture, coords);
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
    if (!offset.IsEmpty()) {
        ctx.AddF32x4("{}=textureLodOffset({},{},{},ivec2({}));", inst, texture, coords, lod_lc,
                     ctx.reg_alloc.Consume(offset));
    } else {
        ctx.AddF32x4("{}=textureLod({},{},{});", inst, texture, coords, lod_lc);
    }
}

void EmitImageSampleDrefImplicitLod([[maybe_unused]] EmitContext& ctx,
                                    [[maybe_unused]] IR::Inst& inst,
                                    [[maybe_unused]] const IR::Value& index,
                                    [[maybe_unused]] std::string_view coords,
                                    [[maybe_unused]] std::string_view dref,
                                    [[maybe_unused]] std::string_view bias_lc,
                                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageSampleDrefExplicitLod([[maybe_unused]] EmitContext& ctx,
                                    [[maybe_unused]] IR::Inst& inst,
                                    [[maybe_unused]] const IR::Value& index,
                                    [[maybe_unused]] std::string_view coords,
                                    [[maybe_unused]] std::string_view dref,
                                    [[maybe_unused]] std::string_view lod_lc,
                                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instruction");
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
