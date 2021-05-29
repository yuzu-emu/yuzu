// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

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
