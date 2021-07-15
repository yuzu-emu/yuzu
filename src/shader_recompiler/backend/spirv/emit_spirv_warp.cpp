// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"

namespace Shader::Backend::SPIRV {
namespace {
Id WarpExtract(EmitContext& ctx, Id value) {
    const Id local_index{ctx.OpLoad(ctx.U32[1], ctx.subgroup_local_invocation_id)};
    return ctx.OpVectorExtractDynamic(ctx.U32[1], value, local_index);
}

Id LoadMask(EmitContext& ctx, Id mask) {
    const Id value{ctx.OpLoad(ctx.U32[4], mask)};
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpCompositeExtract(ctx.U32[1], value, 0U);
    }
    return WarpExtract(ctx, value);
}

void SetInBoundsFlag(IR::Inst* inst, Id result) {
    IR::Inst* const in_bounds{inst->GetAssociatedPseudoOperation(IR::Opcode::GetInBoundsFromOp)};
    if (!in_bounds) {
        return;
    }
    in_bounds->SetDefinition(result);
    in_bounds->Invalidate();
}

Id ComputeMinThreadId(EmitContext& ctx, Id thread_id, Id segmentation_mask) {
    return ctx.OpBitwiseAnd(ctx.U32[1], thread_id, segmentation_mask);
}

Id ComputeMaxThreadId(EmitContext& ctx, Id min_thread_id, Id clamp, Id not_seg_mask) {
    return ctx.OpBitwiseOr(ctx.U32[1], min_thread_id,
                           ctx.OpBitwiseAnd(ctx.U32[1], clamp, not_seg_mask));
}

Id GetMaxThreadId(EmitContext& ctx, Id thread_id, Id clamp, Id segmentation_mask) {
    const Id not_seg_mask{ctx.OpNot(ctx.U32[1], segmentation_mask)};
    const Id min_thread_id{ComputeMinThreadId(ctx, thread_id, segmentation_mask)};
    return ComputeMaxThreadId(ctx, min_thread_id, clamp, not_seg_mask);
}

Id SelectValue(EmitContext& ctx, Id in_range, Id value, Id src_thread_id) {
    return ctx.OpSelect(ctx.U32[1], in_range,
                        ctx.OpSubgroupReadInvocationKHR(ctx.U32[1], value, src_thread_id), value);
}
} // Anonymous namespace

Id EmitLaneId(EmitContext& ctx) {
    const Id id{ctx.OpLoad(ctx.U32[1], ctx.subgroup_local_invocation_id)};
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return id;
    }
    return ctx.OpBitwiseAnd(ctx.U32[1], id, ctx.Const(31U));
}

Id EmitVoteAll(EmitContext& ctx, Id pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpSubgroupAllKHR(ctx.U1, pred);
    }
    const Id mask_ballot{ctx.OpSubgroupBallotKHR(ctx.U32[4], ctx.true_value)};
    const Id active_mask{WarpExtract(ctx, mask_ballot)};
    const Id ballot{WarpExtract(ctx, ctx.OpSubgroupBallotKHR(ctx.U32[4], pred))};
    const Id lhs{ctx.OpBitwiseAnd(ctx.U32[1], ballot, active_mask)};
    return ctx.OpIEqual(ctx.U1, lhs, active_mask);
}

Id EmitVoteAny(EmitContext& ctx, Id pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpSubgroupAnyKHR(ctx.U1, pred);
    }
    const Id mask_ballot{ctx.OpSubgroupBallotKHR(ctx.U32[4], ctx.true_value)};
    const Id active_mask{WarpExtract(ctx, mask_ballot)};
    const Id ballot{WarpExtract(ctx, ctx.OpSubgroupBallotKHR(ctx.U32[4], pred))};
    const Id lhs{ctx.OpBitwiseAnd(ctx.U32[1], ballot, active_mask)};
    return ctx.OpINotEqual(ctx.U1, lhs, ctx.u32_zero_value);
}

Id EmitVoteEqual(EmitContext& ctx, Id pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpSubgroupAllEqualKHR(ctx.U1, pred);
    }
    const Id mask_ballot{ctx.OpSubgroupBallotKHR(ctx.U32[4], ctx.true_value)};
    const Id active_mask{WarpExtract(ctx, mask_ballot)};
    const Id ballot{WarpExtract(ctx, ctx.OpSubgroupBallotKHR(ctx.U32[4], pred))};
    const Id lhs{ctx.OpBitwiseXor(ctx.U32[1], ballot, active_mask)};
    return ctx.OpLogicalOr(ctx.U1, ctx.OpIEqual(ctx.U1, lhs, ctx.u32_zero_value),
                           ctx.OpIEqual(ctx.U1, lhs, active_mask));
}

Id EmitSubgroupBallot(EmitContext& ctx, Id pred) {
    const Id ballot{ctx.OpSubgroupBallotKHR(ctx.U32[4], pred)};
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpCompositeExtract(ctx.U32[1], ballot, 0U);
    }
    return WarpExtract(ctx, ballot);
}

Id EmitSubgroupEqMask(EmitContext& ctx) {
    return LoadMask(ctx, ctx.subgroup_mask_eq);
}

Id EmitSubgroupLtMask(EmitContext& ctx) {
    return LoadMask(ctx, ctx.subgroup_mask_lt);
}

Id EmitSubgroupLeMask(EmitContext& ctx) {
    return LoadMask(ctx, ctx.subgroup_mask_le);
}

Id EmitSubgroupGtMask(EmitContext& ctx) {
    return LoadMask(ctx, ctx.subgroup_mask_gt);
}

Id EmitSubgroupGeMask(EmitContext& ctx) {
    return LoadMask(ctx, ctx.subgroup_mask_ge);
}

Id EmitShuffleIndex(EmitContext& ctx, IR::Inst* inst, Id value, Id index, Id clamp,
                    Id segmentation_mask) {
    const Id not_seg_mask{ctx.OpNot(ctx.U32[1], segmentation_mask)};
    const Id thread_id{ctx.OpLoad(ctx.U32[1], ctx.subgroup_local_invocation_id)};
    const Id min_thread_id{ComputeMinThreadId(ctx, thread_id, segmentation_mask)};
    const Id max_thread_id{ComputeMaxThreadId(ctx, min_thread_id, clamp, not_seg_mask)};

    const Id lhs{ctx.OpBitwiseAnd(ctx.U32[1], index, not_seg_mask)};
    const Id src_thread_id{ctx.OpBitwiseOr(ctx.U32[1], lhs, min_thread_id)};
    const Id in_range{ctx.OpSLessThanEqual(ctx.U1, src_thread_id, max_thread_id)};

    SetInBoundsFlag(inst, in_range);
    return SelectValue(ctx, in_range, value, src_thread_id);
}

Id EmitShuffleUp(EmitContext& ctx, IR::Inst* inst, Id value, Id index, Id clamp,
                 Id segmentation_mask) {
    const Id thread_id{ctx.OpLoad(ctx.U32[1], ctx.subgroup_local_invocation_id)};
    const Id max_thread_id{GetMaxThreadId(ctx, thread_id, clamp, segmentation_mask)};
    const Id src_thread_id{ctx.OpISub(ctx.U32[1], thread_id, index)};
    const Id in_range{ctx.OpSGreaterThanEqual(ctx.U1, src_thread_id, max_thread_id)};

    SetInBoundsFlag(inst, in_range);
    return SelectValue(ctx, in_range, value, src_thread_id);
}

Id EmitShuffleDown(EmitContext& ctx, IR::Inst* inst, Id value, Id index, Id clamp,
                   Id segmentation_mask) {
    const Id thread_id{ctx.OpLoad(ctx.U32[1], ctx.subgroup_local_invocation_id)};
    const Id max_thread_id{GetMaxThreadId(ctx, thread_id, clamp, segmentation_mask)};
    const Id src_thread_id{ctx.OpIAdd(ctx.U32[1], thread_id, index)};
    const Id in_range{ctx.OpSLessThanEqual(ctx.U1, src_thread_id, max_thread_id)};

    SetInBoundsFlag(inst, in_range);
    return SelectValue(ctx, in_range, value, src_thread_id);
}

Id EmitShuffleButterfly(EmitContext& ctx, IR::Inst* inst, Id value, Id index, Id clamp,
                        Id segmentation_mask) {
    const Id thread_id{ctx.OpLoad(ctx.U32[1], ctx.subgroup_local_invocation_id)};
    const Id max_thread_id{GetMaxThreadId(ctx, thread_id, clamp, segmentation_mask)};
    const Id src_thread_id{ctx.OpBitwiseXor(ctx.U32[1], thread_id, index)};
    const Id in_range{ctx.OpSLessThanEqual(ctx.U1, src_thread_id, max_thread_id)};

    SetInBoundsFlag(inst, in_range);
    return SelectValue(ctx, in_range, value, src_thread_id);
}

Id EmitFSwizzleAdd(EmitContext& ctx, Id op_a, Id op_b, Id swizzle) {
    const Id three{ctx.Const(3U)};
    Id mask{ctx.OpLoad(ctx.U32[1], ctx.subgroup_local_invocation_id)};
    mask = ctx.OpBitwiseAnd(ctx.U32[1], mask, three);
    mask = ctx.OpShiftLeftLogical(ctx.U32[1], mask, ctx.Const(1U));
    mask = ctx.OpShiftRightLogical(ctx.U32[1], swizzle, mask);
    mask = ctx.OpBitwiseAnd(ctx.U32[1], mask, three);

    const Id modifier_a{ctx.OpVectorExtractDynamic(ctx.F32[1], ctx.fswzadd_lut_a, mask)};
    const Id modifier_b{ctx.OpVectorExtractDynamic(ctx.F32[1], ctx.fswzadd_lut_b, mask)};

    const Id result_a{ctx.OpFMul(ctx.F32[1], op_a, modifier_a)};
    const Id result_b{ctx.OpFMul(ctx.F32[1], op_b, modifier_b)};
    return ctx.OpFAdd(ctx.F32[1], result_a, result_b);
}

Id EmitDPdxFine(EmitContext& ctx, Id op_a) {
    return ctx.OpDPdxFine(ctx.F32[1], op_a);
}

Id EmitDPdyFine(EmitContext& ctx, Id op_a) {
    return ctx.OpDPdyFine(ctx.F32[1], op_a);
}

Id EmitDPdxCoarse(EmitContext& ctx, Id op_a) {
    return ctx.OpDPdxCoarse(ctx.F32[1], op_a);
}

Id EmitDPdyCoarse(EmitContext& ctx, Id op_a) {
    return ctx.OpDPdyCoarse(ctx.F32[1], op_a);
}

} // namespace Shader::Backend::SPIRV
