// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <fmt/format.h>

namespace Shader::IR {

enum class Pred : u64 {
    P0,
    P1,
    P2,
    P3,
    P4,
    P5,
    P6,
    PT,
};

constexpr size_t NUM_USER_PREDS = 7;
constexpr size_t NUM_PREDS = 8;

[[nodiscard]] constexpr size_t PredIndex(Pred pred) noexcept {
    return static_cast<size_t>(pred);
}

} // namespace Shader::IR

template <>
struct fmt::formatter<Shader::IR::Pred> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::IR::Pred& pred, FormatContext& ctx) {
        if (pred == Shader::IR::Pred::PT) {
            return fmt::format_to(ctx.out(), "PT");
        } else {
            return fmt::format_to(ctx.out(), "P{}", static_cast<int>(pred));
        }
    }
};
