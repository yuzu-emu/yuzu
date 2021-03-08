#pragma once

#include <vector>

#include "common/common_types.h"
#include "shader_recompiler/environment.h"

namespace Shader {

class FileEnvironment final : public Environment {
public:
    explicit FileEnvironment(const char* path);
    ~FileEnvironment() override;

    u64 ReadInstruction(u32 offset) override;

    u32 TextureBoundBuffer() override;

    std::array<u32, 3> WorkgroupSize() override;

private:
    std::vector<u64> data;
};

} // namespace Shader
