// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <string_view>

#include "common/common_types.h"
#include "shader_recompiler/exception.h"
#include "shader_recompiler/frontend/maxwell/decode.h"
#include "shader_recompiler/frontend/maxwell/opcodes.h"

namespace Shader::Maxwell {
namespace {
struct MaskValue {
    u64 mask;
    u64 value;
};

constexpr MaskValue MaskValueFromEncoding(const char* encoding) {
    u64 mask{};
    u64 value{};
    u64 bit{u64(1) << 63};
    while (*encoding) {
        switch (*encoding) {
        case '0':
            mask |= bit;
            break;
        case '1':
            mask |= bit;
            value |= bit;
            break;
        case '-':
            break;
        case ' ':
            break;
        default:
            throw LogicError("Invalid encoding character '{}'", *encoding);
        }
        ++encoding;
        if (*encoding != ' ') {
            bit >>= 1;
        }
    }
    return MaskValue{.mask = mask, .value = value};
}

struct InstEncoding {
    MaskValue mask_value;
    Opcode opcode;
};
constexpr std::array UNORDERED_ENCODINGS{
#define INST(name, cute, encode)                                                                   \
    InstEncoding{                                                                                  \
        .mask_value{MaskValueFromEncoding(encode)},                                                \
        .opcode = Opcode::name,                                                                    \
    },
#include "maxwell.inc"
#undef INST
};

constexpr auto SortedEncodings() {
    std::array encodings{UNORDERED_ENCODINGS};
    std::ranges::sort(encodings, [](const InstEncoding& lhs, const InstEncoding& rhs) {
        return std::popcount(lhs.mask_value.mask) > std::popcount(rhs.mask_value.mask);
    });
    return encodings;
}
constexpr auto ENCODINGS{SortedEncodings()};

constexpr int WidestLeftBits() {
    int bits{64};
    for (const InstEncoding& encoding : ENCODINGS) {
        bits = std::min(bits, std::countr_zero(encoding.mask_value.mask));
    }
    return 64 - bits;
}
constexpr int WIDEST_LEFT_BITS{WidestLeftBits()};
constexpr int MASK_SHIFT{64 - WIDEST_LEFT_BITS};

constexpr size_t ToFastLookupIndex(u64 value) {
    return static_cast<size_t>(value >> MASK_SHIFT);
}

constexpr size_t FastLookupSize() {
    size_t max_width{};
    for (const InstEncoding& encoding : ENCODINGS) {
        max_width = std::max(max_width, ToFastLookupIndex(encoding.mask_value.mask));
    }
    return max_width + 1;
}
constexpr size_t FAST_LOOKUP_SIZE{FastLookupSize()};

struct InstInfo {
    [[nodiscard]] u64 Mask() const noexcept {
        return static_cast<u64>(high_mask) << MASK_SHIFT;
    }

    [[nodiscard]] u64 Value() const noexcept {
        return static_cast<u64>(high_value) << MASK_SHIFT;
    }

    u16 high_mask;
    u16 high_value;
    Opcode opcode;
};

constexpr auto MakeFastLookupTableIndex(size_t index) {
    std::array<InstInfo, 2> encodings{};
    size_t element{};
    for (const auto& encoding : ENCODINGS) {
        const size_t mask{ToFastLookupIndex(encoding.mask_value.mask)};
        const size_t value{ToFastLookupIndex(encoding.mask_value.value)};
        if ((index & mask) == value) {
            encodings.at(element) = InstInfo{
                .high_mask = static_cast<u16>(encoding.mask_value.mask >> MASK_SHIFT),
                .high_value = static_cast<u16>(encoding.mask_value.value >> MASK_SHIFT),
                .opcode = encoding.opcode,
            };
            ++element;
        }
    }
    return encodings;
}

/*constexpr*/ auto MakeFastLookupTable() {
    auto encodings{std::make_unique<std::array<std::array<InstInfo, 2>, FAST_LOOKUP_SIZE>>()};
    for (size_t index = 0; index < FAST_LOOKUP_SIZE; ++index) {
        (*encodings)[index] = MakeFastLookupTableIndex(index);
    }
    return encodings;
}
const auto FAST_LOOKUP_TABLE{MakeFastLookupTable()};
} // Anonymous namespace

Opcode Decode(u64 insn) {
    const auto& table{(*FAST_LOOKUP_TABLE)[ToFastLookupIndex(insn)]};
    const auto it{std::ranges::find_if(
        table, [insn](const InstInfo& info) { return (insn & info.Mask()) == info.Value(); })};
    if (it == table.end()) {
        throw NotImplementedException("Instruction 0x{:016x} is unknown / unimplemented", insn);
    }
    return it->opcode;
}

} // namespace Shader::Maxwell
