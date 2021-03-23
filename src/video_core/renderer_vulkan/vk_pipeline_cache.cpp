// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <memory>
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

class GenericEnvironment : public Shader::Environment {
public:
    explicit GenericEnvironment() = default;
    explicit GenericEnvironment(Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_,
                                u32 start_address_)
        : gpu_memory{&gpu_memory_}, program_base{program_base_} {
        start_address = start_address_;
    }

    ~GenericEnvironment() override = default;

    std::optional<u128> Analyze() {
        const std::optional<u64> size{TryFindSize(start_address)};
        if (!size) {
            return std::nullopt;
        }
        cached_lowest = start_address;
        cached_highest = start_address + static_cast<u32>(*size);
        return Common::CityHash128(reinterpret_cast<const char*>(code.data()), code.size());
    }

    [[nodiscard]] size_t CachedSize() const noexcept {
        return cached_highest - cached_lowest + INST_SIZE;
    }

    [[nodiscard]] size_t ReadSize() const noexcept {
        return read_highest - read_lowest + INST_SIZE;
    }

    [[nodiscard]] bool CanBeSerialized() const noexcept {
        return has_unbound_instructions;
    }

    [[nodiscard]] u128 CalculateHash() const {
        const size_t size{ReadSize()};
        const auto data{std::make_unique<char[]>(size)};
        gpu_memory->ReadBlock(program_base + read_lowest, data.get(), size);
        return Common::CityHash128(data.get(), size);
    }

    u64 ReadInstruction(u32 address) final {
        read_lowest = std::min(read_lowest, address);
        read_highest = std::max(read_highest, address);

        if (address >= cached_lowest && address < cached_highest) {
            return code[address / INST_SIZE];
        }
        has_unbound_instructions = true;
        return gpu_memory->Read<u64>(program_base + address);
    }

    void Serialize(std::ofstream& file) const {
        const u64 code_size{static_cast<u64>(ReadSize())};
        const auto data{std::make_unique<char[]>(code_size)};
        gpu_memory->ReadBlock(program_base + read_lowest, data.get(), code_size);

        const u32 texture_bound{TextureBoundBuffer()};

        file.write(reinterpret_cast<const char*>(&code_size), sizeof(code_size))
            .write(reinterpret_cast<const char*>(&texture_bound), sizeof(texture_bound))
            .write(reinterpret_cast<const char*>(&start_address), sizeof(start_address))
            .write(reinterpret_cast<const char*>(&read_lowest), sizeof(read_lowest))
            .write(reinterpret_cast<const char*>(&read_highest), sizeof(read_highest))
            .write(reinterpret_cast<const char*>(&stage), sizeof(stage))
            .write(data.get(), code_size);
        if (stage == Shader::Stage::Compute) {
            const std::array<u32, 3> workgroup_size{WorkgroupSize()};
            file.write(reinterpret_cast<const char*>(&workgroup_size), sizeof(workgroup_size));
        } else {
            file.write(reinterpret_cast<const char*>(&sph), sizeof(sph));
        }
    }

protected:
    static constexpr size_t INST_SIZE = sizeof(u64);

    std::optional<u64> TryFindSize(GPUVAddr guest_addr) {
        constexpr size_t BLOCK_SIZE = 0x1000;
        constexpr size_t MAXIMUM_SIZE = 0x100000;

        constexpr u64 SELF_BRANCH_A = 0xE2400FFFFF87000FULL;
        constexpr u64 SELF_BRANCH_B = 0xE2400FFFFF07000FULL;

        size_t offset = 0;
        size_t size = BLOCK_SIZE;
        while (size <= MAXIMUM_SIZE) {
            code.resize(size / INST_SIZE);
            u64* const data = code.data() + offset / INST_SIZE;
            gpu_memory->ReadBlock(guest_addr, data, BLOCK_SIZE);
            for (size_t i = 0; i < BLOCK_SIZE; i += INST_SIZE) {
                const u64 inst = data[i / INST_SIZE];
                if (inst == SELF_BRANCH_A || inst == SELF_BRANCH_B) {
                    return offset + i;
                }
            }
            guest_addr += BLOCK_SIZE;
            size += BLOCK_SIZE;
            offset += BLOCK_SIZE;
        }
        return std::nullopt;
    }

    Tegra::MemoryManager* gpu_memory{};
    GPUVAddr program_base{};

    std::vector<u64> code;

    u32 read_lowest = std::numeric_limits<u32>::max();
    u32 read_highest = 0;

    u32 cached_lowest = std::numeric_limits<u32>::max();
    u32 cached_highest = 0;

    bool has_unbound_instructions = false;
};

namespace {
using Shader::Backend::SPIRV::EmitSPIRV;
using Shader::Maxwell::TranslateProgram;

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
            break;
        case Maxwell::ShaderProgram::VertexB:
            stage = Shader::Stage::VertexB;
            break;
        case Maxwell::ShaderProgram::TesselationControl:
            stage = Shader::Stage::TessellationControl;
            break;
        case Maxwell::ShaderProgram::TesselationEval:
            stage = Shader::Stage::TessellationEval;
            break;
        case Maxwell::ShaderProgram::Geometry:
            stage = Shader::Stage::Geometry;
            break;
        case Maxwell::ShaderProgram::Fragment:
            stage = Shader::Stage::Fragment;
            break;
        default:
            UNREACHABLE_MSG("Invalid program={}", program);
        }
    }

    ~GraphicsEnvironment() override = default;

    u32 TextureBoundBuffer() const override {
        return maxwell3d->regs.tex_cb_index;
    }

    std::array<u32, 3> WorkgroupSize() const override {
        throw Shader::LogicError("Requesting workgroup size in a graphics stage");
    }

private:
    Tegra::Engines::Maxwell3D* maxwell3d{};
};

class ComputeEnvironment final : public GenericEnvironment {
public:
    explicit ComputeEnvironment() = default;
    explicit ComputeEnvironment(Tegra::Engines::KeplerCompute& kepler_compute_,
                                Tegra::MemoryManager& gpu_memory_, GPUVAddr program_base_,
                                u32 start_address_)
        : GenericEnvironment{gpu_memory_, program_base_, start_address_}, kepler_compute{
                                                                              &kepler_compute_} {
        stage = Shader::Stage::Compute;
    }

    ~ComputeEnvironment() override = default;

    u32 TextureBoundBuffer() const override {
        return kepler_compute->regs.tex_cb_index;
    }

    std::array<u32, 3> WorkgroupSize() const override {
        const auto& qmd{kepler_compute->launch_description};
        return {qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z};
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
        Common::FS::OpenFStream(file, filename, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            LOG_ERROR(Common_Filesystem, "Failed to open pipeline cache file {}", filename);
            return;
        }
        if (file.tellp() == 0) {
            // Write header...
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
        file.read(reinterpret_cast<char*>(&code_size), sizeof(code_size))
            .read(reinterpret_cast<char*>(&texture_bound), sizeof(texture_bound))
            .read(reinterpret_cast<char*>(&start_address), sizeof(start_address))
            .read(reinterpret_cast<char*>(&read_lowest), sizeof(read_lowest))
            .read(reinterpret_cast<char*>(&read_highest), sizeof(read_highest))
            .read(reinterpret_cast<char*>(&stage), sizeof(stage));
        code = std::make_unique<u64[]>(Common::DivCeil(code_size, sizeof(u64)));
        file.read(reinterpret_cast<char*>(code.get()), code_size);
        if (stage == Shader::Stage::Compute) {
            file.read(reinterpret_cast<char*>(&workgroup_size), sizeof(workgroup_size));
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

    u32 TextureBoundBuffer() const override {
        return texture_bound;
    }

    std::array<u32, 3> WorkgroupSize() const override {
        return workgroup_size;
    }

private:
    std::unique_ptr<u64[]> code;
    std::array<u32, 3> workgroup_size{};
    u32 texture_bound{};
    u32 read_lowest{};
    u32 read_highest{};
};
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

    Common::ThreadWorker worker(11, "PipelineBuilder");
    std::mutex cache_mutex;
    struct {
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
    // Read header...

    while (file.tellg() != end) {
        if (stop_loading) {
            return;
        }
        u32 num_envs{};
        file.read(reinterpret_cast<char*>(&num_envs), sizeof(num_envs));
        auto envs{std::make_shared<std::vector<FileEnvironment>>(num_envs)};
        for (FileEnvironment& env : *envs) {
            env.Deserialize(file);
        }
        if (envs->front().ShaderStage() == Shader::Stage::Compute) {
            ComputePipelineCacheKey key;
            file.read(reinterpret_cast<char*>(&key), sizeof(key));

            worker.QueueWork([this, key, envs, &cache_mutex, &state, &callback] {
                ShaderPools pools;
                ComputePipeline pipeline{CreateComputePipeline(pools, key, envs->front())};

                std::lock_guard lock{cache_mutex};
                compute_cache.emplace(key, std::move(pipeline));
                if (state.has_loaded) {
                    callback(VideoCore::LoadCallbackStage::Build, ++state.built, state.total);
                }
            });
        } else {
            GraphicsPipelineCacheKey key;
            file.read(reinterpret_cast<char*>(&key), sizeof(key));

            worker.QueueWork([this, key, envs, &cache_mutex, &state, &callback] {
                ShaderPools pools;
                boost::container::static_vector<Shader::Environment*, 5> env_ptrs;
                for (auto& env : *envs) {
                    env_ptrs.push_back(&env);
                }
                GraphicsPipeline pipeline{CreateGraphicsPipeline(pools, key, MakeSpan(env_ptrs))};

                std::lock_guard lock{cache_mutex};
                graphics_cache.emplace(key, std::move(pipeline));
                if (state.has_loaded) {
                    callback(VideoCore::LoadCallbackStage::Build, ++state.built, state.total);
                }
            });
        }
        ++state.total;
    }
    {
        std::lock_guard lock{cache_mutex};
        callback(VideoCore::LoadCallbackStage::Build, 0, state.total);
        state.has_loaded = true;
    }
    worker.WaitForRequests();
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
      buffer_cache{buffer_cache_}, texture_cache{texture_cache_} {
    const auto& float_control{device.FloatControlProperties()};
    const VkDriverIdKHR driver_id{device.GetDriverID()};
    profile = Shader::Profile{
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
        .has_broken_spirv_clamp = driver_id == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS_KHR,
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
        return &pipeline;
    }
    pipeline = CreateGraphicsPipeline();
    return &pipeline;
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
        .unique_hash = shader->unique_hash,
        .shared_memory_size = qmd.shared_alloc,
        .workgroup_size{qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z},
    };
    const auto [pair, is_new]{compute_cache.try_emplace(key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return &pipeline;
    }
    pipeline = CreateComputePipeline(key, shader);
    return &pipeline;
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

GraphicsPipeline PipelineCache::CreateGraphicsPipeline(ShaderPools& pools,
                                                       const GraphicsPipelineCacheKey& key,
                                                       std::span<Shader::Environment* const> envs) {
    LOG_INFO(Render_Vulkan, "0x{:016x}", key.Hash());
    size_t env_index{0};
    std::array<Shader::IR::Program, Maxwell::MaxShaderProgram> programs;
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == u128{}) {
            continue;
        }
        Shader::Environment& env{*envs[env_index]};
        ++env_index;

        const u32 cfg_offset{env.StartAddress() + sizeof(Shader::ProgramHeader)};
        Shader::Maxwell::Flow::CFG cfg(env, pools.flow_block, cfg_offset);
        programs[index] = TranslateProgram(pools.inst, pools.block, env, cfg);
    }
    std::array<const Shader::Info*, Maxwell::MaxShaderStage> infos{};
    std::array<vk::ShaderModule, Maxwell::MaxShaderStage> modules;

    u32 binding{0};
    env_index = 0;
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == u128{}) {
            continue;
        }
        UNIMPLEMENTED_IF(index == 0);

        Shader::IR::Program& program{programs[index]};
        const size_t stage_index{index - 1};
        infos[stage_index] = &program.info;

        Shader::Environment& env{*envs[env_index]};
        ++env_index;

        const std::vector<u32> code{EmitSPIRV(profile, env, program, binding)};
        modules[stage_index] = BuildShader(device, code);
    }
    return GraphicsPipeline(maxwell3d, gpu_memory, scheduler, buffer_cache, texture_cache, device,
                            descriptor_pool, update_descriptor_queue, render_pass_cache, key.state,
                            std::move(modules), infos);
}

GraphicsPipeline PipelineCache::CreateGraphicsPipeline() {
    main_pools.ReleaseContents();

    std::array<GraphicsEnvironment, Maxwell::MaxShaderProgram> graphics_envs;
    boost::container::static_vector<GenericEnvironment*, Maxwell::MaxShaderProgram> generic_envs;
    boost::container::static_vector<Shader::Environment*, Maxwell::MaxShaderProgram> envs;

    const GPUVAddr base_addr{maxwell3d.regs.code_address.CodeAddress()};
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (graphics_key.unique_hashes[index] == u128{}) {
            continue;
        }
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};
        GraphicsEnvironment& env{graphics_envs[index]};
        const u32 start_address{maxwell3d.regs.shader_config[index].offset};
        env = GraphicsEnvironment{maxwell3d, gpu_memory, program, base_addr, start_address};
        generic_envs.push_back(&env);
        envs.push_back(&env);
    }
    GraphicsPipeline pipeline{CreateGraphicsPipeline(main_pools, graphics_key, MakeSpan(envs))};
    if (!pipeline_cache_filename.empty()) {
        SerializePipeline(graphics_key, generic_envs, pipeline_cache_filename);
    }
    return pipeline;
}

ComputePipeline PipelineCache::CreateComputePipeline(const ComputePipelineCacheKey& key,
                                                     const ShaderInfo* shader) {
    const GPUVAddr program_base{kepler_compute.regs.code_loc.Address()};
    const auto& qmd{kepler_compute.launch_description};
    ComputeEnvironment env{kepler_compute, gpu_memory, program_base, qmd.program_start};
    main_pools.ReleaseContents();
    ComputePipeline pipeline{CreateComputePipeline(main_pools, key, env)};
    if (!pipeline_cache_filename.empty()) {
        SerializePipeline(key, std::array<const GenericEnvironment*, 1>{&env},
                          pipeline_cache_filename);
    }
    return pipeline;
}

ComputePipeline PipelineCache::CreateComputePipeline(ShaderPools& pools,
                                                     const ComputePipelineCacheKey& key,
                                                     Shader::Environment& env) const {
    LOG_INFO(Render_Vulkan, "0x{:016x}", key.Hash());

    Shader::Maxwell::Flow::CFG cfg{env, pools.flow_block, env.StartAddress()};
    Shader::IR::Program program{TranslateProgram(pools.inst, pools.block, env, cfg)};
    u32 binding{0};
    std::vector<u32> code{EmitSPIRV(profile, env, program, binding)};
    return ComputePipeline{device, descriptor_pool, update_descriptor_queue, program.info,
                           BuildShader(device, code)};
}

} // namespace Vulkan
