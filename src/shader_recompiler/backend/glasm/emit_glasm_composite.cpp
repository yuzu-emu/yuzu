// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
template <auto read_imm, char type, typename... Values>
void CompositeConstruct(EmitContext& ctx, IR::Inst& inst, Values&&... elements) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (std::ranges::any_of(std::array{elements...},
                            [](const IR::Value& value) { return value.IsImmediate(); })) {
        using Type = std::invoke_result_t<decltype(read_imm), IR::Value>;
        const std::array<Type, 4> values{(elements.IsImmediate() ? (elements.*read_imm)() : 0)...};
        ctx.Add("MOV.{} {},{{{},{},{},{}}};", type, ret, fmt::to_string(values[0]),
                fmt::to_string(values[1]), fmt::to_string(values[2]), fmt::to_string(values[3]));
    }
    size_t index{};
    for (const IR::Value& element : {elements...}) {
        if (!element.IsImmediate()) {
            const ScalarU32 value{ctx.reg_alloc.Consume(element)};
            ctx.Add("MOV.{} {}.{},{};", type, ret, "xyzw"[index], value);
        }
        ++index;
    }
}

void CompositeExtract(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index, char type) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (ret == composite && index == 0) {
        // No need to do anything here, the source and destination are the same register
        return;
    }
    ctx.Add("MOV.{} {}.x,{}.{};", type, ret, composite, "xyzw"[index]);
}

template <typename ObjectType>
void CompositeInsert(EmitContext& ctx, IR::Inst& inst, Register composite, ObjectType object,
                     u32 index, char type) {
    const Register ret{ctx.reg_alloc.Define(inst)};
    const char swizzle{"xyzw"[index]};
    if (ret != composite && ret == object) {
        // The object is aliased with the return value, so we have to use a temporary to insert
        ctx.Add("MOV.{} RC,{};"
                "MOV.{} RC.{},{};"
                "MOV.{} {},RC;",
                type, composite, type, swizzle, object, type, ret);
    } else if (ret != composite) {
        // The input composite is not aliased with the return value so we have to copy it before
        // hand. But the insert object is not aliased with the return value, so we don't have to
        // worry about that
        ctx.Add("MOV.{} {},{};"
                "MOV.{} {}.{},{};",
                type, ret, composite, type, ret, swizzle, object);
    } else {
        // The return value is alised so we can just insert the object, it doesn't matter if it's
        // aliased
        ctx.Add("MOV.{} {}.{},{};", type, ret, swizzle, object);
    }
}
} // Anonymous namespace

void EmitCompositeConstructU32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2) {
    CompositeConstruct<&IR::Value::U32, 'U'>(ctx, inst, e1, e2);
}

void EmitCompositeConstructU32x3(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2, const IR::Value& e3) {
    CompositeConstruct<&IR::Value::U32, 'U'>(ctx, inst, e1, e2, e3);
}

void EmitCompositeConstructU32x4(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2, const IR::Value& e3, const IR::Value& e4) {
    CompositeConstruct<&IR::Value::U32, 'U'>(ctx, inst, e1, e2, e3, e4);
}

void EmitCompositeExtractU32x2(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index) {
    CompositeExtract(ctx, inst, composite, index, 'U');
}

void EmitCompositeExtractU32x3(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index) {
    CompositeExtract(ctx, inst, composite, index, 'U');
}

void EmitCompositeExtractU32x4(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index) {
    CompositeExtract(ctx, inst, composite, index, 'U');
}

void EmitCompositeInsertU32x2([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite,
                              [[maybe_unused]] ScalarU32 object, [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeInsertU32x3([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite,
                              [[maybe_unused]] ScalarU32 object, [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeInsertU32x4([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite,
                              [[maybe_unused]] ScalarU32 object, [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeConstructF16x2([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register e1,
                                 [[maybe_unused]] Register e2) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeConstructF16x3([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register e1,
                                 [[maybe_unused]] Register e2, [[maybe_unused]] Register e3) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeConstructF16x4([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] Register e1,
                                 [[maybe_unused]] Register e2, [[maybe_unused]] Register e3,
                                 [[maybe_unused]] Register e4) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeExtractF16x2([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] Register composite, [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeExtractF16x3([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] Register composite, [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeExtractF16x4([[maybe_unused]] EmitContext& ctx,
                               [[maybe_unused]] Register composite, [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeInsertF16x2([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite, [[maybe_unused]] Register object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeInsertF16x3([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite, [[maybe_unused]] Register object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeInsertF16x4([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite, [[maybe_unused]] Register object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeConstructF32x2(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2) {
    CompositeConstruct<&IR::Value::F32, 'F'>(ctx, inst, e1, e2);
}

void EmitCompositeConstructF32x3(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2, const IR::Value& e3) {
    CompositeConstruct<&IR::Value::F32, 'F'>(ctx, inst, e1, e2, e3);
}

void EmitCompositeConstructF32x4(EmitContext& ctx, IR::Inst& inst, const IR::Value& e1,
                                 const IR::Value& e2, const IR::Value& e3, const IR::Value& e4) {
    CompositeConstruct<&IR::Value::F32, 'F'>(ctx, inst, e1, e2, e3, e4);
}

void EmitCompositeExtractF32x2(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index) {
    CompositeExtract(ctx, inst, composite, index, 'F');
}

void EmitCompositeExtractF32x3(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index) {
    CompositeExtract(ctx, inst, composite, index, 'F');
}

void EmitCompositeExtractF32x4(EmitContext& ctx, IR::Inst& inst, Register composite, u32 index) {
    CompositeExtract(ctx, inst, composite, index, 'F');
}

void EmitCompositeInsertF32x2(EmitContext& ctx, IR::Inst& inst, Register composite,
                              ScalarF32 object, u32 index) {
    CompositeInsert(ctx, inst, composite, object, index, 'F');
}

void EmitCompositeInsertF32x3(EmitContext& ctx, IR::Inst& inst, Register composite,
                              ScalarF32 object, u32 index) {
    CompositeInsert(ctx, inst, composite, object, index, 'F');
}

void EmitCompositeInsertF32x4(EmitContext& ctx, IR::Inst& inst, Register composite,
                              ScalarF32 object, u32 index) {
    CompositeInsert(ctx, inst, composite, object, index, 'F');
}

void EmitCompositeConstructF64x2([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeConstructF64x3([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeConstructF64x4([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeExtractF64x2([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeExtractF64x3([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeExtractF64x4([[maybe_unused]] EmitContext& ctx) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeInsertF64x2([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite, [[maybe_unused]] Register object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeInsertF64x3([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite, [[maybe_unused]] Register object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

void EmitCompositeInsertF64x4([[maybe_unused]] EmitContext& ctx,
                              [[maybe_unused]] Register composite, [[maybe_unused]] Register object,
                              [[maybe_unused]] u32 index) {
    throw NotImplementedException("GLASM instruction");
}

} // namespace Shader::Backend::GLASM
