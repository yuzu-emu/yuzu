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
} // namespace

void EmitLaneId([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst) {
    throw NotImplementedException("GLSL Instruction");
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
    ctx.AddU32("{}=uvec2(gl_SubGroupEqMaskARB).x;", inst);
}

void EmitSubgroupLtMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uvec2(gl_SubGroupLtMaskARB).x;", inst);
}

void EmitSubgroupLeMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uvec2(gl_SubGroupLeMaskARB).x;", inst);
}

void EmitSubgroupGtMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uvec2(gl_SubGroupGtMaskARB).x;", inst);
}

void EmitSubgroupGeMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.AddU32("{}=uvec2(gl_SubGroupGeMaskARB).x;", inst);
}

void EmitShuffleIndex(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                      std::string_view index, std::string_view clamp,
                      std::string_view segmentation_mask) {
    const auto not_seg_mask{fmt::format("(~{})", segmentation_mask)};
    const auto thread_id{"gl_SubGroupInvocationARB"};
    const auto min_thread_id{ComputeMinThreadId(thread_id, segmentation_mask)};
    const auto max_thread_id{ComputeMaxThreadId(min_thread_id, clamp, not_seg_mask)};

    const auto lhs{fmt::format("({}&{})", index, not_seg_mask)};
    const auto src_thread_id{fmt::format("({})|({})", lhs, min_thread_id)};
    ctx.Add("shfl_in_bounds=int({})<=int({});", src_thread_id, max_thread_id);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?{}:{};", inst, value, src_thread_id);
}

void EmitShuffleUp(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view index,
                   std::string_view clamp, std::string_view segmentation_mask) {
    const auto thread_id{"gl_SubGroupInvocationARB"};
    const auto max_thread_id{GetMaxThreadId(thread_id, clamp, segmentation_mask)};
    const auto src_thread_id{fmt::format("({}-{})", thread_id, index)};
    ctx.Add("shfl_in_bounds=int({})>=int({});", src_thread_id, max_thread_id);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?{}:{};", inst, value, src_thread_id);
}

void EmitShuffleDown(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                     std::string_view index, std::string_view clamp,
                     std::string_view segmentation_mask) {
    const auto thread_id{"gl_SubGroupInvocationARB"};
    const auto max_thread_id{GetMaxThreadId(thread_id, clamp, segmentation_mask)};
    const auto src_thread_id{fmt::format("({}+{})", thread_id, index)};
    ctx.Add("shfl_in_bounds=int({})<=int({});", src_thread_id, max_thread_id);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?{}:{};", inst, value, src_thread_id);
}

void EmitShuffleButterfly(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                          std::string_view index, std::string_view clamp,
                          std::string_view segmentation_mask) {
    const auto thread_id{"gl_SubGroupInvocationARB"};
    const auto max_thread_id{GetMaxThreadId(thread_id, clamp, segmentation_mask)};
    const auto src_thread_id{fmt::format("({}^{})", thread_id, index)};
    ctx.Add("shfl_in_bounds=int({})<=int({});", src_thread_id, max_thread_id);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?{}:{};", inst, value, src_thread_id);
}

void EmitFSwizzleAdd([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                     [[maybe_unused]] std::string_view op_a, [[maybe_unused]] std::string_view op_b,
                     [[maybe_unused]] std::string_view swizzle) {
    throw NotImplementedException("GLSL Instruction");
}

void EmitDPdxFine(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    ctx.AddF32("{}=dFdxFine({});", inst, op_a);
}

void EmitDPdyFine(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    ctx.AddF32("{}=dFdyFine({});", inst, op_a);
}

void EmitDPdxCoarse(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    ctx.AddF32("{}=dFdxCoarse({});", inst, op_a);
}

void EmitDPdyCoarse(EmitContext& ctx, IR::Inst& inst, std::string_view op_a) {
    ctx.AddF32("{}=dFdyCoarse({});", inst, op_a);
}
} // namespace Shader::Backend::GLSL
