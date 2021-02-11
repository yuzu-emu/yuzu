// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include <boost/container/static_vector.hpp>

namespace Shader {

struct Info {
    struct ConstantBuffer {

    };

    struct {
        bool workgroup_id{};
        bool local_invocation_id{};
        bool fp16{};
        bool fp64{};
    } uses;

    std::array<18
};

} // namespace Shader
