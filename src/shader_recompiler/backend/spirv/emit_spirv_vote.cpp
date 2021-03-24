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

} // namespace Shader::Backend::SPIRV
