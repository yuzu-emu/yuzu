// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/shader_info.h"

namespace Shader::Optimization {
namespace {
void AddConstantBufferDescriptor(Info& info, u32 index, u32 count) {
    if (count != 1) {
        throw NotImplementedException("Constant buffer descriptor indexing");
    }
    if ((info.constant_buffer_mask & (1U << index)) != 0) {
        return;
    }
    info.constant_buffer_mask |= 1U << index;

    auto& cbufs{info.constant_buffer_descriptors};
    cbufs.insert(std::ranges::lower_bound(cbufs, index, {}, &ConstantBufferDescriptor::index),
                 ConstantBufferDescriptor{
                     .index{index},
                     .count{1},
                 });
}

void GetAttribute(Info& info, IR::Attribute attribute) {
    if (IR::IsGeneric(attribute)) {
        info.loads_generics.at(IR::GenericAttributeIndex(attribute)) = true;
        return;
    }
    switch (attribute) {
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        info.loads_position = true;
        break;
    case IR::Attribute::InstanceId:
        info.loads_instance_id = true;
        break;
    case IR::Attribute::VertexId:
        info.loads_vertex_id = true;
        break;
    default:
        throw NotImplementedException("Get attribute {}", attribute);
    }
}

void SetAttribute(Info& info, IR::Attribute attribute) {
    if (IR::IsGeneric(attribute)) {
        info.stores_generics.at(IR::GenericAttributeIndex(attribute)) = true;
        return;
    }
    switch (attribute) {
    case IR::Attribute::PositionX:
    case IR::Attribute::PositionY:
    case IR::Attribute::PositionZ:
    case IR::Attribute::PositionW:
        info.stores_position = true;
        break;
    default:
        throw NotImplementedException("Set attribute {}", attribute);
    }
}

void VisitUsages(Info& info, IR::Inst& inst) {
    switch (inst.Opcode()) {
    case IR::Opcode::CompositeConstructF16x2:
    case IR::Opcode::CompositeConstructF16x3:
    case IR::Opcode::CompositeConstructF16x4:
    case IR::Opcode::CompositeExtractF16x2:
    case IR::Opcode::CompositeExtractF16x3:
    case IR::Opcode::CompositeExtractF16x4:
    case IR::Opcode::CompositeInsertF16x2:
    case IR::Opcode::CompositeInsertF16x3:
    case IR::Opcode::CompositeInsertF16x4:
    case IR::Opcode::SelectF16:
    case IR::Opcode::BitCastU16F16:
    case IR::Opcode::BitCastF16U16:
    case IR::Opcode::PackFloat2x16:
    case IR::Opcode::UnpackFloat2x16:
    case IR::Opcode::ConvertS16F16:
    case IR::Opcode::ConvertS32F16:
    case IR::Opcode::ConvertS64F16:
    case IR::Opcode::ConvertU16F16:
    case IR::Opcode::ConvertU32F16:
    case IR::Opcode::ConvertU64F16:
    case IR::Opcode::ConvertF16S8:
    case IR::Opcode::ConvertF16S16:
    case IR::Opcode::ConvertF16S32:
    case IR::Opcode::ConvertF16S64:
    case IR::Opcode::ConvertF16U8:
    case IR::Opcode::ConvertF16U16:
    case IR::Opcode::ConvertF16U32:
    case IR::Opcode::ConvertF16U64:
    case IR::Opcode::FPAbs16:
    case IR::Opcode::FPAdd16:
    case IR::Opcode::FPCeil16:
    case IR::Opcode::FPFloor16:
    case IR::Opcode::FPFma16:
    case IR::Opcode::FPMul16:
    case IR::Opcode::FPNeg16:
    case IR::Opcode::FPRoundEven16:
    case IR::Opcode::FPSaturate16:
    case IR::Opcode::FPClamp16:
    case IR::Opcode::FPTrunc16:
    case IR::Opcode::FPOrdEqual16:
    case IR::Opcode::FPUnordEqual16:
    case IR::Opcode::FPOrdNotEqual16:
    case IR::Opcode::FPUnordNotEqual16:
    case IR::Opcode::FPOrdLessThan16:
    case IR::Opcode::FPUnordLessThan16:
    case IR::Opcode::FPOrdGreaterThan16:
    case IR::Opcode::FPUnordGreaterThan16:
    case IR::Opcode::FPOrdLessThanEqual16:
    case IR::Opcode::FPUnordLessThanEqual16:
    case IR::Opcode::FPOrdGreaterThanEqual16:
    case IR::Opcode::FPUnordGreaterThanEqual16:
    case IR::Opcode::FPIsNan16:
        info.uses_fp16 = true;
        break;
    case IR::Opcode::CompositeConstructF64x2:
    case IR::Opcode::CompositeConstructF64x3:
    case IR::Opcode::CompositeConstructF64x4:
    case IR::Opcode::CompositeExtractF64x2:
    case IR::Opcode::CompositeExtractF64x3:
    case IR::Opcode::CompositeExtractF64x4:
    case IR::Opcode::CompositeInsertF64x2:
    case IR::Opcode::CompositeInsertF64x3:
    case IR::Opcode::CompositeInsertF64x4:
    case IR::Opcode::SelectF64:
    case IR::Opcode::BitCastU64F64:
    case IR::Opcode::BitCastF64U64:
    case IR::Opcode::PackDouble2x32:
    case IR::Opcode::UnpackDouble2x32:
    case IR::Opcode::FPAbs64:
    case IR::Opcode::FPAdd64:
    case IR::Opcode::FPCeil64:
    case IR::Opcode::FPFloor64:
    case IR::Opcode::FPFma64:
    case IR::Opcode::FPMax64:
    case IR::Opcode::FPMin64:
    case IR::Opcode::FPMul64:
    case IR::Opcode::FPNeg64:
    case IR::Opcode::FPRecip64:
    case IR::Opcode::FPRecipSqrt64:
    case IR::Opcode::FPRoundEven64:
    case IR::Opcode::FPSaturate64:
    case IR::Opcode::FPClamp64:
    case IR::Opcode::FPTrunc64:
    case IR::Opcode::FPOrdEqual64:
    case IR::Opcode::FPUnordEqual64:
    case IR::Opcode::FPOrdNotEqual64:
    case IR::Opcode::FPUnordNotEqual64:
    case IR::Opcode::FPOrdLessThan64:
    case IR::Opcode::FPUnordLessThan64:
    case IR::Opcode::FPOrdGreaterThan64:
    case IR::Opcode::FPUnordGreaterThan64:
    case IR::Opcode::FPOrdLessThanEqual64:
    case IR::Opcode::FPUnordLessThanEqual64:
    case IR::Opcode::FPOrdGreaterThanEqual64:
    case IR::Opcode::FPUnordGreaterThanEqual64:
    case IR::Opcode::FPIsNan64:
    case IR::Opcode::ConvertS16F64:
    case IR::Opcode::ConvertS32F64:
    case IR::Opcode::ConvertS64F64:
    case IR::Opcode::ConvertU16F64:
    case IR::Opcode::ConvertU32F64:
    case IR::Opcode::ConvertU64F64:
    case IR::Opcode::ConvertF32F64:
    case IR::Opcode::ConvertF64F32:
    case IR::Opcode::ConvertF64S8:
    case IR::Opcode::ConvertF64S16:
    case IR::Opcode::ConvertF64S32:
    case IR::Opcode::ConvertF64S64:
    case IR::Opcode::ConvertF64U8:
    case IR::Opcode::ConvertF64U16:
    case IR::Opcode::ConvertF64U32:
    case IR::Opcode::ConvertF64U64:
        info.uses_fp64 = true;
        break;
    default:
        break;
    }
    switch (inst.Opcode()) {
    case IR::Opcode::GetCbufU8:
    case IR::Opcode::GetCbufS8:
    case IR::Opcode::UndefU8:
    case IR::Opcode::LoadGlobalU8:
    case IR::Opcode::LoadGlobalS8:
    case IR::Opcode::WriteGlobalU8:
    case IR::Opcode::WriteGlobalS8:
    case IR::Opcode::LoadStorageU8:
    case IR::Opcode::LoadStorageS8:
    case IR::Opcode::WriteStorageU8:
    case IR::Opcode::WriteStorageS8:
    case IR::Opcode::SelectU8:
    case IR::Opcode::ConvertF16S8:
    case IR::Opcode::ConvertF16U8:
    case IR::Opcode::ConvertF32S8:
    case IR::Opcode::ConvertF32U8:
    case IR::Opcode::ConvertF64S8:
    case IR::Opcode::ConvertF64U8:
        info.uses_int8 = true;
        break;
    default:
        break;
    }
    switch (inst.Opcode()) {
    case IR::Opcode::GetCbufU16:
    case IR::Opcode::GetCbufS16:
    case IR::Opcode::UndefU16:
    case IR::Opcode::LoadGlobalU16:
    case IR::Opcode::LoadGlobalS16:
    case IR::Opcode::WriteGlobalU16:
    case IR::Opcode::WriteGlobalS16:
    case IR::Opcode::LoadStorageU16:
    case IR::Opcode::LoadStorageS16:
    case IR::Opcode::WriteStorageU16:
    case IR::Opcode::WriteStorageS16:
    case IR::Opcode::SelectU16:
    case IR::Opcode::BitCastU16F16:
    case IR::Opcode::BitCastF16U16:
    case IR::Opcode::ConvertS16F16:
    case IR::Opcode::ConvertS16F32:
    case IR::Opcode::ConvertS16F64:
    case IR::Opcode::ConvertU16F16:
    case IR::Opcode::ConvertU16F32:
    case IR::Opcode::ConvertU16F64:
    case IR::Opcode::ConvertF16S16:
    case IR::Opcode::ConvertF16U16:
    case IR::Opcode::ConvertF32S16:
    case IR::Opcode::ConvertF32U16:
    case IR::Opcode::ConvertF64S16:
    case IR::Opcode::ConvertF64U16:
        info.uses_int16 = true;
        break;
    default:
        break;
    }
    switch (inst.Opcode()) {
    case IR::Opcode::GetCbufU64:
    case IR::Opcode::UndefU64:
    case IR::Opcode::LoadGlobalU8:
    case IR::Opcode::LoadGlobalS8:
    case IR::Opcode::LoadGlobalU16:
    case IR::Opcode::LoadGlobalS16:
    case IR::Opcode::LoadGlobal32:
    case IR::Opcode::LoadGlobal64:
    case IR::Opcode::LoadGlobal128:
    case IR::Opcode::WriteGlobalU8:
    case IR::Opcode::WriteGlobalS8:
    case IR::Opcode::WriteGlobalU16:
    case IR::Opcode::WriteGlobalS16:
    case IR::Opcode::WriteGlobal32:
    case IR::Opcode::WriteGlobal64:
    case IR::Opcode::WriteGlobal128:
    case IR::Opcode::SelectU64:
    case IR::Opcode::BitCastU64F64:
    case IR::Opcode::BitCastF64U64:
    case IR::Opcode::PackUint2x32:
    case IR::Opcode::UnpackUint2x32:
    case IR::Opcode::IAdd64:
    case IR::Opcode::ISub64:
    case IR::Opcode::INeg64:
    case IR::Opcode::ShiftLeftLogical64:
    case IR::Opcode::ShiftRightLogical64:
    case IR::Opcode::ShiftRightArithmetic64:
    case IR::Opcode::ConvertS64F16:
    case IR::Opcode::ConvertS64F32:
    case IR::Opcode::ConvertS64F64:
    case IR::Opcode::ConvertU64F16:
    case IR::Opcode::ConvertU64F32:
    case IR::Opcode::ConvertU64F64:
    case IR::Opcode::ConvertU64U32:
    case IR::Opcode::ConvertU32U64:
    case IR::Opcode::ConvertF16U64:
    case IR::Opcode::ConvertF32U64:
    case IR::Opcode::ConvertF64U64:
        info.uses_int64 = true;
        break;
    default:
        break;
    }
    switch (inst.Opcode()) {
    case IR::Opcode::DemoteToHelperInvocation:
        info.uses_demote_to_helper_invocation = true;
        break;
    case IR::Opcode::GetAttribute:
        GetAttribute(info, inst.Arg(0).Attribute());
        break;
    case IR::Opcode::SetAttribute:
        SetAttribute(info, inst.Arg(0).Attribute());
        break;
    case IR::Opcode::SetFragColor:
        info.stores_frag_color[inst.Arg(0).U32()] = true;
        break;
    case IR::Opcode::SetFragDepth:
        info.stores_frag_depth = true;
        break;
    case IR::Opcode::WorkgroupId:
        info.uses_workgroup_id = true;
        break;
    case IR::Opcode::LocalInvocationId:
        info.uses_local_invocation_id = true;
        break;
    case IR::Opcode::ShuffleIndex:
    case IR::Opcode::ShuffleUp:
    case IR::Opcode::ShuffleDown:
    case IR::Opcode::ShuffleButterfly:
        info.uses_subgroup_invocation_id = true;
        break;
    case IR::Opcode::GetCbufU8:
    case IR::Opcode::GetCbufS8:
    case IR::Opcode::GetCbufU16:
    case IR::Opcode::GetCbufS16:
    case IR::Opcode::GetCbufU32:
    case IR::Opcode::GetCbufF32:
    case IR::Opcode::GetCbufU64: {
        if (const IR::Value index{inst.Arg(0)}; index.IsImmediate()) {
            AddConstantBufferDescriptor(info, index.U32(), 1);
        } else {
            throw NotImplementedException("Constant buffer with non-immediate index");
        }
        switch (inst.Opcode()) {
        case IR::Opcode::GetCbufU8:
        case IR::Opcode::GetCbufS8:
            info.used_constant_buffer_types |= IR::Type::U8;
            break;
        case IR::Opcode::GetCbufU16:
        case IR::Opcode::GetCbufS16:
            info.used_constant_buffer_types |= IR::Type::U16;
            break;
        case IR::Opcode::GetCbufU32:
            info.used_constant_buffer_types |= IR::Type::U32;
            break;
        case IR::Opcode::GetCbufF32:
            info.used_constant_buffer_types |= IR::Type::F32;
            break;
        case IR::Opcode::GetCbufU64:
            info.used_constant_buffer_types |= IR::Type::U64;
            break;
        default:
            break;
        }
        break;
    }
    case IR::Opcode::BindlessImageSampleImplicitLod:
    case IR::Opcode::BindlessImageSampleExplicitLod:
    case IR::Opcode::BindlessImageSampleDrefImplicitLod:
    case IR::Opcode::BindlessImageSampleDrefExplicitLod:
    case IR::Opcode::BindlessImageGather:
    case IR::Opcode::BindlessImageGatherDref:
    case IR::Opcode::BoundImageSampleImplicitLod:
    case IR::Opcode::BoundImageSampleExplicitLod:
    case IR::Opcode::BoundImageSampleDrefImplicitLod:
    case IR::Opcode::BoundImageSampleDrefExplicitLod:
    case IR::Opcode::BoundImageGather:
    case IR::Opcode::BoundImageGatherDref:
    case IR::Opcode::ImageSampleImplicitLod:
    case IR::Opcode::ImageSampleExplicitLod:
    case IR::Opcode::ImageSampleDrefImplicitLod:
    case IR::Opcode::ImageSampleDrefExplicitLod:
    case IR::Opcode::ImageGather:
    case IR::Opcode::ImageGatherDref: {
        const TextureType type{inst.Flags<IR::TextureInstInfo>().type};
        info.uses_sampled_1d |= type == TextureType::Color1D || type == TextureType::ColorArray1D ||
                                type == TextureType::Shadow1D || type == TextureType::ShadowArray1D;
        info.uses_sparse_residency |=
            inst.GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp) != nullptr;
        break;
    }
    case IR::Opcode::VoteAll:
    case IR::Opcode::VoteAny:
    case IR::Opcode::VoteEqual:
    case IR::Opcode::SubgroupBallot:
        info.uses_subgroup_vote = true;
        break;
    default:
        break;
    }
}

void VisitFpModifiers(Info& info, IR::Inst& inst) {
    switch (inst.Opcode()) {
    case IR::Opcode::FPAdd16:
    case IR::Opcode::FPFma16:
    case IR::Opcode::FPMul16:
    case IR::Opcode::FPRoundEven16:
    case IR::Opcode::FPFloor16:
    case IR::Opcode::FPCeil16:
    case IR::Opcode::FPTrunc16: {
        const auto control{inst.Flags<IR::FpControl>()};
        switch (control.fmz_mode) {
        case IR::FmzMode::DontCare:
            break;
        case IR::FmzMode::FTZ:
        case IR::FmzMode::FMZ:
            info.uses_fp16_denorms_flush = true;
            break;
        case IR::FmzMode::None:
            info.uses_fp16_denorms_preserve = true;
            break;
        }
        break;
    }
    case IR::Opcode::FPAdd32:
    case IR::Opcode::FPFma32:
    case IR::Opcode::FPMul32:
    case IR::Opcode::FPRoundEven32:
    case IR::Opcode::FPFloor32:
    case IR::Opcode::FPCeil32:
    case IR::Opcode::FPTrunc32:
    case IR::Opcode::FPOrdEqual32:
    case IR::Opcode::FPUnordEqual32:
    case IR::Opcode::FPOrdNotEqual32:
    case IR::Opcode::FPUnordNotEqual32:
    case IR::Opcode::FPOrdLessThan32:
    case IR::Opcode::FPUnordLessThan32:
    case IR::Opcode::FPOrdGreaterThan32:
    case IR::Opcode::FPUnordGreaterThan32:
    case IR::Opcode::FPOrdLessThanEqual32:
    case IR::Opcode::FPUnordLessThanEqual32:
    case IR::Opcode::FPOrdGreaterThanEqual32:
    case IR::Opcode::FPUnordGreaterThanEqual32:
    case IR::Opcode::ConvertF16F32:
    case IR::Opcode::ConvertF64F32: {
        const auto control{inst.Flags<IR::FpControl>()};
        switch (control.fmz_mode) {
        case IR::FmzMode::DontCare:
            break;
        case IR::FmzMode::FTZ:
        case IR::FmzMode::FMZ:
            info.uses_fp32_denorms_flush = true;
            break;
        case IR::FmzMode::None:
            info.uses_fp32_denorms_preserve = true;
            break;
        }
        break;
    }
    default:
        break;
    }
}

void Visit(Info& info, IR::Inst& inst) {
    VisitUsages(info, inst);
    VisitFpModifiers(info, inst);
}
} // Anonymous namespace

void CollectShaderInfoPass(IR::Program& program) {
    Info& info{program.info};
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            Visit(info, inst);
        }
    }
}

} // namespace Shader::Optimization
