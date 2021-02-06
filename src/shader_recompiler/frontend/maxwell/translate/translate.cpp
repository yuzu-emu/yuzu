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

template <auto visitor_method>
static void Invoke(TranslatorVisitor& visitor, Location pc, u64 insn) {
    using MethodType = decltype(visitor_method);
    if constexpr (std::is_invocable_r_v<void, MethodType, TranslatorVisitor&, Location, u64>) {
        (visitor.*visitor_method)(pc, insn);
    } else if constexpr (std::is_invocable_r_v<void, MethodType, TranslatorVisitor&, u64>) {
        (visitor.*visitor_method)(insn);
    } else {
        (visitor.*visitor_method)();
    }
}

IR::Block Translate(ObjectPool<IR::Inst>& inst_pool, Environment& env,
                    const Flow::Block& flow_block) {
    IR::Block block{inst_pool, flow_block.begin.Offset(), flow_block.end.Offset()};
    TranslatorVisitor visitor{env, block};

    const Location pc_end{flow_block.end};
    Location pc{flow_block.begin};
    while (pc != pc_end) {
        const u64 insn{env.ReadInstruction(pc.Offset())};
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
        ++pc;
    }
    return block;
}

} // namespace Shader::Maxwell
