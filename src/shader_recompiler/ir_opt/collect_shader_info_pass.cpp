// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/ir_opt/passes.h"
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
                     .index = index,
                     .count = 1,
                 });
}

void GetPatch(Info& info, IR::Patch patch) {
    if (!IR::IsGeneric(patch)) {
        throw NotImplementedException("Reading non-generic patch {}", patch);
    }
    info.uses_patches.at(IR::GenericPatchIndex(patch)) = true;
}

void SetPatch(Info& info, IR::Patch patch) {
    if (IR::IsGeneric(patch)) {
        info.uses_patches.at(IR::GenericPatchIndex(patch)) = true;
        return;
    }
    switch (patch) {
    case IR::Patch::TessellationLodLeft:
    case IR::Patch::TessellationLodTop:
    case IR::Patch::TessellationLodRight:
    case IR::Patch::TessellationLodBottom:
        info.stores_tess_level_outer = true;
        break;
    case IR::Patch::TessellationLodInteriorU:
    case IR::Patch::TessellationLodInteriorV:
        info.stores_tess_level_inner = true;
        break;
    default:
        throw NotImplementedException("Set patch {}", patch);
    }
}

void CheckCBufNVN(Info& info, IR::Inst& inst) {
    const IR::Value cbuf_index{inst.Arg(0)};
    if (!cbuf_index.IsImmediate()) {
        info.nvn_buffer_used.set();
        return;
    }
    const u32 index{cbuf_index.U32()};
    if (index != 0) {
        return;
    }
    const IR::Value cbuf_offset{inst.Arg(1)};
    if (!cbuf_offset.IsImmediate()) {
        info.nvn_buffer_used.set();
        return;
    }
    const u32 offset{cbuf_offset.U32()};
    const u32 descriptor_size{0x10};
    const u32 upper_limit{info.nvn_buffer_base + descriptor_size * 16};
    if (offset >= info.nvn_buffer_base && offset < upper_limit) {
        const std::size_t nvn_index{(offset - info.nvn_buffer_base) / descriptor_size};
        info.nvn_buffer_used.set(nvn_index, true);
    }
}

void VisitUsages(Info& info, IR::Inst& inst) {
    switch (inst.GetOpcode()) {
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
    case IR::Opcode::GlobalAtomicAddF16x2:
    case IR::Opcode::GlobalAtomicMinF16x2:
    case IR::Opcode::GlobalAtomicMaxF16x2:
    case IR::Opcode::StorageAtomicAddF16x2:
    case IR::Opcode::StorageAtomicMinF16x2:
    case IR::Opcode::StorageAtomicMaxF16x2:
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
    switch (inst.GetOpcode()) {
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
    case IR::Opcode::LoadSharedU8:
    case IR::Opcode::LoadSharedS8:
    case IR::Opcode::WriteSharedU8:
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
    switch (inst.GetOpcode()) {
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
    case IR::Opcode::LoadSharedU16:
    case IR::Opcode::LoadSharedS16:
    case IR::Opcode::WriteSharedU16:
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
    switch (inst.GetOpcode()) {
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
    case IR::Opcode::SharedAtomicExchange64:
    case IR::Opcode::GlobalAtomicIAdd64:
    case IR::Opcode::GlobalAtomicSMin64:
    case IR::Opcode::GlobalAtomicUMin64:
    case IR::Opcode::GlobalAtomicSMax64:
    case IR::Opcode::GlobalAtomicUMax64:
    case IR::Opcode::GlobalAtomicAnd64:
    case IR::Opcode::GlobalAtomicOr64:
    case IR::Opcode::GlobalAtomicXor64:
    case IR::Opcode::GlobalAtomicExchange64:
    case IR::Opcode::StorageAtomicIAdd64:
    case IR::Opcode::StorageAtomicSMin64:
    case IR::Opcode::StorageAtomicUMin64:
    case IR::Opcode::StorageAtomicSMax64:
    case IR::Opcode::StorageAtomicUMax64:
    case IR::Opcode::StorageAtomicAnd64:
    case IR::Opcode::StorageAtomicOr64:
    case IR::Opcode::StorageAtomicXor64:
    case IR::Opcode::StorageAtomicExchange64:
        info.uses_int64 = true;
        break;
    default:
        break;
    }
    switch (inst.GetOpcode()) {
    case IR::Opcode::WriteGlobalU8:
    case IR::Opcode::WriteGlobalS8:
    case IR::Opcode::WriteGlobalU16:
    case IR::Opcode::WriteGlobalS16:
    case IR::Opcode::WriteGlobal32:
    case IR::Opcode::WriteGlobal64:
    case IR::Opcode::WriteGlobal128:
    case IR::Opcode::GlobalAtomicIAdd32:
    case IR::Opcode::GlobalAtomicSMin32:
    case IR::Opcode::GlobalAtomicUMin32:
    case IR::Opcode::GlobalAtomicSMax32:
    case IR::Opcode::GlobalAtomicUMax32:
    case IR::Opcode::GlobalAtomicInc32:
    case IR::Opcode::GlobalAtomicDec32:
    case IR::Opcode::GlobalAtomicAnd32:
    case IR::Opcode::GlobalAtomicOr32:
    case IR::Opcode::GlobalAtomicXor32:
    case IR::Opcode::GlobalAtomicExchange32:
    case IR::Opcode::GlobalAtomicIAdd64:
    case IR::Opcode::GlobalAtomicSMin64:
    case IR::Opcode::GlobalAtomicUMin64:
    case IR::Opcode::GlobalAtomicSMax64:
    case IR::Opcode::GlobalAtomicUMax64:
    case IR::Opcode::GlobalAtomicAnd64:
    case IR::Opcode::GlobalAtomicOr64:
    case IR::Opcode::GlobalAtomicXor64:
    case IR::Opcode::GlobalAtomicExchange64:
    case IR::Opcode::GlobalAtomicAddF32:
    case IR::Opcode::GlobalAtomicAddF16x2:
    case IR::Opcode::GlobalAtomicAddF32x2:
    case IR::Opcode::GlobalAtomicMinF16x2:
    case IR::Opcode::GlobalAtomicMinF32x2:
    case IR::Opcode::GlobalAtomicMaxF16x2:
    case IR::Opcode::GlobalAtomicMaxF32x2:
        info.stores_global_memory = true;
        [[fallthrough]];
    case IR::Opcode::LoadGlobalU8:
    case IR::Opcode::LoadGlobalS8:
    case IR::Opcode::LoadGlobalU16:
    case IR::Opcode::LoadGlobalS16:
    case IR::Opcode::LoadGlobal32:
    case IR::Opcode::LoadGlobal64:
    case IR::Opcode::LoadGlobal128:
        info.uses_int64 = true;
        info.uses_global_memory = true;
        info.used_constant_buffer_types |= IR::Type::U32 | IR::Type::U32x2;
        info.used_storage_buffer_types |= IR::Type::U32 | IR::Type::U32x2 | IR::Type::U32x4;
        break;
    default:
        break;
    }
    switch (inst.GetOpcode()) {
    case IR::Opcode::DemoteToHelperInvocation:
        info.uses_demote_to_helper_invocation = true;
        break;
    case IR::Opcode::GetAttribute:
        info.loads.mask[static_cast<size_t>(inst.Arg(0).Attribute())] = true;
        break;
    case IR::Opcode::SetAttribute:
        info.stores.mask[static_cast<size_t>(inst.Arg(0).Attribute())] = true;
        break;
    case IR::Opcode::GetPatch:
        GetPatch(info, inst.Arg(0).Patch());
        break;
    case IR::Opcode::SetPatch:
        SetPatch(info, inst.Arg(0).Patch());
        break;
    case IR::Opcode::GetAttributeIndexed:
        info.loads_indexed_attributes = true;
        break;
    case IR::Opcode::SetAttributeIndexed:
        info.stores_indexed_attributes = true;
        break;
    case IR::Opcode::SetFragColor:
        info.stores_frag_color[inst.Arg(0).U32()] = true;
        break;
    case IR::Opcode::SetSampleMask:
        info.stores_sample_mask = true;
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
    case IR::Opcode::InvocationId:
        info.uses_invocation_id = true;
        break;
    case IR::Opcode::SampleId:
        info.uses_sample_id = true;
        break;
    case IR::Opcode::IsHelperInvocation:
        info.uses_is_helper_invocation = true;
        break;
    case IR::Opcode::LaneId:
        info.uses_subgroup_invocation_id = true;
        break;
    case IR::Opcode::ShuffleIndex:
    case IR::Opcode::ShuffleUp:
    case IR::Opcode::ShuffleDown:
    case IR::Opcode::ShuffleButterfly:
        info.uses_subgroup_shuffles = true;
        break;
    case IR::Opcode::GetCbufU8:
    case IR::Opcode::GetCbufS8:
    case IR::Opcode::GetCbufU16:
    case IR::Opcode::GetCbufS16:
    case IR::Opcode::GetCbufU32:
    case IR::Opcode::GetCbufF32:
    case IR::Opcode::GetCbufU32x2: {
        const IR::Value index{inst.Arg(0)};
        const IR::Value offset{inst.Arg(1)};
        if (!index.IsImmediate()) {
            throw NotImplementedException("Constant buffer with non-immediate index");
        }
        AddConstantBufferDescriptor(info, index.U32(), 1);
        u32 element_size{};
        switch (inst.GetOpcode()) {
        case IR::Opcode::GetCbufU8:
        case IR::Opcode::GetCbufS8:
            info.used_constant_buffer_types |= IR::Type::U8;
            element_size = 1;
            break;
        case IR::Opcode::GetCbufU16:
        case IR::Opcode::GetCbufS16:
            info.used_constant_buffer_types |= IR::Type::U16;
            element_size = 2;
            break;
        case IR::Opcode::GetCbufU32:
            info.used_constant_buffer_types |= IR::Type::U32;
            element_size = 4;
            break;
        case IR::Opcode::GetCbufF32:
            info.used_constant_buffer_types |= IR::Type::F32;
            element_size = 4;
            break;
        case IR::Opcode::GetCbufU32x2:
            info.used_constant_buffer_types |= IR::Type::U32x2;
            element_size = 8;
            break;
        default:
            break;
        }
        u32& size{info.constant_buffer_used_sizes[index.U32()]};
        if (offset.IsImmediate()) {
            size = Common::AlignUp(std::max(size, offset.U32() + element_size), 16u);
        } else {
            size = 0x10'000;
        }
        break;
    }
    case IR::Opcode::BindlessImageSampleImplicitLod:
    case IR::Opcode::BindlessImageSampleExplicitLod:
    case IR::Opcode::BindlessImageSampleDrefImplicitLod:
    case IR::Opcode::BindlessImageSampleDrefExplicitLod:
    case IR::Opcode::BindlessImageGather:
    case IR::Opcode::BindlessImageGatherDref:
    case IR::Opcode::BindlessImageFetch:
    case IR::Opcode::BindlessImageQueryDimensions:
    case IR::Opcode::BindlessImageQueryLod:
    case IR::Opcode::BindlessImageGradient:
    case IR::Opcode::BoundImageSampleImplicitLod:
    case IR::Opcode::BoundImageSampleExplicitLod:
    case IR::Opcode::BoundImageSampleDrefImplicitLod:
    case IR::Opcode::BoundImageSampleDrefExplicitLod:
    case IR::Opcode::BoundImageGather:
    case IR::Opcode::BoundImageGatherDref:
    case IR::Opcode::BoundImageFetch:
    case IR::Opcode::BoundImageQueryDimensions:
    case IR::Opcode::BoundImageQueryLod:
    case IR::Opcode::BoundImageGradient:
    case IR::Opcode::ImageGather:
    case IR::Opcode::ImageGatherDref:
    case IR::Opcode::ImageFetch:
    case IR::Opcode::ImageQueryDimensions:
    case IR::Opcode::ImageGradient: {
        const TextureType type{inst.Flags<IR::TextureInstInfo>().type};
        info.uses_sampled_1d |= type == TextureType::Color1D || type == TextureType::ColorArray1D;
        info.uses_sparse_residency |=
            inst.GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp) != nullptr;
        break;
    }
    case IR::Opcode::ImageSampleImplicitLod:
    case IR::Opcode::ImageSampleExplicitLod:
    case IR::Opcode::ImageSampleDrefImplicitLod:
    case IR::Opcode::ImageSampleDrefExplicitLod:
    case IR::Opcode::ImageQueryLod: {
        const auto flags{inst.Flags<IR::TextureInstInfo>()};
        const TextureType type{flags.type};
        info.uses_sampled_1d |= type == TextureType::Color1D || type == TextureType::ColorArray1D;
        info.uses_shadow_lod |= flags.is_depth != 0;
        info.uses_sparse_residency |=
            inst.GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp) != nullptr;
        break;
    }
    case IR::Opcode::ImageRead: {
        const auto flags{inst.Flags<IR::TextureInstInfo>()};
        info.uses_typeless_image_reads |= flags.image_format == ImageFormat::Typeless;
        info.uses_sparse_residency |=
            inst.GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp) != nullptr;
        break;
    }
    case IR::Opcode::ImageWrite: {
        const auto flags{inst.Flags<IR::TextureInstInfo>()};
        info.uses_typeless_image_writes |= flags.image_format == ImageFormat::Typeless;
        info.uses_image_buffers |= flags.type == TextureType::Buffer;
        break;
    }
    case IR::Opcode::SubgroupEqMask:
    case IR::Opcode::SubgroupLtMask:
    case IR::Opcode::SubgroupLeMask:
    case IR::Opcode::SubgroupGtMask:
    case IR::Opcode::SubgroupGeMask:
        info.uses_subgroup_mask = true;
        break;
    case IR::Opcode::VoteAll:
    case IR::Opcode::VoteAny:
    case IR::Opcode::VoteEqual:
    case IR::Opcode::SubgroupBallot:
        info.uses_subgroup_vote = true;
        break;
    case IR::Opcode::FSwizzleAdd:
        info.uses_fswzadd = true;
        break;
    case IR::Opcode::DPdxFine:
    case IR::Opcode::DPdyFine:
    case IR::Opcode::DPdxCoarse:
    case IR::Opcode::DPdyCoarse:
        info.uses_derivatives = true;
        break;
    case IR::Opcode::LoadStorageU8:
    case IR::Opcode::LoadStorageS8:
    case IR::Opcode::WriteStorageU8:
    case IR::Opcode::WriteStorageS8:
        info.used_storage_buffer_types |= IR::Type::U8;
        break;
    case IR::Opcode::LoadStorageU16:
    case IR::Opcode::LoadStorageS16:
    case IR::Opcode::WriteStorageU16:
    case IR::Opcode::WriteStorageS16:
        info.used_storage_buffer_types |= IR::Type::U16;
        break;
    case IR::Opcode::LoadStorage32:
    case IR::Opcode::WriteStorage32:
    case IR::Opcode::StorageAtomicIAdd32:
    case IR::Opcode::StorageAtomicUMin32:
    case IR::Opcode::StorageAtomicUMax32:
    case IR::Opcode::StorageAtomicAnd32:
    case IR::Opcode::StorageAtomicOr32:
    case IR::Opcode::StorageAtomicXor32:
    case IR::Opcode::StorageAtomicExchange32:
        info.used_storage_buffer_types |= IR::Type::U32;
        break;
    case IR::Opcode::LoadStorage64:
    case IR::Opcode::WriteStorage64:
        info.used_storage_buffer_types |= IR::Type::U32x2;
        break;
    case IR::Opcode::LoadStorage128:
    case IR::Opcode::WriteStorage128:
        info.used_storage_buffer_types |= IR::Type::U32x4;
        break;
    case IR::Opcode::SharedAtomicSMin32:
        info.uses_atomic_s32_min = true;
        break;
    case IR::Opcode::SharedAtomicSMax32:
        info.uses_atomic_s32_max = true;
        break;
    case IR::Opcode::SharedAtomicInc32:
        info.uses_shared_increment = true;
        break;
    case IR::Opcode::SharedAtomicDec32:
        info.uses_shared_decrement = true;
        break;
    case IR::Opcode::SharedAtomicExchange64:
        info.uses_int64_bit_atomics = true;
        break;
    case IR::Opcode::GlobalAtomicInc32:
    case IR::Opcode::StorageAtomicInc32:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_global_increment = true;
        break;
    case IR::Opcode::GlobalAtomicDec32:
    case IR::Opcode::StorageAtomicDec32:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_global_decrement = true;
        break;
    case IR::Opcode::GlobalAtomicAddF32:
    case IR::Opcode::StorageAtomicAddF32:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_f32_add = true;
        break;
    case IR::Opcode::GlobalAtomicAddF16x2:
    case IR::Opcode::StorageAtomicAddF16x2:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_f16x2_add = true;
        break;
    case IR::Opcode::GlobalAtomicAddF32x2:
    case IR::Opcode::StorageAtomicAddF32x2:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_f32x2_add = true;
        break;
    case IR::Opcode::GlobalAtomicMinF16x2:
    case IR::Opcode::StorageAtomicMinF16x2:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_f16x2_min = true;
        break;
    case IR::Opcode::GlobalAtomicMinF32x2:
    case IR::Opcode::StorageAtomicMinF32x2:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_f32x2_min = true;
        break;
    case IR::Opcode::GlobalAtomicMaxF16x2:
    case IR::Opcode::StorageAtomicMaxF16x2:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_f16x2_max = true;
        break;
    case IR::Opcode::GlobalAtomicMaxF32x2:
    case IR::Opcode::StorageAtomicMaxF32x2:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_f32x2_max = true;
        break;
    case IR::Opcode::StorageAtomicSMin32:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_s32_min = true;
        break;
    case IR::Opcode::StorageAtomicSMax32:
        info.used_storage_buffer_types |= IR::Type::U32;
        info.uses_atomic_s32_max = true;
        break;
    case IR::Opcode::GlobalAtomicIAdd64:
    case IR::Opcode::GlobalAtomicSMin64:
    case IR::Opcode::GlobalAtomicUMin64:
    case IR::Opcode::GlobalAtomicSMax64:
    case IR::Opcode::GlobalAtomicUMax64:
    case IR::Opcode::GlobalAtomicAnd64:
    case IR::Opcode::GlobalAtomicOr64:
    case IR::Opcode::GlobalAtomicXor64:
    case IR::Opcode::GlobalAtomicExchange64:
    case IR::Opcode::StorageAtomicIAdd64:
    case IR::Opcode::StorageAtomicSMin64:
    case IR::Opcode::StorageAtomicUMin64:
    case IR::Opcode::StorageAtomicSMax64:
    case IR::Opcode::StorageAtomicUMax64:
    case IR::Opcode::StorageAtomicAnd64:
    case IR::Opcode::StorageAtomicOr64:
    case IR::Opcode::StorageAtomicXor64:
        info.used_storage_buffer_types |= IR::Type::U64;
        info.uses_int64_bit_atomics = true;
        break;
    case IR::Opcode::BindlessImageAtomicIAdd32:
    case IR::Opcode::BindlessImageAtomicSMin32:
    case IR::Opcode::BindlessImageAtomicUMin32:
    case IR::Opcode::BindlessImageAtomicSMax32:
    case IR::Opcode::BindlessImageAtomicUMax32:
    case IR::Opcode::BindlessImageAtomicInc32:
    case IR::Opcode::BindlessImageAtomicDec32:
    case IR::Opcode::BindlessImageAtomicAnd32:
    case IR::Opcode::BindlessImageAtomicOr32:
    case IR::Opcode::BindlessImageAtomicXor32:
    case IR::Opcode::BindlessImageAtomicExchange32:
    case IR::Opcode::BoundImageAtomicIAdd32:
    case IR::Opcode::BoundImageAtomicSMin32:
    case IR::Opcode::BoundImageAtomicUMin32:
    case IR::Opcode::BoundImageAtomicSMax32:
    case IR::Opcode::BoundImageAtomicUMax32:
    case IR::Opcode::BoundImageAtomicInc32:
    case IR::Opcode::BoundImageAtomicDec32:
    case IR::Opcode::BoundImageAtomicAnd32:
    case IR::Opcode::BoundImageAtomicOr32:
    case IR::Opcode::BoundImageAtomicXor32:
    case IR::Opcode::BoundImageAtomicExchange32:
    case IR::Opcode::ImageAtomicIAdd32:
    case IR::Opcode::ImageAtomicSMin32:
    case IR::Opcode::ImageAtomicUMin32:
    case IR::Opcode::ImageAtomicSMax32:
    case IR::Opcode::ImageAtomicUMax32:
    case IR::Opcode::ImageAtomicInc32:
    case IR::Opcode::ImageAtomicDec32:
    case IR::Opcode::ImageAtomicAnd32:
    case IR::Opcode::ImageAtomicOr32:
    case IR::Opcode::ImageAtomicXor32:
    case IR::Opcode::ImageAtomicExchange32:
        info.uses_atomic_image_u32 = true;
        break;
    default:
        break;
    }
}

void VisitFpModifiers(Info& info, IR::Inst& inst) {
    switch (inst.GetOpcode()) {
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

void VisitCbufs(Info& info, IR::Inst& inst) {
    switch (inst.GetOpcode()) {
    case IR::Opcode::GetCbufU8:
    case IR::Opcode::GetCbufS8:
    case IR::Opcode::GetCbufU16:
    case IR::Opcode::GetCbufS16:
    case IR::Opcode::GetCbufU32:
    case IR::Opcode::GetCbufF32:
    case IR::Opcode::GetCbufU32x2: {
        CheckCBufNVN(info, inst);
        break;
    }
    default:
        break;
    }
}

void Visit(Info& info, IR::Inst& inst) {
    VisitUsages(info, inst);
    VisitFpModifiers(info, inst);
    VisitCbufs(info, inst);
}

void GatherInfoFromHeader(Environment& env, Info& info) {
    Stage stage{env.ShaderStage()};
    if (stage == Stage::Compute) {
        return;
    }
    const auto& header{env.SPH()};
    if (stage == Stage::Fragment) {
        if (!info.loads_indexed_attributes) {
            return;
        }
        for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
            const size_t offset{static_cast<size_t>(IR::Attribute::Generic0X) + index * 4};
            const auto vector{header.ps.imap_generic_vector[index]};
            info.loads.mask[offset + 0] = vector.x != PixelImap::Unused;
            info.loads.mask[offset + 1] = vector.y != PixelImap::Unused;
            info.loads.mask[offset + 2] = vector.z != PixelImap::Unused;
            info.loads.mask[offset + 3] = vector.w != PixelImap::Unused;
        }
        return;
    }
    if (info.loads_indexed_attributes) {
        for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
            const IR::Attribute attribute{IR::Attribute::Generic0X + index * 4};
            const auto mask = header.vtg.InputGeneric(index);
            for (size_t i = 0; i < 4; ++i) {
                info.loads.Set(attribute + i, mask[i]);
            }
        }
        for (size_t index = 0; index < 8; ++index) {
            const u16 mask{header.vtg.clip_distances};
            info.loads.Set(IR::Attribute::ClipDistance0 + index, ((mask >> index) & 1) != 0);
        }
        info.loads.Set(IR::Attribute::PrimitiveId, header.vtg.imap_systemb.primitive_array_id != 0);
        info.loads.Set(IR::Attribute::Layer, header.vtg.imap_systemb.rt_array_index != 0);
        info.loads.Set(IR::Attribute::ViewportIndex, header.vtg.imap_systemb.viewport_index != 0);
        info.loads.Set(IR::Attribute::PointSize, header.vtg.imap_systemb.point_size != 0);
        info.loads.Set(IR::Attribute::PositionX, header.vtg.imap_systemb.position_x != 0);
        info.loads.Set(IR::Attribute::PositionY, header.vtg.imap_systemb.position_y != 0);
        info.loads.Set(IR::Attribute::PositionZ, header.vtg.imap_systemb.position_z != 0);
        info.loads.Set(IR::Attribute::PositionW, header.vtg.imap_systemb.position_w != 0);
        info.loads.Set(IR::Attribute::PointSpriteS, header.vtg.point_sprite_s != 0);
        info.loads.Set(IR::Attribute::PointSpriteT, header.vtg.point_sprite_t != 0);
        info.loads.Set(IR::Attribute::FogCoordinate, header.vtg.fog_coordinate != 0);
        info.loads.Set(IR::Attribute::TessellationEvaluationPointU,
                       header.vtg.tessellation_eval_point_u != 0);
        info.loads.Set(IR::Attribute::TessellationEvaluationPointV,
                       header.vtg.tessellation_eval_point_v != 0);
        info.loads.Set(IR::Attribute::InstanceId, header.vtg.instance_id != 0);
        info.loads.Set(IR::Attribute::VertexId, header.vtg.vertex_id != 0);
        // TODO: Legacy varyings
    }
    if (info.stores_indexed_attributes) {
        for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
            const IR::Attribute attribute{IR::Attribute::Generic0X + index * 4};
            const auto mask{header.vtg.OutputGeneric(index)};
            for (size_t i = 0; i < 4; ++i) {
                info.stores.Set(attribute + i, mask[i]);
            }
        }
        for (size_t index = 0; index < 8; ++index) {
            const u16 mask{header.vtg.omap_systemc.clip_distances};
            info.stores.Set(IR::Attribute::ClipDistance0 + index, ((mask >> index) & 1) != 0);
        }
        info.stores.Set(IR::Attribute::PrimitiveId,
                        header.vtg.omap_systemb.primitive_array_id != 0);
        info.stores.Set(IR::Attribute::Layer, header.vtg.omap_systemb.rt_array_index != 0);
        info.stores.Set(IR::Attribute::ViewportIndex, header.vtg.omap_systemb.viewport_index != 0);
        info.stores.Set(IR::Attribute::PointSize, header.vtg.omap_systemb.point_size != 0);
        info.stores.Set(IR::Attribute::PositionX, header.vtg.omap_systemb.position_x != 0);
        info.stores.Set(IR::Attribute::PositionY, header.vtg.omap_systemb.position_y != 0);
        info.stores.Set(IR::Attribute::PositionZ, header.vtg.omap_systemb.position_z != 0);
        info.stores.Set(IR::Attribute::PositionW, header.vtg.omap_systemb.position_w != 0);
        info.stores.Set(IR::Attribute::PointSpriteS, header.vtg.omap_systemc.point_sprite_s != 0);
        info.stores.Set(IR::Attribute::PointSpriteT, header.vtg.omap_systemc.point_sprite_t != 0);
        info.stores.Set(IR::Attribute::FogCoordinate, header.vtg.omap_systemc.fog_coordinate != 0);
        info.stores.Set(IR::Attribute::TessellationEvaluationPointU,
                        header.vtg.omap_systemc.tessellation_eval_point_u != 0);
        info.stores.Set(IR::Attribute::TessellationEvaluationPointV,
                        header.vtg.omap_systemc.tessellation_eval_point_v != 0);
        info.stores.Set(IR::Attribute::InstanceId, header.vtg.omap_systemc.instance_id != 0);
        info.stores.Set(IR::Attribute::VertexId, header.vtg.omap_systemc.vertex_id != 0);
        // TODO: Legacy varyings
    }
}
} // Anonymous namespace

void CollectShaderInfoPass(Environment& env, IR::Program& program) {
    Info& info{program.info};
    const u32 base{[&] {
        switch (program.stage) {
        case Stage::VertexA:
        case Stage::VertexB:
            return 0x110u;
        case Stage::TessellationControl:
            return 0x210u;
        case Stage::TessellationEval:
            return 0x310u;
        case Stage::Geometry:
            return 0x410u;
        case Stage::Fragment:
            return 0x510u;
        case Stage::Compute:
            return 0x310u;
        }
        throw InvalidArgument("Invalid stage {}", program.stage);
    }()};
    info.nvn_buffer_base = base;

    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            Visit(info, inst);
        }
    }
    GatherInfoFromHeader(env, info);
}

} // namespace Shader::Optimization
