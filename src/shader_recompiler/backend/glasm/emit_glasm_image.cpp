// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

void EmitBindlessImageSampleImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageSampleExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageSampleDrefImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageSampleDrefExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageGather(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageGatherDref(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageFetch(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageQueryDimensions(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageQueryLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageGradient(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageRead(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageWrite(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageSampleImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageSampleExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageSampleDrefImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageSampleDrefExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageGather(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageGatherDref(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageFetch(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageQueryDimensions(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageQueryLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageGradient(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageRead(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageWrite(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

std::string Texture([[maybe_unused]] EmitContext& ctx, IR::TextureInstInfo info,
                    [[maybe_unused]] const IR::Value& index) {
    // FIXME
    return fmt::format("texture[{}]", info.descriptor_index);
}

void EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                Register coords, Register bias_lc,
                                [[maybe_unused]] const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{inst.GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp)};
    const std::string_view op{info.has_bias ? "TXB" : "TEX"};
    const std::string_view lod_clamp{info.has_lod_clamp ? ".LODCLAMP" : ""};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string texture{Texture(ctx, info, index)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    // FIXME
    const bool separate{info.type == TextureType::ColorArrayCube};
    if (separate) {
        ctx.Add("{}.F{}{} {},{},{},{},2D;", op, lod_clamp, sparse_mod, ret, coords, bias_lc,
                texture);
    } else {
        ctx.Add("MOV.F {}.w,{}.x;"
                "{}.F{}{} {},{},{},2D;",
                coords, bias_lc, op, lod_clamp, sparse_mod, ret, coords, texture);
    }
    if (sparse_inst) {
        const Register sparse_ret{ctx.reg_alloc.Define(*sparse_inst)};
        ctx.Add("MOV.S {},-1;"
                "MOV.S {}(NONRESIDENT),0;",
                sparse_ret, sparse_ret);
        sparse_inst->Invalidate();
    }
}

void EmitImageSampleExplicitLod([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                                [[maybe_unused]] const IR::Value& index,
                                [[maybe_unused]] Register coords, [[maybe_unused]] Register lod_lc,
                                [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageSampleDrefImplicitLod([[maybe_unused]] EmitContext& ctx,
                                    [[maybe_unused]] IR::Inst& inst,
                                    [[maybe_unused]] const IR::Value& index,
                                    [[maybe_unused]] Register coords,
                                    [[maybe_unused]] Register dref,
                                    [[maybe_unused]] Register bias_lc,
                                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageSampleDrefExplicitLod([[maybe_unused]] EmitContext& ctx,
                                    [[maybe_unused]] IR::Inst& inst,
                                    [[maybe_unused]] const IR::Value& index,
                                    [[maybe_unused]] Register coords,
                                    [[maybe_unused]] Register dref,
                                    [[maybe_unused]] Register lod_lc,
                                    [[maybe_unused]] const IR::Value& offset) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageGather([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                     [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                     [[maybe_unused]] const IR::Value& offset,
                     [[maybe_unused]] const IR::Value& offset2) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageGatherDref([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                         [[maybe_unused]] const IR::Value& offset,
                         [[maybe_unused]] const IR::Value& offset2,
                         [[maybe_unused]] Register dref) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageFetch([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                    [[maybe_unused]] Register offset, [[maybe_unused]] Register lod,
                    [[maybe_unused]] Register ms) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageQueryDimensions([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                              [[maybe_unused]] const IR::Value& index,
                              [[maybe_unused]] Register lod) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageQueryLod([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageGradient([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                       [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                       [[maybe_unused]] Register derivates, [[maybe_unused]] Register offset,
                       [[maybe_unused]] Register lod_clamp) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageRead([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageWrite([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                    [[maybe_unused]] Register color) {
    throw NotImplementedException("GLASM instruction");
}

} // namespace Shader::Backend::GLASM
