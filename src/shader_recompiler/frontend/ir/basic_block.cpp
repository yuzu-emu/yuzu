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

Block::Block(ObjectPool<Inst>& inst_pool_) : inst_pool{&inst_pool_} {}

Block::~Block() = default;

void Block::AppendNewInst(Opcode op, std::initializer_list<Value> args) {
    PrependNewInst(end(), op, args);
}

Block::iterator Block::PrependNewInst(iterator insertion_point, Opcode op,
                                      std::initializer_list<Value> args, u32 flags) {
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

void Block::AddBranch(Block* block) {
    if (std::ranges::find(imm_successors, block) != imm_successors.end()) {
        throw LogicError("Successor already inserted");
    }
    if (std::ranges::find(block->imm_predecessors, this) != block->imm_predecessors.end()) {
        throw LogicError("Predecessor already inserted");
    }
    imm_successors.push_back(block);
    block->imm_predecessors.push_back(this);
}

static std::string BlockToIndex(const std::map<const Block*, size_t>& block_to_index,
                                Block* block) {
    if (const auto it{block_to_index.find(block)}; it != block_to_index.end()) {
        return fmt::format("{{Block ${}}}", it->second);
    }
    return fmt::format("$<unknown block {:016x}>", reinterpret_cast<u64>(block));
}

static size_t InstIndex(std::map<const Inst*, size_t>& inst_to_index, size_t& inst_index,
                        const Inst* inst) {
    const auto [it, is_inserted]{inst_to_index.emplace(inst, inst_index + 1)};
    if (is_inserted) {
        ++inst_index;
    }
    return it->second;
}

static std::string ArgToIndex(std::map<const Inst*, size_t>& inst_to_index, size_t& inst_index,
                              const Value& arg) {
    if (arg.IsEmpty()) {
        return "<null>";
    }
    if (!arg.IsImmediate() || arg.IsIdentity()) {
        return fmt::format("%{}", InstIndex(inst_to_index, inst_index, arg.Inst()));
    }
    switch (arg.Type()) {
    case Type::U1:
        return fmt::format("#{}", arg.U1() ? "true" : "false");
    case Type::U8:
        return fmt::format("#{}", arg.U8());
    case Type::U16:
        return fmt::format("#{}", arg.U16());
    case Type::U32:
        return fmt::format("#{}", arg.U32());
    case Type::U64:
        return fmt::format("#{}", arg.U64());
    case Type::F32:
        return fmt::format("#{}", arg.F32());
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
    ret += '\n';
    for (const Inst& inst : block) {
        const Opcode op{inst.GetOpcode()};
        ret += fmt::format("[{:016x}] ", reinterpret_cast<u64>(&inst));
        if (TypeOf(op) != Type::Void) {
            ret += fmt::format("%{:<5} = {}", InstIndex(inst_to_index, inst_index, &inst), op);
        } else {
            ret += fmt::format("         {}", op); // '%00000 = ' -> 1 + 5 + 3 = 9 spaces
        }
        const size_t arg_count{inst.NumArgs()};
        for (size_t arg_index = 0; arg_index < arg_count; ++arg_index) {
            const Value arg{inst.Arg(arg_index)};
            const std::string arg_str{ArgToIndex(inst_to_index, inst_index, arg)};
            ret += arg_index != 0 ? ", " : " ";
            if (op == Opcode::Phi) {
                ret += fmt::format("[ {}, {} ]", arg_str,
                                   BlockToIndex(block_to_index, inst.PhiBlock(arg_index)));
            } else {
                ret += arg_str;
            }
            if (op != Opcode::Phi) {
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
    }
    return ret;
}

} // namespace Shader::IR
