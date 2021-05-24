
// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/profile.h"

namespace Shader::Backend::GLSL {
namespace {
static constexpr std::string_view cas_loop{R"(
{} {};
for (;;){{
    {} old_value={};
    {} = atomicCompSwap({},old_value,{}({},{}));
    if ({}==old_value){{break;}}
}})"};

void CasFunction(EmitContext& ctx, IR::Inst& inst, std::string_view ssbo, std::string_view value,
                 std::string_view type, std::string_view function) {
    const auto ret{ctx.reg_alloc.Define(inst)};
    ctx.Add(cas_loop.data(), type, ret, type, ssbo, ret, ssbo, function, ssbo, value, ret);
}
} // namespace

void EmitStorageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicAdd(ssbo{}_u32[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddS32("{}=atomicMin(ssbo{}_s32[{}],int({}));", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicMin(ssbo{}_u32[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddS32("{}=atomicMax(ssbo{}_s32[{}],int({}));", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicMax(ssbo{}_u32[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            [[maybe_unused]] const IR::Value& offset, std::string_view value) {
    // const auto ret{ctx.reg_alloc.Define(inst)};
    // const auto type{"uint"};
    // ctx.Add(cas_loop.data(), type, ret, type, ssbo, ret, ssbo, "CasIncrement", ssbo, value, ret);
    const std::string ssbo{fmt::format("ssbo{}_u32[{}]", binding.U32(), offset.U32())};
    CasFunction(ctx, inst, ssbo, value, "uint", "CasIncrement");
}

void EmitStorageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    const std::string ssbo{fmt::format("ssbo{}_u32[{}]", binding.U32(), offset.U32())};
    CasFunction(ctx, inst, ssbo, value, "uint", "CasDecrement");
}

void EmitStorageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicAnd(ssbo{}_u32[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicOr(ssbo{}_u32[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicXor(ssbo{}_u32[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicExchange(ssbo{}_u32[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicIAdd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    // ctx.AddU64("{}=atomicAdd(ssbo{}_u64[{}],{});", inst, binding.U32(), offset.U32(), value);
    ctx.AddU64("{}=ssbo{}_u64[{}];", inst, binding.U32(), offset.U32());
    ctx.Add("ssbo{}_u64[{}]+={};", binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicSMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddS64("{}=atomicMin(int64_t(ssbo{}_u64[{}]),int64_t({}));", inst, binding.U32(),
               offset.U32(), value);
}

void EmitStorageAtomicUMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=atomicMin(ssbo{}_u64[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicSMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddS64("{}=atomicMax(int64_t(ssbo{}_u64[{}]),int64_t({}));", inst, binding.U32(),
               offset.U32(), value);
}

void EmitStorageAtomicUMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=atomicMax(ssbo{}_u64[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicAnd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=atomicAnd(ssbo{}_u64[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicOr64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=atomicOr(ssbo{}_u64[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicXor64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=atomicXor(ssbo{}_u64[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicExchange64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=atomicExchange(ssbo{}_u64[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicAddF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddF32("{}=atomicAdd(ssbo{}_u32[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicAddF16x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] const IR::Value& offset,
                               [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitStorageAtomicAddF32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] const IR::Value& offset,
                               [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitStorageAtomicMinF16x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] const IR::Value& offset,
                               [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitStorageAtomicMinF32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] const IR::Value& offset,
                               [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitStorageAtomicMaxF16x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] const IR::Value& offset,
                               [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitStorageAtomicMaxF32x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& binding,
                               [[maybe_unused]] const IR::Value& offset,
                               [[maybe_unused]] std::string_view value) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicIAdd32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicSMin32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicUMin32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicSMax32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicUMax32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicInc32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicDec32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicAnd32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicOr32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicXor32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicExchange32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicIAdd64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicSMin64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicUMin64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicSMax64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicUMax64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicInc64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicDec64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicAnd64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicOr64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicXor64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicExchange64(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicAddF32(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicAddF16x2(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicAddF32x2(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicMinF16x2(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicMinF32x2(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicMaxF16x2(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}

void EmitGlobalAtomicMaxF32x2(EmitContext&) {
    throw NotImplementedException("GLSL Instrucion");
}
} // namespace Shader::Backend::GLSL
