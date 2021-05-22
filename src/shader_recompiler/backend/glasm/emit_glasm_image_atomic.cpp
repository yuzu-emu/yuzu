// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {

void EmitImageAtomicIAdd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] const IR::Value& index,
                           [[maybe_unused]] Register coords, [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicSMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] const IR::Value& index,
                           [[maybe_unused]] Register coords, [[maybe_unused]] ScalarS32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicUMin32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] const IR::Value& index,
                           [[maybe_unused]] Register coords, [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicSMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] const IR::Value& index,
                           [[maybe_unused]] Register coords, [[maybe_unused]] ScalarS32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicUMax32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                           [[maybe_unused]] const IR::Value& index,
                           [[maybe_unused]] Register coords, [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicInc32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                          [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicDec32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                          [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicAnd32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                          [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicOr32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                         [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                         [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicXor32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                          [[maybe_unused]] const IR::Value& index, [[maybe_unused]] Register coords,
                          [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitImageAtomicExchange32([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] IR::Inst& inst,
                               [[maybe_unused]] const IR::Value& index,
                               [[maybe_unused]] Register coords, [[maybe_unused]] ScalarU32 value) {
    throw NotImplementedException("GLASM instruction");
}

void EmitBindlessImageAtomicIAdd32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicSMin32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicUMin32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicSMax32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicUMax32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicInc32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicDec32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicAnd32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicOr32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicXor32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicExchange32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicIAdd32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicSMin32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicUMin32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicSMax32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicUMax32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicInc32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicDec32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicAnd32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicOr32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicXor32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicExchange32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

} // namespace Shader::Backend::GLASM
