// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {

[[noreturn]] static void ThrowNotImplemented(Opcode opcode) {
    throw NotImplementedException("Instruction {} is not implemented", opcode);
}

void TranslatorVisitor::ATOM_cas(u64) {
    ThrowNotImplemented(Opcode::ATOM_cas);
}

void TranslatorVisitor::ATOMS_cas(u64) {
    ThrowNotImplemented(Opcode::ATOMS_cas);
}

void TranslatorVisitor::B2R(u64) {
    ThrowNotImplemented(Opcode::B2R);
}

void TranslatorVisitor::BPT(u64) {
    ThrowNotImplemented(Opcode::BPT);
}

void TranslatorVisitor::BRA(u64) {
    ThrowNotImplemented(Opcode::BRA);
}

void TranslatorVisitor::BRK(u64) {
    ThrowNotImplemented(Opcode::BRK);
}

void TranslatorVisitor::CAL() {
    // CAL is a no-op
}

void TranslatorVisitor::CCTL(u64) {
    ThrowNotImplemented(Opcode::CCTL);
}

void TranslatorVisitor::CCTLL(u64) {
    ThrowNotImplemented(Opcode::CCTLL);
}

void TranslatorVisitor::CONT(u64) {
    ThrowNotImplemented(Opcode::CONT);
}

void TranslatorVisitor::CS2R(u64) {
    ThrowNotImplemented(Opcode::CS2R);
}

void TranslatorVisitor::FCHK_reg(u64) {
    ThrowNotImplemented(Opcode::FCHK_reg);
}

void TranslatorVisitor::FCHK_cbuf(u64) {
    ThrowNotImplemented(Opcode::FCHK_cbuf);
}

void TranslatorVisitor::FCHK_imm(u64) {
    ThrowNotImplemented(Opcode::FCHK_imm);
}

void TranslatorVisitor::GETCRSPTR(u64) {
    ThrowNotImplemented(Opcode::GETCRSPTR);
}

void TranslatorVisitor::GETLMEMBASE(u64) {
    ThrowNotImplemented(Opcode::GETLMEMBASE);
}

void TranslatorVisitor::IDE(u64) {
    ThrowNotImplemented(Opcode::IDE);
}

void TranslatorVisitor::IDP_reg(u64) {
    ThrowNotImplemented(Opcode::IDP_reg);
}

void TranslatorVisitor::IDP_imm(u64) {
    ThrowNotImplemented(Opcode::IDP_imm);
}

void TranslatorVisitor::IMAD_reg(u64) {
    ThrowNotImplemented(Opcode::IMAD_reg);
}

void TranslatorVisitor::IMAD_rc(u64) {
    ThrowNotImplemented(Opcode::IMAD_rc);
}

void TranslatorVisitor::IMAD_cr(u64) {
    ThrowNotImplemented(Opcode::IMAD_cr);
}

void TranslatorVisitor::IMAD_imm(u64) {
    ThrowNotImplemented(Opcode::IMAD_imm);
}

void TranslatorVisitor::IMAD32I(u64) {
    ThrowNotImplemented(Opcode::IMAD32I);
}

void TranslatorVisitor::IMADSP_reg(u64) {
    ThrowNotImplemented(Opcode::IMADSP_reg);
}

void TranslatorVisitor::IMADSP_rc(u64) {
    ThrowNotImplemented(Opcode::IMADSP_rc);
}

void TranslatorVisitor::IMADSP_cr(u64) {
    ThrowNotImplemented(Opcode::IMADSP_cr);
}

void TranslatorVisitor::IMADSP_imm(u64) {
    ThrowNotImplemented(Opcode::IMADSP_imm);
}

void TranslatorVisitor::IMUL_reg(u64) {
    ThrowNotImplemented(Opcode::IMUL_reg);
}

void TranslatorVisitor::IMUL_cbuf(u64) {
    ThrowNotImplemented(Opcode::IMUL_cbuf);
}

void TranslatorVisitor::IMUL_imm(u64) {
    ThrowNotImplemented(Opcode::IMUL_imm);
}

void TranslatorVisitor::IMUL32I(u64) {
    ThrowNotImplemented(Opcode::IMUL32I);
}

void TranslatorVisitor::JCAL(u64) {
    ThrowNotImplemented(Opcode::JCAL);
}

void TranslatorVisitor::JMP(u64) {
    ThrowNotImplemented(Opcode::JMP);
}

void TranslatorVisitor::KIL() {
    // KIL is a no-op
}

void TranslatorVisitor::LD(u64) {
    ThrowNotImplemented(Opcode::LD);
}

void TranslatorVisitor::LEPC(u64) {
    ThrowNotImplemented(Opcode::LEPC);
}

void TranslatorVisitor::LONGJMP(u64) {
    ThrowNotImplemented(Opcode::LONGJMP);
}

void TranslatorVisitor::NOP(u64) {
    // NOP is No-Op.
}

void TranslatorVisitor::PBK() {
    // PBK is a no-op
}

void TranslatorVisitor::PCNT() {
    // PCNT is a no-op
}

void TranslatorVisitor::PEXIT(u64) {
    ThrowNotImplemented(Opcode::PEXIT);
}

void TranslatorVisitor::PLONGJMP(u64) {
    ThrowNotImplemented(Opcode::PLONGJMP);
}

void TranslatorVisitor::PRET(u64) {
    ThrowNotImplemented(Opcode::PRET);
}

void TranslatorVisitor::PRMT_reg(u64) {
    ThrowNotImplemented(Opcode::PRMT_reg);
}

void TranslatorVisitor::PRMT_rc(u64) {
    ThrowNotImplemented(Opcode::PRMT_rc);
}

void TranslatorVisitor::PRMT_cr(u64) {
    ThrowNotImplemented(Opcode::PRMT_cr);
}

void TranslatorVisitor::PRMT_imm(u64) {
    ThrowNotImplemented(Opcode::PRMT_imm);
}

void TranslatorVisitor::R2B(u64) {
    ThrowNotImplemented(Opcode::R2B);
}

void TranslatorVisitor::RAM(u64) {
    ThrowNotImplemented(Opcode::RAM);
}

void TranslatorVisitor::RET(u64) {
    ThrowNotImplemented(Opcode::RET);
}

void TranslatorVisitor::RTT(u64) {
    ThrowNotImplemented(Opcode::RTT);
}

void TranslatorVisitor::SAM(u64) {
    ThrowNotImplemented(Opcode::SAM);
}

void TranslatorVisitor::SETCRSPTR(u64) {
    ThrowNotImplemented(Opcode::SETCRSPTR);
}

void TranslatorVisitor::SETLMEMBASE(u64) {
    ThrowNotImplemented(Opcode::SETLMEMBASE);
}

void TranslatorVisitor::SSY() {
    // SSY is a no-op
}

void TranslatorVisitor::ST(u64) {
    ThrowNotImplemented(Opcode::ST);
}

void TranslatorVisitor::STP(u64) {
    ThrowNotImplemented(Opcode::STP);
}

void TranslatorVisitor::SUATOM_cas(u64) {
    ThrowNotImplemented(Opcode::SUATOM_cas);
}

void TranslatorVisitor::SYNC(u64) {
    ThrowNotImplemented(Opcode::SYNC);
}

void TranslatorVisitor::TXA(u64) {
    ThrowNotImplemented(Opcode::TXA);
}

void TranslatorVisitor::VABSDIFF(u64) {
    ThrowNotImplemented(Opcode::VABSDIFF);
}

void TranslatorVisitor::VABSDIFF4(u64) {
    ThrowNotImplemented(Opcode::VABSDIFF4);
}

void TranslatorVisitor::VADD(u64) {
    ThrowNotImplemented(Opcode::VADD);
}

void TranslatorVisitor::VSET(u64) {
    ThrowNotImplemented(Opcode::VSET);
}
void TranslatorVisitor::VSHL(u64) {
    ThrowNotImplemented(Opcode::VSHL);
}

void TranslatorVisitor::VSHR(u64) {
    ThrowNotImplemented(Opcode::VSHR);
}

} // namespace Shader::Maxwell
