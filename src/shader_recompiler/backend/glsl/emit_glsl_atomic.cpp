
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
uint {};
for (;;){{
    uint old_value={};
    {}=atomicCompSwap({},old_value,{}({},{}));
    if ({}==old_value){{break;}}
}})"};

void CasFunction(EmitContext& ctx, std::string_view ret, std::string_view ssbo,
                 std::string_view value, std::string_view function) {
    ctx.Add(cas_loop.data(), ret, ssbo, ret, ssbo, function, ssbo, value, ret);
}

void CasFunctionInt32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                      const IR::Value& offset, std::string_view value, std::string_view function) {
    const auto ret{ctx.reg_alloc.Define(inst)};
    const std::string ssbo{fmt::format("ssbo{}[{}]", binding.U32(), offset.U32())};
    CasFunction(ctx, ret, ssbo, value, function);
}

void CasFunctionF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                    const IR::Value& offset, std::string_view value, std::string_view function) {
    const std::string ssbo{fmt::format("ssbo{}[{}]", binding.U32(), offset.U32())};
    const std::string u32_value{fmt::format("floatBitsToUint({})", value)};
    const auto ret{ctx.reg_alloc.Define(inst)};
    const auto ret_32{ret + "_u32"};
    CasFunction(ctx, ret_32, ssbo, u32_value, function);
    ctx.Add("float {}=uintBitsToFloat({});", ret, ret_32);
}

void CasFunctionF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                      const IR::Value& offset, std::string_view value, std::string_view function) {
    const std::string ssbo{fmt::format("ssbo{}[{}]", binding.U32(), offset.U32())};
    const std::string u32_value{fmt::format("packHalf2x16({})", value)};
    const auto ret{ctx.reg_alloc.Define(inst)};
    CasFunction(ctx, ret, ssbo, u32_value, function);
}
} // namespace

void EmitStorageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicAdd(ssbo{}[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    const std::string u32_value{fmt::format("uint({})", value)};
    CasFunctionInt32(ctx, inst, binding, offset, u32_value, "CasMinS32");
}

void EmitStorageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicMin(ssbo{}[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    const std::string u32_value{fmt::format("uint({})", value)};
    CasFunctionInt32(ctx, inst, binding, offset, u32_value, "CasMaxS32");
}

void EmitStorageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicMax(ssbo{}[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    CasFunctionInt32(ctx, inst, binding, offset, value, "CasIncrement");
}

void EmitStorageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    CasFunctionInt32(ctx, inst, binding, offset, value, "CasDecrement");
}

void EmitStorageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicAnd(ssbo{}[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicOr(ssbo{}[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicXor(ssbo{}[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicExchange(ssbo{}[{}],{});", inst, binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicIAdd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    // LOG_WARNING(..., "Op falling to non-atomic");
    ctx.AddU64("{}=uint64_t(uvec2(ssbo{}[{}],ssbo{}[{}]));", inst, binding.U32(), offset.U32(),
               binding.U32(), offset.U32() + 1);
    ctx.Add("ssbo{}[{}]+=unpackUint2x32({}).x;ssbo{}[{}]+=unpackUint2x32({}).y;", binding.U32(),
            offset.U32(), value, binding.U32(), offset.U32() + 1, value);
}

void EmitStorageAtomicSMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    // LOG_WARNING(..., "Op falling to non-atomic");
    ctx.AddS64("{}=int64_t(ivec2(ssbo{}[{}],ssbo{}[{}]));", inst, binding.U32(), offset.U32(),
               binding.U32(), offset.U32() + 1);
    ctx.Add(R"(
for(int i=0;i<2;++i){{
ssbo{}[{}+i]=uint(min(int(ssbo{}[{}+i]),unpackInt2x32(int64_t({}))[i]));
}}
)",
            binding.U32(), offset.U32(), binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicUMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    // LOG_WARNING(..., "Op falling to non-atomic");
    ctx.AddU64("{}=uint64_t(uvec2(ssbo{}[{}],ssbo{}[{}]));", inst, binding.U32(), offset.U32(),
               binding.U32(), offset.U32() + 1);
    ctx.Add(R"(
for(int i=0;i<2;++i){{
ssbo{}[{}+i]=min(ssbo{}[{}+i],unpackUint2x32(uint64_t({}))[i]);
}}
)",
            binding.U32(), offset.U32(), binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicSMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    // LOG_WARNING(..., "Op falling to non-atomic");
    ctx.AddS64("{}=int64_t(ivec2(ssbo{}[{}],ssbo{}[{}]));", inst, binding.U32(), offset.U32(),
               binding.U32(), offset.U32() + 1);
    ctx.Add(R"(
for(int i=0;i<2;++i){{
ssbo{}[{}+i]=uint(max(int(ssbo{}[{}+i]),unpackInt2x32(int64_t({}))[i]));
}}
)",
            binding.U32(), offset.U32(), binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicUMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    // LOG_WARNING(..., "Op falling to non-atomic");
    ctx.AddU64("{}=uint64_t(uvec2(ssbo{}[{}],ssbo{}[{}]));", inst, binding.U32(), offset.U32(),
               binding.U32(), offset.U32() + 1);
    ctx.Add(R"(
for(int i=0;i<2;++i){{
ssbo{}[{}+i]=max(ssbo{}[{}+i],unpackUint2x32(uint64_t({}))[i]);
}}
)",
            binding.U32(), offset.U32(), binding.U32(), offset.U32(), value);
}

void EmitStorageAtomicAnd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=uint64_t(uvec2(atomicAnd(ssbo{}[{}],unpackUint2x32({}).x),atomicAnd(ssbo{}[{}],"
               "unpackUint2x32({}).y)));",
               inst, binding.U32(), offset.U32(), value, binding.U32(), offset.U32() + 1, value);
}

void EmitStorageAtomicOr64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=uint64_t(uvec2(atomicOr(ssbo{}[{}],unpackUint2x32({}).x),atomicOr(ssbo{}[{}],"
               "unpackUint2x32({}).y)));",
               inst, binding.U32(), offset.U32(), value, binding.U32(), offset.U32() + 1, value);
}

void EmitStorageAtomicXor64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=uint64_t(uvec2(atomicXor(ssbo{}[{}],unpackUint2x32({}).x),atomicXor(ssbo{}[{}],"
               "unpackUint2x32({}).y)));",
               inst, binding.U32(), offset.U32(), value, binding.U32(), offset.U32() + 1, value);
}

void EmitStorageAtomicExchange64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=uint64_t(uvec2(atomicExchange(ssbo{}[{}],unpackUint2x32({}).x),atomicExchange("
               "ssbo{}[{}],unpackUint2x32({}).y)));",
               inst, binding.U32(), offset.U32(), value, binding.U32(), offset.U32() + 1, value);
}

void EmitStorageAtomicAddF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    CasFunctionF32(ctx, inst, binding, offset, value, "CasFloatAdd");
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
    CasFunctionF32x2(ctx, inst, binding, offset, value, "CasFloatAdd32x2");
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
    CasFunctionF32x2(ctx, inst, binding, offset, value, "CasFloatMin32x2");
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
    CasFunctionF32x2(ctx, inst, binding, offset, value, "CasFloatMax32x2");
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
