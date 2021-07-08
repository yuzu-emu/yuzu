// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/common_types.h"

namespace Common {

constexpr u128 INVALID_UUID{{0, 0}};

struct UUID {
    // UUIDs which are 0 are considered invalid!
    u128 uuid;
    UUID() = default;
    constexpr explicit UUID(const u128& id) : uuid{id} {}
    constexpr explicit UUID(const u64 lo, const u64 hi) : uuid{{lo, hi}} {}

    [[nodiscard]] constexpr explicit operator bool() const {
        return uuid != INVALID_UUID;
    }

    [[nodiscard]] constexpr bool operator==(const UUID& rhs) const {
        return uuid == rhs.uuid;
    }

    [[nodiscard]] constexpr bool operator!=(const UUID& rhs) const {
        return !operator==(rhs);
    }

    // TODO(ogniK): Properly generate uuids based on RFC-4122
    [[nodiscard]] static UUID Generate();

    // Set the UUID to {0,0} to be considered an invalid user
    constexpr void Invalidate() {
        uuid = INVALID_UUID;
    }

    // TODO(ogniK): Properly generate a Nintendo ID
    [[nodiscard]] constexpr u64 GetNintendoID() const {
        return uuid[0];
    }

    [[nodiscard]] std::string Format() const;
    [[nodiscard]] std::string FormatSwitch() const;
};
static_assert(sizeof(UUID) == 16, "UUID is an invalid size!");

} // namespace Common
