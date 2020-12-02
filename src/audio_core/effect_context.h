// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include "audio_core/common.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

namespace AudioCore {
enum class EffectType : u8 {
    Invalid = 0,
    BufferMixer = 1,
    Aux = 2,
    Delay = 3,
    Reverb = 4,
    I3dl2Reverb = 5,
    BiquadFilter = 6,
};

enum class UsageStatus : u8 {
    Invalid = 0,
    New = 1,
    Initialized = 2,
    Used = 3,
    Removed = 4,
};

enum class UsageState {
    Invalid = 0,
    Initialized = 1,
    Running = 2,
    Stopped = 3,
};

enum class ParameterStatus : u8 {
    Initialized = 0,
    Updating = 1,
    Updated = 2,
};

struct BufferMixerParams {
    std::array<s8, AudioCommon::MAX_MIX_BUFFERS> input{};
    std::array<s8, AudioCommon::MAX_MIX_BUFFERS> output{};
    std::array<float_le, AudioCommon::MAX_MIX_BUFFERS> volume{};
    s32_le count{};
};
static_assert(sizeof(BufferMixerParams) == 0x94, "BufferMixerParams is an invalid size");

struct AuxInfoDSP {
    u32_le read_offset{};
    u32_le write_offset{};
    u32_le remaining{};
    INSERT_PADDING_WORDS(13);
};
static_assert(sizeof(AuxInfoDSP) == 0x40, "AuxInfoDSP is an invalid size");

struct AuxInfo {
    std::array<s8, AudioCommon::MAX_MIX_BUFFERS> input_mix_buffers{};
    std::array<s8, AudioCommon::MAX_MIX_BUFFERS> output_mix_buffers{};
    u32_le count{};
    s32_le sample_rate{};
    s32_le sample_count{};
    s32_le mix_buffer_count{};
    u64_le send_buffer_info{};
    u64_le send_buffer_base{};

    u64_le return_buffer_info{};
    u64_le return_buffer_base{};
};
static_assert(sizeof(AuxInfo) == 0x60, "AuxInfo is an invalid size");

struct I3dl2ReverbParams {
    std::array<s8, AudioCommon::MAX_CHANNEL_COUNT> input{};
    std::array<s8, AudioCommon::MAX_CHANNEL_COUNT> output{};
    u16_le max_channels{};
    u16_le channel_count{};
    INSERT_PADDING_BYTES(1);
    u32_le sample_rate{};
    f32 room_hf{};
    f32 hf_reference{};
    f32 decay_time{};
    f32 hf_decay_ratio{};
    f32 room{};
    f32 reflection{};
    f32 reverb{};
    f32 diffusion{};
    f32 reflection_delay{};
    f32 reverb_delay{};
    f32 density{};
    f32 dry_gain{};
    ParameterStatus status{};
    INSERT_PADDING_BYTES(3);
};
static_assert(sizeof(I3dl2ReverbParams) == 0x4c, "I3dl2ReverbParams is an invalid size");

struct BiquadFilterParams {
    std::array<s8, AudioCommon::MAX_CHANNEL_COUNT> input{};
    std::array<s8, AudioCommon::MAX_CHANNEL_COUNT> output{};
    std::array<s16_le, 3> numerator;
    std::array<s16_le, 2> denominator;
    s8 channel_count{};
    ParameterStatus status{};
};
static_assert(sizeof(BiquadFilterParams) == 0x18, "BiquadFilterParams is an invalid size");

struct DelayParams {
    std::array<s8, AudioCommon::MAX_CHANNEL_COUNT> input{};
    std::array<s8, AudioCommon::MAX_CHANNEL_COUNT> output{};
    u16_le max_channels{};
    u16_le channels{};
    s32_le max_delay{};
    s32_le delay{};
    s32_le sample_rate{};
    s32_le gain{};
    s32_le feedback_gain{};
    s32_le out_gain{};
    s32_le dry_gain{};
    s32_le channel_spread{};
    s32_le low_pass{};
    ParameterStatus status{};
    INSERT_PADDING_BYTES(3);
};
static_assert(sizeof(DelayParams) == 0x38, "DelayParams is an invalid size");

struct ReverbParams {
    std::array<s8, AudioCommon::MAX_CHANNEL_COUNT> input{};
    std::array<s8, AudioCommon::MAX_CHANNEL_COUNT> output{};
    u16_le max_channels{};
    u16_le channels{};
    s32_le sample_rate{};
    s32_le mode0{};
    s32_le mode0_gain{};
    s32_le pre_delay{};
    s32_le mode1{};
    s32_le mode1_gain{};
    s32_le decay{};
    s32_le hf_decay_ratio{};
    s32_le coloration{};
    s32_le reverb_gain{};
    s32_le out_gain{};
    s32_le dry_gain{};
    ParameterStatus status{};
    INSERT_PADDING_BYTES(3);
};
static_assert(sizeof(ReverbParams) == 0x44, "ReverbParams is an invalid size");

class EffectInfo {
public:
    struct InParams {
        EffectType type{};
        u8 is_new{};
        u8 is_enabled{};
        INSERT_PADDING_BYTES(1);
        s32_le mix_id{};
        u64_le buffer_address{};
        u64_le buffer_size{};
        s32_le processing_order{};
        INSERT_PADDING_BYTES(4);
        union {
            std::array<u8, 0xa0> raw;
        };
    };
    static_assert(sizeof(InParams) == 0xc0, "InParams is an invalid size");

    struct OutParams {
        UsageStatus status{};
        INSERT_PADDING_BYTES(15);
    };
    static_assert(sizeof(OutParams) == 0x10, "OutParams is an invalid size");
};

struct AuxAddress {
    VAddr send_dsp_info{};
    VAddr send_buffer_base{};
    VAddr return_dsp_info{};
    VAddr return_buffer_base{};
};

class EffectBase {
public:
    explicit EffectBase(EffectType effect_type);
    virtual ~EffectBase();

    virtual void Update(EffectInfo::InParams& in_params) = 0;
    virtual void UpdateForCommandGeneration() = 0;
    [[nodiscard]] UsageState GetUsage() const;
    [[nodiscard]] EffectType GetType() const;
    [[nodiscard]] bool IsEnabled() const;
    [[nodiscard]] s32 GetMixID() const;
    [[nodiscard]] s32 GetProcessingOrder() const;

protected:
    UsageState usage{UsageState::Invalid};
    EffectType effect_type{};
    s32 mix_id{};
    s32 processing_order{};
    bool enabled = false;
};

template <typename T>
class EffectGeneric : public EffectBase {
public:
    explicit EffectGeneric(EffectType effect_type) : EffectBase(effect_type) {}

    T& GetParams() {
        return internal_params;
    }

    const I3dl2ReverbParams& GetParams() const {
        return internal_params;
    }

private:
    T internal_params{};
};

class EffectStubbed : public EffectBase {
public:
    explicit EffectStubbed();
    ~EffectStubbed() override;

    void Update(EffectInfo::InParams& in_params) override;
    void UpdateForCommandGeneration() override;
};

class EffectI3dl2Reverb : public EffectGeneric<I3dl2ReverbParams> {
public:
    explicit EffectI3dl2Reverb();
    ~EffectI3dl2Reverb() override;

    void Update(EffectInfo::InParams& in_params) override;
    void UpdateForCommandGeneration() override;

private:
    bool skipped = false;
};

class EffectBiquadFilter : public EffectGeneric<BiquadFilterParams> {
public:
    using StateType = std::array<std::array<s64, 2>, AudioCommon::MAX_CHANNEL_COUNT>;
    explicit EffectBiquadFilter();
    ~EffectBiquadFilter() override;

    void Update(EffectInfo::InParams& in_params) override;
    void UpdateForCommandGeneration() override;

    const StateType& GetState() const;
    StateType& GetState();

private:
    StateType state{};
};

class EffectAuxInfo : public EffectGeneric<AuxInfo> {
public:
    explicit EffectAuxInfo();
    ~EffectAuxInfo() override;

    void Update(EffectInfo::InParams& in_params) override;
    void UpdateForCommandGeneration() override;
    [[nodiscard]] VAddr GetSendInfo() const;
    [[nodiscard]] VAddr GetSendBuffer() const;
    [[nodiscard]] VAddr GetRecvInfo() const;
    [[nodiscard]] VAddr GetRecvBuffer() const;

private:
    VAddr send_info{};
    VAddr send_buffer{};
    VAddr recv_info{};
    VAddr recv_buffer{};
    bool skipped = false;
    AuxAddress addresses{};
};

class EffectDelay : public EffectGeneric<DelayParams> {
public:
    explicit EffectDelay();
    ~EffectDelay() override;

    void Update(EffectInfo::InParams& in_params) override;
    void UpdateForCommandGeneration() override;

private:
    bool skipped = false;
};

class EffectBufferMixer : public EffectGeneric<BufferMixerParams> {
public:
    explicit EffectBufferMixer();
    ~EffectBufferMixer() override;

    void Update(EffectInfo::InParams& in_params) override;
    void UpdateForCommandGeneration() override;
};

class EffectReverb : public EffectGeneric<ReverbParams> {
public:
    explicit EffectReverb();
    ~EffectReverb() override;

    void Update(EffectInfo::InParams& in_params) override;
    void UpdateForCommandGeneration() override;

private:
    bool skipped = false;
};

class EffectContext {
public:
    explicit EffectContext(std::size_t effect_count);
    ~EffectContext();

    [[nodiscard]] std::size_t GetCount() const;
    [[nodiscard]] EffectBase* GetInfo(std::size_t i);
    [[nodiscard]] EffectBase* RetargetEffect(std::size_t i, EffectType effect);
    [[nodiscard]] const EffectBase* GetInfo(std::size_t i) const;

private:
    std::size_t effect_count{};
    std::vector<std::unique_ptr<EffectBase>> effects;
};
} // namespace AudioCore
