// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <optional>

#include <boost/container/small_vector.hpp>

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/breadth_first_search.h"
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
    case IR::Opcode::BindlessImageGather:
    case IR::Opcode::BoundImageGather:
        return IR::Opcode::ImageGather;
    case IR::Opcode::BindlessImageGatherDref:
    case IR::Opcode::BoundImageGatherDref:
        return IR::Opcode::ImageGatherDref;
    case IR::Opcode::BindlessImageFetch:
    case IR::Opcode::BoundImageFetch:
        return IR::Opcode::ImageFetch;
    case IR::Opcode::BoundImageQueryDimensions:
    case IR::Opcode::BindlessImageQueryDimensions:
        return IR::Opcode::ImageQueryDimensions;
    case IR::Opcode::BoundImageQueryLod:
    case IR::Opcode::BindlessImageQueryLod:
        return IR::Opcode::ImageQueryLod;
    case IR::Opcode::BoundImageGradient:
    case IR::Opcode::BindlessImageGradient:
        return IR::Opcode::ImageGradient;
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
    case IR::Opcode::BindlessImageGather:
    case IR::Opcode::BindlessImageGatherDref:
    case IR::Opcode::BindlessImageFetch:
    case IR::Opcode::BindlessImageQueryDimensions:
    case IR::Opcode::BindlessImageQueryLod:
    case IR::Opcode::BindlessImageGradient:
        return true;
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
        return false;
    default:
        throw InvalidArgument("Invalid opcode {}", inst.Opcode());
    }
}

bool IsTextureInstruction(const IR::Inst& inst) {
    return IndexedInstruction(inst) != IR::Opcode::Void;
}

std::optional<ConstBufferAddr> TryGetConstBuffer(const IR::Inst* inst) {
    if (inst->Opcode() != IR::Opcode::GetCbufU32) {
        return std::nullopt;
    }
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

std::optional<ConstBufferAddr> Track(const IR::Value& value) {
    return IR::BreadthFirstSearch(value, TryGetConstBuffer);
}

TextureInst MakeInst(Environment& env, IR::Block* block, IR::Inst& inst) {
    ConstBufferAddr addr;
    if (IsBindless(inst)) {
        const std::optional<ConstBufferAddr> track_addr{Track(inst.Arg(0))};
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
        inst->ReplaceOpcode(IndexedInstruction(*inst));

        const auto& cbuf{texture_inst.cbuf};
        auto flags{inst->Flags<IR::TextureInstInfo>()};
        if (inst->Opcode() == IR::Opcode::ImageQueryDimensions) {
            flags.type.Assign(env.ReadTextureType(cbuf.index, cbuf.offset));
            inst->SetFlags(flags);
        }
        const u32 index{descriptors.Add(TextureDescriptor{
            .type{flags.type},
            .cbuf_index{cbuf.index},
            .cbuf_offset{cbuf.offset},
            .count{1},
        })};
        inst->SetArg(0, IR::Value{index});
    }
}

} // namespace Shader::Optimization
