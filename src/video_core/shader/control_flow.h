// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <set>
#include <variant>

#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/ast.h"
#include "video_core/shader/compiler_settings.h"
#include "video_core/shader/registry.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::ConditionCode;
using Tegra::Shader::Pred;

constexpr s32 exit_branch = -1;

struct Condition {
    Pred predicate{Pred::UnusedIndex};
    ConditionCode cc{ConditionCode::T};

    bool IsUnconditional() const {
        return predicate == Pred::UnusedIndex && cc == ConditionCode::T;
    }

    auto operator<=>(const Condition&) const = default;
};

class SingleBranch {
public:
    SingleBranch() = default;
    SingleBranch(Condition condition, s32 address, bool kill, bool is_sync, bool is_brk,
                 bool ignore)
        : condition{condition}, address{address}, kill{kill}, is_sync{is_sync}, is_brk{is_brk},
          ignore{ignore} {}

    auto operator<=>(const SingleBranch&) const = default;

    Condition condition{};
    s32 address{exit_branch};
    bool kill{};
    bool is_sync{};
    bool is_brk{};
    bool ignore{};
};

struct CaseBranch {
    CaseBranch(u32 cmp_value, u32 address) : cmp_value{cmp_value}, address{address} {}
    u32 cmp_value;
    u32 address;
};

class MultiBranch {
public:
    MultiBranch(u32 gpr, std::vector<CaseBranch>&& branches)
        : gpr{gpr}, branches{std::move(branches)} {}

    u32 gpr{};
    std::vector<CaseBranch> branches{};
};

using BranchData = std::variant<SingleBranch, MultiBranch>;
using BlockBranchInfo = std::shared_ptr<BranchData>;

bool BlockBranchInfoAreEqual(BlockBranchInfo first, BlockBranchInfo second);

struct ShaderBlock {
    u32 start{};
    u32 end{};
    bool ignore_branch{};
    BlockBranchInfo branch{};

    bool operator==(const ShaderBlock& sb) const {
        return std::tie(start, end, ignore_branch) ==
                   std::tie(sb.start, sb.end, sb.ignore_branch) &&
               BlockBranchInfoAreEqual(branch, sb.branch);
    }

    bool operator!=(const ShaderBlock& sb) const {
        return !operator==(sb);
    }
};

struct ShaderCharacteristics {
    std::list<ShaderBlock> blocks{};
    std::set<u32> labels{};
    u32 start{};
    u32 end{};
    ASTManager manager{true, true};
    CompilerSettings settings{};
};

std::unique_ptr<ShaderCharacteristics> ScanFlow(const ProgramCode& program_code, u32 start_address,
                                                const CompilerSettings& settings,
                                                Registry& registry);

} // namespace VideoCommon::Shader
