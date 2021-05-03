// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"

namespace Shader::Backend::SPIRV {
namespace {
void MemoryBarrier(EmitContext& ctx, spv::Scope scope) {
    const auto semantics{
        spv::MemorySemanticsMask::AcquireRelease | spv::MemorySemanticsMask::UniformMemory |
        spv::MemorySemanticsMask::WorkgroupMemory | spv::MemorySemanticsMask::AtomicCounterMemory |
        spv::MemorySemanticsMask::ImageMemory};
    ctx.OpMemoryBarrier(ctx.Const(static_cast<u32>(scope)), ctx.Const(static_cast<u32>(semantics)));
}
} // Anonymous namespace

void EmitBarrier(EmitContext& ctx) {
    const auto execution{spv::Scope::Workgroup};
    const auto memory{spv::Scope::Workgroup};
    const auto memory_semantics{spv::MemorySemanticsMask::AcquireRelease |
                                spv::MemorySemanticsMask::WorkgroupMemory};
    ctx.OpControlBarrier(ctx.Const(static_cast<u32>(execution)),
                         ctx.Const(static_cast<u32>(memory)),
                         ctx.Const(static_cast<u32>(memory_semantics)));
}

void EmitWorkgroupMemoryBarrier(EmitContext& ctx) {
    MemoryBarrier(ctx, spv::Scope::Workgroup);
}

void EmitDeviceMemoryBarrier(EmitContext& ctx) {
    MemoryBarrier(ctx, spv::Scope::Device);
}

} // namespace Shader::Backend::SPIRV
