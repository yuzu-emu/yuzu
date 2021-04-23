// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/maxwell/instruction.h"

namespace Shader::Maxwell {

enum class CompareOp : u64 {
    False,
    LessThan,
    Equal,
    LessThanEqual,
    GreaterThan,
    NotEqual,
    GreaterThanEqual,
    True,
};

enum class BooleanOp : u64 {
    AND,
    OR,
    XOR,
};

enum class PredicateOp : u64 {
    False,
    True,
    Zero,
    NonZero,
};

enum class FPCompareOp : u64 {
    F,
    LT,
    EQ,
    LE,
    GT,
    NE,
    GE,
    NUM,
    Nan,
    LTU,
    EQU,
    LEU,
    GTU,
    NEU,
    GEU,
    T,
};

class TranslatorVisitor {
public:
    explicit TranslatorVisitor(Environment& env_, IR::Block& block) : env{env_}, ir(block) {}

    Environment& env;
    IR::IREmitter ir;

    void AL2P(u64 insn);
    void ALD(u64 insn);
    void AST(u64 insn);
    void ATOM_cas(u64 insn);
    void ATOM(u64 insn);
    void ATOMS_cas(u64 insn);
    void ATOMS(u64 insn);
    void B2R(u64 insn);
    void BAR(u64 insn);
    void BFE_reg(u64 insn);
    void BFE_cbuf(u64 insn);
    void BFE_imm(u64 insn);
    void BFI_reg(u64 insn);
    void BFI_rc(u64 insn);
    void BFI_cr(u64 insn);
    void BFI_imm(u64 insn);
    void BPT(u64 insn);
    void BRA(u64 insn);
    void BRK(u64 insn);
    void BRX(u64 insn);
    void CAL();
    void CCTL(u64 insn);
    void CCTLL(u64 insn);
    void CONT(u64 insn);
    void CS2R(u64 insn);
    void CSET(u64 insn);
    void CSETP(u64 insn);
    void DADD_reg(u64 insn);
    void DADD_cbuf(u64 insn);
    void DADD_imm(u64 insn);
    void DEPBAR();
    void DFMA_reg(u64 insn);
    void DFMA_rc(u64 insn);
    void DFMA_cr(u64 insn);
    void DFMA_imm(u64 insn);
    void DMNMX_reg(u64 insn);
    void DMNMX_cbuf(u64 insn);
    void DMNMX_imm(u64 insn);
    void DMUL_reg(u64 insn);
    void DMUL_cbuf(u64 insn);
    void DMUL_imm(u64 insn);
    void DSET_reg(u64 insn);
    void DSET_cbuf(u64 insn);
    void DSET_imm(u64 insn);
    void DSETP_reg(u64 insn);
    void DSETP_cbuf(u64 insn);
    void DSETP_imm(u64 insn);
    void EXIT();
    void F2F_reg(u64 insn);
    void F2F_cbuf(u64 insn);
    void F2F_imm(u64 insn);
    void F2I_reg(u64 insn);
    void F2I_cbuf(u64 insn);
    void F2I_imm(u64 insn);
    void FADD_reg(u64 insn);
    void FADD_cbuf(u64 insn);
    void FADD_imm(u64 insn);
    void FADD32I(u64 insn);
    void FCHK_reg(u64 insn);
    void FCHK_cbuf(u64 insn);
    void FCHK_imm(u64 insn);
    void FCMP_reg(u64 insn);
    void FCMP_rc(u64 insn);
    void FCMP_cr(u64 insn);
    void FCMP_imm(u64 insn);
    void FFMA_reg(u64 insn);
    void FFMA_rc(u64 insn);
    void FFMA_cr(u64 insn);
    void FFMA_imm(u64 insn);
    void FFMA32I(u64 insn);
    void FLO_reg(u64 insn);
    void FLO_cbuf(u64 insn);
    void FLO_imm(u64 insn);
    void FMNMX_reg(u64 insn);
    void FMNMX_cbuf(u64 insn);
    void FMNMX_imm(u64 insn);
    void FMUL_reg(u64 insn);
    void FMUL_cbuf(u64 insn);
    void FMUL_imm(u64 insn);
    void FMUL32I(u64 insn);
    void FSET_reg(u64 insn);
    void FSET_cbuf(u64 insn);
    void FSET_imm(u64 insn);
    void FSETP_reg(u64 insn);
    void FSETP_cbuf(u64 insn);
    void FSETP_imm(u64 insn);
    void FSWZADD(u64 insn);
    void GETCRSPTR(u64 insn);
    void GETLMEMBASE(u64 insn);
    void HADD2_reg(u64 insn);
    void HADD2_cbuf(u64 insn);
    void HADD2_imm(u64 insn);
    void HADD2_32I(u64 insn);
    void HFMA2_reg(u64 insn);
    void HFMA2_rc(u64 insn);
    void HFMA2_cr(u64 insn);
    void HFMA2_imm(u64 insn);
    void HFMA2_32I(u64 insn);
    void HMUL2_reg(u64 insn);
    void HMUL2_cbuf(u64 insn);
    void HMUL2_imm(u64 insn);
    void HMUL2_32I(u64 insn);
    void HSET2_reg(u64 insn);
    void HSET2_cbuf(u64 insn);
    void HSET2_imm(u64 insn);
    void HSETP2_reg(u64 insn);
    void HSETP2_cbuf(u64 insn);
    void HSETP2_imm(u64 insn);
    void I2F_reg(u64 insn);
    void I2F_cbuf(u64 insn);
    void I2F_imm(u64 insn);
    void I2I_reg(u64 insn);
    void I2I_cbuf(u64 insn);
    void I2I_imm(u64 insn);
    void IADD_reg(u64 insn);
    void IADD_cbuf(u64 insn);
    void IADD_imm(u64 insn);
    void IADD3_reg(u64 insn);
    void IADD3_cbuf(u64 insn);
    void IADD3_imm(u64 insn);
    void IADD32I(u64 insn);
    void ICMP_reg(u64 insn);
    void ICMP_rc(u64 insn);
    void ICMP_cr(u64 insn);
    void ICMP_imm(u64 insn);
    void IDE(u64 insn);
    void IDP_reg(u64 insn);
    void IDP_imm(u64 insn);
    void IMAD_reg(u64 insn);
    void IMAD_rc(u64 insn);
    void IMAD_cr(u64 insn);
    void IMAD_imm(u64 insn);
    void IMAD32I(u64 insn);
    void IMADSP_reg(u64 insn);
    void IMADSP_rc(u64 insn);
    void IMADSP_cr(u64 insn);
    void IMADSP_imm(u64 insn);
    void IMNMX_reg(u64 insn);
    void IMNMX_cbuf(u64 insn);
    void IMNMX_imm(u64 insn);
    void IMUL_reg(u64 insn);
    void IMUL_cbuf(u64 insn);
    void IMUL_imm(u64 insn);
    void IMUL32I(u64 insn);
    void IPA(u64 insn);
    void ISBERD(u64 insn);
    void ISCADD_reg(u64 insn);
    void ISCADD_cbuf(u64 insn);
    void ISCADD_imm(u64 insn);
    void ISCADD32I(u64 insn);
    void ISET_reg(u64 insn);
    void ISET_cbuf(u64 insn);
    void ISET_imm(u64 insn);
    void ISETP_reg(u64 insn);
    void ISETP_cbuf(u64 insn);
    void ISETP_imm(u64 insn);
    void JCAL(u64 insn);
    void JMP(u64 insn);
    void JMX(u64 insn);
    void KIL();
    void LD(u64 insn);
    void LDC(u64 insn);
    void LDG(u64 insn);
    void LDL(u64 insn);
    void LDS(u64 insn);
    void LEA_hi_reg(u64 insn);
    void LEA_hi_cbuf(u64 insn);
    void LEA_lo_reg(u64 insn);
    void LEA_lo_cbuf(u64 insn);
    void LEA_lo_imm(u64 insn);
    void LEPC(u64 insn);
    void LONGJMP(u64 insn);
    void LOP_reg(u64 insn);
    void LOP_cbuf(u64 insn);
    void LOP_imm(u64 insn);
    void LOP3_reg(u64 insn);
    void LOP3_cbuf(u64 insn);
    void LOP3_imm(u64 insn);
    void LOP32I(u64 insn);
    void MEMBAR(u64 insn);
    void MOV_reg(u64 insn);
    void MOV_cbuf(u64 insn);
    void MOV_imm(u64 insn);
    void MOV32I(u64 insn);
    void MUFU(u64 insn);
    void NOP(u64 insn);
    void OUT_reg(u64 insn);
    void OUT_cbuf(u64 insn);
    void OUT_imm(u64 insn);
    void P2R_reg(u64 insn);
    void P2R_cbuf(u64 insn);
    void P2R_imm(u64 insn);
    void PBK();
    void PCNT();
    void PEXIT(u64 insn);
    void PIXLD(u64 insn);
    void PLONGJMP(u64 insn);
    void POPC_reg(u64 insn);
    void POPC_cbuf(u64 insn);
    void POPC_imm(u64 insn);
    void PRET(u64 insn);
    void PRMT_reg(u64 insn);
    void PRMT_rc(u64 insn);
    void PRMT_cr(u64 insn);
    void PRMT_imm(u64 insn);
    void PSET(u64 insn);
    void PSETP(u64 insn);
    void R2B(u64 insn);
    void R2P_reg(u64 insn);
    void R2P_cbuf(u64 insn);
    void R2P_imm(u64 insn);
    void RAM(u64 insn);
    void RED(u64 insn);
    void RET(u64 insn);
    void RRO_reg(u64 insn);
    void RRO_cbuf(u64 insn);
    void RRO_imm(u64 insn);
    void RTT(u64 insn);
    void S2R(u64 insn);
    void SAM(u64 insn);
    void SEL_reg(u64 insn);
    void SEL_cbuf(u64 insn);
    void SEL_imm(u64 insn);
    void SETCRSPTR(u64 insn);
    void SETLMEMBASE(u64 insn);
    void SHF_l_reg(u64 insn);
    void SHF_l_imm(u64 insn);
    void SHF_r_reg(u64 insn);
    void SHF_r_imm(u64 insn);
    void SHFL(u64 insn);
    void SHL_reg(u64 insn);
    void SHL_cbuf(u64 insn);
    void SHL_imm(u64 insn);
    void SHR_reg(u64 insn);
    void SHR_cbuf(u64 insn);
    void SHR_imm(u64 insn);
    void SSY();
    void ST(u64 insn);
    void STG(u64 insn);
    void STL(u64 insn);
    void STP(u64 insn);
    void STS(u64 insn);
    void SUATOM(u64 insn);
    void SUATOM_cas(u64 insn);
    void SULD(u64 insn);
    void SURED(u64 insn);
    void SUST(u64 insn);
    void SYNC(u64 insn);
    void TEX(u64 insn);
    void TEX_b(u64 insn);
    void TEXS(u64 insn);
    void TLD(u64 insn);
    void TLD_b(u64 insn);
    void TLD4(u64 insn);
    void TLD4_b(u64 insn);
    void TLD4S(u64 insn);
    void TLDS(u64 insn);
    void TMML(u64 insn);
    void TMML_b(u64 insn);
    void TXA(u64 insn);
    void TXD(u64 insn);
    void TXD_b(u64 insn);
    void TXQ(u64 insn);
    void TXQ_b(u64 insn);
    void VABSDIFF(u64 insn);
    void VABSDIFF4(u64 insn);
    void VADD(u64 insn);
    void VMAD(u64 insn);
    void VMNMX(u64 insn);
    void VOTE(u64 insn);
    void VOTE_vtg(u64 insn);
    void VSET(u64 insn);
    void VSETP(u64 insn);
    void VSHL(u64 insn);
    void VSHR(u64 insn);
    void XMAD_reg(u64 insn);
    void XMAD_rc(u64 insn);
    void XMAD_cr(u64 insn);
    void XMAD_imm(u64 insn);

    [[nodiscard]] IR::U32 X(IR::Reg reg);
    [[nodiscard]] IR::U64 L(IR::Reg reg);
    [[nodiscard]] IR::F32 F(IR::Reg reg);
    [[nodiscard]] IR::F64 D(IR::Reg reg);

    void X(IR::Reg dest_reg, const IR::U32& value);
    void L(IR::Reg dest_reg, const IR::U64& value);
    void F(IR::Reg dest_reg, const IR::F32& value);
    void D(IR::Reg dest_reg, const IR::F64& value);

    [[nodiscard]] IR::U32 GetReg8(u64 insn);
    [[nodiscard]] IR::U32 GetReg20(u64 insn);
    [[nodiscard]] IR::U32 GetReg39(u64 insn);
    [[nodiscard]] IR::F32 GetFloatReg8(u64 insn);
    [[nodiscard]] IR::F32 GetFloatReg20(u64 insn);
    [[nodiscard]] IR::F32 GetFloatReg39(u64 insn);
    [[nodiscard]] IR::F64 GetDoubleReg20(u64 insn);
    [[nodiscard]] IR::F64 GetDoubleReg39(u64 insn);

    [[nodiscard]] IR::U32 GetCbuf(u64 insn);
    [[nodiscard]] IR::F32 GetFloatCbuf(u64 insn);
    [[nodiscard]] IR::F64 GetDoubleCbuf(u64 insn);
    [[nodiscard]] IR::U64 GetPackedCbuf(u64 insn);

    [[nodiscard]] IR::U32 GetImm20(u64 insn);
    [[nodiscard]] IR::F32 GetFloatImm20(u64 insn);
    [[nodiscard]] IR::F64 GetDoubleImm20(u64 insn);
    [[nodiscard]] IR::U64 GetPackedImm20(u64 insn);

    [[nodiscard]] IR::U32 GetImm32(u64 insn);
    [[nodiscard]] IR::F32 GetFloatImm32(u64 insn);

    void SetZFlag(const IR::U1& value);
    void SetSFlag(const IR::U1& value);
    void SetCFlag(const IR::U1& value);
    void SetOFlag(const IR::U1& value);

    void ResetZero();
    void ResetSFlag();
    void ResetCFlag();
    void ResetOFlag();
};

} // namespace Shader::Maxwell
