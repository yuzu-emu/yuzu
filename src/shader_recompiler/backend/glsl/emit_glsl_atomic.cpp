// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>

#include "shader_recompiler/backend/glsl/emit_context.h"
#include "shader_recompiler/backend/glsl/emit_glsl_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
constexpr char cas_loop[]{
    "for (;;){{uint old={};{}=atomicCompSwap({},old,{}({},{}));if({}==old){{break;}}}}"};

void SharedCasFunction(EmitContext& ctx, IR::Inst& inst, std::string_view offset,
                       std::string_view value, std::string_view function) {
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    const std::string smem{fmt::format("smem[{}>>2]", offset)};
    ctx.Add(cas_loop, smem, ret, smem, function, smem, value, ret);
}

void SsboCasFunction(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                     const IR::Value& offset, std::string_view value, std::string_view function) {
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    const std::string ssbo{fmt::format("{}_ssbo{}[{}>>2]", ctx.stage_name, binding.U32(),
                                       ctx.var_alloc.Consume(offset))};
    ctx.Add(cas_loop, ssbo, ret, ssbo, function, ssbo, value, ret);
}

void SsboCasFunctionF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset, std::string_view value,
                        std::string_view function) {
    const std::string ssbo{fmt::format("{}_ssbo{}[{}>>2]", ctx.stage_name, binding.U32(),
                                       ctx.var_alloc.Consume(offset))};
    const auto ret{ctx.var_alloc.Define(inst, GlslVarType::U32)};
    ctx.Add(cas_loop, ssbo, ret, ssbo, function, ssbo, value, ret);
    ctx.AddF32("{}=utof({});", inst, ret);
}
} // Anonymous namespace

void EmitSharedAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value) {
    ctx.AddU32("{}=atomicAdd(smem[{}>>2],{});", inst, pointer_offset, value);
}

void EmitSharedAtomicSMin32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value) {
    const std::string u32_value{fmt::format("uint({})", value)};
    SharedCasFunction(ctx, inst, pointer_offset, u32_value, "CasMinS32");
}

void EmitSharedAtomicUMin32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value) {
    ctx.AddU32("{}=atomicMin(smem[{}>>2],{});", inst, pointer_offset, value);
}

void EmitSharedAtomicSMax32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value) {
    const std::string u32_value{fmt::format("uint({})", value)};
    SharedCasFunction(ctx, inst, pointer_offset, u32_value, "CasMaxS32");
}

void EmitSharedAtomicUMax32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                            std::string_view value) {
    ctx.AddU32("{}=atomicMax(smem[{}>>2],{});", inst, pointer_offset, value);
}

void EmitSharedAtomicInc32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                           std::string_view value) {
    SharedCasFunction(ctx, inst, pointer_offset, value, "CasIncrement");
}

void EmitSharedAtomicDec32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                           std::string_view value) {
    SharedCasFunction(ctx, inst, pointer_offset, value, "CasDecrement");
}

void EmitSharedAtomicAnd32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                           std::string_view value) {
    ctx.AddU32("{}=atomicAnd(smem[{}>>2],{});", inst, pointer_offset, value);
}

void EmitSharedAtomicOr32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                          std::string_view value) {
    ctx.AddU32("{}=atomicOr(smem[{}>>2],{});", inst, pointer_offset, value);
}

void EmitSharedAtomicXor32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                           std::string_view value) {
    ctx.AddU32("{}=atomicXor(smem[{}>>2],{});", inst, pointer_offset, value);
}

void EmitSharedAtomicExchange32(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                                std::string_view value) {
    ctx.AddU32("{}=atomicExchange(smem[{}>>2],{});", inst, pointer_offset, value);
}

void EmitSharedAtomicExchange64(EmitContext& ctx, IR::Inst& inst, std::string_view pointer_offset,
                                std::string_view value) {
    LOG_WARNING(Shader_GLSL, "Int64 atomics not supported, fallback to non-atomic");
    ctx.AddU64("{}=packUint2x32(uvec2(smem[{}>>2],smem[({}+4)>>2]));", inst, pointer_offset,
               pointer_offset);
    ctx.Add("smem[{}>>2]=unpackUint2x32({}).x;smem[({}+4)>>2]=unpackUint2x32({}).y;",
            pointer_offset, value, pointer_offset, value);
}

void EmitStorageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicAdd({}_ssbo{}[{}>>2],{});", inst, ctx.stage_name, binding.U32(),
               ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    const std::string u32_value{fmt::format("uint({})", value)};
    SsboCasFunction(ctx, inst, binding, offset, u32_value, "CasMinS32");
}

void EmitStorageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicMin({}_ssbo{}[{}>>2],{});", inst, ctx.stage_name, binding.U32(),
               ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    const std::string u32_value{fmt::format("uint({})", value)};
    SsboCasFunction(ctx, inst, binding, offset, u32_value, "CasMaxS32");
}

void EmitStorageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicMax({}_ssbo{}[{}>>2],{});", inst, ctx.stage_name, binding.U32(),
               ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    SsboCasFunction(ctx, inst, binding, offset, value, "CasIncrement");
}

void EmitStorageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    SsboCasFunction(ctx, inst, binding, offset, value, "CasDecrement");
}

void EmitStorageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicAnd({}_ssbo{}[{}>>2],{});", inst, ctx.stage_name, binding.U32(),
               ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicOr({}_ssbo{}[{}>>2],{});", inst, ctx.stage_name, binding.U32(),
               ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicXor({}_ssbo{}[{}>>2],{});", inst, ctx.stage_name, binding.U32(),
               ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 const IR::Value& offset, std::string_view value) {
    ctx.AddU32("{}=atomicExchange({}_ssbo{}[{}>>2],{});", inst, ctx.stage_name, binding.U32(),
               ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicIAdd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    LOG_WARNING(Shader_GLSL, "Int64 atomics not supported, fallback to non-atomic");
    ctx.AddU64("{}=packUint2x32(uvec2({}_ssbo{}[{}>>2],{}_ssbo{}[({}>>2)+1]));", inst,
               ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
               binding.U32(), ctx.var_alloc.Consume(offset));
    ctx.Add("{}_ssbo{}[{}>>2]+=unpackUint2x32({}).x;{}_ssbo{}[({}>>2)+1]+=unpackUint2x32({}).y;",
            ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), value, ctx.stage_name,
            binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicSMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    LOG_WARNING(Shader_GLSL, "Int64 atomics not supported, fallback to non-atomic");
    ctx.AddU64("{}=packInt2x32(ivec2({}_ssbo{}[{}>>2],{}_ssbo{}[({}>>2)+1]));", inst,
               ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
               binding.U32(), ctx.var_alloc.Consume(offset));
    ctx.Add("for(int i=0;i<2;++i){{ "
            "{}_ssbo{}[({}>>2)+i]=uint(min(int({}_ssbo{}[({}>>2)+i]),unpackInt2x32(int64_t({}))[i])"
            ");}}",
            ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
            binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicUMin64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    LOG_WARNING(Shader_GLSL, "Int64 atomics not supported, fallback to non-atomic");
    ctx.AddU64("{}=packUint2x32(uvec2({}_ssbo{}[{}>>2],{}_ssbo{}[({}>>2)+1]));", inst,
               ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
               binding.U32(), ctx.var_alloc.Consume(offset));
    ctx.Add("for(int i=0;i<2;++i){{ "
            "{}_ssbo{}[({}>>2)+i]=min({}_ssbo{}[({}>>2)+i],unpackUint2x32(uint64_t({}))[i]);}}",
            ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
            binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicSMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    LOG_WARNING(Shader_GLSL, "Int64 atomics not supported, fallback to non-atomic");
    ctx.AddU64("{}=packInt2x32(ivec2({}_ssbo{}[{}>>2],{}_ssbo{}[({}>>2)+1]));", inst,
               ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
               binding.U32(), ctx.var_alloc.Consume(offset));
    ctx.Add("for(int i=0;i<2;++i){{ "
            "{}_ssbo{}[({}>>2)+i]=uint(max(int({}_ssbo{}[({}>>2)+i]),unpackInt2x32(int64_t({}))[i])"
            ");}}",
            ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
            binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicUMax64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    LOG_WARNING(Shader_GLSL, "Int64 atomics not supported, fallback to non-atomic");
    ctx.AddU64("{}=packUint2x32(uvec2({}_ssbo{}[{}>>2],{}_ssbo{}[({}>>2)+1]));", inst,
               ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
               binding.U32(), ctx.var_alloc.Consume(offset));
    ctx.Add("for(int "
            "i=0;i<2;++i){{{}_ssbo{}[({}>>2)+i]=max({}_ssbo{}[({}>>2)+i],unpackUint2x32(uint64_t({}"
            "))[i]);}}",
            ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), ctx.stage_name,
            binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicAnd64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU64(
        "{}=packUint2x32(uvec2(atomicAnd({}_ssbo{}[{}>>2],unpackUint2x32({}).x),atomicAnd({}_"
        "ssbo{}[({}>>2)+1],unpackUint2x32({}).y)));",
        inst, ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), value, ctx.stage_name,
        binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicOr64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                           const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=packUint2x32(uvec2(atomicOr({}_ssbo{}[{}>>2],unpackUint2x32({}).x),atomicOr({}_"
               "ssbo{}[({}>>2)+1],unpackUint2x32({}).y)));",
               inst, ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), value,
               ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicXor64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                            const IR::Value& offset, std::string_view value) {
    ctx.AddU64(
        "{}=packUint2x32(uvec2(atomicXor({}_ssbo{}[{}>>2],unpackUint2x32({}).x),atomicXor({}_"
        "ssbo{}[({}>>2)+1],unpackUint2x32({}).y)));",
        inst, ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), value, ctx.stage_name,
        binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicExchange64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                                 const IR::Value& offset, std::string_view value) {
    ctx.AddU64("{}=packUint2x32(uvec2(atomicExchange({}_ssbo{}[{}>>2],unpackUint2x32({}).x),"
               "atomicExchange({}_ssbo{}[({}>>2)+1],unpackUint2x32({}).y)));",
               inst, ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), value,
               ctx.stage_name, binding.U32(), ctx.var_alloc.Consume(offset), value);
}

void EmitStorageAtomicAddF32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                             const IR::Value& offset, std::string_view value) {
    SsboCasFunctionF32(ctx, inst, binding, offset, value, "CasFloatAdd");
}

void EmitStorageAtomicAddF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value) {
    SsboCasFunction(ctx, inst, binding, offset, value, "CasFloatAdd16x2");
}

void EmitStorageAtomicAddF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value) {
    SsboCasFunction(ctx, inst, binding, offset, value, "CasFloatAdd32x2");
}

void EmitStorageAtomicMinF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value) {
    SsboCasFunction(ctx, inst, binding, offset, value, "CasFloatMin16x2");
}

void EmitStorageAtomicMinF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value) {
    SsboCasFunction(ctx, inst, binding, offset, value, "CasFloatMin32x2");
}

void EmitStorageAtomicMaxF16x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value) {
    SsboCasFunction(ctx, inst, binding, offset, value, "CasFloatMax16x2");
}

void EmitStorageAtomicMaxF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                               const IR::Value& offset, std::string_view value) {
    SsboCasFunction(ctx, inst, binding, offset, value, "CasFloatMax32x2");
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
