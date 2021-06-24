// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <utility>

#include "common/assert.h"
#include "common/cityhash.h"
#include "common/common_types.h"
#include "common/div_ceil.h"
#include "common/fs/fs.h"
#include "common/logging/log.h"
#include "shader_recompiler/environment.h"
#include "video_core/memory_manager.h"
#include "video_core/shader_environment.h"
#include "video_core/textures/texture.h"

namespace VideoCommon {

constexpr std::array<char, 8> MAGIC_NUMBER{'y', 'u', 'z', 'u', 'c', 'a', 'c', 'h'};
constexpr u32 CACHE_VERSION = 5;

constexpr size_t INST_SIZE = sizeof(u64);

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

static u64 MakeCbufKey(u32 index, u32 offset) {
    return (static_cast<u64>(index) << 32) | offset;
}

static Shader::TextureType ConvertType(const Tegra::Texture::TICEntry& entry) {
    switch (entry.texture_type) {
    case Tegra::Texture::TextureType::Texture1D:
        return Shader::TextureType::Color1D;
    case Tegra::Texture::TextureType::Texture2D:
    case Tegra::Texture::TextureType::Texture2DNoMipmap:
        return Shader::TextureType::Color2D;
    case Tegra::Texture::TextureType::Texture3D:
        return Shader::TextureType::Color3D;
    case Tegra::Texture::TextureType::TextureCubemap:
        return Shader::TextureType::ColorCube;
    case Tegra::Texture::TextureType::Texture1DArray:
        return Shader::TextureType::ColorArray1D;
    case Tegra::Texture::TextureType::Texture2DArray:
        return Shader::TextureType::ColorArray2D;
    case Tegra::Texture::TextureType::Texture1DBuffer:
        return Shader::TextureType::Buffer;
    case Tegra::Texture::TextureType::TextureCubeArray:
        return Shader::TextureType::ColorArrayCube;
    default:
        throw Shader::NotImplementedException("Unknown texture type");
    }
}

GenericEnvironment::GenericEnvironment(Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_,
                                       u32 start_address_)
    : gpu_memory{&gpu_memory_}, program_base{program_base_} {
    start_address = start_address_;
}

GenericEnvironment::~GenericEnvironment() = default;

u32 GenericEnvironment::TextureBoundBuffer() const {
    return texture_bound;
}

u32 GenericEnvironment::LocalMemorySize() const {
    return local_memory_size;
}

u32 GenericEnvironment::SharedMemorySize() const {
    return shared_memory_size;
}

std::array<u32, 3> GenericEnvironment::WorkgroupSize() const {
    return workgroup_size;
}

u64 GenericEnvironment::ReadInstruction(u32 address) {
    read_lowest = std::min(read_lowest, address);
    read_highest = std::max(read_highest, address);

    if (address >= cached_lowest && address < cached_highest) {
        return code[(address - cached_lowest) / INST_SIZE];
    }
    has_unbound_instructions = true;
    return gpu_memory->Read<u64>(program_base + address);
}

std::optional<u64> GenericEnvironment::Analyze() {
    const std::optional<u64> size{TryFindSize()};
    if (!size) {
        return std::nullopt;
    }
    cached_lowest = start_address;
    cached_highest = start_address + static_cast<u32>(*size);
    return Common::CityHash64(reinterpret_cast<const char*>(code.data()), *size);
}

void GenericEnvironment::SetCachedSize(size_t size_bytes) {
    cached_lowest = start_address;
    cached_highest = start_address + static_cast<u32>(size_bytes);
    code.resize(CachedSize());
    gpu_memory->ReadBlock(program_base + cached_lowest, code.data(), code.size() * sizeof(u64));
}

size_t GenericEnvironment::CachedSize() const noexcept {
    return cached_highest - cached_lowest + INST_SIZE;
}

size_t GenericEnvironment::ReadSize() const noexcept {
    return read_highest - read_lowest + INST_SIZE;
}

bool GenericEnvironment::CanBeSerialized() const noexcept {
    return !has_unbound_instructions;
}

u64 GenericEnvironment::CalculateHash() const {
    const size_t size{ReadSize()};
    const auto data{std::make_unique<char[]>(size)};
    gpu_memory->ReadBlock(program_base + read_lowest, data.get(), size);
    return Common::CityHash64(data.get(), size);
}

void GenericEnvironment::Serialize(std::ofstream& file) const {
    const u64 code_size{static_cast<u64>(CachedSize())};
    const u64 num_texture_types{static_cast<u64>(texture_types.size())};
    const u64 num_cbuf_values{static_cast<u64>(cbuf_values.size())};

    file.write(reinterpret_cast<const char*>(&code_size), sizeof(code_size))
        .write(reinterpret_cast<const char*>(&num_texture_types), sizeof(num_texture_types))
        .write(reinterpret_cast<const char*>(&num_cbuf_values), sizeof(num_cbuf_values))
        .write(reinterpret_cast<const char*>(&local_memory_size), sizeof(local_memory_size))
        .write(reinterpret_cast<const char*>(&texture_bound), sizeof(texture_bound))
        .write(reinterpret_cast<const char*>(&start_address), sizeof(start_address))
        .write(reinterpret_cast<const char*>(&cached_lowest), sizeof(cached_lowest))
        .write(reinterpret_cast<const char*>(&cached_highest), sizeof(cached_highest))
        .write(reinterpret_cast<const char*>(&stage), sizeof(stage))
        .write(reinterpret_cast<const char*>(code.data()), code_size);
    for (const auto [key, type] : texture_types) {
        file.write(reinterpret_cast<const char*>(&key), sizeof(key))
            .write(reinterpret_cast<const char*>(&type), sizeof(type));
    }
    for (const auto [key, type] : cbuf_values) {
        file.write(reinterpret_cast<const char*>(&key), sizeof(key))
            .write(reinterpret_cast<const char*>(&type), sizeof(type));
    }
    if (stage == Shader::Stage::Compute) {
        file.write(reinterpret_cast<const char*>(&workgroup_size), sizeof(workgroup_size))
            .write(reinterpret_cast<const char*>(&shared_memory_size), sizeof(shared_memory_size));
    } else {
        file.write(reinterpret_cast<const char*>(&sph), sizeof(sph));
        if (stage == Shader::Stage::Geometry) {
            file.write(reinterpret_cast<const char*>(&gp_passthrough_mask),
                       sizeof(gp_passthrough_mask));
        }
    }
}

std::optional<u64> GenericEnvironment::TryFindSize() {
    static constexpr size_t BLOCK_SIZE = 0x1000;
    static constexpr size_t MAXIMUM_SIZE = 0x100000;

    static constexpr u64 SELF_BRANCH_A = 0xE2400FFFFF87000FULL;
    static constexpr u64 SELF_BRANCH_B = 0xE2400FFFFF07000FULL;

    GPUVAddr guest_addr{program_base + start_address};
    size_t offset{0};
    size_t size{BLOCK_SIZE};
    while (size <= MAXIMUM_SIZE) {
        code.resize(size / INST_SIZE);
        u64* const data = code.data() + offset / INST_SIZE;
        gpu_memory->ReadBlock(guest_addr, data, BLOCK_SIZE);
        for (size_t index = 0; index < BLOCK_SIZE; index += INST_SIZE) {
            const u64 inst = data[index / INST_SIZE];
            if (inst == SELF_BRANCH_A || inst == SELF_BRANCH_B) {
                return offset + index;
            }
        }
        guest_addr += BLOCK_SIZE;
        size += BLOCK_SIZE;
        offset += BLOCK_SIZE;
    }
    return std::nullopt;
}

Shader::TextureType GenericEnvironment::ReadTextureTypeImpl(GPUVAddr tic_addr, u32 tic_limit,
                                                            bool via_header_index, u32 raw) {
    const auto handle{Tegra::Texture::TexturePair(raw, via_header_index)};
    const GPUVAddr descriptor_addr{tic_addr + handle.first * sizeof(Tegra::Texture::TICEntry)};
    Tegra::Texture::TICEntry entry;
    gpu_memory->ReadBlock(descriptor_addr, &entry, sizeof(entry));
    const Shader::TextureType result{ConvertType(entry)};
    texture_types.emplace(raw, result);
    return result;
}

GraphicsEnvironment::GraphicsEnvironment(Tegra::Engines::Maxwell3D& maxwell3d_,
                                         Tegra::MemoryManager& gpu_memory_,
                                         Maxwell::ShaderProgram program, GPUVAddr program_base_,
                                         u32 start_address_)
    : GenericEnvironment{gpu_memory_, program_base_, start_address_}, maxwell3d{&maxwell3d_} {
    gpu_memory->ReadBlock(program_base + start_address, &sph, sizeof(sph));
    gp_passthrough_mask = maxwell3d->regs.gp_passthrough_mask;
    switch (program) {
    case Maxwell::ShaderProgram::VertexA:
        stage = Shader::Stage::VertexA;
        stage_index = 0;
        break;
    case Maxwell::ShaderProgram::VertexB:
        stage = Shader::Stage::VertexB;
        stage_index = 0;
        break;
    case Maxwell::ShaderProgram::TesselationControl:
        stage = Shader::Stage::TessellationControl;
        stage_index = 1;
        break;
    case Maxwell::ShaderProgram::TesselationEval:
        stage = Shader::Stage::TessellationEval;
        stage_index = 2;
        break;
    case Maxwell::ShaderProgram::Geometry:
        stage = Shader::Stage::Geometry;
        stage_index = 3;
        break;
    case Maxwell::ShaderProgram::Fragment:
        stage = Shader::Stage::Fragment;
        stage_index = 4;
        break;
    default:
        UNREACHABLE_MSG("Invalid program={}", program);
        break;
    }
    const u64 local_size{sph.LocalMemorySize()};
    ASSERT(local_size <= std::numeric_limits<u32>::max());
    local_memory_size = static_cast<u32>(local_size) + sph.common3.shader_local_memory_crs_size;
    texture_bound = maxwell3d->regs.tex_cb_index;
}

u32 GraphicsEnvironment::ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) {
    const auto& cbuf{maxwell3d->state.shader_stages[stage_index].const_buffers[cbuf_index]};
    ASSERT(cbuf.enabled);
    u32 value{};
    if (cbuf_offset < cbuf.size) {
        value = gpu_memory->Read<u32>(cbuf.address + cbuf_offset);
    }
    cbuf_values.emplace(MakeCbufKey(cbuf_index, cbuf_offset), value);
    return value;
}

Shader::TextureType GraphicsEnvironment::ReadTextureType(u32 handle) {
    const auto& regs{maxwell3d->regs};
    const bool via_header_index{regs.sampler_index == Maxwell::SamplerIndex::ViaHeaderIndex};
    return ReadTextureTypeImpl(regs.tic.Address(), regs.tic.limit, via_header_index, handle);
}

ComputeEnvironment::ComputeEnvironment(Tegra::Engines::KeplerCompute& kepler_compute_,
                                       Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_,
                                       u32 start_address_)
    : GenericEnvironment{gpu_memory_, program_base_, start_address_}, kepler_compute{
                                                                          &kepler_compute_} {
    const auto& qmd{kepler_compute->launch_description};
    stage = Shader::Stage::Compute;
    local_memory_size = qmd.local_pos_alloc + qmd.local_crs_alloc;
    texture_bound = kepler_compute->regs.tex_cb_index;
    shared_memory_size = qmd.shared_alloc;
    workgroup_size = {qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z};
}

u32 ComputeEnvironment::ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) {
    const auto& qmd{kepler_compute->launch_description};
    ASSERT(((qmd.const_buffer_enable_mask.Value() >> cbuf_index) & 1) != 0);
    const auto& cbuf{qmd.const_buffer_config[cbuf_index]};
    u32 value{};
    if (cbuf_offset < cbuf.size) {
        value = gpu_memory->Read<u32>(cbuf.Address() + cbuf_offset);
    }
    cbuf_values.emplace(MakeCbufKey(cbuf_index, cbuf_offset), value);
    return value;
}

Shader::TextureType ComputeEnvironment::ReadTextureType(u32 handle) {
    const auto& regs{kepler_compute->regs};
    const auto& qmd{kepler_compute->launch_description};
    return ReadTextureTypeImpl(regs.tic.Address(), regs.tic.limit, qmd.linked_tsc != 0, handle);
}

void FileEnvironment::Deserialize(std::ifstream& file) {
    u64 code_size{};
    u64 num_texture_types{};
    u64 num_cbuf_values{};
    file.read(reinterpret_cast<char*>(&code_size), sizeof(code_size))
        .read(reinterpret_cast<char*>(&num_texture_types), sizeof(num_texture_types))
        .read(reinterpret_cast<char*>(&num_cbuf_values), sizeof(num_cbuf_values))
        .read(reinterpret_cast<char*>(&local_memory_size), sizeof(local_memory_size))
        .read(reinterpret_cast<char*>(&texture_bound), sizeof(texture_bound))
        .read(reinterpret_cast<char*>(&start_address), sizeof(start_address))
        .read(reinterpret_cast<char*>(&read_lowest), sizeof(read_lowest))
        .read(reinterpret_cast<char*>(&read_highest), sizeof(read_highest))
        .read(reinterpret_cast<char*>(&stage), sizeof(stage));
    code = std::make_unique<u64[]>(Common::DivCeil(code_size, sizeof(u64)));
    file.read(reinterpret_cast<char*>(code.get()), code_size);
    for (size_t i = 0; i < num_texture_types; ++i) {
        u32 key;
        Shader::TextureType type;
        file.read(reinterpret_cast<char*>(&key), sizeof(key))
            .read(reinterpret_cast<char*>(&type), sizeof(type));
        texture_types.emplace(key, type);
    }
    for (size_t i = 0; i < num_cbuf_values; ++i) {
        u64 key;
        u32 value;
        file.read(reinterpret_cast<char*>(&key), sizeof(key))
            .read(reinterpret_cast<char*>(&value), sizeof(value));
        cbuf_values.emplace(key, value);
    }
    if (stage == Shader::Stage::Compute) {
        file.read(reinterpret_cast<char*>(&workgroup_size), sizeof(workgroup_size))
            .read(reinterpret_cast<char*>(&shared_memory_size), sizeof(shared_memory_size));
    } else {
        file.read(reinterpret_cast<char*>(&sph), sizeof(sph));
        if (stage == Shader::Stage::Geometry) {
            file.read(reinterpret_cast<char*>(&gp_passthrough_mask), sizeof(gp_passthrough_mask));
        }
    }
}

u64 FileEnvironment::ReadInstruction(u32 address) {
    if (address < read_lowest || address > read_highest) {
        throw Shader::LogicError("Out of bounds address {}", address);
    }
    return code[(address - read_lowest) / sizeof(u64)];
}

u32 FileEnvironment::ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) {
    const auto it{cbuf_values.find(MakeCbufKey(cbuf_index, cbuf_offset))};
    if (it == cbuf_values.end()) {
        throw Shader::LogicError("Uncached read texture type");
    }
    return it->second;
}

Shader::TextureType FileEnvironment::ReadTextureType(u32 handle) {
    const auto it{texture_types.find(handle)};
    if (it == texture_types.end()) {
        throw Shader::LogicError("Uncached read texture type");
    }
    return it->second;
}

u32 FileEnvironment::LocalMemorySize() const {
    return local_memory_size;
}

u32 FileEnvironment::SharedMemorySize() const {
    return shared_memory_size;
}

u32 FileEnvironment::TextureBoundBuffer() const {
    return texture_bound;
}

std::array<u32, 3> FileEnvironment::WorkgroupSize() const {
    return workgroup_size;
}

void SerializePipeline(std::span<const char> key, std::span<const GenericEnvironment* const> envs,
                       const std::filesystem::path& filename) try {
    std::ofstream file(filename, std::ios::binary | std::ios::ate | std::ios::app);
    file.exceptions(std::ifstream::failbit);
    if (!file.is_open()) {
        LOG_ERROR(Common_Filesystem, "Failed to open pipeline cache file {}",
                  Common::FS::PathToUTF8String(filename));
        return;
    }
    if (file.tellp() == 0) {
        // Write header
        file.write(MAGIC_NUMBER.data(), MAGIC_NUMBER.size())
            .write(reinterpret_cast<const char*>(&CACHE_VERSION), sizeof(CACHE_VERSION));
    }
    if (!std::ranges::all_of(envs, &GenericEnvironment::CanBeSerialized)) {
        return;
    }
    const u32 num_envs{static_cast<u32>(envs.size())};
    file.write(reinterpret_cast<const char*>(&num_envs), sizeof(num_envs));
    for (const GenericEnvironment* const env : envs) {
        env->Serialize(file);
    }
    file.write(key.data(), key.size_bytes());

} catch (const std::ios_base::failure& e) {
    LOG_ERROR(Common_Filesystem, "{}", e.what());
    if (!Common::FS::RemoveFile(filename)) {
        LOG_ERROR(Common_Filesystem, "Failed to delete pipeline cache file {}",
                  Common::FS::PathToUTF8String(filename));
    }
}

void LoadPipelines(
    std::stop_token stop_loading, const std::filesystem::path& filename,
    Common::UniqueFunction<void, std::ifstream&, FileEnvironment> load_compute,
    Common::UniqueFunction<void, std::ifstream&, std::vector<FileEnvironment>> load_graphics) try {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return;
    }
    file.exceptions(std::ifstream::failbit);
    const auto end{file.tellg()};
    file.seekg(0, std::ios::beg);

    std::array<char, 8> magic_number;
    u32 cache_version;
    file.read(magic_number.data(), magic_number.size())
        .read(reinterpret_cast<char*>(&cache_version), sizeof(cache_version));
    if (magic_number != MAGIC_NUMBER || cache_version != CACHE_VERSION) {
        file.close();
        if (Common::FS::RemoveFile(filename)) {
            if (magic_number != MAGIC_NUMBER) {
                LOG_ERROR(Common_Filesystem, "Invalid pipeline cache file");
            }
            if (cache_version != CACHE_VERSION) {
                LOG_INFO(Common_Filesystem, "Deleting old pipeline cache");
            }
        } else {
            LOG_ERROR(Common_Filesystem,
                      "Invalid pipeline cache file and failed to delete it in \"{}\"",
                      Common::FS::PathToUTF8String(filename));
        }
        return;
    }
    while (file.tellg() != end) {
        if (stop_loading.stop_requested()) {
            return;
        }
        u32 num_envs{};
        file.read(reinterpret_cast<char*>(&num_envs), sizeof(num_envs));
        std::vector<FileEnvironment> envs(num_envs);
        for (FileEnvironment& env : envs) {
            env.Deserialize(file);
        }
        if (envs.front().ShaderStage() == Shader::Stage::Compute) {
            load_compute(file, std::move(envs.front()));
        } else {
            load_graphics(file, std::move(envs));
        }
    }

} catch (const std::ios_base::failure& e) {
    LOG_ERROR(Common_Filesystem, "{}", e.what());
    if (!Common::FS::RemoveFile(filename)) {
        LOG_ERROR(Common_Filesystem, "Failed to delete pipeline cache file {}",
                  Common::FS::PathToUTF8String(filename));
    }
}

} // namespace VideoCommon
