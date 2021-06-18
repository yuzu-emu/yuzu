// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <bit>
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
    u32 secondary_index;
    u32 secondary_offset;
    IR::U32 dynamic_offset;
    u32 count;
    bool has_secondary;
};

struct TextureInst {
    ConstBufferAddr cbuf;
    IR::Inst* inst;
    IR::Block* block;
};

using TextureInstVector = boost::container::small_vector<TextureInst, 24>;

constexpr u32 DESCRIPTOR_SIZE = 8;
constexpr u32 DESCRIPTOR_SIZE_SHIFT = static_cast<u32>(std::countr_zero(DESCRIPTOR_SIZE));

IR::Opcode IndexedInstruction(const IR::Inst& inst) {
    switch (inst.GetOpcode()) {
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
    case IR::Opcode::BoundImageRead:
    case IR::Opcode::BindlessImageRead:
        return IR::Opcode::ImageRead;
    case IR::Opcode::BoundImageWrite:
    case IR::Opcode::BindlessImageWrite:
        return IR::Opcode::ImageWrite;
    case IR::Opcode::BoundImageAtomicIAdd32:
    case IR::Opcode::BindlessImageAtomicIAdd32:
        return IR::Opcode::ImageAtomicIAdd32;
    case IR::Opcode::BoundImageAtomicSMin32:
    case IR::Opcode::BindlessImageAtomicSMin32:
        return IR::Opcode::ImageAtomicSMin32;
    case IR::Opcode::BoundImageAtomicUMin32:
    case IR::Opcode::BindlessImageAtomicUMin32:
        return IR::Opcode::ImageAtomicUMin32;
    case IR::Opcode::BoundImageAtomicSMax32:
    case IR::Opcode::BindlessImageAtomicSMax32:
        return IR::Opcode::ImageAtomicSMax32;
    case IR::Opcode::BoundImageAtomicUMax32:
    case IR::Opcode::BindlessImageAtomicUMax32:
        return IR::Opcode::ImageAtomicUMax32;
    case IR::Opcode::BoundImageAtomicInc32:
    case IR::Opcode::BindlessImageAtomicInc32:
        return IR::Opcode::ImageAtomicInc32;
    case IR::Opcode::BoundImageAtomicDec32:
    case IR::Opcode::BindlessImageAtomicDec32:
        return IR::Opcode::ImageAtomicDec32;
    case IR::Opcode::BoundImageAtomicAnd32:
    case IR::Opcode::BindlessImageAtomicAnd32:
        return IR::Opcode::ImageAtomicAnd32;
    case IR::Opcode::BoundImageAtomicOr32:
    case IR::Opcode::BindlessImageAtomicOr32:
        return IR::Opcode::ImageAtomicOr32;
    case IR::Opcode::BoundImageAtomicXor32:
    case IR::Opcode::BindlessImageAtomicXor32:
        return IR::Opcode::ImageAtomicXor32;
    case IR::Opcode::BoundImageAtomicExchange32:
    case IR::Opcode::BindlessImageAtomicExchange32:
        return IR::Opcode::ImageAtomicExchange32;
    default:
        return IR::Opcode::Void;
    }
}

bool IsBindless(const IR::Inst& inst) {
    switch (inst.GetOpcode()) {
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
    case IR::Opcode::BindlessImageRead:
    case IR::Opcode::BindlessImageWrite:
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
    case IR::Opcode::BoundImageRead:
    case IR::Opcode::BoundImageWrite:
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
        return false;
    default:
        throw InvalidArgument("Invalid opcode {}", inst.GetOpcode());
    }
}

bool IsTextureInstruction(const IR::Inst& inst) {
    return IndexedInstruction(inst) != IR::Opcode::Void;
}

std::optional<ConstBufferAddr> TryGetConstBuffer(const IR::Inst* inst);

std::optional<ConstBufferAddr> Track(const IR::Value& value) {
    return IR::BreadthFirstSearch(value, TryGetConstBuffer);
}

std::optional<ConstBufferAddr> TryGetConstBuffer(const IR::Inst* inst) {
    switch (inst->GetOpcode()) {
    default:
        return std::nullopt;
    case IR::Opcode::BitwiseOr32: {
        std::optional lhs{Track(inst->Arg(0))};
        std::optional rhs{Track(inst->Arg(1))};
        if (!lhs || !rhs) {
            return std::nullopt;
        }
        if (lhs->has_secondary || rhs->has_secondary) {
            return std::nullopt;
        }
        if (lhs->count > 1 || rhs->count > 1) {
            return std::nullopt;
        }
        if (lhs->index > rhs->index || lhs->offset > rhs->offset) {
            std::swap(lhs, rhs);
        }
        return ConstBufferAddr{
            .index = lhs->index,
            .offset = lhs->offset,
            .secondary_index = rhs->index,
            .secondary_offset = rhs->offset,
            .dynamic_offset = {},
            .count = 1,
            .has_secondary = true,
        };
    }
    case IR::Opcode::GetCbufU32x2:
    case IR::Opcode::GetCbufU32:
        break;
    }
    const IR::Value index{inst->Arg(0)};
    const IR::Value offset{inst->Arg(1)};
    if (!index.IsImmediate()) {
        // Reading a bindless texture from variable indices is valid
        // but not supported here at the moment
        return std::nullopt;
    }
    if (offset.IsImmediate()) {
        return ConstBufferAddr{
            .index = index.U32(),
            .offset = offset.U32(),
            .secondary_index = 0,
            .secondary_offset = 0,
            .dynamic_offset = {},
            .count = 1,
            .has_secondary = false,
        };
    }
    IR::Inst* const offset_inst{offset.InstRecursive()};
    if (offset_inst->GetOpcode() != IR::Opcode::IAdd32) {
        return std::nullopt;
    }
    u32 base_offset{};
    IR::U32 dynamic_offset;
    if (offset_inst->Arg(0).IsImmediate()) {
        base_offset = offset_inst->Arg(0).U32();
        dynamic_offset = IR::U32{offset_inst->Arg(1)};
    } else if (offset_inst->Arg(1).IsImmediate()) {
        base_offset = offset_inst->Arg(1).U32();
        dynamic_offset = IR::U32{offset_inst->Arg(0)};
    } else {
        return std::nullopt;
    }
    return ConstBufferAddr{
        .index = index.U32(),
        .offset = base_offset,
        .secondary_index = 0,
        .secondary_offset = 0,
        .dynamic_offset = dynamic_offset,
        .count = 8,
        .has_secondary = false,
    };
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
            .index = env.TextureBoundBuffer(),
            .offset = inst.Arg(0).U32(),
            .secondary_index = 0,
            .secondary_offset = 0,
            .dynamic_offset = {},
            .count = 1,
            .has_secondary = false,
        };
    }
    return TextureInst{
        .cbuf = addr,
        .inst = &inst,
        .block = block,
    };
}

TextureType ReadTextureType(Environment& env, const ConstBufferAddr& cbuf) {
    const u32 secondary_index{cbuf.has_secondary ? cbuf.secondary_index : cbuf.index};
    const u32 secondary_offset{cbuf.has_secondary ? cbuf.secondary_offset : cbuf.offset};
    const u32 lhs_raw{env.ReadCbufValue(cbuf.index, cbuf.offset)};
    const u32 rhs_raw{env.ReadCbufValue(secondary_index, secondary_offset)};
    return env.ReadTextureType(lhs_raw | rhs_raw);
}

class Descriptors {
public:
    explicit Descriptors(TextureBufferDescriptors& texture_buffer_descriptors_,
                         ImageBufferDescriptors& image_buffer_descriptors_,
                         TextureDescriptors& texture_descriptors_,
                         ImageDescriptors& image_descriptors_)
        : texture_buffer_descriptors{texture_buffer_descriptors_},
          image_buffer_descriptors{image_buffer_descriptors_},
          texture_descriptors{texture_descriptors_}, image_descriptors{image_descriptors_} {}

    u32 Add(const TextureBufferDescriptor& desc) {
        return Add(texture_buffer_descriptors, desc, [&desc](const auto& existing) {
            return desc.cbuf_index == existing.cbuf_index &&
                   desc.cbuf_offset == existing.cbuf_offset &&
                   desc.secondary_cbuf_index == existing.secondary_cbuf_index &&
                   desc.secondary_cbuf_offset == existing.secondary_cbuf_offset &&
                   desc.count == existing.count && desc.size_shift == existing.size_shift &&
                   desc.has_secondary == existing.has_secondary;
        });
    }

    u32 Add(const ImageBufferDescriptor& desc) {
        const u32 index{Add(image_buffer_descriptors, desc, [&desc](const auto& existing) {
            return desc.format == existing.format && desc.cbuf_index == existing.cbuf_index &&
                   desc.cbuf_offset == existing.cbuf_offset && desc.count == existing.count &&
                   desc.size_shift == existing.size_shift;
        })};
        image_buffer_descriptors[index].is_written |= desc.is_written;
        image_buffer_descriptors[index].is_read |= desc.is_read;
        return index;
    }

    u32 Add(const TextureDescriptor& desc) {
        return Add(texture_descriptors, desc, [&desc](const auto& existing) {
            return desc.type == existing.type && desc.is_depth == existing.is_depth &&
                   desc.has_secondary == existing.has_secondary &&
                   desc.cbuf_index == existing.cbuf_index &&
                   desc.cbuf_offset == existing.cbuf_offset &&
                   desc.secondary_cbuf_index == existing.secondary_cbuf_index &&
                   desc.secondary_cbuf_offset == existing.secondary_cbuf_offset &&
                   desc.count == existing.count && desc.size_shift == existing.size_shift;
        });
    }

    u32 Add(const ImageDescriptor& desc) {
        const u32 index{Add(image_descriptors, desc, [&desc](const auto& existing) {
            return desc.type == existing.type && desc.format == existing.format &&
                   desc.cbuf_index == existing.cbuf_index &&
                   desc.cbuf_offset == existing.cbuf_offset && desc.count == existing.count &&
                   desc.size_shift == existing.size_shift;
        })};
        image_descriptors[index].is_written |= desc.is_written;
        image_descriptors[index].is_read |= desc.is_read;
        return index;
    }

private:
    template <typename Descriptors, typename Descriptor, typename Func>
    static u32 Add(Descriptors& descriptors, const Descriptor& desc, Func&& pred) {
        // TODO: Handle arrays
        const auto it{std::ranges::find_if(descriptors, pred)};
        if (it != descriptors.end()) {
            return static_cast<u32>(std::distance(descriptors.begin(), it));
        }
        descriptors.push_back(desc);
        return static_cast<u32>(descriptors.size()) - 1;
    }

    TextureBufferDescriptors& texture_buffer_descriptors;
    ImageBufferDescriptors& image_buffer_descriptors;
    TextureDescriptors& texture_descriptors;
    ImageDescriptors& image_descriptors;
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
    Descriptors descriptors{
        program.info.texture_buffer_descriptors,
        program.info.image_buffer_descriptors,
        program.info.texture_descriptors,
        program.info.image_descriptors,
    };
    for (TextureInst& texture_inst : to_replace) {
        // TODO: Handle arrays
        IR::Inst* const inst{texture_inst.inst};
        inst->ReplaceOpcode(IndexedInstruction(*inst));

        const auto& cbuf{texture_inst.cbuf};
        auto flags{inst->Flags<IR::TextureInstInfo>()};
        switch (inst->GetOpcode()) {
        case IR::Opcode::ImageQueryDimensions:
            flags.type.Assign(ReadTextureType(env, cbuf));
            inst->SetFlags(flags);
            break;
        case IR::Opcode::ImageFetch:
            if (flags.type != TextureType::Color1D) {
                break;
            }
            if (ReadTextureType(env, cbuf) == TextureType::Buffer) {
                // Replace with the bound texture type only when it's a texture buffer
                // If the instruction is 1D and the bound type is 2D, don't change the code and let
                // the rasterizer robustness handle it
                // This happens on Fire Emblem: Three Houses
                flags.type.Assign(TextureType::Buffer);
            }
            break;
        default:
            break;
        }
        u32 index;
        switch (inst->GetOpcode()) {
        case IR::Opcode::ImageRead:
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
        case IR::Opcode::ImageWrite: {
            if (cbuf.has_secondary) {
                throw NotImplementedException("Unexpected separate sampler");
            }
            const bool is_written{inst->GetOpcode() != IR::Opcode::ImageRead};
            const bool is_read{inst->GetOpcode() != IR::Opcode::ImageWrite};
            if (flags.type == TextureType::Buffer) {
                index = descriptors.Add(ImageBufferDescriptor{
                    .format = flags.image_format,
                    .is_written = is_written,
                    .is_read = is_read,
                    .cbuf_index = cbuf.index,
                    .cbuf_offset = cbuf.offset,
                    .count = cbuf.count,
                    .size_shift = DESCRIPTOR_SIZE_SHIFT,
                });
            } else {
                index = descriptors.Add(ImageDescriptor{
                    .type = flags.type,
                    .format = flags.image_format,
                    .is_written = is_written,
                    .is_read = is_read,
                    .cbuf_index = cbuf.index,
                    .cbuf_offset = cbuf.offset,
                    .count = cbuf.count,
                    .size_shift = DESCRIPTOR_SIZE_SHIFT,
                });
            }
            break;
        }
        default:
            if (flags.type == TextureType::Buffer) {
                index = descriptors.Add(TextureBufferDescriptor{
                    .has_secondary = cbuf.has_secondary,
                    .cbuf_index = cbuf.index,
                    .cbuf_offset = cbuf.offset,
                    .secondary_cbuf_index = cbuf.secondary_index,
                    .secondary_cbuf_offset = cbuf.secondary_offset,
                    .count = cbuf.count,
                    .size_shift = DESCRIPTOR_SIZE_SHIFT,
                });
            } else {
                index = descriptors.Add(TextureDescriptor{
                    .type = flags.type,
                    .is_depth = flags.is_depth != 0,
                    .has_secondary = cbuf.has_secondary,
                    .cbuf_index = cbuf.index,
                    .cbuf_offset = cbuf.offset,
                    .secondary_cbuf_index = cbuf.secondary_index,
                    .secondary_cbuf_offset = cbuf.secondary_offset,
                    .count = cbuf.count,
                    .size_shift = DESCRIPTOR_SIZE_SHIFT,
                });
            }
            break;
        }
        flags.descriptor_index.Assign(index);
        inst->SetFlags(flags);

        if (cbuf.count > 1) {
            const auto insert_point{IR::Block::InstructionList::s_iterator_to(*inst)};
            IR::IREmitter ir{*texture_inst.block, insert_point};
            const IR::U32 shift{ir.Imm32(std::countr_zero(DESCRIPTOR_SIZE))};
            inst->SetArg(0, ir.ShiftRightArithmetic(cbuf.dynamic_offset, shift));
        } else {
            inst->SetArg(0, IR::Value{});
        }
    }
}

void JoinTextureInfo(Info& base, Info& source) {
    Descriptors descriptors{
        base.texture_buffer_descriptors,
        base.image_buffer_descriptors,
        base.texture_descriptors,
        base.image_descriptors,
    };
    for (auto& desc : source.texture_buffer_descriptors) {
        descriptors.Add(desc);
    }
    for (auto& desc : source.image_buffer_descriptors) {
        descriptors.Add(desc);
    }
    for (auto& desc : source.texture_descriptors) {
        descriptors.Add(desc);
    }
    for (auto& desc : source.image_descriptors) {
        descriptors.Add(desc);
    }
}

} // namespace Shader::Optimization
