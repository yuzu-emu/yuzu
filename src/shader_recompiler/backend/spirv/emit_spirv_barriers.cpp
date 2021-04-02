// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/ir/modifiers.h"

namespace Shader::Backend::SPIRV {
namespace {
spv::Scope MemoryScopeToSpirVScope(IR::MemoryScope scope) {
    switch (scope) {
    case IR::MemoryScope::Warp:
        return spv::Scope::Subgroup;
    case IR::MemoryScope::Workgroup:
        return spv::Scope::Workgroup;
    case IR::MemoryScope::Device:
        return spv::Scope::Device;
    case IR::MemoryScope::System:
        return spv::Scope::CrossDevice;
    case IR::MemoryScope::DontCare:
        return spv::Scope::Invocation;
    default:
        throw NotImplementedException("Unknown memory scope!");
    }
}

} // namespace

void EmitMemoryBarrier(EmitContext& ctx, IR::Inst* inst) {
    const auto info{inst->Flags<IR::BarrierInstInfo>()};
    const auto semantics =
        spv::MemorySemanticsMask::AcquireRelease | spv::MemorySemanticsMask::UniformMemory |
        spv::MemorySemanticsMask::WorkgroupMemory | spv::MemorySemanticsMask::AtomicCounterMemory |
        spv::MemorySemanticsMask::ImageMemory;
    const auto scope = MemoryScopeToSpirVScope(info.scope);
    ctx.OpMemoryBarrier(ctx.Constant(ctx.U32[1], static_cast<u32>(scope)),
                        ctx.Constant(ctx.U32[1], static_cast<u32>(semantics)));
}

} // namespace Shader::Backend::SPIRV
