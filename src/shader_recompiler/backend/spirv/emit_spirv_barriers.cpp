// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/ir/modifiers.h"

namespace Shader::Backend::SPIRV {
namespace {
void EmitMemoryBarrierImpl(EmitContext& ctx, spv::Scope scope) {
    const auto semantics =
        spv::MemorySemanticsMask::AcquireRelease | spv::MemorySemanticsMask::UniformMemory |
        spv::MemorySemanticsMask::WorkgroupMemory | spv::MemorySemanticsMask::AtomicCounterMemory |
        spv::MemorySemanticsMask::ImageMemory;
    ctx.OpMemoryBarrier(ctx.Constant(ctx.U32[1], static_cast<u32>(scope)),
                        ctx.Constant(ctx.U32[1], static_cast<u32>(semantics)));
}

} // Anonymous namespace

void EmitMemoryBarrierWorkgroupLevel(EmitContext& ctx) {
    EmitMemoryBarrierImpl(ctx, spv::Scope::Workgroup);
}

void EmitMemoryBarrierDeviceLevel(EmitContext& ctx) {
    EmitMemoryBarrierImpl(ctx, spv::Scope::Device);
}

void EmitMemoryBarrierSystemLevel(EmitContext& ctx) {
    EmitMemoryBarrierImpl(ctx, spv::Scope::CrossDevice);
}

} // namespace Shader::Backend::SPIRV
