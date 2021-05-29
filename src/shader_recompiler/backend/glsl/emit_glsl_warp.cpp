// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
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
} // namespace

void EmitShuffleIndex(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                      std::string_view index, std::string_view clamp,
                      std::string_view segmentation_mask) {
    ctx.Add("shfl_in_bounds=int(gl_SubGroupInvocationARB-{})>=int((gl_SubGroupInvocationARB&{})|({}"
            "&~{}));",
            index, segmentation_mask, clamp, segmentation_mask);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?{}:gl_SubGroupInvocationARB-{};", inst, value, index);
}

void EmitShuffleUp(EmitContext& ctx, IR::Inst& inst, std::string_view value, std::string_view index,
                   std::string_view clamp, std::string_view segmentation_mask) {
    ctx.Add("shfl_in_bounds=int(gl_SubGroupInvocationARB-{})>=int((gl_SubGroupInvocationARB&{})|({}"
            "&~{}));",
            index, segmentation_mask, clamp, segmentation_mask);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?readInvocationARB({},gl_SubGroupInvocationARB-{}):"
               "{};",
               inst, value, index, value);
}

void EmitShuffleDown(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                     std::string_view index, std::string_view clamp,
                     std::string_view segmentation_mask) {
    ctx.Add("shfl_in_bounds=int(gl_SubGroupInvocationARB-{})>=int((gl_SubGroupInvocationARB&{})|({}"
            "&~{}));",
            index, segmentation_mask, clamp, segmentation_mask);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?{}:gl_SubGroupInvocationARB-{};", inst, value, index);
}

void EmitShuffleButterfly(EmitContext& ctx, IR::Inst& inst, std::string_view value,
                          std::string_view index, std::string_view clamp,
                          std::string_view segmentation_mask) {
    ctx.Add("shfl_in_bounds=int(gl_SubGroupInvocationARB-{})>=int((gl_SubGroupInvocationARB&{})|({}"
            "&~{}));",
            index, segmentation_mask, clamp, segmentation_mask);
    SetInBoundsFlag(ctx, inst);
    ctx.AddU32("{}=shfl_in_bounds?{}:gl_SubGroupInvocationARB-{};", inst, value, index);
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
