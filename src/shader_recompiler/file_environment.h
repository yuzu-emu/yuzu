#pragma once

#include <vector>

#include "common/common_types.h"
#include "environment.h"

namespace Shader {

class FileEnvironment final : public Environment {
public:
    explicit FileEnvironment(const char* path);
    ~FileEnvironment() override;

    u64 ReadInstruction(u32 offset) const override;

private:
    std::vector<u64> data;
};

} // namespace Shader
