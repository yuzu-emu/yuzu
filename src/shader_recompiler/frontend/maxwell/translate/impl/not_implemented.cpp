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

void TranslatorVisitor::AL2P(u64) {
    ThrowNotImplemented(Opcode::AL2P);
}

void TranslatorVisitor::ALD(u64) {
    ThrowNotImplemented(Opcode::ALD);
}

void TranslatorVisitor::AST(u64) {
    ThrowNotImplemented(Opcode::AST);
}

void TranslatorVisitor::ATOM_cas(u64) {
    ThrowNotImplemented(Opcode::ATOM_cas);
}

void TranslatorVisitor::ATOM(u64) {
    ThrowNotImplemented(Opcode::ATOM);
}

void TranslatorVisitor::ATOMS_cas(u64) {
    ThrowNotImplemented(Opcode::ATOMS_cas);
}

void TranslatorVisitor::ATOMS(u64) {
    ThrowNotImplemented(Opcode::ATOMS);
}

void TranslatorVisitor::B2R(u64) {
    ThrowNotImplemented(Opcode::B2R);
}

void TranslatorVisitor::BAR(u64) {
    ThrowNotImplemented(Opcode::BAR);
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

void TranslatorVisitor::BRX(u64) {
    ThrowNotImplemented(Opcode::BRX);
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

void TranslatorVisitor::CSET(u64) {
    ThrowNotImplemented(Opcode::CSET);
}

void TranslatorVisitor::CSETP(u64) {
    ThrowNotImplemented(Opcode::CSETP);
}

void TranslatorVisitor::DADD_reg(u64) {
    ThrowNotImplemented(Opcode::DADD_reg);
}

void TranslatorVisitor::DADD_cbuf(u64) {
    ThrowNotImplemented(Opcode::DADD_cbuf);
}

void TranslatorVisitor::DADD_imm(u64) {
    ThrowNotImplemented(Opcode::DADD_imm);
}

void TranslatorVisitor::DEPBAR() {
    // DEPBAR is a no-op
}

void TranslatorVisitor::DFMA_reg(u64) {
    ThrowNotImplemented(Opcode::DFMA_reg);
}

void TranslatorVisitor::DFMA_rc(u64) {
    ThrowNotImplemented(Opcode::DFMA_rc);
}

void TranslatorVisitor::DFMA_cr(u64) {
    ThrowNotImplemented(Opcode::DFMA_cr);
}

void TranslatorVisitor::DFMA_imm(u64) {
    ThrowNotImplemented(Opcode::DFMA_imm);
}

void TranslatorVisitor::DMNMX_reg(u64) {
    ThrowNotImplemented(Opcode::DMNMX_reg);
}

void TranslatorVisitor::DMNMX_cbuf(u64) {
    ThrowNotImplemented(Opcode::DMNMX_cbuf);
}

void TranslatorVisitor::DMNMX_imm(u64) {
    ThrowNotImplemented(Opcode::DMNMX_imm);
}

void TranslatorVisitor::DMUL_reg(u64) {
    ThrowNotImplemented(Opcode::DMUL_reg);
}

void TranslatorVisitor::DMUL_cbuf(u64) {
    ThrowNotImplemented(Opcode::DMUL_cbuf);
}

void TranslatorVisitor::DMUL_imm(u64) {
    ThrowNotImplemented(Opcode::DMUL_imm);
}

void TranslatorVisitor::DSET_reg(u64) {
    ThrowNotImplemented(Opcode::DSET_reg);
}

void TranslatorVisitor::DSET_cbuf(u64) {
    ThrowNotImplemented(Opcode::DSET_cbuf);
}

void TranslatorVisitor::DSET_imm(u64) {
    ThrowNotImplemented(Opcode::DSET_imm);
}

void TranslatorVisitor::DSETP_reg(u64) {
    ThrowNotImplemented(Opcode::DSETP_reg);
}

void TranslatorVisitor::DSETP_cbuf(u64) {
    ThrowNotImplemented(Opcode::DSETP_cbuf);
}

void TranslatorVisitor::DSETP_imm(u64) {
    ThrowNotImplemented(Opcode::DSETP_imm);
}

void TranslatorVisitor::EXIT(u64) {
    throw LogicError("Visting EXIT instruction");
}

void TranslatorVisitor::F2F_reg(u64) {
    ThrowNotImplemented(Opcode::F2F_reg);
}

void TranslatorVisitor::F2F_cbuf(u64) {
    ThrowNotImplemented(Opcode::F2F_cbuf);
}

void TranslatorVisitor::F2F_imm(u64) {
    ThrowNotImplemented(Opcode::F2F_imm);
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

void TranslatorVisitor::FSET_reg(u64) {
    ThrowNotImplemented(Opcode::FSET_reg);
}

void TranslatorVisitor::FSET_cbuf(u64) {
    ThrowNotImplemented(Opcode::FSET_cbuf);
}

void TranslatorVisitor::FSET_imm(u64) {
    ThrowNotImplemented(Opcode::FSET_imm);
}

void TranslatorVisitor::FSETP_reg(u64) {
    ThrowNotImplemented(Opcode::FSETP_reg);
}

void TranslatorVisitor::FSETP_cbuf(u64) {
    ThrowNotImplemented(Opcode::FSETP_cbuf);
}

void TranslatorVisitor::FSETP_imm(u64) {
    ThrowNotImplemented(Opcode::FSETP_imm);
}

void TranslatorVisitor::FSWZADD(u64) {
    ThrowNotImplemented(Opcode::FSWZADD);
}

void TranslatorVisitor::GETCRSPTR(u64) {
    ThrowNotImplemented(Opcode::GETCRSPTR);
}

void TranslatorVisitor::GETLMEMBASE(u64) {
    ThrowNotImplemented(Opcode::GETLMEMBASE);
}

void TranslatorVisitor::HFMA2_reg(u64) {
    ThrowNotImplemented(Opcode::HFMA2_reg);
}

void TranslatorVisitor::HFMA2_rc(u64) {
    ThrowNotImplemented(Opcode::HFMA2_rc);
}

void TranslatorVisitor::HFMA2_cr(u64) {
    ThrowNotImplemented(Opcode::HFMA2_cr);
}

void TranslatorVisitor::HFMA2_imm(u64) {
    ThrowNotImplemented(Opcode::HFMA2_imm);
}

void TranslatorVisitor::HFMA2_32I(u64) {
    ThrowNotImplemented(Opcode::HFMA2_32I);
}

void TranslatorVisitor::HMUL2_reg(u64) {
    ThrowNotImplemented(Opcode::HMUL2_reg);
}

void TranslatorVisitor::HMUL2_cbuf(u64) {
    ThrowNotImplemented(Opcode::HMUL2_cbuf);
}

void TranslatorVisitor::HMUL2_imm(u64) {
    ThrowNotImplemented(Opcode::HMUL2_imm);
}

void TranslatorVisitor::HMUL2_32I(u64) {
    ThrowNotImplemented(Opcode::HMUL2_32I);
}

void TranslatorVisitor::HSET2_reg(u64) {
    ThrowNotImplemented(Opcode::HSET2_reg);
}

void TranslatorVisitor::HSET2_cbuf(u64) {
    ThrowNotImplemented(Opcode::HSET2_cbuf);
}

void TranslatorVisitor::HSET2_imm(u64) {
    ThrowNotImplemented(Opcode::HSET2_imm);
}

void TranslatorVisitor::HSETP2_reg(u64) {
    ThrowNotImplemented(Opcode::HSETP2_reg);
}

void TranslatorVisitor::HSETP2_cbuf(u64) {
    ThrowNotImplemented(Opcode::HSETP2_cbuf);
}

void TranslatorVisitor::HSETP2_imm(u64) {
    ThrowNotImplemented(Opcode::HSETP2_imm);
}

void TranslatorVisitor::I2F_reg(u64) {
    ThrowNotImplemented(Opcode::I2F_reg);
}

void TranslatorVisitor::I2F_cbuf(u64) {
    ThrowNotImplemented(Opcode::I2F_cbuf);
}

void TranslatorVisitor::I2F_imm(u64) {
    ThrowNotImplemented(Opcode::I2F_imm);
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

void TranslatorVisitor::ISBERD(u64) {
    ThrowNotImplemented(Opcode::ISBERD);
}

void TranslatorVisitor::JCAL(u64) {
    ThrowNotImplemented(Opcode::JCAL);
}

void TranslatorVisitor::JMP(u64) {
    ThrowNotImplemented(Opcode::JMP);
}

void TranslatorVisitor::JMX(u64) {
    ThrowNotImplemented(Opcode::JMX);
}

void TranslatorVisitor::KIL(u64) {
    ThrowNotImplemented(Opcode::KIL);
}

void TranslatorVisitor::LD(u64) {
    ThrowNotImplemented(Opcode::LD);
}

void TranslatorVisitor::LDL(u64) {
    ThrowNotImplemented(Opcode::LDL);
}

void TranslatorVisitor::LDS(u64) {
    ThrowNotImplemented(Opcode::LDS);
}

void TranslatorVisitor::LEPC(u64) {
    ThrowNotImplemented(Opcode::LEPC);
}

void TranslatorVisitor::LONGJMP(u64) {
    ThrowNotImplemented(Opcode::LONGJMP);
}

void TranslatorVisitor::LOP32I(u64) {
    ThrowNotImplemented(Opcode::LOP32I);
}

void TranslatorVisitor::MEMBAR(u64) {
    ThrowNotImplemented(Opcode::MEMBAR);
}

void TranslatorVisitor::NOP(u64) {
    ThrowNotImplemented(Opcode::NOP);
}

void TranslatorVisitor::OUT_reg(u64) {
    ThrowNotImplemented(Opcode::OUT_reg);
}

void TranslatorVisitor::OUT_cbuf(u64) {
    ThrowNotImplemented(Opcode::OUT_cbuf);
}

void TranslatorVisitor::OUT_imm(u64) {
    ThrowNotImplemented(Opcode::OUT_imm);
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

void TranslatorVisitor::PIXLD(u64) {
    ThrowNotImplemented(Opcode::PIXLD);
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

void TranslatorVisitor::RED(u64) {
    ThrowNotImplemented(Opcode::RED);
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

void TranslatorVisitor::SHFL(u64) {
    ThrowNotImplemented(Opcode::SHFL);
}

void TranslatorVisitor::SSY() {
    // SSY is a no-op
}

void TranslatorVisitor::ST(u64) {
    ThrowNotImplemented(Opcode::ST);
}

void TranslatorVisitor::STL(u64) {
    ThrowNotImplemented(Opcode::STL);
}

void TranslatorVisitor::STP(u64) {
    ThrowNotImplemented(Opcode::STP);
}

void TranslatorVisitor::STS(u64) {
    ThrowNotImplemented(Opcode::STS);
}

void TranslatorVisitor::SUATOM_cas(u64) {
    ThrowNotImplemented(Opcode::SUATOM_cas);
}

void TranslatorVisitor::SULD(u64) {
    ThrowNotImplemented(Opcode::SULD);
}

void TranslatorVisitor::SURED(u64) {
    ThrowNotImplemented(Opcode::SURED);
}

void TranslatorVisitor::SUST(u64) {
    ThrowNotImplemented(Opcode::SUST);
}

void TranslatorVisitor::SYNC(u64) {
    ThrowNotImplemented(Opcode::SYNC);
}

void TranslatorVisitor::TEXS(u64) {
    ThrowNotImplemented(Opcode::TEXS);
}

void TranslatorVisitor::TLD(u64) {
    ThrowNotImplemented(Opcode::TLD);
}

void TranslatorVisitor::TLD_b(u64) {
    ThrowNotImplemented(Opcode::TLD_b);
}

void TranslatorVisitor::TLD4(u64) {
    ThrowNotImplemented(Opcode::TLD4);
}

void TranslatorVisitor::TLD4_b(u64) {
    ThrowNotImplemented(Opcode::TLD4_b);
}

void TranslatorVisitor::TLD4S(u64) {
    ThrowNotImplemented(Opcode::TLD4S);
}

void TranslatorVisitor::TLDS(u64) {
    ThrowNotImplemented(Opcode::TLDS);
}

void TranslatorVisitor::TMML(u64) {
    ThrowNotImplemented(Opcode::TMML);
}

void TranslatorVisitor::TMML_b(u64) {
    ThrowNotImplemented(Opcode::TMML_b);
}

void TranslatorVisitor::TXA(u64) {
    ThrowNotImplemented(Opcode::TXA);
}

void TranslatorVisitor::TXD(u64) {
    ThrowNotImplemented(Opcode::TXD);
}

void TranslatorVisitor::TXD_b(u64) {
    ThrowNotImplemented(Opcode::TXD_b);
}

void TranslatorVisitor::TXQ(u64) {
    ThrowNotImplemented(Opcode::TXQ);
}

void TranslatorVisitor::TXQ_b(u64) {
    ThrowNotImplemented(Opcode::TXQ_b);
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

void TranslatorVisitor::VMAD(u64) {
    ThrowNotImplemented(Opcode::VMAD);
}

void TranslatorVisitor::VMNMX(u64) {
    ThrowNotImplemented(Opcode::VMNMX);
}

void TranslatorVisitor::VOTE(u64) {
    ThrowNotImplemented(Opcode::VOTE);
}

void TranslatorVisitor::VOTE_vtg(u64) {
    ThrowNotImplemented(Opcode::VOTE_vtg);
}

void TranslatorVisitor::VSET(u64) {
    ThrowNotImplemented(Opcode::VSET);
}

void TranslatorVisitor::VSETP(u64) {
    ThrowNotImplemented(Opcode::VSETP);
}

void TranslatorVisitor::VSHL(u64) {
    ThrowNotImplemented(Opcode::VSHL);
}

void TranslatorVisitor::VSHR(u64) {
    ThrowNotImplemented(Opcode::VSHR);
}

} // namespace Shader::Maxwell
