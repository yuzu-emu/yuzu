// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/logging/filter.h"
#include "common/string_util.h"

namespace Common::Log {
namespace {
template <typename It>
Level GetLevelByName(const It begin, const It end) {
    for (u8 i = 0; i < static_cast<u8>(Level::Count); ++i) {
        const char* level_name = GetLevelName(static_cast<Level>(i));
        if (Common::ComparePartialString(begin, end, level_name)) {
            return static_cast<Level>(i);
        }
    }
    return Level::Count;
}

template <typename It>
Class GetClassByName(const It begin, const It end) {
    for (u8 i = 0; i < static_cast<u8>(Class::Count); ++i) {
        const char* level_name = GetLogClassName(static_cast<Class>(i));
        if (Common::ComparePartialString(begin, end, level_name)) {
            return static_cast<Class>(i);
        }
    }
    return Class::Count;
}

template <typename Iterator>
bool ParseFilterRule(Filter& instance, Iterator begin, Iterator end) {
    auto level_separator = std::find(begin, end, ':');
    if (level_separator == end) {
        LOG_ERROR(Log, "Invalid log filter. Must specify a log level after `:`: {}",
                  std::string(begin, end));
        return false;
    }

    const Level level = GetLevelByName(level_separator + 1, end);
    if (level == Level::Count) {
        LOG_ERROR(Log, "Unknown log level in filter: {}", std::string(begin, end));
        return false;
    }

    if (Common::ComparePartialString(begin, level_separator, "*")) {
        instance.ResetAll(level);
        return true;
    }

    const Class log_class = GetClassByName(begin, level_separator);
    if (log_class == Class::Count) {
        LOG_ERROR(Log, "Unknown log class in filter: {}", std::string(begin, end));
        return false;
    }

    instance.SetClassLevel(log_class, level);
    return true;
}
} // Anonymous namespace

/// Macro listing all log classes. Code should define CLS and SUB as desired before invoking this.
#define ALL_LOG_CLASSES()                                                                          \
    CLS(Log)                                                                                       \
    CLS(Common)                                                                                    \
    SUB(Common, Filesystem)                                                                        \
    SUB(Common, Memory)                                                                            \
    CLS(Core)                                                                                      \
    SUB(Core, ARM)                                                                                 \
    SUB(Core, Timing)                                                                              \
    CLS(Config)                                                                                    \
    CLS(Debug)                                                                                     \
    SUB(Debug, Emulated)                                                                           \
    SUB(Debug, GPU)                                                                                \
    SUB(Debug, Breakpoint)                                                                         \
    SUB(Debug, GDBStub)                                                                            \
    CLS(Kernel)                                                                                    \
    SUB(Kernel, SVC)                                                                               \
    CLS(Service)                                                                                   \
    SUB(Service, ACC)                                                                              \
    SUB(Service, Audio)                                                                            \
    SUB(Service, AM)                                                                               \
    SUB(Service, AOC)                                                                              \
    SUB(Service, APM)                                                                              \
    SUB(Service, ARP)                                                                              \
    SUB(Service, BCAT)                                                                             \
    SUB(Service, BPC)                                                                              \
    SUB(Service, BGTC)                                                                             \
    SUB(Service, BTDRV)                                                                            \
    SUB(Service, BTM)                                                                              \
    SUB(Service, Capture)                                                                          \
    SUB(Service, ERPT)                                                                             \
    SUB(Service, ETicket)                                                                          \
    SUB(Service, EUPLD)                                                                            \
    SUB(Service, Fatal)                                                                            \
    SUB(Service, FGM)                                                                              \
    SUB(Service, Friend)                                                                           \
    SUB(Service, FS)                                                                               \
    SUB(Service, GRC)                                                                              \
    SUB(Service, HID)                                                                              \
    SUB(Service, IRS)                                                                              \
    SUB(Service, LBL)                                                                              \
    SUB(Service, LDN)                                                                              \
    SUB(Service, LDR)                                                                              \
    SUB(Service, LM)                                                                               \
    SUB(Service, Migration)                                                                        \
    SUB(Service, Mii)                                                                              \
    SUB(Service, MM)                                                                               \
    SUB(Service, NCM)                                                                              \
    SUB(Service, NFC)                                                                              \
    SUB(Service, NFP)                                                                              \
    SUB(Service, NIFM)                                                                             \
    SUB(Service, NIM)                                                                              \
    SUB(Service, NPNS)                                                                             \
    SUB(Service, NS)                                                                               \
    SUB(Service, NVDRV)                                                                            \
    SUB(Service, OLSC)                                                                             \
    SUB(Service, PCIE)                                                                             \
    SUB(Service, PCTL)                                                                             \
    SUB(Service, PCV)                                                                              \
    SUB(Service, PM)                                                                               \
    SUB(Service, PREPO)                                                                            \
    SUB(Service, PSC)                                                                              \
    SUB(Service, PSM)                                                                              \
    SUB(Service, SET)                                                                              \
    SUB(Service, SM)                                                                               \
    SUB(Service, SPL)                                                                              \
    SUB(Service, SSL)                                                                              \
    SUB(Service, TCAP)                                                                             \
    SUB(Service, Time)                                                                             \
    SUB(Service, USB)                                                                              \
    SUB(Service, VI)                                                                               \
    SUB(Service, WLAN)                                                                             \
    CLS(HW)                                                                                        \
    SUB(HW, Memory)                                                                                \
    SUB(HW, LCD)                                                                                   \
    SUB(HW, GPU)                                                                                   \
    SUB(HW, AES)                                                                                   \
    CLS(IPC)                                                                                       \
    CLS(Frontend)                                                                                  \
    CLS(Render)                                                                                    \
    SUB(Render, Software)                                                                          \
    SUB(Render, OpenGL)                                                                            \
    SUB(Render, Vulkan)                                                                            \
    CLS(Shader)                                                                                    \
    SUB(Shader, SPIRV)                                                                             \
    SUB(Shader, GLASM)                                                                             \
    SUB(Shader, GLSL)                                                                              \
    CLS(Audio)                                                                                     \
    SUB(Audio, DSP)                                                                                \
    SUB(Audio, Sink)                                                                               \
    CLS(Input)                                                                                     \
    CLS(Network)                                                                                   \
    CLS(Loader)                                                                                    \
    CLS(CheatEngine)                                                                               \
    CLS(Crypto)                                                                                    \
    CLS(WebService)

// GetClassName is a macro defined by Windows.h, grrr...
const char* GetLogClassName(Class log_class) {
    switch (log_class) {
#define CLS(x)                                                                                     \
    case Class::x:                                                                                 \
        return #x;
#define SUB(x, y)                                                                                  \
    case Class::x##_##y:                                                                           \
        return #x "." #y;
        ALL_LOG_CLASSES()
#undef CLS
#undef SUB
    case Class::Count:
        break;
    }
    return "Invalid";
}

const char* GetLevelName(Level log_level) {
#define LVL(x)                                                                                     \
    case Level::x:                                                                                 \
        return #x
    switch (log_level) {
        LVL(Trace);
        LVL(Debug);
        LVL(Info);
        LVL(Warning);
        LVL(Error);
        LVL(Critical);
    case Level::Count:
        break;
    }
#undef LVL
    return "Invalid";
}

Filter::Filter(Level default_level) {
    ResetAll(default_level);
}

void Filter::ResetAll(Level level) {
    class_levels.fill(level);
}

void Filter::SetClassLevel(Class log_class, Level level) {
    class_levels[static_cast<std::size_t>(log_class)] = level;
}

void Filter::ParseFilterString(std::string_view filter_view) {
    auto clause_begin = filter_view.cbegin();
    while (clause_begin != filter_view.cend()) {
        auto clause_end = std::find(clause_begin, filter_view.cend(), ' ');

        // If clause isn't empty
        if (clause_end != clause_begin) {
            ParseFilterRule(*this, clause_begin, clause_end);
        }

        if (clause_end != filter_view.cend()) {
            // Skip over the whitespace
            ++clause_end;
        }
        clause_begin = clause_end;
    }
}

bool Filter::CheckMessage(Class log_class, Level level) const {
    return static_cast<u8>(level) >=
           static_cast<u8>(class_levels[static_cast<std::size_t>(log_class)]);
}

bool Filter::IsDebug() const {
    return std::any_of(class_levels.begin(), class_levels.end(), [](const Level& l) {
        return static_cast<u8>(l) <= static_cast<u8>(Level::Debug);
    });
}

} // namespace Common::Log
