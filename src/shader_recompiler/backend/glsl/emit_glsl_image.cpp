// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {

void EmitImageSampleImplicitLod([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] const IR::Value& index,
                                [[maybe_unused]] std::string_view coords,
                                [[maybe_unused]] std::string_view bias_lc,
                                [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitImageSampleExplicitLod([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] const IR::Value& index,
                                [[maybe_unused]] std::string_view coords,
                                [[maybe_unused]] std::string_view lod_lc,
                                [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLSL Instruction");
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
