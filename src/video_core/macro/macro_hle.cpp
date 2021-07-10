// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <vector>
#include "common/scope_exit.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/macro/macro_hle.h"
#include "video_core/rasterizer_interface.h"

namespace Tegra {

namespace {
// HLE'd functions
void HLE_771BB18C62444DA0(Engines::Maxwell3D& maxwell3d, const std::vector<u32>& parameters) {
    const u32 instance_count = parameters[2] & maxwell3d.GetRegisterValue(0xD1B);

    maxwell3d.regs.draw.topology.Assign(
        static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[0] & 0x3ffffff));
    maxwell3d.regs.vb_base_instance = parameters[5];
    maxwell3d.mme_draw.instance_count = instance_count;
    maxwell3d.regs.vb_element_base = parameters[3];
    maxwell3d.regs.index_array.count = parameters[1];
    maxwell3d.regs.index_array.first = parameters[4];

    if (maxwell3d.ShouldExecute()) {
        maxwell3d.Rasterizer().Draw(true, true);
    }
    maxwell3d.regs.index_array.count = 0;
    maxwell3d.mme_draw.instance_count = 0;
    maxwell3d.mme_draw.current_mode = Engines::Maxwell3D::MMEDrawMode::Undefined;
}

void HLE_0D61FC9FAAC9FCAD(Engines::Maxwell3D& maxwell3d, const std::vector<u32>& parameters) {
    const u32 count = (maxwell3d.GetRegisterValue(0xD1B) & parameters[2]);

    maxwell3d.regs.vertex_buffer.first = parameters[3];
    maxwell3d.regs.vertex_buffer.count = parameters[1];
    maxwell3d.regs.vb_base_instance = parameters[4];
    maxwell3d.regs.draw.topology.Assign(
        static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[0]));
    maxwell3d.mme_draw.instance_count = count;

    if (maxwell3d.ShouldExecute()) {
        maxwell3d.Rasterizer().Draw(false, true);
    }
    maxwell3d.regs.vertex_buffer.count = 0;
    maxwell3d.mme_draw.instance_count = 0;
    maxwell3d.mme_draw.current_mode = Engines::Maxwell3D::MMEDrawMode::Undefined;
}

void HLE_0217920100488FF7(Engines::Maxwell3D& maxwell3d, const std::vector<u32>& parameters) {
    const u32 instance_count = (maxwell3d.GetRegisterValue(0xD1B) & parameters[2]);
    const u32 element_base = parameters[4];
    const u32 base_instance = parameters[5];
    maxwell3d.regs.index_array.first = parameters[3];
    maxwell3d.regs.reg_array[0x446] = element_base; // vertex id base?
    maxwell3d.regs.index_array.count = parameters[1];
    maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
    maxwell3d.regs.vb_element_base = element_base;
    maxwell3d.regs.vb_base_instance = base_instance;
    maxwell3d.mme_draw.instance_count = instance_count;
    maxwell3d.CallMethodFromMME(0x8e3, 0x640);
    maxwell3d.CallMethodFromMME(0x8e4, element_base);
    maxwell3d.CallMethodFromMME(0x8e5, base_instance);
    maxwell3d.regs.draw.topology.Assign(
        static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[0]));
    if (maxwell3d.ShouldExecute()) {
        maxwell3d.Rasterizer().Draw(true, true);
    }
    maxwell3d.regs.reg_array[0x446] = 0x0; // vertex id base?
    maxwell3d.regs.index_array.count = 0;
    maxwell3d.regs.vb_element_base = 0x0;
    maxwell3d.regs.vb_base_instance = 0x0;
    maxwell3d.mme_draw.instance_count = 0;
    maxwell3d.CallMethodFromMME(0x8e3, 0x640);
    maxwell3d.CallMethodFromMME(0x8e4, 0x0);
    maxwell3d.CallMethodFromMME(0x8e5, 0x0);
    maxwell3d.mme_draw.current_mode = Engines::Maxwell3D::MMEDrawMode::Undefined;
}

// Multidraw Indirect
void HLE_3f5e74b9c9a50164(Engines::Maxwell3D& maxwell3d, const std::vector<u32>& parameters) {
    SCOPE_EXIT({
        // Clean everything.
        maxwell3d.regs.reg_array[0x446] = 0x0; // vertex id base?
        maxwell3d.regs.index_array.count = 0;
        maxwell3d.regs.vb_element_base = 0x0;
        maxwell3d.regs.vb_base_instance = 0x0;
        maxwell3d.mme_draw.instance_count = 0;
        maxwell3d.CallMethodFromMME(0x8e3, 0x640);
        maxwell3d.CallMethodFromMME(0x8e4, 0x0);
        maxwell3d.CallMethodFromMME(0x8e5, 0x0);
        maxwell3d.mme_draw.current_mode = Engines::Maxwell3D::MMEDrawMode::Undefined;
        maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
    });
    const u32 start_indirect = parameters[0];
    const u32 end_indirect = parameters[1];
    if (start_indirect >= end_indirect) {
        // Nothing to do.
        return;
    }
    const auto topology =
        static_cast<Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology>(parameters[2]);
    maxwell3d.regs.draw.topology.Assign(topology);
    const u32 padding = parameters[3];
    const std::size_t max_draws = parameters[4];

    const u32 indirect_words = 5 + padding;
    const std::size_t first_draw = start_indirect;
    const std::size_t effective_draws = end_indirect - start_indirect;
    const std::size_t last_draw = start_indirect + std::min(effective_draws, max_draws);

    for (std::size_t index = first_draw; index < last_draw; index++) {
        const std::size_t base = index * indirect_words + 5;
        const u32 num_vertices = parameters[base];
        const u32 instance_count = parameters[base + 1];
        const u32 first_index = parameters[base + 2];
        const u32 base_vertex = parameters[base + 3];
        const u32 base_instance = parameters[base + 4];
        maxwell3d.regs.index_array.first = first_index;
        maxwell3d.regs.reg_array[0x446] = base_vertex;
        maxwell3d.regs.index_array.count = num_vertices;
        maxwell3d.regs.vb_element_base = base_vertex;
        maxwell3d.regs.vb_base_instance = base_instance;
        maxwell3d.mme_draw.instance_count = instance_count;
        maxwell3d.CallMethodFromMME(0x8e3, 0x640);
        maxwell3d.CallMethodFromMME(0x8e4, base_vertex);
        maxwell3d.CallMethodFromMME(0x8e5, base_instance);
        maxwell3d.dirty.flags[VideoCommon::Dirty::IndexBuffer] = true;
        if (maxwell3d.ShouldExecute()) {
            maxwell3d.Rasterizer().Draw(true, true);
        }
        maxwell3d.mme_draw.current_mode = Engines::Maxwell3D::MMEDrawMode::Undefined;
    }
}

} // Anonymous namespace

constexpr std::array<std::pair<u64, HLEFunction>, 4> hle_funcs{{
    {0x771BB18C62444DA0, &HLE_771BB18C62444DA0},
    {0x0D61FC9FAAC9FCAD, &HLE_0D61FC9FAAC9FCAD},
    {0x0217920100488FF7, &HLE_0217920100488FF7},
    {0x3f5e74b9c9a50164, &HLE_3f5e74b9c9a50164},
}};

HLEMacro::HLEMacro(Engines::Maxwell3D& maxwell3d_) : maxwell3d{maxwell3d_} {}
HLEMacro::~HLEMacro() = default;

std::optional<std::unique_ptr<CachedMacro>> HLEMacro::GetHLEProgram(u64 hash) const {
    const auto it = std::find_if(hle_funcs.cbegin(), hle_funcs.cend(),
                                 [hash](const auto& pair) { return pair.first == hash; });
    if (it == hle_funcs.end()) {
        return std::nullopt;
    }
    return std::make_unique<HLEMacroImpl>(maxwell3d, it->second);
}

HLEMacroImpl::~HLEMacroImpl() = default;

HLEMacroImpl::HLEMacroImpl(Engines::Maxwell3D& maxwell3d_, HLEFunction func_)
    : maxwell3d{maxwell3d_}, func{func_} {}

void HLEMacroImpl::Execute(const std::vector<u32>& parameters, u32 method) {
    func(maxwell3d, parameters);
}

} // namespace Tegra
