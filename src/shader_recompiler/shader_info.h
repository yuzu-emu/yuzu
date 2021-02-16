// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_types.h"

#include <boost/container/static_vector.hpp>

namespace Shader {

struct Info {
    static constexpr size_t MAX_CBUFS{18};
    static constexpr size_t MAX_SSBOS{16};

    struct ConstantBufferDescriptor {
        u32 index;
        u32 count;
    };

    struct StorageBufferDescriptor {
        u32 cbuf_index;
        u32 cbuf_offset;
        u32 count;
    };

    bool uses_workgroup_id{};
    bool uses_local_invocation_id{};
    bool uses_fp16{};
    bool uses_fp64{};

    u32 constant_buffer_mask{};

    std::array<ConstantBufferDescriptor*, MAX_CBUFS> constant_buffers{};
    boost::container::static_vector<ConstantBufferDescriptor, MAX_CBUFS>
        constant_buffer_descriptors;

    std::array<StorageBufferDescriptor*, MAX_SSBOS> storage_buffers{};
    boost::container::static_vector<StorageBufferDescriptor, MAX_SSBOS> storage_buffers_descriptors;
};

} // namespace Shader
