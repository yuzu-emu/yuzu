// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

void EmitIAdd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.Add("ADD {},{},{};", inst, a, b);
}

void EmitIAdd64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitISub32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.Add("SUB {},{},{};", inst, a, b);
}

void EmitISub64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIMul32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitINeg32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitINeg64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIAbs32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitIAbs64([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftLeftLogical32([[maybe_unused]] EmitContext& ctx,
                            [[maybe_unused]] std::string_view base,
                            [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftLeftLogical64([[maybe_unused]] EmitContext& ctx,
                            [[maybe_unused]] std::string_view base,
                            [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightLogical32([[maybe_unused]] EmitContext& ctx,
                             [[maybe_unused]] std::string_view base,
                             [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightLogical64([[maybe_unused]] EmitContext& ctx,
                             [[maybe_unused]] std::string_view base,
                             [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightArithmetic32([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string_view base,
                                [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitShiftRightArithmetic64([[maybe_unused]] EmitContext& ctx,
                                [[maybe_unused]] std::string_view base,
                                [[maybe_unused]] std::string_view shift) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseAnd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.Add("AND {},{},{};", inst, a, b);
}

void EmitBitwiseOr32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                     [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.Add("OR {},{},{};", inst, a, b);
}

void EmitBitwiseXor32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    ctx.Add("XOR {},{},{};", inst, a, b);
}

void EmitBitFieldInsert(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                        std::string_view insert, std::string_view offset, std::string_view count) {
    ctx.Add("MOV.U RC.x,{};MOV.U RC.y,{};", count, offset);
    ctx.Add("BFI.U {},RC,{},{};", inst, insert, base);
}

void EmitBitFieldSExtract(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                          std::string_view offset, std::string_view count) {
    ctx.Add("MOV.U RC.x,{};MOV.U RC.y,{};", count, offset);
    ctx.Add("BFE.S {},RC,{};", inst, base);
}

void EmitBitFieldUExtract(EmitContext& ctx, IR::Inst& inst, std::string_view base,
                          std::string_view offset, std::string_view count) {
    ctx.Add("MOV.U RC.x,{};MOV.U RC.y,{};", count, offset);
    ctx.Add("BFE.U {},RC,{};", inst, base);
}

void EmitBitReverse32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    ctx.Add("BFR {},{};", inst, value);
}

void EmitBitCount32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBitwiseNot32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view value) {
    ctx.Add("NOT {},{};", inst, value);
}

void EmitFindSMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitFindUMsb32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                    [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view a, [[maybe_unused]] std::string_view b) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value, [[maybe_unused]] std::string_view min,
                  [[maybe_unused]] std::string_view max) {
    throw NotImplementedException("GLASM instruction");
}

void EmitUClamp32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                  [[maybe_unused]] std::string_view value, [[maybe_unused]] std::string_view min,
                  [[maybe_unused]] std::string_view max) {
    throw NotImplementedException("GLASM instruction");
}

void EmitSLessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view lhs, [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SLT.S {},{},{};", inst, lhs, rhs);
}

void EmitULessThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view lhs, [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SLT.U {},{},{};", inst, lhs, rhs);
}

void EmitIEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                [[maybe_unused]] std::string_view lhs, [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SEQ {},{},{};", inst, lhs, rhs);
}

void EmitSLessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                        [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SLE.S {},{},{};", inst, lhs, rhs);
}

void EmitULessThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                        [[maybe_unused]] std::string_view lhs,
                        [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SLE.U {},{},{};", inst, lhs, rhs);
}

void EmitSGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SGT.S {},{},{};", inst, lhs, rhs);
}

void EmitUGreaterThan([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                      [[maybe_unused]] std::string_view lhs,
                      [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SGT.U {},{},{};", inst, lhs, rhs);
}

void EmitINotEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                   [[maybe_unused]] std::string_view lhs, [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SNE.U {},{},{};", inst, lhs, rhs);
}

void EmitSGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SGE.S {},{},{};", inst, lhs, rhs);
}

void EmitUGreaterThanEqual([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] std::string_view lhs,
                           [[maybe_unused]] std::string_view rhs) {
    ctx.Add("SGE.U {},{},{};", inst, lhs, rhs);
}

} // namespace Shader::Backend::GLASM
