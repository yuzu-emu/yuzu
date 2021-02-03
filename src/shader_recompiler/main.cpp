// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <filesystem>

#include <fmt/format.h>

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
    for (int i = 0; i < 300; ++i) {
        for (auto& env : map) {
            // fmt::print(stdout, "Decoding {}\n", path.string());
            const Location start_address{0};
            auto cfg{std::make_unique<Flow::CFG>(*env, start_address)};
            // fmt::print(stdout, "{}\n", cfg->Dot());
            // IR::Program program{env, cfg};
            // Optimize(program);
            // const std::string code{EmitGLASM(program)};
        }
    }
}

int main() {
    // RunDatabase();

    // FileEnvironment env{"D:\\Shaders\\Database\\test.bin"};
    FileEnvironment env{"D:\\Shaders\\Database\\Oninaki\\CS8F146B41DB6BD826.bin"};
    auto cfg{std::make_unique<Flow::CFG>(env, 0)};
    // fmt::print(stdout, "{}\n", cfg->Dot());

    Program program{env, *cfg};
    fmt::print(stdout, "{}\n", DumpProgram(program));
}
