// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>

#include <fmt/format.h>

#include "common/bit_cast.h"
#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/exception.h"

namespace Shader::IR {
class Inst;
class Value;
} // namespace Shader::IR

namespace Shader::Backend::GLASM {

class EmitContext;

enum class Type : u32 {
    Void,
    Register,
    U32,
    U64,
};

struct Id {
    union {
        u32 raw;
        BitField<0, 1, u32> is_valid;
        BitField<1, 1, u32> is_long;
        BitField<2, 1, u32> is_spill;
        BitField<3, 1, u32> is_condition_code;
        BitField<4, 1, u32> is_null;
        BitField<5, 27, u32> index;
    };

    bool operator==(Id rhs) const noexcept {
        return raw == rhs.raw;
    }
    bool operator!=(Id rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(sizeof(Id) == sizeof(u32));

struct Value {
    Type type;
    union {
        Id id;
        u32 imm_u32;
        u64 imm_u64;
    };

    bool operator==(const Value& rhs) const noexcept {
        if (type != rhs.type) {
            return false;
        }
        switch (type) {
        case Type::Void:
            return true;
        case Type::Register:
            return id == rhs.id;
        case Type::U32:
            return imm_u32 == rhs.imm_u32;
        case Type::U64:
            return imm_u64 == rhs.imm_u64;
        }
        return false;
    }
    bool operator!=(const Value& rhs) const noexcept {
        return !operator==(rhs);
    }
};
struct Register : Value {};
struct ScalarRegister : Value {};
struct ScalarU32 : Value {};
struct ScalarS32 : Value {};
struct ScalarF32 : Value {};
struct ScalarF64 : Value {};

class RegAlloc {
public:
    RegAlloc(EmitContext& ctx_) : ctx{ctx_} {}

    Register Define(IR::Inst& inst);

    Register LongDefine(IR::Inst& inst);

    [[nodiscard]] Value Peek(const IR::Value& value);

    Value Consume(const IR::Value& value);

    void Unref(IR::Inst& inst);

    [[nodiscard]] Register AllocReg();

    [[nodiscard]] Register AllocLongReg();

    void FreeReg(Register reg);

    void InvalidateConditionCodes() {
        // This does nothing for now
    }

    [[nodiscard]] size_t NumUsedRegisters() const noexcept {
        return num_used_registers;
    }

    [[nodiscard]] size_t NumUsedLongRegisters() const noexcept {
        return num_used_long_registers;
    }

    [[nodiscard]] bool IsEmpty() const noexcept {
        return register_use.none() && long_register_use.none();
    }

    /// Returns true if the instruction is expected to be aliased to another
    static bool IsAliased(const IR::Inst& inst);

    /// Returns the underlying value out of an alias sequence
    static IR::Inst& AliasInst(IR::Inst& inst);

private:
    static constexpr size_t NUM_REGS = 4096;
    static constexpr size_t NUM_ELEMENTS = 4;

    Value MakeImm(const IR::Value& value);

    Register Define(IR::Inst& inst, bool is_long);

    Value PeekInst(IR::Inst& inst);

    Value ConsumeInst(IR::Inst& inst);

    Id Alloc(bool is_long);

    void Free(Id id);

    EmitContext& ctx;
    size_t num_used_registers{};
    size_t num_used_long_registers{};
    std::bitset<NUM_REGS> register_use{};
    std::bitset<NUM_REGS> long_register_use{};
};

template <bool scalar, typename FormatContext>
auto FormatTo(FormatContext& ctx, Id id) {
    if (id.is_condition_code != 0) {
        throw NotImplementedException("Condition code emission");
    }
    if (id.is_spill != 0) {
        throw NotImplementedException("Spill emission");
    }
    if constexpr (scalar) {
        if (id.is_null != 0) {
            return fmt::format_to(ctx.out(), "{}", id.is_long != 0 ? "DC.x" : "RC.x");
        }
        if (id.is_long != 0) {
            return fmt::format_to(ctx.out(), "D{}.x", id.index.Value());
        } else {
            return fmt::format_to(ctx.out(), "R{}.x", id.index.Value());
        }
    } else {
        if (id.is_null != 0) {
            return fmt::format_to(ctx.out(), "{}", id.is_long != 0 ? "DC" : "RC");
        }
        if (id.is_long != 0) {
            return fmt::format_to(ctx.out(), "D{}", id.index.Value());
        } else {
            return fmt::format_to(ctx.out(), "R{}", id.index.Value());
        }
    }
}

} // namespace Shader::Backend::GLASM

template <>
struct fmt::formatter<Shader::Backend::GLASM::Id> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(Shader::Backend::GLASM::Id id, FormatContext& ctx) {
        return Shader::Backend::GLASM::FormatTo<true>(ctx, id);
    }
};

template <>
struct fmt::formatter<Shader::Backend::GLASM::Register> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::Backend::GLASM::Register& value, FormatContext& ctx) {
        if (value.type != Shader::Backend::GLASM::Type::Register) {
            throw Shader::InvalidArgument("Register value type is not register");
        }
        return Shader::Backend::GLASM::FormatTo<false>(ctx, value.id);
    }
};

template <>
struct fmt::formatter<Shader::Backend::GLASM::ScalarRegister> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::Backend::GLASM::ScalarRegister& value, FormatContext& ctx) {
        if (value.type != Shader::Backend::GLASM::Type::Register) {
            throw Shader::InvalidArgument("Register value type is not register");
        }
        return Shader::Backend::GLASM::FormatTo<true>(ctx, value.id);
    }
};

template <>
struct fmt::formatter<Shader::Backend::GLASM::ScalarU32> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::Backend::GLASM::ScalarU32& value, FormatContext& ctx) {
        switch (value.type) {
        case Shader::Backend::GLASM::Type::Void:
            break;
        case Shader::Backend::GLASM::Type::Register:
            return Shader::Backend::GLASM::FormatTo<true>(ctx, value.id);
        case Shader::Backend::GLASM::Type::U32:
            return fmt::format_to(ctx.out(), "{}", value.imm_u32);
        case Shader::Backend::GLASM::Type::U64:
            break;
        }
        throw Shader::InvalidArgument("Invalid value type {}", value.type);
    }
};

template <>
struct fmt::formatter<Shader::Backend::GLASM::ScalarS32> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::Backend::GLASM::ScalarS32& value, FormatContext& ctx) {
        switch (value.type) {
        case Shader::Backend::GLASM::Type::Void:
            break;
        case Shader::Backend::GLASM::Type::Register:
            return Shader::Backend::GLASM::FormatTo<true>(ctx, value.id);
        case Shader::Backend::GLASM::Type::U32:
            return fmt::format_to(ctx.out(), "{}", static_cast<s32>(value.imm_u32));
        case Shader::Backend::GLASM::Type::U64:
            break;
        }
        throw Shader::InvalidArgument("Invalid value type {}", value.type);
    }
};

template <>
struct fmt::formatter<Shader::Backend::GLASM::ScalarF32> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::Backend::GLASM::ScalarF32& value, FormatContext& ctx) {
        switch (value.type) {
        case Shader::Backend::GLASM::Type::Void:
            break;
        case Shader::Backend::GLASM::Type::Register:
            return Shader::Backend::GLASM::FormatTo<true>(ctx, value.id);
        case Shader::Backend::GLASM::Type::U32:
            return fmt::format_to(ctx.out(), "{}", Common::BitCast<f32>(value.imm_u32));
        case Shader::Backend::GLASM::Type::U64:
            break;
        }
        throw Shader::InvalidArgument("Invalid value type {}", value.type);
    }
};

template <>
struct fmt::formatter<Shader::Backend::GLASM::ScalarF64> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::Backend::GLASM::ScalarF64& value, FormatContext& ctx) {
        switch (value.type) {
        case Shader::Backend::GLASM::Type::Void:
            break;
        case Shader::Backend::GLASM::Type::Register:
            return Shader::Backend::GLASM::FormatTo<true>(ctx, value.id);
        case Shader::Backend::GLASM::Type::U32:
            break;
        case Shader::Backend::GLASM::Type::U64:
            return format_to(ctx.out(), "{}", Common::BitCast<f64>(value.imm_u64));
        }
        throw Shader::InvalidArgument("Invalid value type {}", value.type);
    }
};
