// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include "common/bit_cast.h"
#include "common/cityhash.h"
#include "common/file_util.h"
#include "common/microprofile.h"
#include "common/thread_worker.h"
#include "core/core.h"
#include "core/memory.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/program.h"
#include "shader_recompiler/program_header.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/pipeline_helper.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_notify.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
MICROPROFILE_DECLARE(Vulkan_PipelineCache);

template <typename Container>
auto MakeSpan(Container& container) {
    return std::span(container.data(), container.size());
}

static u64 MakeCbufKey(u32 index, u32 offset) {
    return (static_cast<u64>(index) << 32) | offset;
}

class GenericEnvironment : public Shader::Environment {
public:
    explicit GenericEnvironment() = default;
    explicit GenericEnvironment(Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_,
                                u32 start_address_)
        : gpu_memory{&gpu_memory_}, program_base{program_base_} {
        start_address = start_address_;
    }

    ~GenericEnvironment() override = default;

    u32 TextureBoundBuffer() const final {
        return texture_bound;
    }

    u32 LocalMemorySize() const final {
        return local_memory_size;
    }

    u32 SharedMemorySize() const final {
        return shared_memory_size;
    }

    std::array<u32, 3> WorkgroupSize() const final {
        return workgroup_size;
    }

    u64 ReadInstruction(u32 address) final {
        read_lowest = std::min(read_lowest, address);
        read_highest = std::max(read_highest, address);

        if (address >= cached_lowest && address < cached_highest) {
            return code[(address - cached_lowest) / INST_SIZE];
        }
        has_unbound_instructions = true;
        return gpu_memory->Read<u64>(program_base + address);
    }

    std::optional<u128> Analyze() {
        const std::optional<u64> size{TryFindSize()};
        if (!size) {
            return std::nullopt;
        }
        cached_lowest = start_address;
        cached_highest = start_address + static_cast<u32>(*size);
        return Common::CityHash128(reinterpret_cast<const char*>(code.data()), *size);
    }

    void SetCachedSize(size_t size_bytes) {
        cached_lowest = start_address;
        cached_highest = start_address + static_cast<u32>(size_bytes);
        code.resize(CachedSize());
        gpu_memory->ReadBlock(program_base + cached_lowest, code.data(), code.size() * sizeof(u64));
    }

    [[nodiscard]] size_t CachedSize() const noexcept {
        return cached_highest - cached_lowest + INST_SIZE;
    }

    [[nodiscard]] size_t ReadSize() const noexcept {
        return read_highest - read_lowest + INST_SIZE;
    }

    [[nodiscard]] bool CanBeSerialized() const noexcept {
        return !has_unbound_instructions;
    }

    [[nodiscard]] u128 CalculateHash() const {
        const size_t size{ReadSize()};
        const auto data{std::make_unique<char[]>(size)};
        gpu_memory->ReadBlock(program_base + read_lowest, data.get(), size);
        return Common::CityHash128(data.get(), size);
    }

    void Serialize(std::ofstream& file) const {
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
                .write(reinterpret_cast<const char*>(&shared_memory_size),
                       sizeof(shared_memory_size));
        } else {
            file.write(reinterpret_cast<const char*>(&sph), sizeof(sph));
        }
    }

protected:
    static constexpr size_t INST_SIZE = sizeof(u64);

    std::optional<u64> TryFindSize() {
        constexpr size_t BLOCK_SIZE = 0x1000;
        constexpr size_t MAXIMUM_SIZE = 0x100000;

        constexpr u64 SELF_BRANCH_A = 0xE2400FFFFF87000FULL;
        constexpr u64 SELF_BRANCH_B = 0xE2400FFFFF07000FULL;

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

    Shader::TextureType ReadTextureTypeImpl(GPUVAddr tic_addr, u32 tic_limit, bool via_header_index,
                                            GPUVAddr cbuf_addr, u32 cbuf_size, u32 cbuf_index,
                                            u32 cbuf_offset) {
        const u32 raw{cbuf_offset < cbuf_size ? gpu_memory->Read<u32>(cbuf_addr + cbuf_offset) : 0};
        const TextureHandle handle{raw, via_header_index};
        const GPUVAddr descriptor_addr{tic_addr + handle.image * sizeof(Tegra::Texture::TICEntry)};
        Tegra::Texture::TICEntry entry;
        gpu_memory->ReadBlock(descriptor_addr, &entry, sizeof(entry));

        const Shader::TextureType result{[&] {
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
        }()};
        texture_types.emplace(MakeCbufKey(cbuf_index, cbuf_offset), result);
        return result;
    }

    Tegra::MemoryManager* gpu_memory{};
    GPUVAddr program_base{};

    std::vector<u64> code;
    std::unordered_map<u64, Shader::TextureType> texture_types;
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

namespace {
using Shader::Backend::SPIRV::EmitSPIRV;
using Shader::Maxwell::TranslateProgram;

// TODO: Move this to a separate file
constexpr std::array<char, 8> MAGIC_NUMBER{'y', 'u', 'z', 'u', 'c', 'a', 'c', 'h'};
constexpr u32 CACHE_VERSION{1};

class GraphicsEnvironment final : public GenericEnvironment {
public:
    explicit GraphicsEnvironment() = default;
    explicit GraphicsEnvironment(Tegra::Engines::Maxwell3D& maxwell3d_,
                                 Tegra::MemoryManager& gpu_memory_, Maxwell::ShaderProgram program,
                                 GPUVAddr program_base_, u32 start_address_)
        : GenericEnvironment{gpu_memory_, program_base_, start_address_}, maxwell3d{&maxwell3d_} {
        gpu_memory->ReadBlock(program_base + start_address, &sph, sizeof(sph));
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
        local_memory_size = static_cast<u32>(local_size);
        texture_bound = maxwell3d->regs.tex_cb_index;
    }

    ~GraphicsEnvironment() override = default;

    u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) override {
        const auto& cbuf{maxwell3d->state.shader_stages[stage_index].const_buffers[cbuf_index]};
        ASSERT(cbuf.enabled);
        u32 value{};
        if (cbuf_offset < cbuf.size) {
            value = gpu_memory->Read<u32>(cbuf.address + cbuf_offset);
        }
        cbuf_values.emplace(MakeCbufKey(cbuf_index, cbuf_offset), value);
        return value;
    }

    Shader::TextureType ReadTextureType(u32 cbuf_index, u32 cbuf_offset) override {
        const auto& regs{maxwell3d->regs};
        const auto& cbuf{maxwell3d->state.shader_stages[stage_index].const_buffers[cbuf_index]};
        ASSERT(cbuf.enabled);
        const bool via_header_index{regs.sampler_index == Maxwell::SamplerIndex::ViaHeaderIndex};
        return ReadTextureTypeImpl(regs.tic.Address(), regs.tic.limit, via_header_index,
                                   cbuf.address, cbuf.size, cbuf_index, cbuf_offset);
    }

private:
    Tegra::Engines::Maxwell3D* maxwell3d{};
    size_t stage_index{};
};

class ComputeEnvironment final : public GenericEnvironment {
public:
    explicit ComputeEnvironment() = default;
    explicit ComputeEnvironment(Tegra::Engines::KeplerCompute& kepler_compute_,
                                Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_,
                                u32 start_address_)
        : GenericEnvironment{gpu_memory_, program_base_, start_address_}, kepler_compute{
                                                                              &kepler_compute_} {
        const auto& qmd{kepler_compute->launch_description};
        stage = Shader::Stage::Compute;
        local_memory_size = qmd.local_pos_alloc;
        texture_bound = kepler_compute->regs.tex_cb_index;
        shared_memory_size = qmd.shared_alloc;
        workgroup_size = {qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z};
    }

    ~ComputeEnvironment() override = default;

    u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) override {
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

    Shader::TextureType ReadTextureType(u32 cbuf_index, u32 cbuf_offset) override {
        const auto& regs{kepler_compute->regs};
        const auto& qmd{kepler_compute->launch_description};
        ASSERT(((qmd.const_buffer_enable_mask.Value() >> cbuf_index) & 1) != 0);
        const auto& cbuf{qmd.const_buffer_config[cbuf_index]};
        return ReadTextureTypeImpl(regs.tic.Address(), regs.tic.limit, qmd.linked_tsc != 0,
                                   cbuf.Address(), cbuf.size, cbuf_index, cbuf_offset);
    }

private:
    Tegra::Engines::KeplerCompute* kepler_compute{};
};

void SerializePipeline(std::span<const char> key, std::span<const GenericEnvironment* const> envs,
                       std::ofstream& file) {
    if (!std::ranges::all_of(envs, &GenericEnvironment::CanBeSerialized)) {
        return;
    }
    const u32 num_envs{static_cast<u32>(envs.size())};
    file.write(reinterpret_cast<const char*>(&num_envs), sizeof(num_envs));
    for (const GenericEnvironment* const env : envs) {
        env->Serialize(file);
    }
    file.write(key.data(), key.size_bytes());
}

template <typename Key, typename Envs>
void SerializePipeline(const Key& key, const Envs& envs, const std::string& filename) {
    try {
        std::ofstream file;
        file.exceptions(std::ifstream::failbit);
        Common::FS::OpenFStream(file, filename, std::ios::binary | std::ios::ate | std::ios::app);
        if (!file.is_open()) {
            LOG_ERROR(Common_Filesystem, "Failed to open pipeline cache file {}", filename);
            return;
        }
        if (file.tellp() == 0) {
            file.write(MAGIC_NUMBER.data(), MAGIC_NUMBER.size())
                .write(reinterpret_cast<const char*>(&CACHE_VERSION), sizeof(CACHE_VERSION));
        }
        const std::span key_span(reinterpret_cast<const char*>(&key), sizeof(key));
        SerializePipeline(key_span, MakeSpan(envs), file);

    } catch (const std::ios_base::failure& e) {
        LOG_ERROR(Common_Filesystem, "{}", e.what());
        if (!Common::FS::Delete(filename)) {
            LOG_ERROR(Common_Filesystem, "Failed to delete pipeline cache file {}", filename);
        }
    }
}

class FileEnvironment final : public Shader::Environment {
public:
    void Deserialize(std::ifstream& file) {
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
            u64 key;
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
        }
    }

    u64 ReadInstruction(u32 address) override {
        if (address < read_lowest || address > read_highest) {
            throw Shader::LogicError("Out of bounds address {}", address);
        }
        return code[(address - read_lowest) / sizeof(u64)];
    }

    u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) override {
        const auto it{cbuf_values.find(MakeCbufKey(cbuf_index, cbuf_offset))};
        if (it == cbuf_values.end()) {
            throw Shader::LogicError("Uncached read texture type");
        }
        return it->second;
    }

    Shader::TextureType ReadTextureType(u32 cbuf_index, u32 cbuf_offset) override {
        const auto it{texture_types.find(MakeCbufKey(cbuf_index, cbuf_offset))};
        if (it == texture_types.end()) {
            throw Shader::LogicError("Uncached read texture type");
        }
        return it->second;
    }

    u32 LocalMemorySize() const override {
        return local_memory_size;
    }

    u32 SharedMemorySize() const override {
        return shared_memory_size;
    }

    u32 TextureBoundBuffer() const override {
        return texture_bound;
    }

    std::array<u32, 3> WorkgroupSize() const override {
        return workgroup_size;
    }

private:
    std::unique_ptr<u64[]> code;
    std::unordered_map<u64, Shader::TextureType> texture_types;
    std::unordered_map<u64, u32> cbuf_values;
    std::array<u32, 3> workgroup_size{};
    u32 local_memory_size{};
    u32 shared_memory_size{};
    u32 texture_bound{};
    u32 read_lowest{};
    u32 read_highest{};
};

Shader::CompareFunction MaxwellToCompareFunction(Maxwell::ComparisonOp comparison) {
    switch (comparison) {
    case Maxwell::ComparisonOp::Never:
    case Maxwell::ComparisonOp::NeverOld:
        return Shader::CompareFunction::Never;
    case Maxwell::ComparisonOp::Less:
    case Maxwell::ComparisonOp::LessOld:
        return Shader::CompareFunction::Less;
    case Maxwell::ComparisonOp::Equal:
    case Maxwell::ComparisonOp::EqualOld:
        return Shader::CompareFunction::Equal;
    case Maxwell::ComparisonOp::LessEqual:
    case Maxwell::ComparisonOp::LessEqualOld:
        return Shader::CompareFunction::LessThanEqual;
    case Maxwell::ComparisonOp::Greater:
    case Maxwell::ComparisonOp::GreaterOld:
        return Shader::CompareFunction::Greater;
    case Maxwell::ComparisonOp::NotEqual:
    case Maxwell::ComparisonOp::NotEqualOld:
        return Shader::CompareFunction::NotEqual;
    case Maxwell::ComparisonOp::GreaterEqual:
    case Maxwell::ComparisonOp::GreaterEqualOld:
        return Shader::CompareFunction::GreaterThanEqual;
    case Maxwell::ComparisonOp::Always:
    case Maxwell::ComparisonOp::AlwaysOld:
        return Shader::CompareFunction::Always;
    }
    UNIMPLEMENTED_MSG("Unimplemented comparison op={}", comparison);
    return {};
}
} // Anonymous namespace

void PipelineCache::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                      const VideoCore::DiskResourceLoadCallback& callback) {
    if (title_id == 0) {
        return;
    }
    std::string shader_dir{Common::FS::GetUserPath(Common::FS::UserPath::ShaderDir)};
    std::string base_dir{shader_dir + "/vulkan"};
    std::string transferable_dir{base_dir + "/transferable"};
    std::string precompiled_dir{base_dir + "/precompiled"};
    if (!Common::FS::CreateDir(shader_dir) || !Common::FS::CreateDir(base_dir) ||
        !Common::FS::CreateDir(transferable_dir) || !Common::FS::CreateDir(precompiled_dir)) {
        LOG_ERROR(Common_Filesystem, "Failed to create pipeline cache directories");
        return;
    }
    pipeline_cache_filename = fmt::format("{}/{:016x}.bin", transferable_dir, title_id);

    struct {
        std::mutex mutex;
        size_t total{0};
        size_t built{0};
        bool has_loaded{false};
    } state;

    std::ifstream file;
    Common::FS::OpenFStream(file, pipeline_cache_filename, std::ios::binary | std::ios::ate);
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
        if (Common::FS::Delete(pipeline_cache_filename)) {
            if (magic_number != MAGIC_NUMBER) {
                LOG_ERROR(Render_Vulkan, "Invalid pipeline cache file");
            }
            if (cache_version != CACHE_VERSION) {
                LOG_INFO(Render_Vulkan, "Deleting old pipeline cache");
            }
        } else {
            LOG_ERROR(Render_Vulkan,
                      "Invalid pipeline cache file and failed to delete it in \"{}\"",
                      pipeline_cache_filename);
        }
        return;
    }
    while (file.tellg() != end) {
        if (stop_loading) {
            return;
        }
        u32 num_envs{};
        file.read(reinterpret_cast<char*>(&num_envs), sizeof(num_envs));
        std::vector<FileEnvironment> envs(num_envs);
        for (FileEnvironment& env : envs) {
            env.Deserialize(file);
        }
        if (envs.front().ShaderStage() == Shader::Stage::Compute) {
            ComputePipelineCacheKey key;
            file.read(reinterpret_cast<char*>(&key), sizeof(key));

            workers.QueueWork([this, key, envs = std::move(envs), &state, &callback]() mutable {
                ShaderPools pools;
                auto pipeline{CreateComputePipeline(pools, key, envs.front(), false)};

                std::lock_guard lock{state.mutex};
                compute_cache.emplace(key, std::move(pipeline));
                ++state.built;
                if (state.has_loaded) {
                    callback(VideoCore::LoadCallbackStage::Build, state.built, state.total);
                }
            });
        } else {
            GraphicsPipelineCacheKey key;
            file.read(reinterpret_cast<char*>(&key), sizeof(key));

            workers.QueueWork([this, key, envs = std::move(envs), &state, &callback]() mutable {
                ShaderPools pools;
                boost::container::static_vector<Shader::Environment*, 5> env_ptrs;
                for (auto& env : envs) {
                    env_ptrs.push_back(&env);
                }
                auto pipeline{CreateGraphicsPipeline(pools, key, MakeSpan(env_ptrs), false)};

                std::lock_guard lock{state.mutex};
                graphics_cache.emplace(key, std::move(pipeline));
                ++state.built;
                if (state.has_loaded) {
                    callback(VideoCore::LoadCallbackStage::Build, state.built, state.total);
                }
            });
        }
        ++state.total;
    }
    {
        std::lock_guard lock{state.mutex};
        callback(VideoCore::LoadCallbackStage::Build, 0, state.total);
        state.has_loaded = true;
    }
    workers.WaitForRequests();
}

size_t ComputePipelineCacheKey::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this);
    return static_cast<size_t>(hash);
}

bool ComputePipelineCacheKey::operator==(const ComputePipelineCacheKey& rhs) const noexcept {
    return std::memcmp(&rhs, this, sizeof *this) == 0;
}

size_t GraphicsPipelineCacheKey::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), Size());
    return static_cast<size_t>(hash);
}

bool GraphicsPipelineCacheKey::operator==(const GraphicsPipelineCacheKey& rhs) const noexcept {
    return std::memcmp(&rhs, this, Size()) == 0;
}

PipelineCache::PipelineCache(RasterizerVulkan& rasterizer_, Tegra::GPU& gpu_,
                             Tegra::Engines::Maxwell3D& maxwell3d_,
                             Tegra::Engines::KeplerCompute& kepler_compute_,
                             Tegra::MemoryManager& gpu_memory_, const Device& device_,
                             VKScheduler& scheduler_, VKDescriptorPool& descriptor_pool_,
                             VKUpdateDescriptorQueue& update_descriptor_queue_,
                             RenderPassCache& render_pass_cache_, BufferCache& buffer_cache_,
                             TextureCache& texture_cache_)
    : VideoCommon::ShaderCache<ShaderInfo>{rasterizer_}, gpu{gpu_}, maxwell3d{maxwell3d_},
      kepler_compute{kepler_compute_}, gpu_memory{gpu_memory_}, device{device_},
      scheduler{scheduler_}, descriptor_pool{descriptor_pool_},
      update_descriptor_queue{update_descriptor_queue_}, render_pass_cache{render_pass_cache_},
      buffer_cache{buffer_cache_}, texture_cache{texture_cache_},
      workers(std::max(std::thread::hardware_concurrency(), 2U) - 1, "yuzu:PipelineBuilder"),
      serialization_thread(1, "yuzu:PipelineSerialization") {
    const auto& float_control{device.FloatControlProperties()};
    const VkDriverIdKHR driver_id{device.GetDriverID()};
    base_profile = Shader::Profile{
        .supported_spirv = device.IsKhrSpirv1_4Supported() ? 0x00010400U : 0x00010000U,
        .unified_descriptor_binding = true,
        .support_vertex_instance_id = false,
        .support_float_controls = true,
        .support_separate_denorm_behavior = float_control.denormBehaviorIndependence ==
                                            VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR,
        .support_separate_rounding_mode =
            float_control.roundingModeIndependence == VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR,
        .support_fp16_denorm_preserve = float_control.shaderDenormPreserveFloat16 != VK_FALSE,
        .support_fp32_denorm_preserve = float_control.shaderDenormPreserveFloat32 != VK_FALSE,
        .support_fp16_denorm_flush = float_control.shaderDenormFlushToZeroFloat16 != VK_FALSE,
        .support_fp32_denorm_flush = float_control.shaderDenormFlushToZeroFloat32 != VK_FALSE,
        .support_fp16_signed_zero_nan_preserve =
            float_control.shaderSignedZeroInfNanPreserveFloat16 != VK_FALSE,
        .support_fp32_signed_zero_nan_preserve =
            float_control.shaderSignedZeroInfNanPreserveFloat32 != VK_FALSE,
        .support_fp64_signed_zero_nan_preserve =
            float_control.shaderSignedZeroInfNanPreserveFloat64 != VK_FALSE,
        .support_explicit_workgroup_layout = device.IsKhrWorkgroupMemoryExplicitLayoutSupported(),
        .support_vote = true,
        .support_viewport_index_layer_non_geometry =
            device.IsExtShaderViewportIndexLayerSupported(),
        .support_typeless_image_loads = device.IsFormatlessImageLoadSupported(),
        .warp_size_potentially_larger_than_guest = device.IsWarpSizePotentiallyBiggerThanGuest(),
        .support_int64_atomics = device.IsExtShaderAtomicInt64Supported(),
        .has_broken_spirv_clamp = driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS_KHR,
        .generic_input_types{},
        .fixed_state_point_size{},
        .alpha_test_func{},
        .xfb_varyings{},
    };
}

PipelineCache::~PipelineCache() = default;

GraphicsPipeline* PipelineCache::CurrentGraphicsPipeline() {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    if (!RefreshStages()) {
        return nullptr;
    }
    graphics_key.state.Refresh(maxwell3d, device.IsExtExtendedDynamicStateSupported());

    const auto [pair, is_new]{graphics_cache.try_emplace(graphics_key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return pipeline.get();
    }
    pipeline = CreateGraphicsPipeline();
    return pipeline.get();
}

ComputePipeline* PipelineCache::CurrentComputePipeline() {
    MICROPROFILE_SCOPE(Vulkan_PipelineCache);

    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    const GPUVAddr shader_addr{program_base + qmd.program_start};
    const std::optional<VAddr> cpu_shader_addr{gpu_memory.GpuToCpuAddress(shader_addr)};
    if (!cpu_shader_addr) {
        return nullptr;
    }
    const ShaderInfo* shader{TryGet(*cpu_shader_addr)};
    if (!shader) {
        ComputeEnvironment env{kepler_compute, gpu_memory, program_base, qmd.program_start};
        shader = MakeShaderInfo(env, *cpu_shader_addr);
    }
    const ComputePipelineCacheKey key{
        .unique_hash{shader->unique_hash},
        .shared_memory_size{qmd.shared_alloc},
        .workgroup_size{qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z},
    };
    const auto [pair, is_new]{compute_cache.try_emplace(key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return pipeline.get();
    }
    pipeline = CreateComputePipeline(key, shader);
    return pipeline.get();
}

bool PipelineCache::RefreshStages() {
    const GPUVAddr base_addr{maxwell3d.regs.code_address.CodeAddress()};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (!maxwell3d.regs.IsShaderConfigEnabled(index)) {
            graphics_key.unique_hashes[index] = u128{};
            continue;
        }
        const auto& shader_config{maxwell3d.regs.shader_config[index]};
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};
        const GPUVAddr shader_addr{base_addr + shader_config.offset};
        const std::optional<VAddr> cpu_shader_addr{gpu_memory.GpuToCpuAddress(shader_addr)};
        if (!cpu_shader_addr) {
            LOG_ERROR(Render_Vulkan, "Invalid GPU address for shader 0x{:016x}", shader_addr);
            return false;
        }
        const ShaderInfo* shader_info{TryGet(*cpu_shader_addr)};
        if (!shader_info) {
            const u32 start_address{shader_config.offset};
            GraphicsEnvironment env{maxwell3d, gpu_memory, program, base_addr, start_address};
            shader_info = MakeShaderInfo(env, *cpu_shader_addr);
        }
        shader_infos[index] = shader_info;
        graphics_key.unique_hashes[index] = shader_info->unique_hash;
    }
    return true;
}

const ShaderInfo* PipelineCache::MakeShaderInfo(GenericEnvironment& env, VAddr cpu_addr) {
    auto info = std::make_unique<ShaderInfo>();
    if (const std::optional<u128> cached_hash{env.Analyze()}) {
        info->unique_hash = *cached_hash;
        info->size_bytes = env.CachedSize();
    } else {
        // Slow path, not really hit on commercial games
        // Build a control flow graph to get the real shader size
        main_pools.flow_block.ReleaseContents();
        Shader::Maxwell::Flow::CFG cfg{env, main_pools.flow_block, env.StartAddress()};
        info->unique_hash = env.CalculateHash();
        info->size_bytes = env.ReadSize();
    }
    const size_t size_bytes{info->size_bytes};
    const ShaderInfo* const result{info.get()};
    Register(std::move(info), cpu_addr, size_bytes);
    return result;
}

std::unique_ptr<GraphicsPipeline> PipelineCache::CreateGraphicsPipeline(
    ShaderPools& pools, const GraphicsPipelineCacheKey& key,
    std::span<Shader::Environment* const> envs, bool build_in_parallel) {
    LOG_INFO(Render_Vulkan, "0x{:016x}", key.Hash());
    size_t env_index{0};
    std::array<Shader::IR::Program, Maxwell::MaxShaderProgram> programs;
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == u128{}) {
            continue;
        }
        Shader::Environment& env{*envs[env_index]};
        ++env_index;

        const u32 cfg_offset{static_cast<u32>(env.StartAddress() + sizeof(Shader::ProgramHeader))};
        Shader::Maxwell::Flow::CFG cfg(env, pools.flow_block, cfg_offset);
        programs[index] = TranslateProgram(pools.inst, pools.block, env, cfg);
    }
    std::array<const Shader::Info*, Maxwell::MaxShaderStage> infos{};
    std::array<vk::ShaderModule, Maxwell::MaxShaderStage> modules;

    u32 binding{0};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == u128{}) {
            continue;
        }
        UNIMPLEMENTED_IF(index == 0);

        Shader::IR::Program& program{programs[index]};
        const size_t stage_index{index - 1};
        infos[stage_index] = &program.info;

        const Shader::Profile profile{MakeProfile(key, program)};
        const std::vector<u32> code{EmitSPIRV(profile, program, binding)};
        device.SaveShader(code);
        modules[stage_index] = BuildShader(device, code);
        if (device.HasDebuggingToolAttached()) {
            const std::string name{fmt::format("{:016x}{:016x}", key.unique_hashes[index][0],
                                               key.unique_hashes[index][1])};
            modules[stage_index].SetObjectNameEXT(name.c_str());
        }
    }
    Common::ThreadWorker* const thread_worker{build_in_parallel ? &workers : nullptr};
    return std::make_unique<GraphicsPipeline>(
        maxwell3d, gpu_memory, scheduler, buffer_cache, texture_cache, device, descriptor_pool,
        update_descriptor_queue, thread_worker, render_pass_cache, key.state, std::move(modules),
        infos);
}

std::unique_ptr<GraphicsPipeline> PipelineCache::CreateGraphicsPipeline() {
    main_pools.ReleaseContents();

    std::array<GraphicsEnvironment, Maxwell::MaxShaderProgram> graphics_envs;
    boost::container::static_vector<Shader::Environment*, Maxwell::MaxShaderProgram> envs;

    const GPUVAddr base_addr{maxwell3d.regs.code_address.CodeAddress()};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (graphics_key.unique_hashes[index] == u128{}) {
            continue;
        }
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};
        auto& env{graphics_envs[index]};
        const u32 start_address{maxwell3d.regs.shader_config[index].offset};
        env = GraphicsEnvironment{maxwell3d, gpu_memory, program, base_addr, start_address};
        env.SetCachedSize(shader_infos[index]->size_bytes);

        envs.push_back(&env);
    }
    auto pipeline{CreateGraphicsPipeline(main_pools, graphics_key, MakeSpan(envs), true)};
    if (pipeline_cache_filename.empty()) {
        return pipeline;
    }
    serialization_thread.QueueWork([this, key = graphics_key, envs = std::move(graphics_envs)] {
        boost::container::static_vector<const GenericEnvironment*, Maxwell::MaxShaderProgram>
            env_ptrs;
        for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
            if (key.unique_hashes[index] != u128{}) {
                env_ptrs.push_back(&envs[index]);
            }
        }
        SerializePipeline(key, env_ptrs, pipeline_cache_filename);
    });
    return pipeline;
}

std::unique_ptr<ComputePipeline> PipelineCache::CreateComputePipeline(
    const ComputePipelineCacheKey& key, const ShaderInfo* shader) {
    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    ComputeEnvironment env{kepler_compute, gpu_memory, program_base, qmd.program_start};
    env.SetCachedSize(shader->size_bytes);

    main_pools.ReleaseContents();
    auto pipeline{CreateComputePipeline(main_pools, key, env, true)};
    if (!pipeline_cache_filename.empty()) {
        serialization_thread.QueueWork([this, key, env = std::move(env)] {
            SerializePipeline(key, std::array<const GenericEnvironment*, 1>{&env},
                              pipeline_cache_filename);
        });
    }
    return pipeline;
}

std::unique_ptr<ComputePipeline> PipelineCache::CreateComputePipeline(
    ShaderPools& pools, const ComputePipelineCacheKey& key, Shader::Environment& env,
    bool build_in_parallel) {
    LOG_INFO(Render_Vulkan, "0x{:016x}", key.Hash());

    Shader::Maxwell::Flow::CFG cfg{env, pools.flow_block, env.StartAddress()};
    Shader::IR::Program program{TranslateProgram(pools.inst, pools.block, env, cfg)};
    u32 binding{0};
    const std::vector<u32> code{EmitSPIRV(base_profile, program, binding)};
    device.SaveShader(code);
    vk::ShaderModule spv_module{BuildShader(device, code)};
    if (device.HasDebuggingToolAttached()) {
        const auto name{fmt::format("{:016x}{:016x}", key.unique_hash[0], key.unique_hash[1])};
        spv_module.SetObjectNameEXT(name.c_str());
    }
    Common::ThreadWorker* const thread_worker{build_in_parallel ? &workers : nullptr};
    return std::make_unique<ComputePipeline>(device, descriptor_pool, update_descriptor_queue,
                                             thread_worker, program.info, std::move(spv_module));
}

static Shader::AttributeType CastAttributeType(const FixedPipelineState::VertexAttribute& attr) {
    if (attr.enabled == 0) {
        return Shader::AttributeType::Disabled;
    }
    switch (attr.Type()) {
    case Maxwell::VertexAttribute::Type::SignedNorm:
    case Maxwell::VertexAttribute::Type::UnsignedNorm:
    case Maxwell::VertexAttribute::Type::UnsignedScaled:
    case Maxwell::VertexAttribute::Type::SignedScaled:
    case Maxwell::VertexAttribute::Type::Float:
        return Shader::AttributeType::Float;
    case Maxwell::VertexAttribute::Type::SignedInt:
        return Shader::AttributeType::SignedInt;
    case Maxwell::VertexAttribute::Type::UnsignedInt:
        return Shader::AttributeType::UnsignedInt;
    }
    return Shader::AttributeType::Float;
}

static std::vector<Shader::TransformFeedbackVarying> MakeTransformFeedbackVaryings(
    const GraphicsPipelineCacheKey& key) {
    static constexpr std::array VECTORS{
        28,  // gl_Position
        32,  // Generic 0
        36,  // Generic 1
        40,  // Generic 2
        44,  // Generic 3
        48,  // Generic 4
        52,  // Generic 5
        56,  // Generic 6
        60,  // Generic 7
        64,  // Generic 8
        68,  // Generic 9
        72,  // Generic 10
        76,  // Generic 11
        80,  // Generic 12
        84,  // Generic 13
        88,  // Generic 14
        92,  // Generic 15
        96,  // Generic 16
        100, // Generic 17
        104, // Generic 18
        108, // Generic 19
        112, // Generic 20
        116, // Generic 21
        120, // Generic 22
        124, // Generic 23
        128, // Generic 24
        132, // Generic 25
        136, // Generic 26
        140, // Generic 27
        144, // Generic 28
        148, // Generic 29
        152, // Generic 30
        156, // Generic 31
        160, // gl_FrontColor
        164, // gl_FrontSecondaryColor
        160, // gl_BackColor
        164, // gl_BackSecondaryColor
        192, // gl_TexCoord[0]
        196, // gl_TexCoord[1]
        200, // gl_TexCoord[2]
        204, // gl_TexCoord[3]
        208, // gl_TexCoord[4]
        212, // gl_TexCoord[5]
        216, // gl_TexCoord[6]
        220, // gl_TexCoord[7]
    };
    std::vector<Shader::TransformFeedbackVarying> xfb(256);
    for (size_t buffer = 0; buffer < Maxwell::NumTransformFeedbackBuffers; ++buffer) {
        const auto& locations = key.state.xfb_state.varyings[buffer];
        const auto& layout = key.state.xfb_state.layouts[buffer];
        const u32 varying_count = layout.varying_count;
        u32 highest = 0;
        for (u32 offset = 0; offset < varying_count; ++offset) {
            const u32 base_offset = offset;
            const u8 location = locations[offset];

            Shader::TransformFeedbackVarying varying;
            varying.buffer = layout.stream;
            varying.stride = layout.stride;
            varying.offset = offset * 4;
            varying.components = 1;

            if (std::ranges::find(VECTORS, Common::AlignDown(location, 4)) != VECTORS.end()) {
                UNIMPLEMENTED_IF_MSG(location % 4 != 0, "Unaligned TFB");

                const u8 base_index = location / 4;
                while (offset + 1 < varying_count && base_index == locations[offset + 1] / 4) {
                    ++offset;
                    ++varying.components;
                }
            }
            xfb[location] = varying;
            highest = std::max(highest, (base_offset + varying.components) * 4);
        }
        UNIMPLEMENTED_IF(highest != layout.stride);
    }
    return xfb;
}

Shader::Profile PipelineCache::MakeProfile(const GraphicsPipelineCacheKey& key,
                                           const Shader::IR::Program& program) {
    Shader::Profile profile{base_profile};

    const Shader::Stage stage{program.stage};
    const bool has_geometry{key.unique_hashes[4] != u128{}};
    const bool gl_ndc{key.state.ndc_minus_one_to_one != 0};
    const float point_size{Common::BitCast<float>(key.state.point_size)};
    switch (stage) {
    case Shader::Stage::VertexB:
        if (!has_geometry) {
            if (key.state.topology == Maxwell::PrimitiveTopology::Points) {
                profile.fixed_state_point_size = point_size;
            }
            if (key.state.xfb_enabled != 0) {
                profile.xfb_varyings = MakeTransformFeedbackVaryings(key);
            }
            profile.convert_depth_mode = gl_ndc;
        }
        std::ranges::transform(key.state.attributes, profile.generic_input_types.begin(),
                               &CastAttributeType);
        break;
    case Shader::Stage::Geometry:
        if (program.output_topology == Shader::OutputTopology::PointList) {
            profile.fixed_state_point_size = point_size;
        }
        if (key.state.xfb_enabled != 0) {
            profile.xfb_varyings = MakeTransformFeedbackVaryings(key);
        }
        profile.convert_depth_mode = gl_ndc;
        break;
    case Shader::Stage::Fragment:
        profile.alpha_test_func = MaxwellToCompareFunction(
            key.state.UnpackComparisonOp(key.state.alpha_test_func.Value()));
        profile.alpha_test_reference = Common::BitCast<float>(key.state.alpha_test_ref);
        break;
    default:
        break;
    }
    switch (key.state.topology) {
    case Maxwell::PrimitiveTopology::Points:
        profile.input_topology = Shader::InputTopology::Points;
        break;
    case Maxwell::PrimitiveTopology::Lines:
    case Maxwell::PrimitiveTopology::LineLoop:
    case Maxwell::PrimitiveTopology::LineStrip:
        profile.input_topology = Shader::InputTopology::Lines;
        break;
    case Maxwell::PrimitiveTopology::Triangles:
    case Maxwell::PrimitiveTopology::TriangleStrip:
    case Maxwell::PrimitiveTopology::TriangleFan:
    case Maxwell::PrimitiveTopology::Quads:
    case Maxwell::PrimitiveTopology::QuadStrip:
    case Maxwell::PrimitiveTopology::Polygon:
    case Maxwell::PrimitiveTopology::Patches:
        profile.input_topology = Shader::InputTopology::Triangles;
        break;
    case Maxwell::PrimitiveTopology::LinesAdjacency:
    case Maxwell::PrimitiveTopology::LineStripAdjacency:
        profile.input_topology = Shader::InputTopology::LinesAdjacency;
        break;
    case Maxwell::PrimitiveTopology::TrianglesAdjacency:
    case Maxwell::PrimitiveTopology::TriangleStripAdjacency:
        profile.input_topology = Shader::InputTopology::TrianglesAdjacency;
        break;
    }
    profile.force_early_z = key.state.early_z != 0;
    return profile;
}

} // namespace Vulkan
