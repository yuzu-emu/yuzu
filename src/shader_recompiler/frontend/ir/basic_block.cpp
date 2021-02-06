// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>

#include "common/bit_cast.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {

Block::Block(ObjectPool<Inst>& inst_pool_, u32 begin, u32 end)
    : inst_pool{&inst_pool_}, location_begin{begin}, location_end{end} {}

Block::~Block() = default;

void Block::AppendNewInst(Opcode op, std::initializer_list<Value> args) {
    PrependNewInst(end(), op, args);
}

Block::iterator Block::PrependNewInst(iterator insertion_point, Opcode op,
                                      std::initializer_list<Value> args, u64 flags) {
    Inst* const inst{inst_pool->Create(op, flags)};
    const auto result_it{instructions.insert(insertion_point, *inst)};

    if (inst->NumArgs() != args.size()) {
        throw InvalidArgument("Invalid number of arguments {} in {}", args.size(), op);
    }
    std::ranges::for_each(args, [inst, index = size_t{0}](const Value& arg) mutable {
        inst->SetArg(index, arg);
        ++index;
    });
    return result_it;
}

void Block::AddImmediatePredecessor(IR::Block* immediate_predecessor) {
    imm_predecessors.push_back(immediate_predecessor);
}

u32 Block::LocationBegin() const noexcept {
    return location_begin;
}

u32 Block::LocationEnd() const noexcept {
    return location_end;
}

Block::InstructionList& Block::Instructions() noexcept {
    return instructions;
}

const Block::InstructionList& Block::Instructions() const noexcept {
    return instructions;
}

std::span<IR::Block* const> Block::ImmediatePredecessors() const noexcept {
    return imm_predecessors;
}

static std::string BlockToIndex(const std::map<const Block*, size_t>& block_to_index,
                                Block* block) {
    if (const auto it{block_to_index.find(block)}; it != block_to_index.end()) {
        return fmt::format("{{Block ${}}}", it->second);
    }
    return fmt::format("$<unknown block {:016x}>", reinterpret_cast<u64>(block));
}

static std::string ArgToIndex(const std::map<const Block*, size_t>& block_to_index,
                              const std::map<const Inst*, size_t>& inst_to_index,
                              const Value& arg) {
    if (arg.IsEmpty()) {
        return "<null>";
    }
    if (arg.IsLabel()) {
        return BlockToIndex(block_to_index, arg.Label());
    }
    if (!arg.IsImmediate()) {
        if (const auto it{inst_to_index.find(arg.Inst())}; it != inst_to_index.end()) {
            return fmt::format("%{}", it->second);
        }
        return fmt::format("%<unknown inst {:016x}>", reinterpret_cast<u64>(arg.Inst()));
    }
    switch (arg.Type()) {
    case Type::U1:
        return fmt::format("#{}", arg.U1() ? '1' : '0');
    case Type::U8:
        return fmt::format("#{}", arg.U8());
    case Type::U16:
        return fmt::format("#{}", arg.U16());
    case Type::U32:
        return fmt::format("#{}", arg.U32());
    case Type::U64:
        return fmt::format("#{}", arg.U64());
    case Type::Reg:
        return fmt::format("{}", arg.Reg());
    case Type::Pred:
        return fmt::format("{}", arg.Pred());
    case Type::Attribute:
        return fmt::format("{}", arg.Attribute());
    default:
        return "<unknown immediate type>";
    }
}

std::string DumpBlock(const Block& block) {
    size_t inst_index{0};
    std::map<const Inst*, size_t> inst_to_index;
    return DumpBlock(block, {}, inst_to_index, inst_index);
}

std::string DumpBlock(const Block& block, const std::map<const Block*, size_t>& block_to_index,
                      std::map<const Inst*, size_t>& inst_to_index, size_t& inst_index) {
    std::string ret{"Block"};
    if (const auto it{block_to_index.find(&block)}; it != block_to_index.end()) {
        ret += fmt::format(" ${}", it->second);
    }
    ret += fmt::format(": begin={:04x} end={:04x}\n", block.LocationBegin(), block.LocationEnd());

    for (const Inst& inst : block) {
        const Opcode op{inst.Opcode()};
        ret += fmt::format("[{:016x}] ", reinterpret_cast<u64>(&inst));
        if (TypeOf(op) != Type::Void) {
            ret += fmt::format("%{:<5} = {}", inst_index, op);
        } else {
            ret += fmt::format("         {}", op); // '%00000 = ' -> 1 + 5 + 3 = 9 spaces
        }
        if (op == Opcode::Phi) {
            size_t val_index{0};
            for (const auto& [phi_block, phi_val] : inst.PhiOperands()) {
                ret += val_index != 0 ? ", " : " ";
                ret += fmt::format("[ {}, {} ]", ArgToIndex(block_to_index, inst_to_index, phi_val),
                                   BlockToIndex(block_to_index, phi_block));
                ++val_index;
            }
        } else {
            const size_t arg_count{NumArgsOf(op)};
            for (size_t arg_index = 0; arg_index < arg_count; ++arg_index) {
                const Value arg{inst.Arg(arg_index)};
                ret += arg_index != 0 ? ", " : " ";
                ret += ArgToIndex(block_to_index, inst_to_index, arg);

                const Type actual_type{arg.Type()};
                const Type expected_type{ArgTypeOf(op, arg_index)};
                if (!AreTypesCompatible(actual_type, expected_type)) {
                    ret += fmt::format("<type error: {} != {}>", actual_type, expected_type);
                }
            }
        }
        if (TypeOf(op) != Type::Void) {
            ret += fmt::format(" (uses: {})\n", inst.UseCount());
        } else {
            ret += '\n';
        }

        inst_to_index.emplace(&inst, inst_index);
        ++inst_index;
    }
    return ret;
}

} // namespace Shader::IR
