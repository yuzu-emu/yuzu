// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <string>
#include <tuple>

#include "common/div_ceil.h"
#include "common/settings.h"
#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::GLASM {
namespace {
template <class Func>
struct FuncTraits {};

template <class ReturnType_, class... Args>
struct FuncTraits<ReturnType_ (*)(Args...)> {
    using ReturnType = ReturnType_;

    static constexpr size_t NUM_ARGS = sizeof...(Args);

    template <size_t I>
    using ArgType = std::tuple_element_t<I, std::tuple<Args...>>;
};

template <typename T>
struct Identity {
    Identity(T data_) : data{data_} {}

    T Extract() {
        return data;
    }

    T data;
};

template <bool scalar>
class RegWrapper {
public:
    RegWrapper(EmitContext& ctx, const IR::Value& ir_value) : reg_alloc{ctx.reg_alloc} {
        const Value value{reg_alloc.Peek(ir_value)};
        if (value.type == Type::Register) {
            inst = ir_value.InstRecursive();
            reg = Register{value};
        } else {
            reg = value.type == Type::U64 ? reg_alloc.AllocLongReg() : reg_alloc.AllocReg();
        }
        switch (value.type) {
        case Type::Register:
        case Type::Void:
            break;
        case Type::U32:
            ctx.Add("MOV.U {}.x,{};", reg, value.imm_u32);
            break;
        case Type::U64:
            ctx.Add("MOV.U64 {}.x,{};", reg, value.imm_u64);
            break;
        }
    }

    auto Extract() {
        if (inst) {
            reg_alloc.Unref(*inst);
        } else {
            reg_alloc.FreeReg(reg);
        }
        return std::conditional_t<scalar, ScalarRegister, Register>{Value{reg}};
    }

private:
    RegAlloc& reg_alloc;
    IR::Inst* inst{};
    Register reg{};
};

template <typename ArgType>
class ValueWrapper {
public:
    ValueWrapper(EmitContext& ctx, const IR::Value& ir_value_)
        : reg_alloc{ctx.reg_alloc}, ir_value{ir_value_}, value{reg_alloc.Peek(ir_value)} {}

    ArgType Extract() {
        if (!ir_value.IsImmediate()) {
            reg_alloc.Unref(*ir_value.InstRecursive());
        }
        return value;
    }

private:
    RegAlloc& reg_alloc;
    const IR::Value& ir_value;
    ArgType value;
};

template <typename ArgType>
auto Arg(EmitContext& ctx, const IR::Value& arg) {
    if constexpr (std::is_same_v<ArgType, Register>) {
        return RegWrapper<false>{ctx, arg};
    } else if constexpr (std::is_same_v<ArgType, ScalarRegister>) {
        return RegWrapper<true>{ctx, arg};
    } else if constexpr (std::is_base_of_v<Value, ArgType>) {
        return ValueWrapper<ArgType>{ctx, arg};
    } else if constexpr (std::is_same_v<ArgType, const IR::Value&>) {
        return Identity<const IR::Value&>{arg};
    } else if constexpr (std::is_same_v<ArgType, u32>) {
        return Identity{arg.U32()};
    } else if constexpr (std::is_same_v<ArgType, IR::Attribute>) {
        return Identity{arg.Attribute()};
    } else if constexpr (std::is_same_v<ArgType, IR::Patch>) {
        return Identity{arg.Patch()};
    } else if constexpr (std::is_same_v<ArgType, IR::Reg>) {
        return Identity{arg.Reg()};
    }
}

template <auto func, bool is_first_arg_inst>
struct InvokeCall {
    template <typename... Args>
    InvokeCall(EmitContext& ctx, IR::Inst* inst, Args&&... args) {
        if constexpr (is_first_arg_inst) {
            func(ctx, *inst, args.Extract()...);
        } else {
            func(ctx, args.Extract()...);
        }
    }
};

template <auto func, bool is_first_arg_inst, size_t... I>
void Invoke(EmitContext& ctx, IR::Inst* inst, std::index_sequence<I...>) {
    using Traits = FuncTraits<decltype(func)>;
    if constexpr (is_first_arg_inst) {
        InvokeCall<func, is_first_arg_inst>{
            ctx, inst, Arg<typename Traits::template ArgType<I + 2>>(ctx, inst->Arg(I))...};
    } else {
        InvokeCall<func, is_first_arg_inst>{
            ctx, inst, Arg<typename Traits::template ArgType<I + 1>>(ctx, inst->Arg(I))...};
    }
}

template <auto func>
void Invoke(EmitContext& ctx, IR::Inst* inst) {
    using Traits = FuncTraits<decltype(func)>;
    static_assert(Traits::NUM_ARGS >= 1, "Insufficient arguments");
    if constexpr (Traits::NUM_ARGS == 1) {
        Invoke<func, false>(ctx, inst, std::make_index_sequence<0>{});
    } else {
        using FirstArgType = typename Traits::template ArgType<1>;
        static constexpr bool is_first_arg_inst = std::is_same_v<FirstArgType, IR::Inst&>;
        using Indices = std::make_index_sequence<Traits::NUM_ARGS - (is_first_arg_inst ? 2 : 1)>;
        Invoke<func, is_first_arg_inst>(ctx, inst, Indices{});
    }
}

void EmitInst(EmitContext& ctx, IR::Inst* inst) {
    switch (inst->GetOpcode()) {
#define OPCODE(name, result_type, ...)                                                             \
    case IR::Opcode::name:                                                                         \
        return Invoke<&Emit##name>(ctx, inst);
#include "shader_recompiler/frontend/ir/opcodes.inc"
#undef OPCODE
    }
    throw LogicError("Invalid opcode {}", inst->GetOpcode());
}

bool IsReference(IR::Inst& inst) {
    return inst.GetOpcode() == IR::Opcode::Reference;
}

void PrecolorInst(IR::Inst& phi) {
    // Insert phi moves before references to avoid overwritting other phis
    const size_t num_args{phi.NumArgs()};
    for (size_t i = 0; i < num_args; ++i) {
        IR::Block& phi_block{*phi.PhiBlock(i)};
        auto it{std::find_if_not(phi_block.rbegin(), phi_block.rend(), IsReference).base()};
        IR::IREmitter ir{phi_block, it};
        const IR::Value arg{phi.Arg(i)};
        if (arg.IsImmediate()) {
            ir.PhiMove(phi, arg);
        } else {
            ir.PhiMove(phi, IR::Value{&RegAlloc::AliasInst(*arg.Inst())});
        }
    }
    for (size_t i = 0; i < num_args; ++i) {
        IR::IREmitter{*phi.PhiBlock(i)}.Reference(IR::Value{&phi});
    }
}

void Precolor(const IR::Program& program) {
    for (IR::Block* const block : program.blocks) {
        for (IR::Inst& phi : block->Instructions()) {
            if (!IR::IsPhi(phi)) {
                break;
            }
            PrecolorInst(phi);
        }
    }
}

void EmitCode(EmitContext& ctx, const IR::Program& program) {
    const auto eval{
        [&](const IR::U1& cond) { return ScalarS32{ctx.reg_alloc.Consume(IR::Value{cond})}; }};
    for (const IR::AbstractSyntaxNode& node : program.syntax_list) {
        switch (node.type) {
        case IR::AbstractSyntaxNode::Type::Block:
            for (IR::Inst& inst : node.data.block->Instructions()) {
                EmitInst(ctx, &inst);
            }
            break;
        case IR::AbstractSyntaxNode::Type::If:
            ctx.Add("MOV.S.CC RC,{};"
                    "IF NE.x;",
                    eval(node.data.if_node.cond));
            break;
        case IR::AbstractSyntaxNode::Type::EndIf:
            ctx.Add("ENDIF;");
            break;
        case IR::AbstractSyntaxNode::Type::Loop:
            ctx.Add("REP;");
            break;
        case IR::AbstractSyntaxNode::Type::Repeat:
            if (!Settings::values.disable_shader_loop_safety_checks) {
                const u32 loop_index{ctx.num_safety_loop_vars++};
                const u32 vector_index{loop_index / 4};
                const char component{"xyzw"[loop_index % 4]};
                ctx.Add("SUB.S.CC loop{}.{},loop{}.{},1;"
                        "BRK(LT.{});",
                        vector_index, component, vector_index, component, component);
            }
            if (node.data.repeat.cond.IsImmediate()) {
                if (node.data.repeat.cond.U1()) {
                    ctx.Add("ENDREP;");
                } else {
                    ctx.Add("BRK;"
                            "ENDREP;");
                }
            } else {
                ctx.Add("MOV.S.CC RC,{};"
                        "BRK(EQ.x);"
                        "ENDREP;",
                        eval(node.data.repeat.cond));
            }
            break;
        case IR::AbstractSyntaxNode::Type::Break:
            if (node.data.break_node.cond.IsImmediate()) {
                if (node.data.break_node.cond.U1()) {
                    ctx.Add("BRK;");
                }
            } else {
                ctx.Add("MOV.S.CC RC,{};"
                        "BRK (NE.x);",
                        eval(node.data.break_node.cond));
            }
            break;
        case IR::AbstractSyntaxNode::Type::Return:
        case IR::AbstractSyntaxNode::Type::Unreachable:
            ctx.Add("RET;");
            break;
        }
    }
    if (!ctx.reg_alloc.IsEmpty()) {
        LOG_WARNING(Shader_GLASM, "Register leak after generating code");
    }
}

void SetupOptions(const IR::Program& program, const Profile& profile,
                  const RuntimeInfo& runtime_info, std::string& header) {
    const Info& info{program.info};
    const Stage stage{program.stage};

    // TODO: Track the shared atomic ops
    header += "OPTION NV_internal;"
              "OPTION NV_shader_storage_buffer;"
              "OPTION NV_gpu_program_fp64;";
    if (info.uses_int64_bit_atomics) {
        header += "OPTION NV_shader_atomic_int64;";
    }
    if (info.uses_atomic_f32_add) {
        header += "OPTION NV_shader_atomic_float;";
    }
    if (info.uses_atomic_f16x2_add || info.uses_atomic_f16x2_min || info.uses_atomic_f16x2_max) {
        header += "OPTION NV_shader_atomic_fp16_vector;";
    }
    if (info.uses_subgroup_invocation_id || info.uses_subgroup_mask || info.uses_subgroup_vote ||
        info.uses_fswzadd) {
        header += "OPTION NV_shader_thread_group;";
    }
    if (info.uses_subgroup_shuffles) {
        header += "OPTION NV_shader_thread_shuffle;";
    }
    if (info.uses_sparse_residency) {
        header += "OPTION EXT_sparse_texture2;";
    }
    const bool stores_viewport_layer{info.stores[IR::Attribute::ViewportIndex] ||
                                     info.stores[IR::Attribute::Layer]};
    if ((stage != Stage::Geometry && stores_viewport_layer) ||
        info.stores[IR::Attribute::ViewportMask]) {
        if (profile.support_viewport_index_layer_non_geometry) {
            header += "OPTION NV_viewport_array2;";
        }
    }
    if (program.is_geometry_passthrough && profile.support_geometry_shader_passthrough) {
        header += "OPTION NV_geometry_shader_passthrough;";
    }
    if (info.uses_typeless_image_reads && profile.support_typeless_image_loads) {
        header += "OPTION EXT_shader_image_load_formatted;";
    }
    if (profile.support_derivative_control) {
        header += "OPTION ARB_derivative_control;";
    }
    if (stage == Stage::Fragment && runtime_info.force_early_z != 0) {
        header += "OPTION NV_early_fragment_tests;";
    }
    if (stage == Stage::Fragment) {
        header += "OPTION ARB_draw_buffers;";
    }
}

std::string_view StageHeader(Stage stage) {
    switch (stage) {
    case Stage::VertexA:
    case Stage::VertexB:
        return "!!NVvp5.0\n";
    case Stage::TessellationControl:
        return "!!NVtcp5.0\n";
    case Stage::TessellationEval:
        return "!!NVtep5.0\n";
    case Stage::Geometry:
        return "!!NVgp5.0\n";
    case Stage::Fragment:
        return "!!NVfp5.0\n";
    case Stage::Compute:
        return "!!NVcp5.0\n";
    }
    throw InvalidArgument("Invalid stage {}", stage);
}

std::string_view InputPrimitive(InputTopology topology) {
    switch (topology) {
    case InputTopology::Points:
        return "POINTS";
    case InputTopology::Lines:
        return "LINES";
    case InputTopology::LinesAdjacency:
        return "LINESS_ADJACENCY";
    case InputTopology::Triangles:
        return "TRIANGLES";
    case InputTopology::TrianglesAdjacency:
        return "TRIANGLES_ADJACENCY";
    }
    throw InvalidArgument("Invalid input topology {}", topology);
}

std::string_view OutputPrimitive(OutputTopology topology) {
    switch (topology) {
    case OutputTopology::PointList:
        return "POINTS";
    case OutputTopology::LineStrip:
        return "LINE_STRIP";
    case OutputTopology::TriangleStrip:
        return "TRIANGLE_STRIP";
    }
    throw InvalidArgument("Invalid output topology {}", topology);
}

std::string_view GetTessMode(TessPrimitive primitive) {
    switch (primitive) {
    case TessPrimitive::Triangles:
        return "TRIANGLES";
    case TessPrimitive::Quads:
        return "QUADS";
    case TessPrimitive::Isolines:
        return "ISOLINES";
    }
    throw InvalidArgument("Invalid tessellation primitive {}", primitive);
}

std::string_view GetTessSpacing(TessSpacing spacing) {
    switch (spacing) {
    case TessSpacing::Equal:
        return "EQUAL";
    case TessSpacing::FractionalOdd:
        return "FRACTIONAL_ODD";
    case TessSpacing::FractionalEven:
        return "FRACTIONAL_EVEN";
    }
    throw InvalidArgument("Invalid tessellation spacing {}", spacing);
}
} // Anonymous namespace

std::string EmitGLASM(const Profile& profile, const RuntimeInfo& runtime_info, IR::Program& program,
                      Bindings& bindings) {
    EmitContext ctx{program, bindings, profile, runtime_info};
    Precolor(program);
    EmitCode(ctx, program);
    std::string header{StageHeader(program.stage)};
    SetupOptions(program, profile, runtime_info, header);
    switch (program.stage) {
    case Stage::TessellationControl:
        header += fmt::format("VERTICES_OUT {};", program.invocations);
        break;
    case Stage::TessellationEval:
        header += fmt::format("TESS_MODE {};"
                              "TESS_SPACING {};"
                              "TESS_VERTEX_ORDER {};",
                              GetTessMode(runtime_info.tess_primitive),
                              GetTessSpacing(runtime_info.tess_spacing),
                              runtime_info.tess_clockwise ? "CW" : "CCW");
        break;
    case Stage::Geometry:
        header += fmt::format("PRIMITIVE_IN {};", InputPrimitive(runtime_info.input_topology));
        if (program.is_geometry_passthrough) {
            if (profile.support_geometry_shader_passthrough) {
                for (size_t index = 0; index < IR::NUM_GENERICS; ++index) {
                    if (program.info.passthrough.Generic(index)) {
                        header += fmt::format("PASSTHROUGH result.attrib[{}];", index);
                    }
                }
                if (program.info.passthrough.AnyComponent(IR::Attribute::PositionX)) {
                    header += "PASSTHROUGH result.position;";
                }
            } else {
                LOG_WARNING(Shader_GLASM, "Passthrough geometry program used but not supported");
            }
        } else {
            header +=
                fmt::format("VERTICES_OUT {};"
                            "PRIMITIVE_OUT {};",
                            program.output_vertices, OutputPrimitive(program.output_topology));
        }
        break;
    case Stage::Compute:
        header += fmt::format("GROUP_SIZE {} {} {};", program.workgroup_size[0],
                              program.workgroup_size[1], program.workgroup_size[2]);
        break;
    default:
        break;
    }
    if (program.shared_memory_size > 0) {
        header += fmt::format("SHARED_MEMORY {};", program.shared_memory_size);
        header += fmt::format("SHARED shared_mem[]={{program.sharedmem}};");
    }
    header += "TEMP ";
    for (size_t index = 0; index < ctx.reg_alloc.NumUsedRegisters(); ++index) {
        header += fmt::format("R{},", index);
    }
    if (program.local_memory_size > 0) {
        header += fmt::format("lmem[{}],", program.local_memory_size);
    }
    if (program.info.uses_fswzadd) {
        header += "FSWZA[4],FSWZB[4],";
    }
    const u32 num_safety_loop_vectors{Common::DivCeil(ctx.num_safety_loop_vars, 4u)};
    for (u32 index = 0; index < num_safety_loop_vectors; ++index) {
        header += fmt::format("loop{},", index);
    }
    header += "RC;"
              "LONG TEMP ";
    for (size_t index = 0; index < ctx.reg_alloc.NumUsedLongRegisters(); ++index) {
        header += fmt::format("D{},", index);
    }
    header += "DC;";
    if (program.info.uses_fswzadd) {
        header += "MOV.F FSWZA[0],-1;"
                  "MOV.F FSWZA[1],1;"
                  "MOV.F FSWZA[2],-1;"
                  "MOV.F FSWZA[3],0;"
                  "MOV.F FSWZB[0],-1;"
                  "MOV.F FSWZB[1],-1;"
                  "MOV.F FSWZB[2],1;"
                  "MOV.F FSWZB[3],-1;";
    }
    for (u32 index = 0; index < num_safety_loop_vectors; ++index) {
        header += fmt::format("MOV.S loop{},{{0x2000,0x2000,0x2000,0x2000}};", index);
    }
    if (ctx.uses_y_direction) {
        header += "PARAM y_direction[1]={state.material.front.ambient};";
    }
    ctx.code.insert(0, header);
    ctx.code += "END";
    return ctx.code;
}

} // namespace Shader::Backend::GLASM
