// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <optional>
#include <boost/container_hash/hash.hpp>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/macro/macro.h"
#include "video_core/macro/macro_hle.h"
#include "video_core/macro/macro_interpreter.h"
#ifdef ARCHITECTURE_x86_64
#include "video_core/macro/macro_jit_x64.h"
#elif ARCHITECTURE_ARM64

#endif
namespace Tegra {

MacroEngine::MacroEngine(Engines::Maxwell3D& maxwell3d)
    : hle_macros{std::make_unique<Tegra::HLEMacro>(maxwell3d)} {}

MacroEngine::~MacroEngine() = default;

void MacroEngine::AddCode(u32 method, u32 data) {
    uploaded_macro_code[method].push_back(data);
}

void MacroEngine::Execute(Engines::Maxwell3D& maxwell3d, u32 method,
                          const std::vector<u32>& parameters) {
    auto compiled_macro = macro_cache.find(method);
    if (compiled_macro != macro_cache.end()) {
        const auto& cache_info = compiled_macro->second;
        if (cache_info.has_hle_program) {
            cache_info.hle_program->Execute(parameters, method);
        } else {
            cache_info.lle_program->Execute(parameters, method);
        }
    } else {
        // Macro not compiled, check if it's uploaded and if so, compile it
        std::optional<u32> mid_method;
        const auto macro_code = uploaded_macro_code.find(method);
        if (macro_code == uploaded_macro_code.end()) {
            for (const auto& [method_base, code] : uploaded_macro_code) {
                if (method >= method_base && (method - method_base) < code.size()) {
                    mid_method = method_base;
                    break;
                }
            }
            if (!mid_method.has_value()) {
                UNREACHABLE_MSG("Macro 0x{0:x} was not uploaded", method);
                return;
            }
        }
        auto& cache_info = macro_cache[method];

        if (!mid_method.has_value()) {
            cache_info.lle_program = Compile(macro_code->second);
            cache_info.hash = boost::hash_value(macro_code->second);
        } else {
            const auto& macro_cached = uploaded_macro_code[mid_method.value()];
            const auto rebased_method = method - mid_method.value();
            auto& code = uploaded_macro_code[method];
            code.resize(macro_cached.size() - rebased_method);
            std::memcpy(code.data(), macro_cached.data() + rebased_method,
                        code.size() * sizeof(u32));
            cache_info.hash = boost::hash_value(code);
            cache_info.lle_program = Compile(code);
        }

        auto hle_program = hle_macros->GetHLEProgram(cache_info.hash);
        if (hle_program.has_value()) {
            cache_info.has_hle_program = true;
            cache_info.hle_program = std::move(hle_program.value());
            cache_info.hle_program->Execute(parameters, method);
        } else {
            cache_info.lle_program->Execute(parameters, method);
        }
    }
}

std::unique_ptr<MacroEngine> GetMacroEngine(Engines::Maxwell3D& maxwell3d) {
    if (Settings::values.disable_macro_jit) {
        return std::make_unique<MacroInterpreter>(maxwell3d);
    }
#ifdef ARCHITECTURE_x86_64
    return std::make_unique<MacroJITx64>(maxwell3d);
#else
    return std::make_unique<MacroInterpreter>(maxwell3d);
#endif
}

} // namespace Tegra
