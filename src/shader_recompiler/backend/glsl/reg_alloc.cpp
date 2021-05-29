// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <string_view>

#include <fmt/format.h>

#include "shader_recompiler/backend/glsl/reg_alloc.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLSL {
namespace {
std::string Representation(Id id) {
    if (id.is_condition_code != 0) {
        throw NotImplementedException("Condition code");
    }
    if (id.is_spill != 0) {
        throw NotImplementedException("Spilling");
    }
    const u32 index{static_cast<u32>(id.index)};
    return fmt::format("R{}", index);
}

std::string FormatFloat(std::string_view value, IR::Type type) {
    // TODO: Confirm FP64 nan/inf
    if (type == IR::Type::F32) {
        if (value == "nan") {
            return "uintBitsToFloat(0x7fc00000)";
        }
        if (value == "inf") {
            return "uintBitsToFloat(0x7f800000)";
        }
        if (value == "-inf") {
            return "uintBitsToFloat(0xff800000)";
        }
    }
    const bool needs_dot = value.find_first_of('.') == std::string_view::npos;
    const bool needs_suffix = !value.ends_with('f');
    const auto suffix = type == IR::Type::F32 ? "f" : "lf";
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

std::string RegAlloc::Define(IR::Inst& inst) {
    const Id id{Alloc()};
    inst.SetDefinition<Id>(id);
    return Representation(id);
}

std::string RegAlloc::Define(IR::Inst& inst, Type type) {
    const Id id{Alloc()};
    std::string type_str = "";
    if (!register_defined[id.index]) {
        register_defined[id.index] = true;
        // type_str = GetGlslType(type);
        reg_types.push_back(GetGlslType(type));
        ++num_used_registers;
    }
    inst.SetDefinition<Id>(id);
    return type_str + Representation(id);
}

std::string RegAlloc::Define(IR::Inst& inst, IR::Type type) {
    return Define(inst, RegType(type));
}

std::string RegAlloc::Consume(const IR::Value& value) {
    return value.IsImmediate() ? MakeImm(value) : Consume(*value.InstRecursive());
}

std::string RegAlloc::Consume(IR::Inst& inst) {
    inst.DestructiveRemoveUsage();
    // TODO: reuse variables of same type if possible
    // if (!inst.HasUses()) {
    //     Free(id);
    // }
    return Representation(inst.Definition<Id>());
}

Type RegAlloc::RegType(IR::Type type) {
    switch (type) {
    case IR::Type::U1:
        return Type::U1;
    case IR::Type::U32:
        return Type::U32;
    case IR::Type::F32:
        return Type::F32;
    case IR::Type::U64:
        return Type::U64;
    case IR::Type::F64:
        return Type::F64;
    default:
        throw NotImplementedException("IR type {}", type);
    }
}

std::string RegAlloc::GetGlslType(Type type) {
    switch (type) {
    case Type::U1:
        return "bool ";
    case Type::F16x2:
        return "f16vec2 ";
    case Type::U32:
        return "uint ";
    case Type::S32:
        return "int ";
    case Type::F32:
        return "float ";
    case Type::S64:
        return "int64_t ";
    case Type::U64:
        return "uint64_t ";
    case Type::F64:
        return "double ";
    case Type::U32x2:
        return "uvec2 ";
    case Type::F32x2:
        return "vec2 ";
    case Type::U32x3:
        return "uvec3 ";
    case Type::F32x3:
        return "vec3 ";
    case Type::U32x4:
        return "uvec4 ";
    case Type::F32x4:
        return "vec4 ";
    case Type::Void:
        return "";
    default:
        throw NotImplementedException("Type {}", type);
    }
}

std::string RegAlloc::GetGlslType(IR::Type type) {
    return GetGlslType(RegType(type));
}

Id RegAlloc::Alloc() {
    if (num_used_registers < NUM_REGS) {
        for (size_t reg = 0; reg < NUM_REGS; ++reg) {
            if (register_use[reg]) {
                continue;
            }
            register_use[reg] = true;
            Id ret{};
            ret.is_valid.Assign(1);
            ret.is_long.Assign(0);
            ret.is_spill.Assign(0);
            ret.is_condition_code.Assign(0);
            ret.index.Assign(static_cast<u32>(reg));
            return ret;
        }
    }
    throw NotImplementedException("Register spilling");
}

void RegAlloc::Free(Id id) {
    if (id.is_spill != 0) {
        throw NotImplementedException("Free spill");
    }
    register_use[id.index] = false;
}

} // namespace Shader::Backend::GLSL
