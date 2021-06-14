// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLASM {

void EmitLaneId(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.S {}.x,{}.threadid;", inst, ctx.stage_name);
}

void EmitVoteAll(EmitContext& ctx, IR::Inst& inst, ScalarS32 pred) {
    ctx.Add("TGALL.S {}.x,{};", inst, pred);
}

void EmitVoteAny(EmitContext& ctx, IR::Inst& inst, ScalarS32 pred) {
    ctx.Add("TGANY.S {}.x,{};", inst, pred);
}

void EmitVoteEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 pred) {
    ctx.Add("TGEQ.S {}.x,{};", inst, pred);
}

void EmitSubgroupBallot(EmitContext& ctx, IR::Inst& inst, ScalarS32 pred) {
    ctx.Add("TGBALLOT {}.x,{};", inst, pred);
}

void EmitSubgroupEqMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.U {},{}.threadeqmask;", inst, ctx.stage_name);
}

void EmitSubgroupLtMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.U {},{}.threadltmask;", inst, ctx.stage_name);
}

void EmitSubgroupLeMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.U {},{}.threadlemask;", inst, ctx.stage_name);
}

void EmitSubgroupGtMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.U {},{}.threadgtmask;", inst, ctx.stage_name);
}

void EmitSubgroupGeMask(EmitContext& ctx, IR::Inst& inst) {
    ctx.Add("MOV.U {},{}.threadgemask;", inst, ctx.stage_name);
}

static void Shuffle(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                    const IR::Value& clamp, const IR::Value& segmentation_mask,
                    std::string_view op) {
    IR::Inst* const in_bounds{inst.GetAssociatedPseudoOperation(IR::Opcode::GetInBoundsFromOp)};
    if (in_bounds) {
        in_bounds->Invalidate();
    }
    std::string mask;
    if (clamp.IsImmediate() && segmentation_mask.IsImmediate()) {
        mask = fmt::to_string(clamp.U32() | (segmentation_mask.U32() << 8));
    } else {
        mask = "RC";
        ctx.Add("BFI.U RC.x,{{5,8,0,0}},{},{};",
                ScalarU32{ctx.reg_alloc.Consume(segmentation_mask)},
                ScalarU32{ctx.reg_alloc.Consume(clamp)});
    }
    const Register value_ret{ctx.reg_alloc.Define(inst)};
    if (in_bounds) {
        const Register bounds_ret{ctx.reg_alloc.Define(*in_bounds)};
        ctx.Add("SHF{}.U {},{},{},{};"
                "MOV.U {}.x,{}.y;",
                op, bounds_ret, value, index, mask, value_ret, bounds_ret);
    } else {
        ctx.Add("SHF{}.U {},{},{},{};"
                "MOV.U {}.x,{}.y;",
                op, value_ret, value, index, mask, value_ret, value_ret);
    }
}

void EmitShuffleIndex(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                      const IR::Value& clamp, const IR::Value& segmentation_mask) {
    Shuffle(ctx, inst, value, index, clamp, segmentation_mask, "IDX");
}

void EmitShuffleUp(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                   const IR::Value& clamp, const IR::Value& segmentation_mask) {
    Shuffle(ctx, inst, value, index, clamp, segmentation_mask, "UP");
}

void EmitShuffleDown(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                     const IR::Value& clamp, const IR::Value& segmentation_mask) {
    Shuffle(ctx, inst, value, index, clamp, segmentation_mask, "DOWN");
}

void EmitShuffleButterfly(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 index,
                          const IR::Value& clamp, const IR::Value& segmentation_mask) {
    Shuffle(ctx, inst, value, index, clamp, segmentation_mask, "XOR");
}

void EmitFSwizzleAdd(EmitContext& ctx, IR::Inst& inst, ScalarF32 op_a, ScalarF32 op_b,
                     ScalarU32 swizzle) {
    const auto ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("AND.U RC.z,{}.threadid,3;"
            "SHL.U RC.z,RC.z,1;"
            "SHR.U RC.z,{},RC.z;"
            "AND.U RC.z,RC.z,3;"
            "MUL.F RC.x,{},FSWZA[RC.z];"
            "MUL.F RC.y,{},FSWZB[RC.z];"
            "ADD.F {}.x,RC.x,RC.y;",
            ctx.stage_name, swizzle, op_a, op_b, ret);
}

void EmitDPdxFine(EmitContext& ctx, IR::Inst& inst, ScalarF32 p) {
    if (ctx.profile.support_derivative_control) {
        ctx.Add("DDX.FINE {}.x,{};", inst, p);
    } else {
        LOG_WARNING(Shader_GLASM, "Fine derivatives not supported by device");
        ctx.Add("DDX {}.x,{};", inst, p);
    }
}

void EmitDPdyFine(EmitContext& ctx, IR::Inst& inst, ScalarF32 p) {
    if (ctx.profile.support_derivative_control) {
        ctx.Add("DDY.FINE {}.x,{};", inst, p);
    } else {
        LOG_WARNING(Shader_GLASM, "Fine derivatives not supported by device");
        ctx.Add("DDY {}.x,{};", inst, p);
    }
}

void EmitDPdxCoarse(EmitContext& ctx, IR::Inst& inst, ScalarF32 p) {
    if (ctx.profile.support_derivative_control) {
        ctx.Add("DDX.COARSE {}.x,{};", inst, p);
    } else {
        LOG_WARNING(Shader_GLASM, "Coarse derivatives not supported by device");
        ctx.Add("DDX {}.x,{};", inst, p);
    }
}

void EmitDPdyCoarse(EmitContext& ctx, IR::Inst& inst, ScalarF32 p) {
    if (ctx.profile.support_derivative_control) {
        ctx.Add("DDY.COARSE {}.x,{};", inst, p);
    } else {
        LOG_WARNING(Shader_GLASM, "Coarse derivatives not supported by device");
        ctx.Add("DDY {}.x,{};", inst, p);
    }
}

} // namespace Shader::Backend::GLASM
