// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "audio_core/effect_context.h"

namespace AudioCore {
namespace {
bool ValidChannelCountForEffect(s32 channel_count) {
    return channel_count == 1 || channel_count == 2 || channel_count == 4 || channel_count == 6;
}
} // namespace

EffectContext::EffectContext(std::size_t effect_count) : effect_count(effect_count) {
    effects.reserve(effect_count);
    std::generate_n(std::back_inserter(effects), effect_count,
                    [] { return std::make_unique<EffectStubbed>(); });
}
EffectContext::~EffectContext() = default;

std::size_t EffectContext::GetCount() const {
    return effect_count;
}

EffectBase* EffectContext::GetInfo(std::size_t i) {
    return effects.at(i).get();
}

EffectBase* EffectContext::RetargetEffect(std::size_t i, EffectType effect) {
    switch (effect) {
    case EffectType::Invalid:
        effects[i] = std::make_unique<EffectStubbed>();
        break;
    case EffectType::BufferMixer:
        effects[i] = std::make_unique<EffectBufferMixer>();
        break;
    case EffectType::Aux:
        effects[i] = std::make_unique<EffectAuxInfo>();
        break;
    case EffectType::Delay:
        effects[i] = std::make_unique<EffectDelay>();
        break;
    case EffectType::Reverb:
        effects[i] = std::make_unique<EffectReverb>();
        break;
    case EffectType::I3dl2Reverb:
        effects[i] = std::make_unique<EffectI3dl2Reverb>();
        break;
    case EffectType::BiquadFilter:
        effects[i] = std::make_unique<EffectBiquadFilter>();
        break;
    default:
        UNREACHABLE_MSG("Unimplemented effect {}", effect);
        effects[i] = std::make_unique<EffectStubbed>();
    }
    return GetInfo(i);
}

const EffectBase* EffectContext::GetInfo(std::size_t i) const {
    return effects.at(i).get();
}

EffectStubbed::EffectStubbed() : EffectBase::EffectBase(EffectType::Invalid) {}
EffectStubbed::~EffectStubbed() = default;

void EffectStubbed::Update(EffectInfo::InParams& in_params) {}
void EffectStubbed::UpdateForCommandGeneration() {}

EffectBase::EffectBase(EffectType effect_type) : effect_type(effect_type) {}
EffectBase::~EffectBase() = default;

UsageState EffectBase::GetUsage() const {
    return usage;
}

EffectType EffectBase::GetType() const {
    return effect_type;
}

bool EffectBase::IsEnabled() const {
    return enabled;
}

s32 EffectBase::GetMixID() const {
    return mix_id;
}

s32 EffectBase::GetProcessingOrder() const {
    return processing_order;
}

EffectI3dl2Reverb::EffectI3dl2Reverb() : EffectGeneric::EffectGeneric(EffectType::I3dl2Reverb) {}
EffectI3dl2Reverb::~EffectI3dl2Reverb() = default;

void EffectI3dl2Reverb::Update(EffectInfo::InParams& in_params) {
    auto& internal_params = GetParams();
    const auto* reverb_params = reinterpret_cast<I3dl2ReverbParams*>(in_params.raw.data());
    if (!ValidChannelCountForEffect(reverb_params->max_channels)) {
        UNREACHABLE_MSG("Invalid reverb max channel count {}", reverb_params->max_channels);
        return;
    }

    const auto last_status = internal_params.status;
    mix_id = in_params.mix_id;
    processing_order = in_params.processing_order;
    internal_params = *reverb_params;
    if (!ValidChannelCountForEffect(reverb_params->channel_count)) {
        internal_params.channel_count = internal_params.max_channels;
    }
    enabled = in_params.is_enabled;
    if (last_status != ParameterStatus::Updated) {
        internal_params.status = last_status;
    }

    if (in_params.is_new || skipped) {
        usage = UsageState::Initialized;
        internal_params.status = ParameterStatus::Initialized;
        skipped = in_params.buffer_address == 0 || in_params.buffer_size == 0;
    }
}

void EffectI3dl2Reverb::UpdateForCommandGeneration() {
    if (enabled) {
        usage = UsageState::Running;
    } else {
        usage = UsageState::Stopped;
    }
    GetParams().status = ParameterStatus::Updated;
}

EffectBiquadFilter::EffectBiquadFilter() : EffectGeneric::EffectGeneric(EffectType::BiquadFilter) {}
EffectBiquadFilter::~EffectBiquadFilter() = default;

void EffectBiquadFilter::Update(EffectInfo::InParams& in_params) {
    auto& internal_params = GetParams();
    const auto* biquad_params = reinterpret_cast<BiquadFilterParams*>(in_params.raw.data());
    mix_id = in_params.mix_id;
    processing_order = in_params.processing_order;
    internal_params = *biquad_params;
    enabled = in_params.is_enabled;
}

void EffectBiquadFilter::UpdateForCommandGeneration() {
    if (enabled) {
        usage = UsageState::Running;
    } else {
        usage = UsageState::Stopped;
    }
    GetParams().status = ParameterStatus::Updated;
}

const EffectBiquadFilter::StateType& EffectBiquadFilter::GetState() const {
    return state;
}

EffectBiquadFilter::StateType& EffectBiquadFilter::GetState() {
    return state;
}

EffectAuxInfo::EffectAuxInfo() : EffectGeneric::EffectGeneric(EffectType::Aux) {}
EffectAuxInfo::~EffectAuxInfo() = default;

void EffectAuxInfo::Update(EffectInfo::InParams& in_params) {
    const auto* aux_params = reinterpret_cast<AuxInfo*>(in_params.raw.data());
    mix_id = in_params.mix_id;
    processing_order = in_params.processing_order;
    GetParams() = *aux_params;
    enabled = in_params.is_enabled;

    if (in_params.is_new || skipped) {
        skipped = aux_params->send_buffer_info == 0 || aux_params->return_buffer_info == 0;
        if (skipped) {
            return;
        }

        // There's two AuxInfos which are an identical size, the first one is managed by the cpu,
        // the second is managed by the dsp. All we care about is managing the DSP one
        send_info = aux_params->send_buffer_info + sizeof(AuxInfoDSP);
        send_buffer = aux_params->send_buffer_info + (sizeof(AuxInfoDSP) * 2);

        recv_info = aux_params->return_buffer_info + sizeof(AuxInfoDSP);
        recv_buffer = aux_params->return_buffer_info + (sizeof(AuxInfoDSP) * 2);
    }
}

void EffectAuxInfo::UpdateForCommandGeneration() {
    if (enabled) {
        usage = UsageState::Running;
    } else {
        usage = UsageState::Stopped;
    }
}

VAddr EffectAuxInfo::GetSendInfo() const {
    return send_info;
}

VAddr EffectAuxInfo::GetSendBuffer() const {
    return send_buffer;
}

VAddr EffectAuxInfo::GetRecvInfo() const {
    return recv_info;
}

VAddr EffectAuxInfo::GetRecvBuffer() const {
    return recv_buffer;
}

EffectDelay::EffectDelay() : EffectGeneric::EffectGeneric(EffectType::Delay) {}
EffectDelay::~EffectDelay() = default;

void EffectDelay::Update(EffectInfo::InParams& in_params) {
    const auto* delay_params = reinterpret_cast<DelayParams*>(in_params.raw.data());
    auto& internal_params = GetParams();
    if (!ValidChannelCountForEffect(delay_params->max_channels)) {
        return;
    }

    const auto last_status = internal_params.status;
    mix_id = in_params.mix_id;
    processing_order = in_params.processing_order;
    internal_params = *delay_params;
    if (!ValidChannelCountForEffect(delay_params->channels)) {
        internal_params.channels = internal_params.max_channels;
    }
    enabled = in_params.is_enabled;

    if (last_status != ParameterStatus::Updated) {
        internal_params.status = last_status;
    }

    if (in_params.is_new || skipped) {
        usage = UsageState::Initialized;
        internal_params.status = ParameterStatus::Initialized;
        skipped = in_params.buffer_address == 0 || in_params.buffer_size == 0;
    }
}

void EffectDelay::UpdateForCommandGeneration() {
    if (enabled) {
        usage = UsageState::Running;
    } else {
        usage = UsageState::Stopped;
    }
    GetParams().status = ParameterStatus::Updated;
}

EffectBufferMixer::EffectBufferMixer() : EffectGeneric::EffectGeneric(EffectType::BufferMixer) {}
EffectBufferMixer::~EffectBufferMixer() = default;

void EffectBufferMixer::Update(EffectInfo::InParams& in_params) {
    mix_id = in_params.mix_id;
    processing_order = in_params.processing_order;
    GetParams() = *reinterpret_cast<BufferMixerParams*>(in_params.raw.data());
    enabled = in_params.is_enabled;
}

void EffectBufferMixer::UpdateForCommandGeneration() {
    if (enabled) {
        usage = UsageState::Running;
    } else {
        usage = UsageState::Stopped;
    }
}

EffectReverb::EffectReverb() : EffectGeneric::EffectGeneric(EffectType::Reverb) {}
EffectReverb::~EffectReverb() = default;

void EffectReverb::Update(EffectInfo::InParams& in_params) {
    const auto* reverb_params = reinterpret_cast<ReverbParams*>(in_params.raw.data());
    auto& internal_params = GetParams();
    if (!ValidChannelCountForEffect(reverb_params->max_channels)) {
        return;
    }

    const auto last_status = internal_params.status;
    mix_id = in_params.mix_id;
    processing_order = in_params.processing_order;
    internal_params = *reverb_params;
    if (!ValidChannelCountForEffect(reverb_params->channels)) {
        internal_params.channels = internal_params.max_channels;
    }
    enabled = in_params.is_enabled;

    if (last_status != ParameterStatus::Updated) {
        internal_params.status = last_status;
    }

    if (in_params.is_new || skipped) {
        usage = UsageState::Initialized;
        internal_params.status = ParameterStatus::Initialized;
        skipped = in_params.buffer_address == 0 || in_params.buffer_size == 0;
    }
}

void EffectReverb::UpdateForCommandGeneration() {
    if (enabled) {
        usage = UsageState::Running;
    } else {
        usage = UsageState::Stopped;
    }
    GetParams().status = ParameterStatus::Updated;
}

} // namespace AudioCore
