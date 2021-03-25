// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"

namespace Shader::Backend::SPIRV {
namespace {
Id LargeWarpBallot(EmitContext& ctx, Id ballot) {
    const Id shift{ctx.Constant(ctx.U32[1], 5)};
    const Id local_index{ctx.OpLoad(ctx.U32[1], ctx.subgroup_local_invocation_id)};
    return ctx.OpVectorExtractDynamic(ctx.U32[1], ballot, local_index);
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

Id EmitVoteAll(EmitContext& ctx, Id pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpSubgroupAllKHR(ctx.U1, pred);
    }
    const Id mask_ballot{ctx.OpSubgroupBallotKHR(ctx.U32[4], ctx.true_value)};
    const Id active_mask{LargeWarpBallot(ctx, mask_ballot)};
    const Id ballot{LargeWarpBallot(ctx, ctx.OpSubgroupBallotKHR(ctx.U32[4], pred))};
    const Id lhs{ctx.OpBitwiseAnd(ctx.U32[1], ballot, active_mask)};
    return ctx.OpIEqual(ctx.U1, lhs, active_mask);
}

Id EmitVoteAny(EmitContext& ctx, Id pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpSubgroupAnyKHR(ctx.U1, pred);
    }
    const Id mask_ballot{ctx.OpSubgroupBallotKHR(ctx.U32[4], ctx.true_value)};
    const Id active_mask{LargeWarpBallot(ctx, mask_ballot)};
    const Id ballot{LargeWarpBallot(ctx, ctx.OpSubgroupBallotKHR(ctx.U32[4], pred))};
    const Id lhs{ctx.OpBitwiseAnd(ctx.U32[1], ballot, active_mask)};
    return ctx.OpINotEqual(ctx.U1, lhs, ctx.u32_zero_value);
}

Id EmitVoteEqual(EmitContext& ctx, Id pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpSubgroupAllEqualKHR(ctx.U1, pred);
    }
    const Id mask_ballot{ctx.OpSubgroupBallotKHR(ctx.U32[4], ctx.true_value)};
    const Id active_mask{LargeWarpBallot(ctx, mask_ballot)};
    const Id ballot{LargeWarpBallot(ctx, ctx.OpSubgroupBallotKHR(ctx.U32[4], pred))};
    const Id lhs{ctx.OpBitwiseXor(ctx.U32[1], ballot, active_mask)};
    return ctx.OpLogicalOr(ctx.U1, ctx.OpIEqual(ctx.U1, lhs, ctx.u32_zero_value),
                           ctx.OpIEqual(ctx.U1, lhs, active_mask));
}

Id EmitSubgroupBallot(EmitContext& ctx, Id pred) {
    const Id ballot{ctx.OpSubgroupBallotKHR(ctx.U32[4], pred)};
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        return ctx.OpCompositeExtract(ctx.U32[1], ballot, 0U);
    }
    return LargeWarpBallot(ctx, ballot);
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

} // namespace Shader::Backend::SPIRV
