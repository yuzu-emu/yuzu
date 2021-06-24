// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <bit>
#include <memory>
#include <ranges>
#include <vector>

#include "common/settings.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/post_order.h"
#include "shader_recompiler/frontend/maxwell/structured_control_flow.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"
#include "shader_recompiler/frontend/maxwell/translate_program.h"
#include "shader_recompiler/host_translate_info.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Shader::Maxwell {
namespace {
IR::BlockList GenerateBlocks(const IR::AbstractSyntaxList& syntax_list) {
    auto syntax_blocks{syntax_list | std::views::filter([](const auto& node) {
                           return node.type == IR::AbstractSyntaxNode::Type::Block;
                       })};
    IR::BlockList blocks(std::ranges::distance(syntax_blocks));
    std::ranges::transform(syntax_blocks, blocks.begin(),
                           [](const IR::AbstractSyntaxNode& node) { return node.data.block; });
    return blocks;
}

void RemoveUnreachableBlocks(IR::Program& program) {
    // Some blocks might be unreachable if a function call exists unconditionally
    // If this happens the number of blocks and post order blocks will mismatch
    if (program.blocks.size() == program.post_order_blocks.size()) {
        return;
    }
    const auto begin{program.blocks.begin() + 1};
    const auto end{program.blocks.end()};
    const auto pred{[](IR::Block* block) { return block->ImmPredecessors().empty(); }};
    program.blocks.erase(std::remove_if(begin, end, pred), end);
}

void CollectInterpolationInfo(Environment& env, IR::Program& program) {
    if (program.stage != Stage::Fragment) {
        return;
    }
    const ProgramHeader& sph{env.SPH()};
    for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
        std::optional<PixelImap> imap;
        for (const PixelImap value : sph.ps.GenericInputMap(static_cast<u32>(index))) {
            if (value == PixelImap::Unused) {
                continue;
            }
            if (imap && imap != value) {
                throw NotImplementedException("Per component interpolation");
            }
            imap = value;
        }
        if (!imap) {
            continue;
        }
        program.info.interpolation[index] = [&] {
            switch (*imap) {
            case PixelImap::Unused:
            case PixelImap::Perspective:
                return Interpolation::Smooth;
            case PixelImap::Constant:
                return Interpolation::Flat;
            case PixelImap::ScreenLinear:
                return Interpolation::NoPerspective;
            }
            throw NotImplementedException("Unknown interpolation {}", *imap);
        }();
    }
}

void AddNVNStorageBuffers(IR::Program& program) {
    if (!program.info.uses_global_memory) {
        return;
    }
    const u32 driver_cbuf{0};
    const u32 descriptor_size{0x10};
    const u32 num_buffers{16};
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
    auto& descs{program.info.storage_buffers_descriptors};
    for (u32 index = 0; index < num_buffers; ++index) {
        if (!program.info.nvn_buffer_used[index]) {
            continue;
        }
        const u32 offset{base + index * descriptor_size};
        const auto it{std::ranges::find(descs, offset, &StorageBufferDescriptor::cbuf_offset)};
        if (it != descs.end()) {
            it->is_written |= program.info.stores_global_memory;
            continue;
        }
        descs.push_back({
            .cbuf_index = driver_cbuf,
            .cbuf_offset = offset,
            .count = 1,
            .is_written = program.info.stores_global_memory,
        });
    }
}
} // Anonymous namespace

IR::Program TranslateProgram(ObjectPool<IR::Inst>& inst_pool, ObjectPool<IR::Block>& block_pool,
                             Environment& env, Flow::CFG& cfg, const HostTranslateInfo& host_info) {
    IR::Program program;
    program.syntax_list = BuildASL(inst_pool, block_pool, env, cfg);
    program.blocks = GenerateBlocks(program.syntax_list);
    program.post_order_blocks = PostOrder(program.syntax_list.front());
    program.stage = env.ShaderStage();
    program.local_memory_size = env.LocalMemorySize();
    switch (program.stage) {
    case Stage::TessellationControl: {
        const ProgramHeader& sph{env.SPH()};
        program.invocations = sph.common2.threads_per_input_primitive;
        break;
    }
    case Stage::Geometry: {
        const ProgramHeader& sph{env.SPH()};
        program.output_topology = sph.common3.output_topology;
        program.output_vertices = sph.common4.max_output_vertices;
        program.invocations = sph.common2.threads_per_input_primitive;
        program.is_geometry_passthrough = sph.common0.geometry_passthrough != 0;
        if (program.is_geometry_passthrough) {
            const auto& mask{env.GpPassthroughMask()};
            program.info.passthrough.mask |= ~std::bit_cast<std::bitset<256>>(mask);
        }
        break;
    }
    case Stage::Compute:
        program.workgroup_size = env.WorkgroupSize();
        program.shared_memory_size = env.SharedMemorySize();
        break;
    default:
        break;
    }
    RemoveUnreachableBlocks(program);

    // Replace instructions before the SSA rewrite
    if (!host_info.support_float16) {
        Optimization::LowerFp16ToFp32(program);
    }
    if (!host_info.support_int64) {
        Optimization::LowerInt64ToInt32(program);
    }
    Optimization::SsaRewritePass(program);

    Optimization::GlobalMemoryToStorageBufferPass(program);
    Optimization::TexturePass(env, program);

    Optimization::ConstantPropagationPass(program);
    Optimization::DeadCodeEliminationPass(program);
    if (Settings::values.renderer_debug) {
        Optimization::VerificationPass(program);
    }
    Optimization::CollectShaderInfoPass(env, program);
    CollectInterpolationInfo(env, program);
    AddNVNStorageBuffers(program);
    return program;
}

IR::Program MergeDualVertexPrograms(IR::Program& vertex_a, IR::Program& vertex_b,
                                    Environment& env_vertex_b) {
    IR::Program result{};
    Optimization::VertexATransformPass(vertex_a);
    Optimization::VertexBTransformPass(vertex_b);
    for (const auto& term : vertex_a.syntax_list) {
        if (term.type != IR::AbstractSyntaxNode::Type::Return) {
            result.syntax_list.push_back(term);
        }
    }
    result.syntax_list.insert(result.syntax_list.end(), vertex_b.syntax_list.begin(),
                              vertex_b.syntax_list.end());
    result.blocks = GenerateBlocks(result.syntax_list);
    result.post_order_blocks = vertex_b.post_order_blocks;
    for (const auto& block : vertex_a.post_order_blocks) {
        result.post_order_blocks.push_back(block);
    }
    result.stage = Stage::VertexB;
    result.info = vertex_a.info;
    result.local_memory_size = std::max(vertex_a.local_memory_size, vertex_b.local_memory_size);
    result.info.loads.mask |= vertex_b.info.loads.mask;
    result.info.stores.mask |= vertex_b.info.stores.mask;

    Optimization::JoinTextureInfo(result.info, vertex_b.info);
    Optimization::JoinStorageInfo(result.info, vertex_b.info);
    Optimization::DeadCodeEliminationPass(result);
    if (Settings::values.renderer_debug) {
        Optimization::VerificationPass(result);
    }
    Optimization::CollectShaderInfoPass(env_vertex_b, result);
    return result;
}

} // namespace Shader::Maxwell
