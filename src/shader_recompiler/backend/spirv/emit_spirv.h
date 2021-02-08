// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <sirit/sirit.h>

#include <boost/container/flat_map.hpp>

#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/ir/program.h"

namespace Shader::Backend::SPIRV {

using Sirit::Id;

class DefMap {
public:
    void Define(IR::Inst* inst, Id def_id) {
        const InstInfo info{.use_count{inst->UseCount()}, .def_id{def_id}};
        const auto it{map.insert(map.end(), std::make_pair(inst, info))};
        if (it == map.end()) {
            throw LogicError("Defining already defined instruction");
        }
    }

    [[nodiscard]] Id Consume(IR::Inst* inst) {
        const auto it{map.find(inst)};
        if (it == map.end()) {
            throw LogicError("Consuming undefined instruction");
        }
        const Id def_id{it->second.def_id};
        if (--it->second.use_count == 0) {
            map.erase(it);
        }
        return def_id;
    }

private:
    struct InstInfo {
        int use_count;
        Id def_id;
    };

    boost::container::flat_map<IR::Inst*, InstInfo> map;
};

class VectorTypes {
public:
    void Define(Sirit::Module& sirit_ctx, Id base_type, std::string_view name) {
        defs[0] = sirit_ctx.Name(base_type, name);

        std::array<char, 6> def_name;
        for (int i = 1; i < 4; ++i) {
            const std::string_view def_name_view(
                def_name.data(),
                fmt::format_to_n(def_name.data(), def_name.size(), "{}x{}", name, i + 1).size);
            defs[i] = sirit_ctx.Name(sirit_ctx.TypeVector(base_type, i + 1), def_name_view);
        }
    }

    [[nodiscard]] Id operator[](size_t size) const noexcept {
        return defs[size - 1];
    }

private:
    std::array<Id, 4> defs;
};

class EmitContext final : public Sirit::Module {
public:
    explicit EmitContext(IR::Program& program);
    ~EmitContext();

    [[nodiscard]] Id Def(const IR::Value& value) {
        if (!value.IsImmediate()) {
            return def_map.Consume(value.Inst());
        }
        switch (value.Type()) {
        case IR::Type::U32:
            return Constant(u32[1], value.U32());
        case IR::Type::F32:
            return Constant(f32[1], value.F32());
        default:
            throw NotImplementedException("Immediate type {}", value.Type());
        }
    }

    void Define(IR::Inst* inst, Id def_id) {
        def_map.Define(inst, def_id);
    }

    [[nodiscard]] Id BlockLabel(IR::Block* block) const {
        const auto it{std::ranges::lower_bound(block_label_map, block, {},
                                               &std::pair<IR::Block*, Id>::first)};
        if (it == block_label_map.end()) {
            throw LogicError("Undefined block");
        }
        return it->second;
    }

    Id void_id{};
    Id u1{};
    VectorTypes f32;
    VectorTypes u32;
    VectorTypes f16;
    VectorTypes f64;

    Id workgroup_id{};
    Id local_invocation_id{};

private:
    DefMap def_map;
    std::vector<std::pair<IR::Block*, Id>> block_label_map;
};

class EmitSPIRV {
public:
    explicit EmitSPIRV(IR::Program& program);

private:
    void EmitInst(EmitContext& ctx, IR::Inst* inst);

    // Microinstruction emitters
    void EmitPhi(EmitContext& ctx);
    void EmitVoid(EmitContext& ctx);
    void EmitIdentity(EmitContext& ctx);
    void EmitBranch(EmitContext& ctx, IR::Inst* inst);
    void EmitBranchConditional(EmitContext& ctx, IR::Inst* inst);
    void EmitExit(EmitContext& ctx);
    void EmitReturn(EmitContext& ctx);
    void EmitUnreachable(EmitContext& ctx);
    void EmitGetRegister(EmitContext& ctx);
    void EmitSetRegister(EmitContext& ctx);
    void EmitGetPred(EmitContext& ctx);
    void EmitSetPred(EmitContext& ctx);
    Id EmitGetCbuf(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
    void EmitGetAttribute(EmitContext& ctx);
    void EmitSetAttribute(EmitContext& ctx);
    void EmitGetAttributeIndexed(EmitContext& ctx);
    void EmitSetAttributeIndexed(EmitContext& ctx);
    void EmitGetZFlag(EmitContext& ctx);
    void EmitGetSFlag(EmitContext& ctx);
    void EmitGetCFlag(EmitContext& ctx);
    void EmitGetOFlag(EmitContext& ctx);
    void EmitSetZFlag(EmitContext& ctx);
    void EmitSetSFlag(EmitContext& ctx);
    void EmitSetCFlag(EmitContext& ctx);
    void EmitSetOFlag(EmitContext& ctx);
    Id EmitWorkgroupId(EmitContext& ctx);
    Id EmitLocalInvocationId(EmitContext& ctx);
    void EmitUndef1(EmitContext& ctx);
    void EmitUndef8(EmitContext& ctx);
    void EmitUndef16(EmitContext& ctx);
    void EmitUndef32(EmitContext& ctx);
    void EmitUndef64(EmitContext& ctx);
    void EmitLoadGlobalU8(EmitContext& ctx);
    void EmitLoadGlobalS8(EmitContext& ctx);
    void EmitLoadGlobalU16(EmitContext& ctx);
    void EmitLoadGlobalS16(EmitContext& ctx);
    void EmitLoadGlobal32(EmitContext& ctx);
    void EmitLoadGlobal64(EmitContext& ctx);
    void EmitLoadGlobal128(EmitContext& ctx);
    void EmitWriteGlobalU8(EmitContext& ctx);
    void EmitWriteGlobalS8(EmitContext& ctx);
    void EmitWriteGlobalU16(EmitContext& ctx);
    void EmitWriteGlobalS16(EmitContext& ctx);
    void EmitWriteGlobal32(EmitContext& ctx);
    void EmitWriteGlobal64(EmitContext& ctx);
    void EmitWriteGlobal128(EmitContext& ctx);
    void EmitLoadStorageU8(EmitContext& ctx);
    void EmitLoadStorageS8(EmitContext& ctx);
    void EmitLoadStorageU16(EmitContext& ctx);
    void EmitLoadStorageS16(EmitContext& ctx);
    Id EmitLoadStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset);
    void EmitLoadStorage64(EmitContext& ctx);
    void EmitLoadStorage128(EmitContext& ctx);
    void EmitWriteStorageU8(EmitContext& ctx);
    void EmitWriteStorageS8(EmitContext& ctx);
    void EmitWriteStorageU16(EmitContext& ctx);
    void EmitWriteStorageS16(EmitContext& ctx);
    void EmitWriteStorage32(EmitContext& ctx);
    void EmitWriteStorage64(EmitContext& ctx);
    void EmitWriteStorage128(EmitContext& ctx);
    void EmitCompositeConstructU32x2(EmitContext& ctx);
    void EmitCompositeConstructU32x3(EmitContext& ctx);
    void EmitCompositeConstructU32x4(EmitContext& ctx);
    void EmitCompositeExtractU32x2(EmitContext& ctx);
    Id EmitCompositeExtractU32x3(EmitContext& ctx, Id vector, u32 index);
    void EmitCompositeExtractU32x4(EmitContext& ctx);
    void EmitCompositeConstructF16x2(EmitContext& ctx);
    void EmitCompositeConstructF16x3(EmitContext& ctx);
    void EmitCompositeConstructF16x4(EmitContext& ctx);
    void EmitCompositeExtractF16x2(EmitContext& ctx);
    void EmitCompositeExtractF16x3(EmitContext& ctx);
    void EmitCompositeExtractF16x4(EmitContext& ctx);
    void EmitCompositeConstructF32x2(EmitContext& ctx);
    void EmitCompositeConstructF32x3(EmitContext& ctx);
    void EmitCompositeConstructF32x4(EmitContext& ctx);
    void EmitCompositeExtractF32x2(EmitContext& ctx);
    void EmitCompositeExtractF32x3(EmitContext& ctx);
    void EmitCompositeExtractF32x4(EmitContext& ctx);
    void EmitCompositeConstructF64x2(EmitContext& ctx);
    void EmitCompositeConstructF64x3(EmitContext& ctx);
    void EmitCompositeConstructF64x4(EmitContext& ctx);
    void EmitCompositeExtractF64x2(EmitContext& ctx);
    void EmitCompositeExtractF64x3(EmitContext& ctx);
    void EmitCompositeExtractF64x4(EmitContext& ctx);
    void EmitSelect8(EmitContext& ctx);
    void EmitSelect16(EmitContext& ctx);
    void EmitSelect32(EmitContext& ctx);
    void EmitSelect64(EmitContext& ctx);
    void EmitBitCastU16F16(EmitContext& ctx);
    Id EmitBitCastU32F32(EmitContext& ctx, Id value);
    void EmitBitCastU64F64(EmitContext& ctx);
    void EmitBitCastF16U16(EmitContext& ctx);
    Id EmitBitCastF32U32(EmitContext& ctx, Id value);
    void EmitBitCastF64U64(EmitContext& ctx);
    void EmitPackUint2x32(EmitContext& ctx);
    void EmitUnpackUint2x32(EmitContext& ctx);
    void EmitPackFloat2x16(EmitContext& ctx);
    void EmitUnpackFloat2x16(EmitContext& ctx);
    void EmitPackDouble2x32(EmitContext& ctx);
    void EmitUnpackDouble2x32(EmitContext& ctx);
    void EmitGetZeroFromOp(EmitContext& ctx);
    void EmitGetSignFromOp(EmitContext& ctx);
    void EmitGetCarryFromOp(EmitContext& ctx);
    void EmitGetOverflowFromOp(EmitContext& ctx);
    void EmitFPAbs16(EmitContext& ctx);
    void EmitFPAbs32(EmitContext& ctx);
    void EmitFPAbs64(EmitContext& ctx);
    Id EmitFPAdd16(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
    Id EmitFPAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
    Id EmitFPAdd64(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
    Id EmitFPFma16(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c);
    Id EmitFPFma32(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c);
    Id EmitFPFma64(EmitContext& ctx, IR::Inst* inst, Id a, Id b, Id c);
    void EmitFPMax32(EmitContext& ctx);
    void EmitFPMax64(EmitContext& ctx);
    void EmitFPMin32(EmitContext& ctx);
    void EmitFPMin64(EmitContext& ctx);
    Id EmitFPMul16(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
    Id EmitFPMul32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
    Id EmitFPMul64(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
    void EmitFPNeg16(EmitContext& ctx);
    void EmitFPNeg32(EmitContext& ctx);
    void EmitFPNeg64(EmitContext& ctx);
    void EmitFPRecip32(EmitContext& ctx);
    void EmitFPRecip64(EmitContext& ctx);
    void EmitFPRecipSqrt32(EmitContext& ctx);
    void EmitFPRecipSqrt64(EmitContext& ctx);
    void EmitFPSqrt(EmitContext& ctx);
    void EmitFPSin(EmitContext& ctx);
    void EmitFPSinNotReduced(EmitContext& ctx);
    void EmitFPExp2(EmitContext& ctx);
    void EmitFPExp2NotReduced(EmitContext& ctx);
    void EmitFPCos(EmitContext& ctx);
    void EmitFPCosNotReduced(EmitContext& ctx);
    void EmitFPLog2(EmitContext& ctx);
    void EmitFPSaturate16(EmitContext& ctx);
    void EmitFPSaturate32(EmitContext& ctx);
    void EmitFPSaturate64(EmitContext& ctx);
    void EmitFPRoundEven16(EmitContext& ctx);
    void EmitFPRoundEven32(EmitContext& ctx);
    void EmitFPRoundEven64(EmitContext& ctx);
    void EmitFPFloor16(EmitContext& ctx);
    void EmitFPFloor32(EmitContext& ctx);
    void EmitFPFloor64(EmitContext& ctx);
    void EmitFPCeil16(EmitContext& ctx);
    void EmitFPCeil32(EmitContext& ctx);
    void EmitFPCeil64(EmitContext& ctx);
    void EmitFPTrunc16(EmitContext& ctx);
    void EmitFPTrunc32(EmitContext& ctx);
    void EmitFPTrunc64(EmitContext& ctx);
    Id EmitIAdd32(EmitContext& ctx, IR::Inst* inst, Id a, Id b);
    void EmitIAdd64(EmitContext& ctx);
    Id EmitISub32(EmitContext& ctx, Id a, Id b);
    void EmitISub64(EmitContext& ctx);
    Id EmitIMul32(EmitContext& ctx, Id a, Id b);
    void EmitINeg32(EmitContext& ctx);
    void EmitIAbs32(EmitContext& ctx);
    Id EmitShiftLeftLogical32(EmitContext& ctx, Id base, Id shift);
    void EmitShiftRightLogical32(EmitContext& ctx);
    void EmitShiftRightArithmetic32(EmitContext& ctx);
    void EmitBitwiseAnd32(EmitContext& ctx);
    void EmitBitwiseOr32(EmitContext& ctx);
    void EmitBitwiseXor32(EmitContext& ctx);
    void EmitBitFieldInsert(EmitContext& ctx);
    void EmitBitFieldSExtract(EmitContext& ctx);
    Id EmitBitFieldUExtract(EmitContext& ctx, Id base, Id offset, Id count);
    void EmitSLessThan(EmitContext& ctx);
    void EmitULessThan(EmitContext& ctx);
    void EmitIEqual(EmitContext& ctx);
    void EmitSLessThanEqual(EmitContext& ctx);
    void EmitULessThanEqual(EmitContext& ctx);
    void EmitSGreaterThan(EmitContext& ctx);
    void EmitUGreaterThan(EmitContext& ctx);
    void EmitINotEqual(EmitContext& ctx);
    void EmitSGreaterThanEqual(EmitContext& ctx);
    Id EmitUGreaterThanEqual(EmitContext& ctx, Id lhs, Id rhs);
    void EmitLogicalOr(EmitContext& ctx);
    void EmitLogicalAnd(EmitContext& ctx);
    void EmitLogicalXor(EmitContext& ctx);
    void EmitLogicalNot(EmitContext& ctx);
    void EmitConvertS16F16(EmitContext& ctx);
    void EmitConvertS16F32(EmitContext& ctx);
    void EmitConvertS16F64(EmitContext& ctx);
    void EmitConvertS32F16(EmitContext& ctx);
    void EmitConvertS32F32(EmitContext& ctx);
    void EmitConvertS32F64(EmitContext& ctx);
    void EmitConvertS64F16(EmitContext& ctx);
    void EmitConvertS64F32(EmitContext& ctx);
    void EmitConvertS64F64(EmitContext& ctx);
    void EmitConvertU16F16(EmitContext& ctx);
    void EmitConvertU16F32(EmitContext& ctx);
    void EmitConvertU16F64(EmitContext& ctx);
    void EmitConvertU32F16(EmitContext& ctx);
    void EmitConvertU32F32(EmitContext& ctx);
    void EmitConvertU32F64(EmitContext& ctx);
    void EmitConvertU64F16(EmitContext& ctx);
    void EmitConvertU64F32(EmitContext& ctx);
    void EmitConvertU64F64(EmitContext& ctx);
    void EmitConvertU64U32(EmitContext& ctx);
    void EmitConvertU32U64(EmitContext& ctx);
};

} // namespace Shader::Backend::SPIRV
