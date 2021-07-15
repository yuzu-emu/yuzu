// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
namespace {
void SetInBoundsFlag(EmitContext& ctx, IR::Inst& inst) {
    IR::Inst* const in_bounds{inst.GetAssociatedPseudoOperation(IR::Opcode::GetInBoundsFromOp)};
    if (!in_bounds) {
        return;
    }
    ctx.AddU1("{}=shfl_in_bounds;", *in_bounds);
    in_bounds->Invalidate();
}

std::string ComputeMinThreadId(std::string_view thread_id, std::string_view segmentation_mask) {
    return fmt::format("({}&{})", thread_id, segmentation_mask);
}

std::string ComputeMaxThreadId(std::string_view min_thread_id, std::string_view clamp,
                               std::string_view not_seg_mask) {
    return fmt::format("({})|({}&{})", min_thread_id, clamp, not_seg_mask);
}

std::string GetMaxThreadId(std::string_view thread_id, std::string_view clamp,
                           std::string_view segmentation_mask) {
    const auto not_seg_mask{fmt::format("(~{})", segmentation_mask)};
    const auto min_thread_id{ComputeMinThreadId(thread_id, segmentation_mask)};
    return ComputeMaxThreadId(min_thread_id, clamp, not_seg_mask);
}

void UseShuffleNv(EmitContext& ctx, IR::Inst& inst, std::string_view shfl_op,
                  std::string_view value, std::string_view index,
                  [[maybe_unused]] std::string_view clamp, std::string_view segmentation_mask) {
    const auto width{fmt::format("32u>>(bitCount({}&31u))", segmentation_mask)};
    ctx.AddU32("{}={}({},{},{},shfl_in_bounds);", inst, shfl_op, value, index, width);
    SetInBoundsFlag(ctx, inst);
}
} // Anonymous namespace

void EmitLaneId(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=gl_SubGroupInvocationARB&31u;", inst);
}

void EmitVoteAll(EmitContext& ctx, IR::Inst& inst, std::string_view pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        ctx.AddU1("{}=allInvocationsEqualARB({});", inst, pred);
    } else {
        const auto active_mask{fmt::format("uvec2(ballotARB(true))[gl_SubGroupInvocationARB]")};
        const auto ballot{fmt::format("uvec2(ballotARB({}))[gl_SubGroupInvocationARB]", pred)};
        ctx.AddU1("{}=({}&{})=={};", inst, ballot, active_mask, active_mask);
    }
}

void EmitVoteAny(EmitContext& ctx, IR::Inst& inst, std::string_view pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        ctx.AddU1("{}=anyInvocationARB({});", inst, pred);
    } else {
        const auto active_mask{fmt::format("uvec2(ballotARB(true))[gl_SubGroupInvocationARB]")};
        const auto ballot{fmt::format("uvec2(ballotARB({}))[gl_SubGroupInvocationARB]", pred)};
        ctx.AddU1("{}=({}&{})!=0u;", inst, ballot, active_mask, active_mask);
    }
}

void EmitVoteEqual(EmitContext& ctx, IR::Inst& inst, std::string_view pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        ctx.AddU1("{}=allInvocationsEqualARB({});", inst, pred);
    } else {
        const auto active_mask{fmt::format("uvec2(ballotARB(true))[gl_SubGroupInvocationARB]")};
        const auto ballot{fmt::format("uvec2(ballotARB({}))[gl_SubGroupInvocationARB]", pred)};
        const auto value{fmt::format("({}^{})", ballot, active_mask)};
        ctx.AddU1("{}=({}==0)||({}=={});", inst, value, value, active_mask);
    }
}

void EmitSubgroupBallot(EmitContext& ctx, IR::Inst& inst, std::string_view pred) {
    if (!ctx.profile.warp_size_potentially_larger_than_guest) {
        ctx.AddU32("{}=uvec2(ballotARB({})).x;", inst, pred);
    } else {
        ctx.AddU32("{}=uvec2(ballotARB({}))[gl_SubGroupInvocationARB];", inst, pred);
    }
}

void EmitSubgroupEqMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uint(gl_SubGroupEqMaskARB.x);", inst);
}

void EmitSubgroupLtMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uint(gl_SubGroupLtMaskARB.x);", inst);
}

void EmitSubgroupLeMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uint(gl_SubGroupLeMaskARB.x);", inst);
}

void EmitSubgroupGtMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uint(gl_SubGroupGtMaskARB.x);", inst);
}

void EmitSubgroupGeMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uint(gl_SubGroupGeMaskARB.x);", inst);
}

void EmitShuffleIndex(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                      std::string_view index, std::string_view clamp,
                      std::string_view segmentation_mask) {
    if (ctx.profile.support_gl_warp_intrinsics) {
        UseShuffleNv(ctx, inst, "shuffleNV", value, index, clamp, segmentation_mask);
        return;
    }
    const auto not_seg_mask{fmt::format("(~{})", segmentation_mask)};
    const auto thread_id{"gl_SubGroupInvocationARB"};
    const auto min_thread_id{ComputeMinThreadId(thread_id, segmentation_mask)};
    const auto max_thread_id{ComputeMaxThreadId(min_thread_id, clamp, not_seg_mask)};

    const auto lhs{fmt::format("({}&{})", index, not_seg_mask)};
    const auto src_thread_id{fmt::format("({})|({})", lhs, min_thread_id)};
    ctx.Add("shfl_in_bounds=int({})<=int({});", src_thread_id, max_thread_id);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?readInvocationARB({},{}):{};", inst, value, src_thread_id, value);
}

void EmitShuffleUp(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view index,
                   std::string_view clamp, std::string_view segmentation_mask) {
    if (ctx.profile.support_gl_warp_intrinsics) {
        UseShuffleNv(ctx, inst, "shuffleUpNV", value, index, clamp, segmentation_mask);
        return;
    }
    const auto thread_id{"gl_SubGroupInvocationARB"};
    const auto max_thread_id{GetMaxThreadId(thread_id, clamp, segmentation_mask)};
    const auto src_thread_id{fmt::format("({}-{})", thread_id, index)};
    ctx.Add("shfl_in_bounds=int({})>=int({});", src_thread_id, max_thread_id);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?readInvocationARB({},{}):{};", inst, value, src_thread_id, value);
}

void EmitShuffleDown(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                     std::string_view index, std::string_view clamp,
                     std::string_view segmentation_mask) {
    if (ctx.profile.support_gl_warp_intrinsics) {
        UseShuffleNv(ctx, inst, "shuffleDownNV", value, index, clamp, segmentation_mask);
        return;
    }
    const auto thread_id{"gl_SubGroupInvocationARB"};
    const auto max_thread_id{GetMaxThreadId(thread_id, clamp, segmentation_mask)};
    const auto src_thread_id{fmt::format("({}+{})", thread_id, index)};
    ctx.Add("shfl_in_bounds=int({})<=int({});", src_thread_id, max_thread_id);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?readInvocationARB({},{}):{};", inst, value, src_thread_id, value);
}

void EmitShuffleButterfly(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                          std::string_view index, std::string_view clamp,
                          std::string_view segmentation_mask) {
    if (ctx.profile.support_gl_warp_intrinsics) {
        UseShuffleNv(ctx, inst, "shuffleXorNV", value, index, clamp, segmentation_mask);
        return;
    }
    const auto thread_id{"gl_SubGroupInvocationARB"};
    const auto max_thread_id{GetMaxThreadId(thread_id, clamp, segmentation_mask)};
    const auto src_thread_id{fmt::format("({}^{})", thread_id, index)};
    ctx.Add("shfl_in_bounds=int({})<=int({});", src_thread_id, max_thread_id);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?readInvocationARB({},{}):{};", inst, value, src_thread_id, value);
}

void EmitFSwizzleAdd(EmitContext& ctx, IR::Inst& inst, std::string_view op_a, std::string_view op_b,
                     std::string_view swizzle) {
    const auto mask{fmt::format("({}>>((gl_SubGroupInvocationARB&3)<<1))&3", swizzle)};
    const std::string modifier_a = fmt::format("FSWZ_A[{}]", mask);
    const std::string modifier_b = fmt::format("FSWZ_B[{}]", mask);
    ctx.AddF32("{}=({}*{})+({}*{});", inst, op_a, modifier_a, op_b, modifier_b);
}

void EmitDPdxFine(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    if (ctx.profile.support_gl_derivative_control) {
        ctx.AddF32("{}=dFdxFine({});", inst, op_a);
    } else {
        LOG_WARNING(Shader_GLSL, "Device does not support dFdxFine, fallback to dFdx");
        ctx.AddF32("{}=dFdx({});", inst, op_a);
    }
}

void EmitDPdyFine(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    if (ctx.profile.support_gl_derivative_control) {
        ctx.AddF32("{}=dFdyFine({});", inst, op_a);
    } else {
        LOG_WARNING(Shader_GLSL, "Device does not support dFdyFine, fallback to dFdy");
        ctx.AddF32("{}=dFdy({});", inst, op_a);
    }
}

void EmitDPdxCoarse(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    if (ctx.profile.support_gl_derivative_control) {
        ctx.AddF32("{}=dFdxCoarse({});", inst, op_a);
    } else {
        LOG_WARNING(Shader_GLSL, "Device does not support dFdxCoarse, fallback to dFdx");
        ctx.AddF32("{}=dFdx({});", inst, op_a);
    }
}

void EmitDPdyCoarse(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    if (ctx.profile.support_gl_derivative_control) {
        ctx.AddF32("{}=dFdyCoarse({});", inst, op_a);
    } else {
        LOG_WARNING(Shader_GLSL, "Device does not support dFdyCoarse, fallback to dFdy");
        ctx.AddF32("{}=dFdy({});", inst, op_a);
    }
}
} // namespace Shader::Backend::GLSL
