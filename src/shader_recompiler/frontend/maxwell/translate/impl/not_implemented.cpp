// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/opcode.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Maxwell {

[[maybe_unused]] static inline void DumpOptimized(IR::Block& block) {
    auto raw{IR::DumpBlock(block)};

    Optimization::GetSetElimination(block);
    Optimization::DeadCodeEliminationPass(block);
    Optimization::IdentityRemovalPass(block);
    auto dumped{IR::DumpBlock(block)};

    fmt::print(stderr, "{}", dumped);
}

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

void TranslatorVisitor::BFE_reg(u64) {
    ThrowNotImplemented(Opcode::BFE_reg);
}

void TranslatorVisitor::BFE_cbuf(u64) {
    ThrowNotImplemented(Opcode::BFE_cbuf);
}

void TranslatorVisitor::BFE_imm(u64) {
    ThrowNotImplemented(Opcode::BFE_imm);
}

void TranslatorVisitor::BFI_reg(u64) {
    ThrowNotImplemented(Opcode::BFI_reg);
}

void TranslatorVisitor::BFI_rc(u64) {
    ThrowNotImplemented(Opcode::BFI_rc);
}

void TranslatorVisitor::BFI_cr(u64) {
    ThrowNotImplemented(Opcode::BFI_cr);
}

void TranslatorVisitor::BFI_imm(u64) {
    ThrowNotImplemented(Opcode::BFI_imm);
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

void TranslatorVisitor::CAL(u64) {
    ThrowNotImplemented(Opcode::CAL);
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

void TranslatorVisitor::DEPBAR(u64) {
    ThrowNotImplemented(Opcode::DEPBAR);
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

void TranslatorVisitor::FADD_reg(u64) {
    ThrowNotImplemented(Opcode::FADD_reg);
}

void TranslatorVisitor::FADD_cbuf(u64) {
    ThrowNotImplemented(Opcode::FADD_cbuf);
}

void TranslatorVisitor::FADD_imm(u64) {
    ThrowNotImplemented(Opcode::FADD_imm);
}

void TranslatorVisitor::FADD32I(u64) {
    ThrowNotImplemented(Opcode::FADD32I);
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

void TranslatorVisitor::FCMP_reg(u64) {
    ThrowNotImplemented(Opcode::FCMP_reg);
}

void TranslatorVisitor::FCMP_rc(u64) {
    ThrowNotImplemented(Opcode::FCMP_rc);
}

void TranslatorVisitor::FCMP_cr(u64) {
    ThrowNotImplemented(Opcode::FCMP_cr);
}

void TranslatorVisitor::FCMP_imm(u64) {
    ThrowNotImplemented(Opcode::FCMP_imm);
}

void TranslatorVisitor::FFMA_reg(u64) {
    ThrowNotImplemented(Opcode::FFMA_reg);
}

void TranslatorVisitor::FFMA_rc(u64) {
    ThrowNotImplemented(Opcode::FFMA_rc);
}

void TranslatorVisitor::FFMA_cr(u64) {
    ThrowNotImplemented(Opcode::FFMA_cr);
}

void TranslatorVisitor::FFMA_imm(u64) {
    ThrowNotImplemented(Opcode::FFMA_imm);
}

void TranslatorVisitor::FFMA32I(u64) {
    ThrowNotImplemented(Opcode::FFMA32I);
}

void TranslatorVisitor::FLO_reg(u64) {
    ThrowNotImplemented(Opcode::FLO_reg);
}

void TranslatorVisitor::FLO_cbuf(u64) {
    ThrowNotImplemented(Opcode::FLO_cbuf);
}

void TranslatorVisitor::FLO_imm(u64) {
    ThrowNotImplemented(Opcode::FLO_imm);
}

void TranslatorVisitor::FMNMX_reg(u64) {
    ThrowNotImplemented(Opcode::FMNMX_reg);
}

void TranslatorVisitor::FMNMX_cbuf(u64) {
    ThrowNotImplemented(Opcode::FMNMX_cbuf);
}

void TranslatorVisitor::FMNMX_imm(u64) {
    ThrowNotImplemented(Opcode::FMNMX_imm);
}

void TranslatorVisitor::FMUL_reg(u64) {
    ThrowNotImplemented(Opcode::FMUL_reg);
}

void TranslatorVisitor::FMUL_cbuf(u64) {
    ThrowNotImplemented(Opcode::FMUL_cbuf);
}

void TranslatorVisitor::FMUL_imm(u64) {
    ThrowNotImplemented(Opcode::FMUL_imm);
}

void TranslatorVisitor::FMUL32I(u64) {
    ThrowNotImplemented(Opcode::FMUL32I);
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

void TranslatorVisitor::HADD2_reg(u64) {
    ThrowNotImplemented(Opcode::HADD2_reg);
}

void TranslatorVisitor::HADD2_cbuf(u64) {
    ThrowNotImplemented(Opcode::HADD2_cbuf);
}

void TranslatorVisitor::HADD2_imm(u64) {
    ThrowNotImplemented(Opcode::HADD2_imm);
}

void TranslatorVisitor::HADD2_32I(u64) {
    ThrowNotImplemented(Opcode::HADD2_32I);
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

void TranslatorVisitor::I2I_reg(u64) {
    ThrowNotImplemented(Opcode::I2I_reg);
}

void TranslatorVisitor::I2I_cbuf(u64) {
    ThrowNotImplemented(Opcode::I2I_cbuf);
}

void TranslatorVisitor::I2I_imm(u64) {
    ThrowNotImplemented(Opcode::I2I_imm);
}

void TranslatorVisitor::IADD_reg(u64) {
    ThrowNotImplemented(Opcode::IADD_reg);
}

void TranslatorVisitor::IADD_cbuf(u64) {
    ThrowNotImplemented(Opcode::IADD_cbuf);
}

void TranslatorVisitor::IADD_imm(u64) {
    ThrowNotImplemented(Opcode::IADD_imm);
}

void TranslatorVisitor::IADD3_reg(u64) {
    ThrowNotImplemented(Opcode::IADD3_reg);
}

void TranslatorVisitor::IADD3_cbuf(u64) {
    ThrowNotImplemented(Opcode::IADD3_cbuf);
}

void TranslatorVisitor::IADD3_imm(u64) {
    ThrowNotImplemented(Opcode::IADD3_imm);
}

void TranslatorVisitor::IADD32I(u64) {
    ThrowNotImplemented(Opcode::IADD32I);
}

void TranslatorVisitor::ICMP_reg(u64) {
    ThrowNotImplemented(Opcode::ICMP_reg);
}

void TranslatorVisitor::ICMP_rc(u64) {
    ThrowNotImplemented(Opcode::ICMP_rc);
}

void TranslatorVisitor::ICMP_cr(u64) {
    ThrowNotImplemented(Opcode::ICMP_cr);
}

void TranslatorVisitor::ICMP_imm(u64) {
    ThrowNotImplemented(Opcode::ICMP_imm);
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

void TranslatorVisitor::IMNMX_reg(u64) {
    ThrowNotImplemented(Opcode::IMNMX_reg);
}

void TranslatorVisitor::IMNMX_cbuf(u64) {
    ThrowNotImplemented(Opcode::IMNMX_cbuf);
}

void TranslatorVisitor::IMNMX_imm(u64) {
    ThrowNotImplemented(Opcode::IMNMX_imm);
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

void TranslatorVisitor::ISCADD_reg(u64) {
    ThrowNotImplemented(Opcode::ISCADD_reg);
}

void TranslatorVisitor::ISCADD_cbuf(u64) {
    ThrowNotImplemented(Opcode::ISCADD_cbuf);
}

void TranslatorVisitor::ISCADD_imm(u64) {
    ThrowNotImplemented(Opcode::ISCADD_imm);
}

void TranslatorVisitor::ISCADD32I(u64) {
    ThrowNotImplemented(Opcode::ISCADD32I);
}

void TranslatorVisitor::ISET_reg(u64) {
    ThrowNotImplemented(Opcode::ISET_reg);
}

void TranslatorVisitor::ISET_cbuf(u64) {
    ThrowNotImplemented(Opcode::ISET_cbuf);
}

void TranslatorVisitor::ISET_imm(u64) {
    ThrowNotImplemented(Opcode::ISET_imm);
}

void TranslatorVisitor::ISETP_reg(u64) {
    ThrowNotImplemented(Opcode::ISETP_reg);
}

void TranslatorVisitor::ISETP_cbuf(u64) {
    ThrowNotImplemented(Opcode::ISETP_cbuf);
}

void TranslatorVisitor::ISETP_imm(u64) {
    ThrowNotImplemented(Opcode::ISETP_imm);
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

void TranslatorVisitor::LDC(u64) {
    ThrowNotImplemented(Opcode::LDC);
}

void TranslatorVisitor::LDG(u64) {
    ThrowNotImplemented(Opcode::LDG);
}

void TranslatorVisitor::LDL(u64) {
    ThrowNotImplemented(Opcode::LDL);
}

void TranslatorVisitor::LDS(u64) {
    ThrowNotImplemented(Opcode::LDS);
}

void TranslatorVisitor::LEA_hi_reg(u64) {
    ThrowNotImplemented(Opcode::LEA_hi_reg);
}

void TranslatorVisitor::LEA_hi_cbuf(u64) {
    ThrowNotImplemented(Opcode::LEA_hi_cbuf);
}

void TranslatorVisitor::LEA_lo_reg(u64) {
    ThrowNotImplemented(Opcode::LEA_lo_reg);
}

void TranslatorVisitor::LEA_lo_cbuf(u64) {
    ThrowNotImplemented(Opcode::LEA_lo_cbuf);
}

void TranslatorVisitor::LEA_lo_imm(u64) {
    ThrowNotImplemented(Opcode::LEA_lo_imm);
}

void TranslatorVisitor::LEPC(u64) {
    ThrowNotImplemented(Opcode::LEPC);
}

void TranslatorVisitor::LONGJMP(u64) {
    ThrowNotImplemented(Opcode::LONGJMP);
}

void TranslatorVisitor::LOP_reg(u64) {
    ThrowNotImplemented(Opcode::LOP_reg);
}

void TranslatorVisitor::LOP_cbuf(u64) {
    ThrowNotImplemented(Opcode::LOP_cbuf);
}

void TranslatorVisitor::LOP_imm(u64) {
    ThrowNotImplemented(Opcode::LOP_imm);
}

void TranslatorVisitor::LOP3_reg(u64) {
    ThrowNotImplemented(Opcode::LOP3_reg);
}

void TranslatorVisitor::LOP3_cbuf(u64) {
    ThrowNotImplemented(Opcode::LOP3_cbuf);
}

void TranslatorVisitor::LOP3_imm(u64) {
    ThrowNotImplemented(Opcode::LOP3_imm);
}

void TranslatorVisitor::LOP32I(u64) {
    ThrowNotImplemented(Opcode::LOP32I);
}

void TranslatorVisitor::MEMBAR(u64) {
    ThrowNotImplemented(Opcode::MEMBAR);
}

void TranslatorVisitor::MOV32I(u64) {
    ThrowNotImplemented(Opcode::MOV32I);
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

void TranslatorVisitor::P2R_reg(u64) {
    ThrowNotImplemented(Opcode::P2R_reg);
}

void TranslatorVisitor::P2R_cbuf(u64) {
    ThrowNotImplemented(Opcode::P2R_cbuf);
}

void TranslatorVisitor::P2R_imm(u64) {
    ThrowNotImplemented(Opcode::P2R_imm);
}

void TranslatorVisitor::PBK(u64) {
    // PBK is a no-op
}

void TranslatorVisitor::PCNT(u64) {
    ThrowNotImplemented(Opcode::PCNT);
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

void TranslatorVisitor::POPC_reg(u64) {
    ThrowNotImplemented(Opcode::POPC_reg);
}

void TranslatorVisitor::POPC_cbuf(u64) {
    ThrowNotImplemented(Opcode::POPC_cbuf);
}

void TranslatorVisitor::POPC_imm(u64) {
    ThrowNotImplemented(Opcode::POPC_imm);
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

void TranslatorVisitor::PSET(u64) {
    ThrowNotImplemented(Opcode::PSET);
}

void TranslatorVisitor::PSETP(u64) {
    ThrowNotImplemented(Opcode::PSETP);
}

void TranslatorVisitor::R2B(u64) {
    ThrowNotImplemented(Opcode::R2B);
}

void TranslatorVisitor::R2P_reg(u64) {
    ThrowNotImplemented(Opcode::R2P_reg);
}

void TranslatorVisitor::R2P_cbuf(u64) {
    ThrowNotImplemented(Opcode::R2P_cbuf);
}

void TranslatorVisitor::R2P_imm(u64) {
    ThrowNotImplemented(Opcode::R2P_imm);
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

void TranslatorVisitor::RRO_reg(u64) {
    ThrowNotImplemented(Opcode::RRO_reg);
}

void TranslatorVisitor::RRO_cbuf(u64) {
    ThrowNotImplemented(Opcode::RRO_cbuf);
}

void TranslatorVisitor::RRO_imm(u64) {
    ThrowNotImplemented(Opcode::RRO_imm);
}

void TranslatorVisitor::RTT(u64) {
    ThrowNotImplemented(Opcode::RTT);
}

void TranslatorVisitor::S2R(u64) {
    ThrowNotImplemented(Opcode::S2R);
}

void TranslatorVisitor::SAM(u64) {
    ThrowNotImplemented(Opcode::SAM);
}

void TranslatorVisitor::SEL_reg(u64) {
    ThrowNotImplemented(Opcode::SEL_reg);
}

void TranslatorVisitor::SEL_cbuf(u64) {
    ThrowNotImplemented(Opcode::SEL_cbuf);
}

void TranslatorVisitor::SEL_imm(u64) {
    ThrowNotImplemented(Opcode::SEL_imm);
}

void TranslatorVisitor::SETCRSPTR(u64) {
    ThrowNotImplemented(Opcode::SETCRSPTR);
}

void TranslatorVisitor::SETLMEMBASE(u64) {
    ThrowNotImplemented(Opcode::SETLMEMBASE);
}

void TranslatorVisitor::SHF_l_reg(u64) {
    ThrowNotImplemented(Opcode::SHF_l_reg);
}

void TranslatorVisitor::SHF_l_imm(u64) {
    ThrowNotImplemented(Opcode::SHF_l_imm);
}

void TranslatorVisitor::SHF_r_reg(u64) {
    ThrowNotImplemented(Opcode::SHF_r_reg);
}

void TranslatorVisitor::SHF_r_imm(u64) {
    ThrowNotImplemented(Opcode::SHF_r_imm);
}

void TranslatorVisitor::SHFL(u64) {
    ThrowNotImplemented(Opcode::SHFL);
}

void TranslatorVisitor::SHL_reg(u64) {
    ThrowNotImplemented(Opcode::SHL_reg);
}

void TranslatorVisitor::SHL_cbuf(u64) {
    ThrowNotImplemented(Opcode::SHL_cbuf);
}

void TranslatorVisitor::SHL_imm(u64) {
    ThrowNotImplemented(Opcode::SHL_imm);
}

void TranslatorVisitor::SHR_reg(u64) {
    ThrowNotImplemented(Opcode::SHR_reg);
}

void TranslatorVisitor::SHR_cbuf(u64) {
    ThrowNotImplemented(Opcode::SHR_cbuf);
}

void TranslatorVisitor::SHR_imm(u64) {
    ThrowNotImplemented(Opcode::SHR_imm);
}

void TranslatorVisitor::SSY(u64) {
    ThrowNotImplemented(Opcode::SSY);
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

void TranslatorVisitor::TEX(u64) {
    ThrowNotImplemented(Opcode::TEX);
}

void TranslatorVisitor::TEX_b(u64) {
    ThrowNotImplemented(Opcode::TEX_b);
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

void TranslatorVisitor::XMAD_reg(u64) {
    ThrowNotImplemented(Opcode::XMAD_reg);
}

void TranslatorVisitor::XMAD_rc(u64) {
    ThrowNotImplemented(Opcode::XMAD_rc);
}

void TranslatorVisitor::XMAD_cr(u64) {
    ThrowNotImplemented(Opcode::XMAD_cr);
}

void TranslatorVisitor::XMAD_imm(u64) {
    ThrowNotImplemented(Opcode::XMAD_imm);
}

} // namespace Shader::Maxwell
