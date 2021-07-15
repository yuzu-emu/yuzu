// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <compare>
#include <iterator>

#include <fmt/format.h>

#include "common/common_types.h"
#include "shader_recompiler/exception.h"

namespace Shader::Maxwell {

class Location {
    static constexpr u32 VIRTUAL_BIAS{4};

public:
    constexpr Location() = default;

    constexpr Location(u32 initial_offset) : offset{initial_offset} {
        if (initial_offset % 8 != 0) {
            throw InvalidArgument("initial_offset={} is not a multiple of 8", initial_offset);
        }
        Align();
    }

    constexpr Location Virtual() const noexcept {
        Location virtual_location;
        virtual_location.offset = offset - VIRTUAL_BIAS;
        return virtual_location;
    }

    [[nodiscard]] constexpr u32 Offset() const noexcept {
        return offset;
    }

    [[nodiscard]] constexpr bool IsVirtual() const {
        return offset % 8 == VIRTUAL_BIAS;
    }

    constexpr auto operator<=>(const Location&) const noexcept = default;

    constexpr Location operator++() noexcept {
        const Location copy{*this};
        Step();
        return copy;
    }

    constexpr Location operator++(int) noexcept {
        Step();
        return *this;
    }

    constexpr Location operator--() noexcept {
        const Location copy{*this};
        Back();
        return copy;
    }

    constexpr Location operator--(int) noexcept {
        Back();
        return *this;
    }

    constexpr Location operator+(int number) const {
        Location new_pc{*this};
        while (number > 0) {
            --number;
            ++new_pc;
        }
        while (number < 0) {
            ++number;
            --new_pc;
        }
        return new_pc;
    }

    constexpr Location operator-(int number) const {
        return operator+(-number);
    }

private:
    constexpr void Align() {
        offset += offset % 32 == 0 ? 8 : 0;
    }

    constexpr void Step() {
        offset += 8 + (offset % 32 == 24 ? 8 : 0);
    }

    constexpr void Back() {
        offset -= 8 + (offset % 32 == 8 ? 8 : 0);
    }

    u32 offset{0xcccccccc};
};

} // namespace Shader::Maxwell

template <>
struct fmt::formatter<Shader::Maxwell::Location> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::Maxwell::Location& location, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{:04x}", location.Offset());
    }
};
