// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/container/static_vector.hpp>

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/ir/modifiers.h"

namespace Shader::Backend::SPIRV {
namespace {
class ImageOperands {
public:
    explicit ImageOperands(EmitContext& ctx, bool has_bias, bool has_lod, bool has_lod_clamp,
                           Id lod, Id offset) {
        if (has_bias) {
            const Id bias{has_lod_clamp ? ctx.OpCompositeExtract(ctx.F32[1], lod, 0) : lod};
            Add(spv::ImageOperandsMask::Bias, bias);
        }
        if (has_lod) {
            const Id lod_value{has_lod_clamp ? ctx.OpCompositeExtract(ctx.F32[1], lod, 0) : lod};
            Add(spv::ImageOperandsMask::Lod, lod_value);
        }
        if (Sirit::ValidId(offset)) {
            Add(spv::ImageOperandsMask::Offset, offset);
        }
        if (has_lod_clamp) {
            const Id lod_clamp{has_bias ? ctx.OpCompositeExtract(ctx.F32[1], lod, 1) : lod};
            Add(spv::ImageOperandsMask::MinLod, lod_clamp);
        }
    }

    explicit ImageOperands(EmitContext& ctx, const IR::Value& offset, const IR::Value& offset2) {
        if (offset2.IsEmpty()) {
            if (offset.IsEmpty()) {
                return;
            }
            Add(spv::ImageOperandsMask::Offset, ctx.Def(offset));
            return;
        }
        const std::array values{offset.InstRecursive(), offset2.InstRecursive()};
        if (!values[0]->AreAllArgsImmediates() || !values[1]->AreAllArgsImmediates()) {
            throw NotImplementedException("Not all arguments in PTP are immediate");
        }
        const IR::Opcode opcode{values[0]->Opcode()};
        if (opcode != values[1]->Opcode() || opcode != IR::Opcode::CompositeConstructU32x4) {
            throw LogicError("Invalid PTP arguments");
        }
        auto read{[&](int a, int b) { return ctx.Constant(ctx.U32[1], values[a]->Arg(b).U32()); }};

        const Id offsets{
            ctx.ConstantComposite(ctx.TypeArray(ctx.U32[2], ctx.Constant(ctx.U32[1], 4)),
                                  ctx.ConstantComposite(ctx.U32[2], read(0, 0), read(0, 1)),
                                  ctx.ConstantComposite(ctx.U32[2], read(0, 2), read(0, 3)),
                                  ctx.ConstantComposite(ctx.U32[2], read(1, 0), read(1, 1)),
                                  ctx.ConstantComposite(ctx.U32[2], read(1, 2), read(1, 3)))};
        Add(spv::ImageOperandsMask::ConstOffsets, offsets);
    }

    explicit ImageOperands(Id offset, Id lod, Id ms) {
        if (Sirit::ValidId(lod)) {
            Add(spv::ImageOperandsMask::Lod, lod);
        }
        if (Sirit::ValidId(offset)) {
            Add(spv::ImageOperandsMask::Offset, offset);
        }
        if (Sirit::ValidId(ms)) {
            Add(spv::ImageOperandsMask::Sample, ms);
        }
    }

    void Add(spv::ImageOperandsMask new_mask, Id value) {
        mask = static_cast<spv::ImageOperandsMask>(static_cast<unsigned>(mask) |
                                                   static_cast<unsigned>(new_mask));
        operands.push_back(value);
    }

    std::span<const Id> Span() const noexcept {
        return std::span{operands.data(), operands.size()};
    }

    spv::ImageOperandsMask Mask() const noexcept {
        return mask;
    }

private:
    boost::container::static_vector<Id, 3> operands;
    spv::ImageOperandsMask mask{};
};

Id Texture(EmitContext& ctx, const IR::Value& index) {
    if (index.IsImmediate()) {
        const TextureDefinition def{ctx.textures.at(index.U32())};
        return ctx.OpLoad(def.sampled_type, def.id);
    }
    throw NotImplementedException("Indirect texture sample");
}

Id TextureImage(EmitContext& ctx, const IR::Value& index) {
    if (index.IsImmediate()) {
        const TextureDefinition def{ctx.textures.at(index.U32())};
        return ctx.OpImage(def.image_type, ctx.OpLoad(def.sampled_type, def.id));
    }
    throw NotImplementedException("Indirect texture sample");
}

Id Decorate(EmitContext& ctx, IR::Inst* inst, Id sample) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    if (info.relaxed_precision != 0) {
        ctx.Decorate(sample, spv::Decoration::RelaxedPrecision);
    }
    return sample;
}

template <typename MethodPtrType, typename... Args>
Id Emit(MethodPtrType sparse_ptr, MethodPtrType non_sparse_ptr, EmitContext& ctx, IR::Inst* inst,
        Id result_type, Args&&... args) {
    IR::Inst* const sparse{inst->GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp)};
    if (!sparse) {
        return Decorate(ctx, inst, (ctx.*non_sparse_ptr)(result_type, std::forward<Args>(args)...));
    }
    const Id struct_type{ctx.TypeStruct(ctx.U32[1], result_type)};
    const Id sample{(ctx.*sparse_ptr)(struct_type, std::forward<Args>(args)...)};
    const Id resident_code{ctx.OpCompositeExtract(ctx.U32[1], sample, 0U)};
    sparse->SetDefinition(ctx.OpImageSparseTexelsResident(ctx.U1, resident_code));
    sparse->Invalidate();
    Decorate(ctx, inst, sample);
    return ctx.OpCompositeExtract(result_type, sample, 1U);
}
} // Anonymous namespace

Id EmitBindlessImageSampleImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageSampleExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageSampleDrefImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageSampleDrefExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageGather(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageGatherDref(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageFetch(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageQueryDimensions(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBindlessImageQueryLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageSampleImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageSampleExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageSampleDrefImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageSampleDrefExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageGather(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageGatherDref(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageFetch(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageQueryDimensions(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitBoundImageQueryLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

Id EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                              Id bias_lc, Id offset) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const ImageOperands operands(ctx, info.has_bias != 0, false, info.has_lod_clamp != 0, bias_lc,
                                 offset);
    return Emit(&EmitContext::OpImageSparseSampleImplicitLod,
                &EmitContext::OpImageSampleImplicitLod, ctx, inst, ctx.F32[4], Texture(ctx, index),
                coords, operands.Mask(), operands.Span());
}

Id EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                              Id lod_lc, Id offset) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const ImageOperands operands(ctx, false, true, info.has_lod_clamp != 0, lod_lc, offset);
    return Emit(&EmitContext::OpImageSparseSampleExplicitLod,
                &EmitContext::OpImageSampleExplicitLod, ctx, inst, ctx.F32[4], Texture(ctx, index),
                coords, operands.Mask(), operands.Span());
}

Id EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                  Id coords, Id dref, Id bias_lc, Id offset) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const ImageOperands operands(ctx, info.has_bias != 0, false, info.has_lod_clamp != 0, bias_lc,
                                 offset);
    return Emit(&EmitContext::OpImageSparseSampleDrefImplicitLod,
                &EmitContext::OpImageSampleDrefImplicitLod, ctx, inst, ctx.F32[1],
                Texture(ctx, index), coords, dref, operands.Mask(), operands.Span());
}

Id EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst* inst, const IR::Value& index,
                                  Id coords, Id dref, Id lod_lc, Id offset) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const ImageOperands operands(ctx, false, true, info.has_lod_clamp != 0, lod_lc, offset);
    return Emit(&EmitContext::OpImageSparseSampleDrefExplicitLod,
                &EmitContext::OpImageSampleDrefExplicitLod, ctx, inst, ctx.F32[1],
                Texture(ctx, index), coords, dref, operands.Mask(), operands.Span());
}

Id EmitImageGather(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                   const IR::Value& offset, const IR::Value& offset2) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const ImageOperands operands(ctx, offset, offset2);
    return Emit(&EmitContext::OpImageSparseGather, &EmitContext::OpImageGather, ctx, inst,
                ctx.F32[4], Texture(ctx, index), coords,
                ctx.Constant(ctx.U32[1], info.gather_component.Value()), operands.Mask(),
                operands.Span());
}

Id EmitImageGatherDref(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords,
                       const IR::Value& offset, const IR::Value& offset2, Id dref) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const ImageOperands operands(ctx, offset, offset2);
    return Emit(&EmitContext::OpImageSparseDrefGather, &EmitContext::OpImageDrefGather, ctx, inst,
                ctx.F32[4], Texture(ctx, index), coords, dref, operands.Mask(), operands.Span());
}

Id EmitImageFetch(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id coords, Id offset,
                  Id lod, Id ms) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const ImageOperands operands(offset, lod, ms);
    return Emit(&EmitContext::OpImageSparseFetch, &EmitContext::OpImageFetch, ctx, inst, ctx.F32[4],
                TextureImage(ctx, index), coords, operands.Mask(), operands.Span());
}

Id EmitImageQueryDimensions(EmitContext& ctx, IR::Inst* inst, const IR::Value& index, Id lod) {
    const auto info{inst->Flags<IR::TextureInstInfo>()};
    const Id image{TextureImage(ctx, index)};
    const Id zero{ctx.u32_zero_value};
    const auto mips{[&] { return ctx.OpImageQueryLevels(ctx.U32[1], image); }};
    switch (info.type) {
    case TextureType::Color1D:
    case TextureType::Shadow1D:
        return ctx.OpCompositeConstruct(ctx.U32[4], ctx.OpImageQuerySizeLod(ctx.U32[1], image, lod),
                                        zero, zero, mips());
    case TextureType::ColorArray1D:
    case TextureType::Color2D:
    case TextureType::ColorCube:
    case TextureType::ShadowArray1D:
    case TextureType::Shadow2D:
    case TextureType::ShadowCube:
        return ctx.OpCompositeConstruct(ctx.U32[4], ctx.OpImageQuerySizeLod(ctx.U32[2], image, lod),
                                        zero, mips());
    case TextureType::ColorArray2D:
    case TextureType::Color3D:
    case TextureType::ColorArrayCube:
    case TextureType::ShadowArray2D:
    case TextureType::Shadow3D:
    case TextureType::ShadowArrayCube:
        return ctx.OpCompositeConstruct(ctx.U32[4], ctx.OpImageQuerySizeLod(ctx.U32[3], image, lod),
                                        mips());
    }
    throw LogicError("Unspecified image type {}", info.type.Value());
}

Id EmitImageQueryLod(EmitContext& ctx, IR::Inst*, const IR::Value& index, Id coords) {
    const Id zero{ctx.f32_zero_value};
    const Id sampler{Texture(ctx, index)};
    return ctx.OpCompositeConstruct(ctx.F32[4], ctx.OpImageQueryLod(ctx.F32[2], sampler, coords),
                                    zero, zero);
}

} // namespace Shader::Backend::SPIRV
