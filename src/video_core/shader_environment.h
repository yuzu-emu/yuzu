// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <iosfwd>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "common/unique_function.h"
#include "shader_recompiler/environment.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/textures/texture.h"

namespace Tegra {
class Memorymanager;
}

namespace VideoCommon {

class GenericEnvironment : public Shader::Environment {
public:
    explicit GenericEnvironment() = default;
    explicit GenericEnvironment(Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_,
                                u32 start_address_);

    ~GenericEnvironment() override;

    [[nodiscard]] u32 TextureBoundBuffer() const final;

    [[nodiscard]] u32 LocalMemorySize() const final;

    [[nodiscard]] u32 SharedMemorySize() const final;

    [[nodiscard]] std::array<u32, 3> WorkgroupSize() const final;

    [[nodiscard]] u64 ReadInstruction(u32 address) final;

    [[nodiscard]] std::optional<u64> Analyze();

    void SetCachedSize(size_t size_bytes);

    [[nodiscard]] size_t CachedSize() const noexcept;

    [[nodiscard]] size_t ReadSize() const noexcept;

    [[nodiscard]] bool CanBeSerialized() const noexcept;

    [[nodiscard]] u64 CalculateHash() const;

    void Serialize(std::ofstream& file) const;

protected:
    std::optional<u64> TryFindSize();

    Shader::TextureType ReadTextureTypeImpl(GPUVAddr tic_addr, u32 tic_limit, bool via_header_index,
                                            u32 raw);

    Tegra::MemoryManager* gpu_memory{};
    GPUVAddr program_base{};

    std::vector<u64> code;
    std::unordered_map<u32, Shader::TextureType> texture_types;
    std::unordered_map<u64, u32> cbuf_values;

    u32 local_memory_size{};
    u32 texture_bound{};
    u32 shared_memory_size{};
    std::array<u32, 3> workgroup_size{};

    u32 read_lowest = std::numeric_limits<u32>::max();
    u32 read_highest = 0;

    u32 cached_lowest = std::numeric_limits<u32>::max();
    u32 cached_highest = 0;

    bool has_unbound_instructions = false;
};

class GraphicsEnvironment final : public GenericEnvironment {
public:
    explicit GraphicsEnvironment() = default;
    explicit GraphicsEnvironment(Tegra::Engines::Maxwell3D& maxwell3d_,
                                 Tegra::MemoryManager& gpu_memory_,
                                 Tegra::Engines::Maxwell3D::Regs::ShaderProgram program,
                                 GPUVAddr program_base_, u32 start_address_);

    ~GraphicsEnvironment() override = default;

    u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) override;

    Shader::TextureType ReadTextureType(u32 handle) override;

private:
    Tegra::Engines::Maxwell3D* maxwell3d{};
    size_t stage_index{};
};

class ComputeEnvironment final : public GenericEnvironment {
public:
    explicit ComputeEnvironment() = default;
    explicit ComputeEnvironment(Tegra::Engines::KeplerCompute& kepler_compute_,
                                Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_,
                                u32 start_address_);

    ~ComputeEnvironment() override = default;

    u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) override;

    Shader::TextureType ReadTextureType(u32 handle) override;

private:
    Tegra::Engines::KeplerCompute* kepler_compute{};
};

class FileEnvironment final : public Shader::Environment {
public:
    FileEnvironment() = default;
    ~FileEnvironment() override = default;

    FileEnvironment& operator=(FileEnvironment&&) noexcept = default;
    FileEnvironment(FileEnvironment&&) noexcept = default;

    FileEnvironment& operator=(const FileEnvironment&) = delete;
    FileEnvironment(const FileEnvironment&) = delete;

    void Deserialize(std::ifstream& file);

    [[nodiscard]] u64 ReadInstruction(u32 address) override;

    [[nodiscard]] u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) override;

    [[nodiscard]] Shader::TextureType ReadTextureType(u32 handle) override;

    [[nodiscard]] u32 LocalMemorySize() const override;

    [[nodiscard]] u32 SharedMemorySize() const override;

    [[nodiscard]] u32 TextureBoundBuffer() const override;

    [[nodiscard]] std::array<u32, 3> WorkgroupSize() const override;

private:
    std::unique_ptr<u64[]> code;
    std::unordered_map<u32, Shader::TextureType> texture_types;
    std::unordered_map<u64, u32> cbuf_values;
    std::array<u32, 3> workgroup_size{};
    u32 local_memory_size{};
    u32 shared_memory_size{};
    u32 texture_bound{};
    u32 read_lowest{};
    u32 read_highest{};
};

void SerializePipeline(std::span<const char> key, std::span<const GenericEnvironment* const> envs,
                       const std::filesystem::path& filename);

template <typename Key, typename Envs>
void SerializePipeline(const Key& key, const Envs& envs, const std::filesystem::path& filename) {
    static_assert(std::is_trivially_copyable_v<Key>);
    static_assert(std::has_unique_object_representations_v<Key>);
    SerializePipeline(std::span(reinterpret_cast<const char*>(&key), sizeof(key)),
                      std::span(envs.data(), envs.size()), filename);
}

void LoadPipelines(
    std::stop_token stop_loading, const std::filesystem::path& filename,
    Common::UniqueFunction<void, std::ifstream&, FileEnvironment> load_compute,
    Common::UniqueFunction<void, std::ifstream&, std::vector<FileEnvironment>> load_graphics);

} // namespace VideoCommon
