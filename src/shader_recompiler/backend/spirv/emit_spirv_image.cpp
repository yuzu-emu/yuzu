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
        return ctx.OpLoad(def.type, def.id);
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

} // namespace Shader::Backend::SPIRV
