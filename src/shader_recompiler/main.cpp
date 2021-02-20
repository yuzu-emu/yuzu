// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <filesystem>

#include <fmt/format.h>

#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/file_environment.h"
#include "shader_recompiler/frontend/ir/basic_block.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/decode.h"
#include "shader_recompiler/frontend/maxwell/location.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/frontend/maxwell/translate/translate.h"

using namespace Shader;
using namespace Shader::Maxwell;

template <typename Func>
static void ForEachFile(const std::filesystem::path& path, Func&& func) {
    std::filesystem::directory_iterator end;
    for (std::filesystem::directory_iterator it{path}; it != end; ++it) {
        if (std::filesystem::is_directory(*it)) {
            ForEachFile(*it, func);
        } else {
            func(*it);
        }
    }
}

void RunDatabase() {
    std::vector<std::unique_ptr<FileEnvironment>> map;
    ForEachFile("D:\\Shaders\\Database", [&](const std::filesystem::path& path) {
        map.emplace_back(std::make_unique<FileEnvironment>(path.string().c_str()));
    });
    ObjectPool<Flow::Block> block_pool;
    using namespace std::chrono;
    auto t0 = high_resolution_clock::now();
    int N = 1;
    int n = 0;
    for (int i = 0; i < N; ++i) {
        for (auto& env : map) {
            ++n;
            // fmt::print(stdout, "Decoding {}\n", path.string());

            const Location start_address{0};
            block_pool.ReleaseContents();
            Flow::CFG cfg{*env, block_pool, start_address};
            // fmt::print(stdout, "{}\n", cfg->Dot());
            // IR::Program program{env, cfg};
            // Optimize(program);
            // const std::string code{EmitGLASM(program)};
        }
    }
    auto t = high_resolution_clock::now();
    fmt::print(stdout, "{} ms", duration_cast<milliseconds>(t - t0).count() / double(N));
}

static constexpr Profile PROFILE{
    .unified_descriptor_binding = true,
    .support_float_controls = true,
    .support_separate_denorm_behavior = true,
    .support_separate_rounding_mode = true,
    .support_fp16_denorm_preserve = true,
    .support_fp32_denorm_preserve = true,
    .support_fp16_denorm_flush = true,
    .support_fp32_denorm_flush = true,
};

int main() {
    // RunDatabase();

    ObjectPool<Flow::Block> flow_block_pool;
    ObjectPool<IR::Inst> inst_pool;
    ObjectPool<IR::Block> block_pool;

    // FileEnvironment env{"D:\\Shaders\\Database\\Oninaki\\CS8F146B41DB6BD826.bin"};
    FileEnvironment env{"D:\\Shaders\\shader.bin"};
    block_pool.ReleaseContents();
    inst_pool.ReleaseContents();
    flow_block_pool.ReleaseContents();
    Flow::CFG cfg{env, flow_block_pool, 0};
    fmt::print(stdout, "{}\n", cfg.Dot());
    IR::Program program{TranslateProgram(inst_pool, block_pool, env, cfg)};
    fmt::print(stdout, "{}\n", IR::DumpProgram(program));
    const std::vector<u32> spirv{Backend::SPIRV::EmitSPIRV(PROFILE, env, program)};
    std::FILE* const file{std::fopen("D:\\shader.spv", "wb")};
    std::fwrite(spirv.data(), spirv.size(), sizeof(u32), file);
    std::fclose(file);
    std::system("spirv-dis D:\\shader.spv");
}
