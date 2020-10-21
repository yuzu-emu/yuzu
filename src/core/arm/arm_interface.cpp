// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <optional>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "core/memory.h"

namespace Core {
namespace {

constexpr u64 ELF_DYNAMIC_TAG_NULL = 0;
constexpr u64 ELF_DYNAMIC_TAG_STRTAB = 5;
constexpr u64 ELF_DYNAMIC_TAG_SYMTAB = 6;
constexpr u64 ELF_DYNAMIC_TAG_SYMENT = 11;

enum class ELFSymbolType : u8 {
    None = 0,
    Object = 1,
    Function = 2,
    Section = 3,
    File = 4,
    Common = 5,
    TLS = 6,
};

enum class ELFSymbolBinding : u8 {
    Local = 0,
    Global = 1,
    Weak = 2,
};

enum class ELFSymbolVisibility : u8 {
    Default = 0,
    Internal = 1,
    Hidden = 2,
    Protected = 3,
};

struct ELFSymbol {
    u32 name_index;
    union {
        u8 info;

        BitField<0, 4, ELFSymbolType> type;
        BitField<4, 4, ELFSymbolBinding> binding;
    };
    ELFSymbolVisibility visibility;
    u16 sh_index;
    u64 value;
    u64 size;
};
static_assert(sizeof(ELFSymbol) == 0x18, "ELFSymbol has incorrect size.");

using Symbols = std::vector<std::pair<ELFSymbol, std::string>>;

Symbols GetSymbols(VAddr text_offset, Core::Memory::Memory& memory) {
    const auto mod_offset = text_offset + memory.Read32(text_offset + 4);

    if (mod_offset < text_offset || (mod_offset & 0b11) != 0 ||
        memory.Read32(mod_offset) != Common::MakeMagic('M', 'O', 'D', '0')) {
        return {};
    }

    const auto dynamic_offset = memory.Read32(mod_offset + 0x4) + mod_offset;

    VAddr string_table_offset{};
    VAddr symbol_table_offset{};
    u64 symbol_entry_size{};

    VAddr dynamic_index = dynamic_offset;
    while (true) {
        const u64 tag = memory.Read64(dynamic_index);
        const u64 value = memory.Read64(dynamic_index + 0x8);
        dynamic_index += 0x10;

        if (tag == ELF_DYNAMIC_TAG_NULL) {
            break;
        }

        if (tag == ELF_DYNAMIC_TAG_STRTAB) {
            string_table_offset = value;
        } else if (tag == ELF_DYNAMIC_TAG_SYMTAB) {
            symbol_table_offset = value;
        } else if (tag == ELF_DYNAMIC_TAG_SYMENT) {
            symbol_entry_size = value;
        }
    }

    if (string_table_offset == 0 || symbol_table_offset == 0 || symbol_entry_size == 0) {
        return {};
    }

    const auto string_table_address = text_offset + string_table_offset;
    const auto symbol_table_address = text_offset + symbol_table_offset;

    Symbols out;

    VAddr symbol_index = symbol_table_address;
    while (symbol_index < string_table_address) {
        ELFSymbol symbol{};
        memory.ReadBlock(symbol_index, &symbol, sizeof(ELFSymbol));

        VAddr string_offset = string_table_address + symbol.name_index;
        std::string name;
        for (u8 c = memory.Read8(string_offset); c != 0; c = memory.Read8(++string_offset)) {
            name += static_cast<char>(c);
        }

        symbol_index += symbol_entry_size;
        out.push_back({symbol, name});
    }

    return out;
}

std::optional<std::string> GetSymbolName(const Symbols& symbols, VAddr func_address) {
    const auto iter =
        std::find_if(symbols.begin(), symbols.end(), [func_address](const auto& pair) {
            const auto& symbol = pair.first;
            const auto end_address = symbol.value + symbol.size;
            return func_address >= symbol.value && func_address < end_address;
        });

    if (iter == symbols.end()) {
        return std::nullopt;
    }

    return iter->second;
}

} // Anonymous namespace

constexpr u64 SEGMENT_BASE = 0x7100000000ull;

std::vector<ARM_Interface::BacktraceEntry> ARM_Interface::GetBacktraceFromContext(
    System& system, const ThreadContext64& ctx) {
    std::vector<BacktraceEntry> out;
    auto& memory = system.Memory();

    auto fp = ctx.cpu_registers[29];
    auto lr = ctx.cpu_registers[30];
    while (true) {
        out.push_back({"", 0, lr, 0});
        if (!fp) {
            break;
        }
        lr = memory.Read64(fp + 8) - 4;
        fp = memory.Read64(fp);
    }

    std::map<VAddr, std::string> modules;
    auto& loader{system.GetAppLoader()};
    if (loader.ReadNSOModules(modules) != Loader::ResultStatus::Success) {
        return {};
    }

    std::map<std::string, Symbols> symbols;
    for (const auto& mod : modules) {
        symbols.insert_or_assign(mod.second, GetSymbols(mod.first, memory));
    }

    for (auto& entry : out) {
        VAddr base = 0;
        for (auto iter = modules.rbegin(); iter != modules.rend(); ++iter) {
            const auto& mod{*iter};
            if (entry.original_address >= mod.first) {
                entry.mod = mod.second;
                base = mod.first;
                break;
            }
        }

        entry.offset = entry.original_address - base;
        entry.address = SEGMENT_BASE + entry.offset;

        if (entry.mod.empty())
            entry.mod = "unknown";

        const auto symbol_set = symbols.find(entry.mod);
        if (symbol_set != symbols.end()) {
            const auto symbol = GetSymbolName(symbol_set->second, entry.offset);
            if (symbol.has_value()) {
                // TODO(DarkLordZach): Add demangling of symbol names.
                entry.name = *symbol;
            }
        }
    }

    return out;
}

std::vector<ARM_Interface::BacktraceEntry> ARM_Interface::GetBacktrace() const {
    std::vector<BacktraceEntry> out;
    auto& memory = system.Memory();

    auto fp = GetReg(29);
    auto lr = GetReg(30);
    while (true) {
        out.push_back({"", 0, lr, 0, ""});
        if (!fp) {
            break;
        }
        lr = memory.Read64(fp + 8) - 4;
        fp = memory.Read64(fp);
    }

    std::map<VAddr, std::string> modules;
    auto& loader{system.GetAppLoader()};
    if (loader.ReadNSOModules(modules) != Loader::ResultStatus::Success) {
        return {};
    }

    std::map<std::string, Symbols> symbols;
    for (const auto& mod : modules) {
        symbols.insert_or_assign(mod.second, GetSymbols(mod.first, memory));
    }

    for (auto& entry : out) {
        VAddr base = 0;
        for (auto iter = modules.rbegin(); iter != modules.rend(); ++iter) {
            const auto& mod{*iter};
            if (entry.original_address >= mod.first) {
                entry.mod = mod.second;
                base = mod.first;
                break;
            }
        }

        entry.offset = entry.original_address - base;
        entry.address = SEGMENT_BASE + entry.offset;

        if (entry.mod.empty())
            entry.mod = "unknown";

        const auto symbol_set = symbols.find(entry.mod);
        if (symbol_set != symbols.end()) {
            const auto symbol = GetSymbolName(symbol_set->second, entry.offset);
            if (symbol.has_value()) {
                // TODO(DarkLordZach): Add demangling of symbol names.
                entry.name = *symbol;
            }
        }
    }

    return out;
}

void ARM_Interface::LogBacktrace() const {
    const VAddr sp = GetReg(13);
    const VAddr pc = GetPC();
    LOG_ERROR(Core_ARM, "Backtrace, sp={:016X}, pc={:016X}", sp, pc);
    LOG_ERROR(Core_ARM, "{:20}{:20}{:20}{:20}{}", "Module Name", "Address", "Original Address",
              "Offset", "Symbol");
    LOG_ERROR(Core_ARM, "");

    const auto backtrace = GetBacktrace();
    for (const auto& entry : backtrace) {
        LOG_ERROR(Core_ARM, "{:20}{:016X}    {:016X}    {:016X}    {}", entry.mod, entry.address,
                  entry.original_address, entry.offset, entry.name);
    }
}

} // namespace Core
