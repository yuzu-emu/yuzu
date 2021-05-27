// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/maxwell/decode.h"
#include "shader_recompiler/frontend/maxwell/location.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"

namespace Shader::Maxwell {

template <auto method>
static void Invoke(TranslatorVisitor& visitor, Location pc, u64 insn) {
    using MethodType = decltype(method);
    if constexpr (std::is_invocable_r_v<void, MethodType, TranslatorVisitor&, Location, u64>) {
        (visitor.*method)(pc, insn);
    } else if constexpr (std::is_invocable_r_v<void, MethodType, TranslatorVisitor&, u64>) {
        (visitor.*method)(insn);
    } else {
        (visitor.*method)();
    }
}

void Translate(Environment& env, IR::Block* block, u32 location_begin, u32 location_end) {
    if (location_begin == location_end) {
        return;
    }
    TranslatorVisitor visitor{env, *block};
    for (Location pc = location_begin; pc != location_end; ++pc) {
        const u64 insn{env.ReadInstruction(pc.Offset())};
        try {
            const Opcode opcode{Decode(insn)};
            switch (opcode) {
#define INST(name, cute, mask)                                                                     \
    case Opcode::name:                                                                             \
        Invoke<&TranslatorVisitor::name>(visitor, pc, insn);                                       \
        break;
#include "shader_recompiler/frontend/maxwell/maxwell.inc"
#undef OPCODE
            default:
                throw LogicError("Invalid opcode {}", opcode);
            }
        } catch (Exception& exception) {
            exception.Prepend(fmt::format("Translate {}: ", Decode(insn)));
            throw;
        }
    }
}

} // namespace Shader::Maxwell
