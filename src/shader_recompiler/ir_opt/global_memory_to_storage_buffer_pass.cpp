// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <compare>
#include <optional>
#include <ranges>

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>

#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/microinstruction.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Optimization {
namespace {
/// Address in constant buffers to the storage buffer descriptor
struct StorageBufferAddr {
    auto operator<=>(const StorageBufferAddr&) const noexcept = default;

    u32 index;
    u32 offset;
};

/// Block iterator to a global memory instruction and the storage buffer it uses
struct StorageInst {
    StorageBufferAddr storage_buffer;
    IR::Block::iterator inst;
};

/// Bias towards a certain range of constant buffers when looking for storage buffers
struct Bias {
    u32 index;
    u32 offset_begin;
    u32 offset_end;
};

using StorageBufferSet =
    boost::container::flat_set<StorageBufferAddr, std::less<StorageBufferAddr>,
                               boost::container::small_vector<StorageBufferAddr, 16>>;
using StorageInstVector = boost::container::small_vector<StorageInst, 32>;

/// Returns true when the instruction is a global memory instruction
bool IsGlobalMemory(const IR::Inst& inst) {
    switch (inst.Opcode()) {
    case IR::Opcode::LoadGlobalS8:
    case IR::Opcode::LoadGlobalU8:
    case IR::Opcode::LoadGlobalS16:
    case IR::Opcode::LoadGlobalU16:
    case IR::Opcode::LoadGlobal32:
    case IR::Opcode::LoadGlobal64:
    case IR::Opcode::LoadGlobal128:
    case IR::Opcode::WriteGlobalS8:
    case IR::Opcode::WriteGlobalU8:
    case IR::Opcode::WriteGlobalS16:
    case IR::Opcode::WriteGlobalU16:
    case IR::Opcode::WriteGlobal32:
    case IR::Opcode::WriteGlobal64:
    case IR::Opcode::WriteGlobal128:
        return true;
    default:
        return false;
    }
}

/// Converts a global memory opcode to its storage buffer equivalent
IR::Opcode GlobalToStorage(IR::Opcode opcode) {
    switch (opcode) {
    case IR::Opcode::LoadGlobalS8:
        return IR::Opcode::LoadStorageS8;
    case IR::Opcode::LoadGlobalU8:
        return IR::Opcode::LoadStorageU8;
    case IR::Opcode::LoadGlobalS16:
        return IR::Opcode::LoadStorageS16;
    case IR::Opcode::LoadGlobalU16:
        return IR::Opcode::LoadStorageU16;
    case IR::Opcode::LoadGlobal32:
        return IR::Opcode::LoadStorage32;
    case IR::Opcode::LoadGlobal64:
        return IR::Opcode::LoadStorage64;
    case IR::Opcode::LoadGlobal128:
        return IR::Opcode::LoadStorage128;
    case IR::Opcode::WriteGlobalS8:
        return IR::Opcode::WriteStorageS8;
    case IR::Opcode::WriteGlobalU8:
        return IR::Opcode::WriteStorageU8;
    case IR::Opcode::WriteGlobalS16:
        return IR::Opcode::WriteStorageS16;
    case IR::Opcode::WriteGlobalU16:
        return IR::Opcode::WriteStorageU16;
    case IR::Opcode::WriteGlobal32:
        return IR::Opcode::WriteStorage32;
    case IR::Opcode::WriteGlobal64:
        return IR::Opcode::WriteStorage64;
    case IR::Opcode::WriteGlobal128:
        return IR::Opcode::WriteStorage128;
    default:
        throw InvalidArgument("Invalid global memory opcode {}", opcode);
    }
}

/// Returns true when a storage buffer address satisfies a bias
bool MeetsBias(const StorageBufferAddr& storage_buffer, const Bias& bias) noexcept {
    return storage_buffer.index == bias.index && storage_buffer.offset >= bias.offset_begin &&
           storage_buffer.offset < bias.offset_end;
}

/// Discards a global memory operation, reads return zero and writes are ignored
void DiscardGlobalMemory(IR::Block& block, IR::Block::iterator inst) {
    const IR::Value zero{u32{0}};
    switch (inst->Opcode()) {
    case IR::Opcode::LoadGlobalS8:
    case IR::Opcode::LoadGlobalU8:
    case IR::Opcode::LoadGlobalS16:
    case IR::Opcode::LoadGlobalU16:
    case IR::Opcode::LoadGlobal32:
        inst->ReplaceUsesWith(zero);
        break;
    case IR::Opcode::LoadGlobal64:
        inst->ReplaceUsesWith(IR::Value{
            &*block.PrependNewInst(inst, IR::Opcode::CompositeConstructU32x2, {zero, zero})});
        break;
    case IR::Opcode::LoadGlobal128:
        inst->ReplaceUsesWith(IR::Value{&*block.PrependNewInst(
            inst, IR::Opcode::CompositeConstructU32x4, {zero, zero, zero, zero})});
        break;
    case IR::Opcode::WriteGlobalS8:
    case IR::Opcode::WriteGlobalU8:
    case IR::Opcode::WriteGlobalS16:
    case IR::Opcode::WriteGlobalU16:
    case IR::Opcode::WriteGlobal32:
    case IR::Opcode::WriteGlobal64:
    case IR::Opcode::WriteGlobal128:
        inst->Invalidate();
        break;
    default:
        throw LogicError("Invalid opcode to discard its global memory operation {}",
                         inst->Opcode());
    }
}

/// Recursively tries to track the storage buffer address used by a global memory instruction
std::optional<StorageBufferAddr> Track(const IR::Value& value, const Bias* bias) {
    if (value.IsImmediate()) {
        // Immediates can't be a storage buffer
        return std::nullopt;
    }
    const IR::Inst* const inst{value.InstRecursive()};
    if (inst->Opcode() == IR::Opcode::GetCbuf) {
        const IR::Value index{inst->Arg(0)};
        const IR::Value offset{inst->Arg(1)};
        if (!index.IsImmediate()) {
            // Definitely not a storage buffer if it's read from a non-immediate index
            return std::nullopt;
        }
        if (!offset.IsImmediate()) {
            // TODO: Support SSBO arrays
            return std::nullopt;
        }
        const StorageBufferAddr storage_buffer{
            .index{index.U32()},
            .offset{offset.U32()},
        };
        if (bias && !MeetsBias(storage_buffer, *bias)) {
            // We have to blacklist some addresses in case we wrongly point to them
            return std::nullopt;
        }
        return storage_buffer;
    }
    // Reversed loops are more likely to find the right result
    for (size_t arg = inst->NumArgs(); arg--;) {
        if (const std::optional storage_buffer{Track(inst->Arg(arg), bias)}) {
            return *storage_buffer;
        }
    }
    return std::nullopt;
}

/// Collects the storage buffer used by a global memory instruction and the instruction itself
void CollectStorageBuffers(IR::Block& block, IR::Block::iterator inst,
                           StorageBufferSet& storage_buffer_set, StorageInstVector& to_replace) {
    // NVN puts storage buffers in a specific range, we have to bias towards these addresses to
    // avoid getting false positives
    static constexpr Bias nvn_bias{
        .index{0},
        .offset_begin{0x110},
        .offset_end{0x610},
    };
    // First try to find storage buffers in the NVN address
    const IR::U64 addr{inst->Arg(0)};
    std::optional<StorageBufferAddr> storage_buffer{Track(addr, &nvn_bias)};
    if (!storage_buffer) {
        // If it fails, track without a bias
        storage_buffer = Track(addr, nullptr);
        if (!storage_buffer) {
            // If that also failed, drop the global memory usage
            DiscardGlobalMemory(block, inst);
        }
    }
    // Collect storage buffer and the instruction
    storage_buffer_set.insert(*storage_buffer);
    to_replace.push_back(StorageInst{
        .storage_buffer{*storage_buffer},
        .inst{inst},
    });
}

/// Tries to track the first 32-bits of a global memory instruction
std::optional<IR::U32> TrackLowAddress(IR::IREmitter& ir, IR::Inst* inst) {
    // The first argument is the low level GPU pointer to the global memory instruction
    const IR::U64 addr{inst->Arg(0)};
    if (addr.IsImmediate()) {
        // Not much we can do if it's an immediate
        return std::nullopt;
    }
    // This address is expected to either be a PackUint2x32 or a IAdd64
    IR::Inst* addr_inst{addr.InstRecursive()};
    s32 imm_offset{0};
    if (addr_inst->Opcode() == IR::Opcode::IAdd64) {
        // If it's an IAdd64, get the immediate offset it is applying and grab the address
        // instruction. This expects for the instruction to be canonicalized having the address on
        // the first argument and the immediate offset on the second one.
        const IR::U64 imm_offset_value{addr_inst->Arg(1)};
        if (!imm_offset_value.IsImmediate()) {
            return std::nullopt;
        }
        imm_offset = static_cast<s32>(static_cast<s64>(imm_offset_value.U64()));
        const IR::U64 iadd_addr{addr_inst->Arg(0)};
        if (iadd_addr.IsImmediate()) {
            return std::nullopt;
        }
        addr_inst = iadd_addr.Inst();
    }
    // With IAdd64 handled, now PackUint2x32 is expected without exceptions
    if (addr_inst->Opcode() != IR::Opcode::PackUint2x32) {
        return std::nullopt;
    }
    // PackUint2x32 is expected to be generated from a vector
    const IR::Value vector{addr_inst->Arg(0)};
    if (vector.IsImmediate()) {
        return std::nullopt;
    }
    // This vector is expected to be a CompositeConstructU32x2
    IR::Inst* const vector_inst{vector.InstRecursive()};
    if (vector_inst->Opcode() != IR::Opcode::CompositeConstructU32x2) {
        return std::nullopt;
    }
    // Grab the first argument from the CompositeConstructU32x2, this is the low address.
    // Re-apply the offset in case we found one.
    const IR::U32 low_addr{vector_inst->Arg(0)};
    return imm_offset != 0 ? IR::U32{ir.IAdd(low_addr, ir.Imm32(imm_offset))} : low_addr;
}

/// Returns the offset in indices (not bytes) for an equivalent storage instruction
IR::U32 StorageOffset(IR::Block& block, IR::Block::iterator inst, StorageBufferAddr buffer) {
    IR::IREmitter ir{block, inst};
    IR::U32 offset;
    if (const std::optional<IR::U32> low_addr{TrackLowAddress(ir, &*inst)}) {
        offset = *low_addr;
    } else {
        offset = ir.ConvertU(32, IR::U64{inst->Arg(0)});
    }
    // Subtract the least significant 32 bits from the guest offset. The result is the storage
    // buffer offset in bytes.
    const IR::U32 low_cbuf{ir.GetCbuf(ir.Imm32(buffer.index), ir.Imm32(buffer.offset))};
    return ir.ISub(offset, low_cbuf);
}

/// Replace a global memory load instruction with its storage buffer equivalent
void ReplaceLoad(IR::Block& block, IR::Block::iterator inst, const IR::U32& storage_index,
                 const IR::U32& offset) {
    const IR::Opcode new_opcode{GlobalToStorage(inst->Opcode())};
    const IR::Value value{&*block.PrependNewInst(inst, new_opcode, {storage_index, offset})};
    inst->ReplaceUsesWith(value);
}

/// Replace a global memory write instruction with its storage buffer equivalent
void ReplaceWrite(IR::Block& block, IR::Block::iterator inst, const IR::U32& storage_index,
                  const IR::U32& offset) {
    const IR::Opcode new_opcode{GlobalToStorage(inst->Opcode())};
    block.PrependNewInst(inst, new_opcode, {storage_index, offset, inst->Arg(1)});
    inst->Invalidate();
}

/// Replace a global memory instruction with its storage buffer equivalent
void Replace(IR::Block& block, IR::Block::iterator inst, const IR::U32& storage_index,
             const IR::U32& offset) {
    switch (inst->Opcode()) {
    case IR::Opcode::LoadGlobalS8:
    case IR::Opcode::LoadGlobalU8:
    case IR::Opcode::LoadGlobalS16:
    case IR::Opcode::LoadGlobalU16:
    case IR::Opcode::LoadGlobal32:
    case IR::Opcode::LoadGlobal64:
    case IR::Opcode::LoadGlobal128:
        return ReplaceLoad(block, inst, storage_index, offset);
    case IR::Opcode::WriteGlobalS8:
    case IR::Opcode::WriteGlobalU8:
    case IR::Opcode::WriteGlobalS16:
    case IR::Opcode::WriteGlobalU16:
    case IR::Opcode::WriteGlobal32:
    case IR::Opcode::WriteGlobal64:
    case IR::Opcode::WriteGlobal128:
        return ReplaceWrite(block, inst, storage_index, offset);
    default:
        throw InvalidArgument("Invalid global memory opcode {}", inst->Opcode());
    }
}
} // Anonymous namespace

void GlobalMemoryToStorageBufferPass(IR::Block& block) {
    StorageBufferSet storage_buffers;
    StorageInstVector to_replace;

    for (IR::Block::iterator inst{block.begin()}; inst != block.end(); ++inst) {
        if (!IsGlobalMemory(*inst)) {
            continue;
        }
        CollectStorageBuffers(block, inst, storage_buffers, to_replace);
    }
    for (const auto [storage_buffer, inst] : to_replace) {
        const auto it{storage_buffers.find(storage_buffer)};
        const IR::U32 storage_index{IR::Value{static_cast<u32>(storage_buffers.index_of(it))}};
        const IR::U32 offset{StorageOffset(block, inst, storage_buffer)};
        Replace(block, inst, storage_index, offset);
    }
}

} // namespace Shader::Optimization
