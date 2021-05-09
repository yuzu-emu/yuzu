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
    Register,
    U32,
    S32,
    F32,
};

struct Id {
    union {
        u32 raw;
        BitField<0, 30, u32> index;
        BitField<30, 1, u32> is_spill;
        BitField<31, 1, u32> is_condition_code;
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
        s32 imm_s32;
        f32 imm_f32;
    };

    bool operator==(const Value& rhs) const noexcept {
        if (type != rhs.type) {
            return false;
        }
        switch (type) {
        case Type::Register:
            return id == rhs.id;
        case Type::U32:
            return imm_u32 == rhs.imm_u32;
        case Type::S32:
            return imm_s32 == rhs.imm_s32;
        case Type::F32:
            return Common::BitCast<u32>(imm_f32) == Common::BitCast<u32>(rhs.imm_f32);
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

class RegAlloc {
public:
    RegAlloc(EmitContext& ctx_) : ctx{ctx_} {}

    Register Define(IR::Inst& inst);

    Value Consume(const IR::Value& value);

    Register AllocReg();

    void FreeReg(Register reg);

    [[nodiscard]] size_t NumUsedRegisters() const noexcept {
        return num_used_registers;
    }

private:
    static constexpr size_t NUM_REGS = 4096;
    static constexpr size_t NUM_ELEMENTS = 4;

    Value Consume(IR::Inst& inst);

    Id Alloc();

    void Free(Id id);

    EmitContext& ctx;
    size_t num_used_registers{};
    std::bitset<NUM_REGS> register_use{};
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
        return fmt::format_to(ctx.out(), "R{}.x", id.index.Value());
    } else {
        return fmt::format_to(ctx.out(), "R{}", id.index.Value());
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
        return FormatTo<true>(ctx, id);
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
        return FormatTo<false>(ctx, value.id);
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
        return FormatTo<true>(ctx, value.id);
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
        case Shader::Backend::GLASM::Type::Register:
            return FormatTo<true>(ctx, value.id);
        case Shader::Backend::GLASM::Type::U32:
            return fmt::format_to(ctx.out(), "{}", value.imm_u32);
        case Shader::Backend::GLASM::Type::S32:
            return fmt::format_to(ctx.out(), "{}", static_cast<u32>(value.imm_s32));
        case Shader::Backend::GLASM::Type::F32:
            return fmt::format_to(ctx.out(), "{}", Common::BitCast<u32>(value.imm_f32));
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
        case Shader::Backend::GLASM::Type::Register:
            return FormatTo<true>(ctx, value.id);
        case Shader::Backend::GLASM::Type::U32:
            return fmt::format_to(ctx.out(), "{}", static_cast<s32>(value.imm_u32));
        case Shader::Backend::GLASM::Type::S32:
            return fmt::format_to(ctx.out(), "{}", value.imm_s32);
        case Shader::Backend::GLASM::Type::F32:
            return fmt::format_to(ctx.out(), "{}", Common::BitCast<s32>(value.imm_f32));
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
        case Shader::Backend::GLASM::Type::Register:
            return FormatTo<true>(ctx, value.id);
        case Shader::Backend::GLASM::Type::U32:
            return fmt::format_to(ctx.out(), "{}", Common::BitCast<u32>(value.imm_u32));
        case Shader::Backend::GLASM::Type::S32:
            return fmt::format_to(ctx.out(), "{}", Common::BitCast<s32>(value.imm_s32));
        case Shader::Backend::GLASM::Type::F32:
            return fmt::format_to(ctx.out(), "{}", value.imm_f32);
        }
        throw Shader::InvalidArgument("Invalid value type {}", value.type);
    }
};
