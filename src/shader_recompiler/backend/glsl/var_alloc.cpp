// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <string_view>

#include <fmt/format.h>

#include "shader_recompiler/backend/glsl/var_alloc.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
std::string TypePrefix(GlslVarType type) {
    switch (type) {
    case GlslVarType::U1:
        return "b_";
    case GlslVarType::F16x2:
        return "f16x2_";
    case GlslVarType::U32:
        return "u_";
    case GlslVarType::F32:
        return "f_";
    case GlslVarType::U64:
        return "u64_";
    case GlslVarType::F64:
        return "d_";
    case GlslVarType::U32x2:
        return "u2_";
    case GlslVarType::F32x2:
        return "f2_";
    case GlslVarType::U32x3:
        return "u3_";
    case GlslVarType::F32x3:
        return "f3_";
    case GlslVarType::U32x4:
        return "u4_";
    case GlslVarType::F32x4:
        return "f4_";
    case GlslVarType::PrecF32:
        return "pf_";
    case GlslVarType::PrecF64:
        return "pd_";
    case GlslVarType::Void:
        return "";
    default:
        throw NotImplementedException("Type {}", type);
    }
}

std::string FormatFloat(std::string_view value, IR::Type type) {
    // TODO: Confirm FP64 nan/inf
    if (type == IR::Type::F32) {
        if (value == "nan") {
            return "utof(0x7fc00000)";
        }
        if (value == "inf") {
            return "utof(0x7f800000)";
        }
        if (value == "-inf") {
            return "utof(0xff800000)";
        }
    }
    if (value.find_first_of('e') != std::string_view::npos) {
        // scientific notation
        const auto cast{type == IR::Type::F32 ? "float" : "double"};
        return fmt::format("{}({})", cast, value);
    }
    const bool needs_dot{value.find_first_of('.') == std::string_view::npos};
    const bool needs_suffix{!value.ends_with('f')};
    const auto suffix{type == IR::Type::F32 ? "f" : "lf"};
    return fmt::format("{}{}{}", value, needs_dot ? "." : "", needs_suffix ? suffix : "");
}

std::string MakeImm(const IR::Value& value) {
    switch (value.Type()) {
    case IR::Type::U1:
        return fmt::format("{}", value.U1() ? "true" : "false");
    case IR::Type::U32:
        return fmt::format("{}u", value.U32());
    case IR::Type::F32:
        return FormatFloat(fmt::format("{}", value.F32()), IR::Type::F32);
    case IR::Type::U64:
        return fmt::format("{}ul", value.U64());
    case IR::Type::F64:
        return FormatFloat(fmt::format("{}", value.F64()), IR::Type::F64);
    case IR::Type::Void:
        return "";
    default:
        throw NotImplementedException("Immediate type {}", value.Type());
    }
}
} // Anonymous namespace

std::string VarAlloc::Representation(u32 index, GlslVarType type) const {
    const auto prefix{TypePrefix(type)};
    return fmt::format("{}{}", prefix, index);
}

std::string VarAlloc::Representation(Id id) const {
    return Representation(id.index, id.type);
}

std::string VarAlloc::Define(IR::Inst& inst, GlslVarType type) {
    if (inst.HasUses()) {
        inst.SetDefinition<Id>(Alloc(type));
        return Representation(inst.Definition<Id>());
    } else {
        Id id{};
        id.type.Assign(type);
        GetUseTracker(type).uses_temp = true;
        inst.SetDefinition<Id>(id);
        return 't' + Representation(inst.Definition<Id>());
    }
}

std::string VarAlloc::Define(IR::Inst& inst, IR::Type type) {
    return Define(inst, RegType(type));
}

std::string VarAlloc::PhiDefine(IR::Inst& inst, IR::Type type) {
    return AddDefine(inst, RegType(type));
}

std::string VarAlloc::AddDefine(IR::Inst& inst, GlslVarType type) {
    if (inst.HasUses()) {
        inst.SetDefinition<Id>(Alloc(type));
        return Representation(inst.Definition<Id>());
    } else {
        return "";
    }
    return Representation(inst.Definition<Id>());
}

std::string VarAlloc::Consume(const IR::Value& value) {
    return value.IsImmediate() ? MakeImm(value) : ConsumeInst(*value.InstRecursive());
}

std::string VarAlloc::ConsumeInst(IR::Inst& inst) {
    inst.DestructiveRemoveUsage();
    if (!inst.HasUses()) {
        Free(inst.Definition<Id>());
    }
    return Representation(inst.Definition<Id>());
}

std::string VarAlloc::GetGlslType(IR::Type type) const {
    return GetGlslType(RegType(type));
}

Id VarAlloc::Alloc(GlslVarType type) {
    auto& use_tracker{GetUseTracker(type)};
    const auto num_vars{use_tracker.var_use.size()};
    for (size_t var = 0; var < num_vars; ++var) {
        if (use_tracker.var_use[var]) {
            continue;
        }
        use_tracker.num_used = std::max(use_tracker.num_used, var + 1);
        use_tracker.var_use[var] = true;
        Id ret{};
        ret.is_valid.Assign(1);
        ret.type.Assign(type);
        ret.index.Assign(static_cast<u32>(var));
        return ret;
    }
    // Allocate a new variable
    use_tracker.var_use.push_back(true);
    Id ret{};
    ret.is_valid.Assign(1);
    ret.type.Assign(type);
    ret.index.Assign(static_cast<u32>(use_tracker.num_used));
    ++use_tracker.num_used;
    return ret;
}

void VarAlloc::Free(Id id) {
    if (id.is_valid == 0) {
        throw LogicError("Freeing invalid variable");
    }
    auto& use_tracker{GetUseTracker(id.type)};
    use_tracker.var_use[id.index] = false;
}

GlslVarType VarAlloc::RegType(IR::Type type) const {
    switch (type) {
    case IR::Type::U1:
        return GlslVarType::U1;
    case IR::Type::U32:
        return GlslVarType::U32;
    case IR::Type::F32:
        return GlslVarType::F32;
    case IR::Type::U64:
        return GlslVarType::U64;
    case IR::Type::F64:
        return GlslVarType::F64;
    default:
        throw NotImplementedException("IR type {}", type);
    }
}

std::string VarAlloc::GetGlslType(GlslVarType type) const {
    switch (type) {
    case GlslVarType::U1:
        return "bool";
    case GlslVarType::F16x2:
        return "f16vec2";
    case GlslVarType::U32:
        return "uint";
    case GlslVarType::F32:
    case GlslVarType::PrecF32:
        return "float";
    case GlslVarType::U64:
        return "uint64_t";
    case GlslVarType::F64:
    case GlslVarType::PrecF64:
        return "double";
    case GlslVarType::U32x2:
        return "uvec2";
    case GlslVarType::F32x2:
        return "vec2";
    case GlslVarType::U32x3:
        return "uvec3";
    case GlslVarType::F32x3:
        return "vec3";
    case GlslVarType::U32x4:
        return "uvec4";
    case GlslVarType::F32x4:
        return "vec4";
    case GlslVarType::Void:
        return "";
    default:
        throw NotImplementedException("Type {}", type);
    }
}

VarAlloc::UseTracker& VarAlloc::GetUseTracker(GlslVarType type) {
    switch (type) {
    case GlslVarType::U1:
        return var_bool;
    case GlslVarType::F16x2:
        return var_f16x2;
    case GlslVarType::U32:
        return var_u32;
    case GlslVarType::F32:
        return var_f32;
    case GlslVarType::U64:
        return var_u64;
    case GlslVarType::F64:
        return var_f64;
    case GlslVarType::U32x2:
        return var_u32x2;
    case GlslVarType::F32x2:
        return var_f32x2;
    case GlslVarType::U32x3:
        return var_u32x3;
    case GlslVarType::F32x3:
        return var_f32x3;
    case GlslVarType::U32x4:
        return var_u32x4;
    case GlslVarType::F32x4:
        return var_f32x4;
    case GlslVarType::PrecF32:
        return var_precf32;
    case GlslVarType::PrecF64:
        return var_precf64;
    default:
        throw NotImplementedException("Type {}", type);
    }
}

const VarAlloc::UseTracker& VarAlloc::GetUseTracker(GlslVarType type) const {
    switch (type) {
    case GlslVarType::U1:
        return var_bool;
    case GlslVarType::F16x2:
        return var_f16x2;
    case GlslVarType::U32:
        return var_u32;
    case GlslVarType::F32:
        return var_f32;
    case GlslVarType::U64:
        return var_u64;
    case GlslVarType::F64:
        return var_f64;
    case GlslVarType::U32x2:
        return var_u32x2;
    case GlslVarType::F32x2:
        return var_f32x2;
    case GlslVarType::U32x3:
        return var_u32x3;
    case GlslVarType::F32x3:
        return var_f32x3;
    case GlslVarType::U32x4:
        return var_u32x4;
    case GlslVarType::F32x4:
        return var_f32x4;
    case GlslVarType::PrecF32:
        return var_precf32;
    case GlslVarType::PrecF64:
        return var_precf64;
    default:
        throw NotImplementedException("Type {}", type);
    }
}

} // namespace Shader::Backend::GLSL
