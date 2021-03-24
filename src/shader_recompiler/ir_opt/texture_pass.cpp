// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <optional>

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/ir_opt/passes.h"
#include "shader_recompiler/shader_info.h"

namespace Shader::Optimization {
namespace {
struct ConstBufferAddr {
    u32 index;
    u32 offset;
};

struct TextureInst {
    ConstBufferAddr cbuf;
    IR::Inst* inst;
    IR::Block* block;
};

using TextureInstVector = boost::container::small_vector<TextureInst, 24>;

using VisitedBlocks = boost::container::flat_set<IR::Block*, std::less<IR::Block*>,
                                                 boost::container::small_vector<IR::Block*, 2>>;

IR::Opcode IndexedInstruction(const IR::Inst& inst) {
    switch (inst.Opcode()) {
    case IR::Opcode::BindlessImageSampleImplicitLod:
    case IR::Opcode::BoundImageSampleImplicitLod:
        return IR::Opcode::ImageSampleImplicitLod;
    case IR::Opcode::BoundImageSampleExplicitLod:
    case IR::Opcode::BindlessImageSampleExplicitLod:
        return IR::Opcode::ImageSampleExplicitLod;
    case IR::Opcode::BoundImageSampleDrefImplicitLod:
    case IR::Opcode::BindlessImageSampleDrefImplicitLod:
        return IR::Opcode::ImageSampleDrefImplicitLod;
    case IR::Opcode::BoundImageSampleDrefExplicitLod:
    case IR::Opcode::BindlessImageSampleDrefExplicitLod:
        return IR::Opcode::ImageSampleDrefExplicitLod;
    default:
        return IR::Opcode::Void;
    }
}

bool IsBindless(const IR::Inst& inst) {
    switch (inst.Opcode()) {
    case IR::Opcode::BindlessImageSampleImplicitLod:
    case IR::Opcode::BindlessImageSampleExplicitLod:
    case IR::Opcode::BindlessImageSampleDrefImplicitLod:
    case IR::Opcode::BindlessImageSampleDrefExplicitLod:
        return true;
    case IR::Opcode::BoundImageSampleImplicitLod:
    case IR::Opcode::BoundImageSampleExplicitLod:
    case IR::Opcode::BoundImageSampleDrefImplicitLod:
    case IR::Opcode::BoundImageSampleDrefExplicitLod:
        return false;
    default:
        throw InvalidArgument("Invalid opcode {}", inst.Opcode());
    }
}

bool IsTextureInstruction(const IR::Inst& inst) {
    return IndexedInstruction(inst) != IR::Opcode::Void;
}

std::optional<ConstBufferAddr> Track(IR::Block* block, const IR::Value& value,
                                     VisitedBlocks& visited) {
    if (value.IsImmediate()) {
        // Immediates can't be a storage buffer
        return std::nullopt;
    }
    const IR::Inst* const inst{value.InstRecursive()};
    if (inst->Opcode() == IR::Opcode::GetCbufU32) {
        const IR::Value index{inst->Arg(0)};
        const IR::Value offset{inst->Arg(1)};
        if (!index.IsImmediate()) {
            // Reading a bindless texture from variable indices is valid
            // but not supported here at the moment
            return std::nullopt;
        }
        if (!offset.IsImmediate()) {
            // TODO: Support arrays of textures
            return std::nullopt;
        }
        return ConstBufferAddr{
            .index{index.U32()},
            .offset{offset.U32()},
        };
    }
    // Reversed loops are more likely to find the right result
    for (size_t arg = inst->NumArgs(); arg--;) {
        IR::Block* inst_block{block};
        if (inst->Opcode() == IR::Opcode::Phi) {
            // If we are going through a phi node, mark the current block as visited
            visited.insert(block);
            // and skip already visited blocks to avoid looping forever
            IR::Block* const phi_block{inst->PhiBlock(arg)};
            if (visited.contains(phi_block)) {
                // Already visited, skip
                continue;
            }
            inst_block = phi_block;
        }
        const std::optional storage_buffer{Track(inst_block, inst->Arg(arg), visited)};
        if (storage_buffer) {
            return *storage_buffer;
        }
    }
    return std::nullopt;
}

TextureInst MakeInst(Environment& env, IR::Block* block, IR::Inst& inst) {
    ConstBufferAddr addr;
    if (IsBindless(inst)) {
        VisitedBlocks visited;
        const std::optional<ConstBufferAddr> track_addr{Track(block, inst.Arg(0), visited)};
        if (!track_addr) {
            throw NotImplementedException("Failed to track bindless texture constant buffer");
        }
        addr = *track_addr;
    } else {
        addr = ConstBufferAddr{
            .index{env.TextureBoundBuffer()},
            .offset{inst.Arg(0).U32()},
        };
    }
    return TextureInst{
        .cbuf{addr},
        .inst{&inst},
        .block{block},
    };
}

class Descriptors {
public:
    explicit Descriptors(TextureDescriptors& descriptors_) : descriptors{descriptors_} {}

    u32 Add(const TextureDescriptor& descriptor) {
        // TODO: Handle arrays
        auto it{std::ranges::find_if(descriptors, [&descriptor](const TextureDescriptor& existing) {
            return descriptor.cbuf_index == existing.cbuf_index &&
                   descriptor.cbuf_offset == existing.cbuf_offset &&
                   descriptor.type == existing.type;
        })};
        if (it != descriptors.end()) {
            return static_cast<u32>(std::distance(descriptors.begin(), it));
        }
        descriptors.push_back(descriptor);
        return static_cast<u32>(descriptors.size()) - 1;
    }

private:
    TextureDescriptors& descriptors;
};
} // Anonymous namespace

void TexturePass(Environment& env, IR::Program& program) {
    TextureInstVector to_replace;
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            if (!IsTextureInstruction(inst)) {
                continue;
            }
            to_replace.push_back(MakeInst(env, block, inst));
        }
    }
    // Sort instructions to visit textures by constant buffer index, then by offset
    std::ranges::sort(to_replace, [](const auto& lhs, const auto& rhs) {
        return lhs.cbuf.offset < rhs.cbuf.offset;
    });
    std::stable_sort(to_replace.begin(), to_replace.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.cbuf.index < rhs.cbuf.index;
    });
    Descriptors descriptors{program.info.texture_descriptors};
    for (TextureInst& texture_inst : to_replace) {
        // TODO: Handle arrays
        IR::Inst* const inst{texture_inst.inst};
        const u32 index{descriptors.Add(TextureDescriptor{
            .type{inst->Flags<IR::TextureInstInfo>().type},
            .cbuf_index{texture_inst.cbuf.index},
            .cbuf_offset{texture_inst.cbuf.offset},
            .count{1},
        })};
        inst->ReplaceOpcode(IndexedInstruction(*inst));
        inst->SetArg(0, IR::Value{index});
    }
}

} // namespace Shader::Optimization
