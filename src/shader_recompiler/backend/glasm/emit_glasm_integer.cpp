// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

void EmitIAdd32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("ADD.S {}.x,{},{};", inst, a, b);
}

void EmitIAdd64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register a,
                [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitISub32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("SUB.S {}.x,{},{};", inst, a, b);
}

void EmitISub64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register a,
                [[maybe_unused]] Register b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIMul32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("MUL.S {}.x,{},{};", inst, a, b);
}

void EmitINeg32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("MOV.S {},-{};", inst, value);
}

void EmitINeg64(EmitContext& ctx, IR::Inst& inst, Register value) {
    ctx.LongAdd("MOV.S64 {},-{};", inst, value);
}

void EmitIAbs32(EmitContext& ctx, IR::Inst& inst, ScalarS32 value) {
    ctx.Add("ABS.S {},{};", inst, value);
}

void EmitIAbs64(EmitContext& ctx, IR::Inst& inst, Register value) {
    ctx.LongAdd("MOV.S64 {},|{}|;", inst, value);
}

void EmitShiftLeftLogical32(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 shift) {
    ctx.Add("SHL.U {}.x,{},{};", inst, base, shift);
}

void EmitShiftLeftLogical64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register base,
                            [[maybe_unused]] Register shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightLogical32(EmitContext& ctx, IR::Inst& inst, ScalarU32 base, ScalarU32 shift) {
    ctx.Add("SHR.U {}.x,{},{};", inst, base, shift);
}

void EmitShiftRightLogical64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register base,
                             [[maybe_unused]] Register shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightArithmetic32(EmitContext& ctx, IR::Inst& inst, ScalarS32 base, ScalarS32 shift) {
    ctx.Add("SHR.S {}.x,{},{};", inst, base, shift);
}

void EmitShiftRightArithmetic64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register base,
                                [[maybe_unused]] Register shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseAnd32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("AND.S {}.x,{},{};", inst, a, b);
}

void EmitBitwiseOr32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("OR.S {}.x,{},{};", inst, a, b);
}

void EmitBitwiseXor32(EmitContext& ctx, IR::Inst& inst, ScalarS32 a, ScalarS32 b) {
    ctx.Add("XOR.S {}.x,{},{};", inst, a, b);
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
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (count.type != Type::Register && offset.type != Type::Register) {
        ctx.Add("BFE.U {},{{{},{},0,0}},{};", ret, count, offset, base);
    } else {
        ctx.Add("MOV.U RC.x,{};"
                "MOV.U RC.y,{};"
                "BFE.U {},RC,{};",
                count, offset, ret, base);
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
