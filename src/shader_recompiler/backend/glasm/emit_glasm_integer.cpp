// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
void BitwiseLogicalOp(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b,
                      std::string_view lop) {
    const auto zero = inst.GetAssociatedPseudoOperation(IR::Opcode::GetZeroFromOp);
    const auto sign = inst.GetAssociatedPseudoOperation(IR::Opcode::GetSignFromOp);
    if (zero) {
        zero->Invalidate();
    }
    if (sign) {
        sign->Invalidate();
    }
    if (zero || sign) {
        ctx.reg_alloc.InvalidateConditionCodes();
    }
    const auto ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("{}.S {}.x,{},{};", lop, ret, a, b);
    if (zero) {
        ctx.Add("SEQ.S {},{},0;", *zero, ret);
    }
    if (sign) {
        ctx.Add("SLT.S {},{},0;", *sign, ret);
    }
}
} // Anonymous namespace

void EmitIAdd32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    const std::array flags{
        inst.GetAssociatedPseudoOperation(IR::Opcode::GetZeroFromOp),
        inst.GetAssociatedPseudoOperation(IR::Opcode::GetSignFromOp),
        inst.GetAssociatedPseudoOperation(IR::Opcode::GetCarryFromOp),
        inst.GetAssociatedPseudoOperation(IR::Opcode::GetOverflowFromOp),
    };
    for (IR::Inst* const flag_inst : flags) {
        if (flag_inst) {
            flag_inst->Invalidate();
        }
    }
    const bool cc{inst.HasAssociatedPseudoOperation()};
    const std::string_view cc_mod{cc ? ".CC" : ""};
    if (cc) {
        ctx.reg_alloc.InvalidateConditionCodes();
    }
    const auto ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("ADD.S{} {}.x,{},{};", cc_mod, ret, a, b);
    if (!cc) {
        return;
    }
    static constexpr std::array<std::string_view, 4> masks{"", "SF", "CF", "OF"};
    for (size_t flag_index = 0; flag_index < flags.size(); ++flag_index) {
        if (!flags[flag_index]) {
            continue;
        }
        const auto flag_ret{ctx.reg_alloc.Define(*flags[flag_index])};
        if (flag_index == 0) {
            ctx.Add("SEQ.S {}.x,{}.x,0;", flag_ret, ret);
        } else {
            // We could use conditional execution here, but it's broken on Nvidia's compiler
            ctx.Add("IF {}.x;"
                    "MOV.S {}.x,-1;"
                    "ELSE;"
                    "MOV.S {}.x,0;"
                    "ENDIF;",
                    masks[flag_index], flag_ret, flag_ret);
        }
    }
}

void EmitIAdd64(EmitContext& ctx, IR::Inst& inst, Register a, Register b) {
    ctx.LongAdd("ADD.S64 {}.x,{}.x,{}.x;", inst, a, b);
}

void EmitISub32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("SUB.S {}.x,{},{};", inst, a, b);
}

void EmitISub64(EmitContext& ctx, IR::Inst& inst, Register a, Register b) {
    ctx.LongAdd("SUB.S64 {}.x,{}.x,{}.x;", inst, a, b);
}

void EmitIMul32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("MUL.S {}.x,{},{};", inst, a, b);
}

void EmitINeg32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    if (value.type != Type::Register && static_cast<s32>(value.imm_u32) < 0) {
        ctx.Add("MOV.S {},{};", inst, -static_cast<s32>(value.imm_u32));
    } else {
        ctx.Add("MOV.S {},-{};", inst, value);
    }
}

void EmitINeg64(EmitContext& ctx, IR::Inst& inst, Register value) {
    ctx.LongAdd("MOV.S64 {},-{};", inst, value);
}

void EmitIAbs32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("ABS.S {},{};", inst, value);
}

void EmitShiftLeftLogical32(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 shift) {
    ctx.Add("SHL.U {}.x,{},{};", inst, base, shift);
}

void EmitShiftLeftLogical64(EmitContext& ctx, IR::Inst& inst, ScalarRegister base,
                            ScalarU32 shift) {
    ctx.LongAdd("SHL.U64 {}.x,{},{};", inst, base, shift);
}

void EmitShiftRightLogical32(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 shift) {
    ctx.Add("SHR.U {}.x,{},{};", inst, base, shift);
}

void EmitShiftRightLogical64(EmitContext& ctx, IR::Inst& inst, ScalarRegister base,
                             ScalarU32 shift) {
    ctx.LongAdd("SHR.U64 {}.x,{},{};", inst, base, shift);
}

void EmitShiftRightArithmetic32(EmitContext& ctx, IR::Inst& inst, ScalarS32 base, ScalarS32 shift) {
    ctx.Add("SHR.S {}.x,{},{};", inst, base, shift);
}

void EmitShiftRightArithmetic64(EmitContext& ctx, IR::Inst& inst, ScalarRegister base,
                                ScalarS32 shift) {
    ctx.LongAdd("SHR.S64 {}.x,{},{};", inst, base, shift);
}

void EmitBitwiseAnd32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    BitwiseLogicalOp(ctx, inst, a, b, "AND");
}

void EmitBitwiseOr32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    BitwiseLogicalOp(ctx, inst, a, b, "OR");
}

void EmitBitwiseXor32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    BitwiseLogicalOp(ctx, inst, a, b, "XOR");
}

void EmitBitFieldInsert(EmitContext& ctx, IR::Inst& inst, ScalarS32 base, ScalarS32 insert,
                        ScalarS32 offset, ScalarS32 count) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (count.type != Type::Register && offset.type != Type::Register) {
        ctx.Add("BFI.S {},{{{},{},0,0}},{},{};", ret, count, offset, insert, base);
    } else {
        ctx.Add("MOV.S RC.x,{};"
                "MOV.S RC.y,{};"
                "BFI.S {},RC,{},{};",
                count, offset, ret, insert, base);
    }
}

void EmitBitFieldSExtract(EmitContext& ctx, IR::Inst& inst, ScalarS32 base, ScalarS32 offset,
                          ScalarS32 count) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (count.type != Type::Register && offset.type != Type::Register) {
        ctx.Add("BFE.S {},{{{},{},0,0}},{};", ret, count, offset, base);
    } else {
        ctx.Add("MOV.S RC.x,{};"
                "MOV.S RC.y,{};"
                "BFE.S {},RC,{};",
                count, offset, ret, base);
    }
}

void EmitBitFieldUExtract(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 offset,
                          ScalarU32 count) {
    const auto zero = inst.GetAssociatedPseudoOperation(IR::Opcode::GetZeroFromOp);
    const auto sign = inst.GetAssociatedPseudoOperation(IR::Opcode::GetSignFromOp);
    if (zero) {
        zero->Invalidate();
    }
    if (sign) {
        sign->Invalidate();
    }
    if (zero || sign) {
        ctx.reg_alloc.InvalidateConditionCodes();
    }
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (count.type != Type::Register && offset.type != Type::Register) {
        ctx.Add("BFE.U {},{{{},{},0,0}},{};", ret, count, offset, base);
    } else {
        ctx.Add("MOV.U RC.x,{};"
                "MOV.U RC.y,{};"
                "BFE.U {},RC,{};",
                count, offset, ret, base);
    }
    if (zero) {
        ctx.Add("SEQ.S {},{},0;", *zero, ret);
    }
    if (sign) {
        ctx.Add("SLT.S {},{},0;", *sign, ret);
    }
}

void EmitBitReverse32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("BFR {},{};", inst, value);
}

void EmitBitCount32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("BTC {},{};", inst, value);
}

void EmitBitwiseNot32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("NOT.S {},{};", inst, value);
}

void EmitFindSMsb32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("BTFM.S {},{};", inst, value);
}

void EmitFindUMsb32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value) {
    ctx.Add("BTFM.U {},{};", inst, value);
}

void EmitSMin32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("MIN.S {},{},{};", inst, a, b);
}

void EmitUMin32(EmitContext& ctx, IR::Inst& inst, ScalarU32 a, ScalarU32 b) {
    ctx.Add("MIN.U {},{},{};", inst, a, b);
}

void EmitSMax32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("MAX.S {},{},{};", inst, a, b);
}

void EmitUMax32(EmitContext& ctx, IR::Inst& inst, ScalarU32 a, ScalarU32 b) {
    ctx.Add("MAX.U {},{},{};", inst, a, b);
}

void EmitSClamp32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value, ScalarS32 min, ScalarS32 max) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("MIN.S RC.x,{},{};"
            "MAX.S {}.x,RC.x,{};",
            max, value, ret, min);
}

void EmitUClamp32(EmitContext& ctx, IR::Inst& inst, ScalarU32 value, ScalarU32 min, ScalarU32 max) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("MIN.U RC.x,{},{};"
            "MAX.U {}.x,RC.x,{};",
            max, value, ret, min);
}

void EmitSLessThan(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs) {
    ctx.Add("SLT.S {}.x,{},{};", inst, lhs, rhs);
}

void EmitULessThan(EmitContext& ctx, IR::Inst& inst, ScalarU32 lhs, ScalarU32 rhs) {
    ctx.Add("SLT.U {}.x,{},{};", inst, lhs, rhs);
}

void EmitIEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs) {
    ctx.Add("SEQ.S {}.x,{},{};", inst, lhs, rhs);
}

void EmitSLessThanEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs) {
    ctx.Add("SLE.S {}.x,{},{};", inst, lhs, rhs);
}

void EmitULessThanEqual(EmitContext& ctx, IR::Inst& inst, ScalarU32 lhs, ScalarU32 rhs) {
    ctx.Add("SLE.U {}.x,{},{};", inst, lhs, rhs);
}

void EmitSGreaterThan(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs) {
    ctx.Add("SGT.S {}.x,{},{};", inst, lhs, rhs);
}

void EmitUGreaterThan(EmitContext& ctx, IR::Inst& inst, ScalarU32 lhs, ScalarU32 rhs) {
    ctx.Add("SGT.U {}.x,{},{};", inst, lhs, rhs);
}

void EmitINotEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs) {
    ctx.Add("SNE.U {}.x,{},{};", inst, lhs, rhs);
}

void EmitSGreaterThanEqual(EmitContext& ctx, IR::Inst& inst, ScalarS32 lhs, ScalarS32 rhs) {
    ctx.Add("SGE.S {}.x,{},{};", inst, lhs, rhs);
}

void EmitUGreaterThanEqual(EmitContext& ctx, IR::Inst& inst, ScalarU32 lhs, ScalarU32 rhs) {
    ctx.Add("SGE.U {}.x,{},{};", inst, lhs, rhs);
}

} // namespace Shader::Backend::GLASM
