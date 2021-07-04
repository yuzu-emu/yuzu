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
constexpr char cas_loop[]{"for(;;){{uint old_value={};uint "
                          "cas_result=atomicCompSwap({},old_value,bitfieldInsert({},{},{},{}));"
                          "if(cas_result==old_value){{break;}}}}"};

void SsboWriteCas(EmitContext& ctx, const IR::Value& binding, std::string_view offset_var,
                  std::string_view value, std::string_view bit_offset, u32 num_bits) {
    const auto ssbo{fmt::format("{}_ssbo{}[{}>>2]", ctx.stage_name, binding.U32(), offset_var)};
    ctx.Add(cas_loop, ssbo, ssbo, ssbo, value, bit_offset, num_bits);
}
} // Anonymous namespace

void EmitLoadGlobalU8(EmitContext&) {
    NotImplemented();
}

void EmitLoadGlobalS8(EmitContext&) {
    NotImplemented();
}

void EmitLoadGlobalU16(EmitContext&) {
    NotImplemented();
}

void EmitLoadGlobalS16(EmitContext&) {
    NotImplemented();
}

void EmitLoadGlobal32(EmitContext& ctx, IR::Inst& inst, std::string_view address) {
    if (ctx.profile.support_int64) {
        return ctx.AddU32("{}=LoadGlobal32({});", inst, address);
    }
    LOG_WARNING(Shader_GLSL, "Int64 not supported, ignoring memory operation");
    ctx.AddU32("{}=0u;", inst);
}

void EmitLoadGlobal64(EmitContext& ctx, IR::Inst& inst, std::string_view address) {
    if (ctx.profile.support_int64) {
        return ctx.AddU32x2("{}=LoadGlobal64({});", inst, address);
    }
    LOG_WARNING(Shader_GLSL, "Int64 not supported, ignoring memory operation");
    ctx.AddU32x2("{}=uvec2(0);", inst);
}

void EmitLoadGlobal128(EmitContext& ctx, IR::Inst& inst, std::string_view address) {
    if (ctx.profile.support_int64) {
        return ctx.AddU32x4("{}=LoadGlobal128({});", inst, address);
    }
    LOG_WARNING(Shader_GLSL, "Int64 not supported, ignoring memory operation");
    ctx.AddU32x4("{}=uvec4(0);", inst);
}

void EmitWriteGlobalU8(EmitContext&) {
    NotImplemented();
}

void EmitWriteGlobalS8(EmitContext&) {
    NotImplemented();
}

void EmitWriteGlobalU16(EmitContext&) {
    NotImplemented();
}

void EmitWriteGlobalS16(EmitContext&) {
    NotImplemented();
}

void EmitWriteGlobal32(EmitContext& ctx, std::string_view address, std::string_view value) {
    if (ctx.profile.support_int64) {
        return ctx.Add("WriteGlobal32({},{});", address, value);
    }
    LOG_WARNING(Shader_GLSL, "Int64 not supported, ignoring memory operation");
}

void EmitWriteGlobal64(EmitContext& ctx, std::string_view address, std::string_view value) {
    if (ctx.profile.support_int64) {
        return ctx.Add("WriteGlobal64({},{});", address, value);
    }
    LOG_WARNING(Shader_GLSL, "Int64 not supported, ignoring memory operation");
}

void EmitWriteGlobal128(EmitContext& ctx, std::string_view address, std::string_view value) {
    if (ctx.profile.support_int64) {
        return ctx.Add("WriteGlobal128({},{});", address, value);
    }
    LOG_WARNING(Shader_GLSL, "Int64 not supported, ignoring memory operation");
}

void EmitLoadStorageU8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.AddU32("{}=bitfieldExtract({}_ssbo{}[{}>>2],int({}%4)*8,8);", inst, ctx.stage_name,
               binding.U32(), offset_var, offset_var);
}

void EmitLoadStorageS8(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.AddU32("{}=bitfieldExtract(int({}_ssbo{}[{}>>2]),int({}%4)*8,8);", inst, ctx.stage_name,
               binding.U32(), offset_var, offset_var);
}

void EmitLoadStorageU16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.AddU32("{}=bitfieldExtract({}_ssbo{}[{}>>2],int(({}>>1)%2)*16,16);", inst, ctx.stage_name,
               binding.U32(), offset_var, offset_var);
}

void EmitLoadStorageS16(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.AddU32("{}=bitfieldExtract(int({}_ssbo{}[{}>>2]),int(({}>>1)%2)*16,16);", inst,
               ctx.stage_name, binding.U32(), offset_var, offset_var);
}

void EmitLoadStorage32(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.AddU32("{}={}_ssbo{}[{}>>2];", inst, ctx.stage_name, binding.U32(), offset_var);
}

void EmitLoadStorage64(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                       const IR::Value& offset) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.AddU32x2("{}=uvec2({}_ssbo{}[{}>>2],{}_ssbo{}[({}+4)>>2]);", inst, ctx.stage_name,
                 binding.U32(), offset_var, ctx.stage_name, binding.U32(), offset_var);
}

void EmitLoadStorage128(EmitContext& ctx, IR::Inst& inst, const IR::Value& binding,
                        const IR::Value& offset) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.AddU32x4("{}=uvec4({}_ssbo{}[{}>>2],{}_ssbo{}[({}+4)>>2],{}_ssbo{}[({}+8)>>2],{}_ssbo{}[({}"
                 "+12)>>2]);",
                 inst, ctx.stage_name, binding.U32(), offset_var, ctx.stage_name, binding.U32(),
                 offset_var, ctx.stage_name, binding.U32(), offset_var, ctx.stage_name,
                 binding.U32(), offset_var);
}

void EmitWriteStorageU8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    const auto bit_offset{fmt::format("int({}%4)*8", offset_var)};
    SsboWriteCas(ctx, binding, offset_var, value, bit_offset, 8);
}

void EmitWriteStorageS8(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    const auto bit_offset{fmt::format("int({}%4)*8", offset_var)};
    SsboWriteCas(ctx, binding, offset_var, value, bit_offset, 8);
}

void EmitWriteStorageU16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         std::string_view value) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    const auto bit_offset{fmt::format("int(({}>>1)%2)*16", offset_var)};
    SsboWriteCas(ctx, binding, offset_var, value, bit_offset, 16);
}

void EmitWriteStorageS16(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         std::string_view value) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    const auto bit_offset{fmt::format("int(({}>>1)%2)*16", offset_var)};
    SsboWriteCas(ctx, binding, offset_var, value, bit_offset, 16);
}

void EmitWriteStorage32(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.Add("{}_ssbo{}[{}>>2]={};", ctx.stage_name, binding.U32(), offset_var, value);
}

void EmitWriteStorage64(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                        std::string_view value) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.Add("{}_ssbo{}[{}>>2]={}.x;", ctx.stage_name, binding.U32(), offset_var, value);
    ctx.Add("{}_ssbo{}[({}+4)>>2]={}.y;", ctx.stage_name, binding.U32(), offset_var, value);
}

void EmitWriteStorage128(EmitContext& ctx, const IR::Value& binding, const IR::Value& offset,
                         std::string_view value) {
    const auto offset_var{ctx.var_alloc.Consume(offset)};
    ctx.Add("{}_ssbo{}[{}>>2]={}.x;", ctx.stage_name, binding.U32(), offset_var, value);
    ctx.Add("{}_ssbo{}[({}+4)>>2]={}.y;", ctx.stage_name, binding.U32(), offset_var, value);
    ctx.Add("{}_ssbo{}[({}+8)>>2]={}.z;", ctx.stage_name, binding.U32(), offset_var, value);
    ctx.Add("{}_ssbo{}[({}+12)>>2]={}.w;", ctx.stage_name, binding.U32(), offset_var, value);
}
} // namespace Shader::Backend::GLSL
