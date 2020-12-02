// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/algorithm/interpolate.h"
#include "audio_core/command_generator.h"
#include "audio_core/effect_context.h"
#include "audio_core/mix_context.h"
#include "audio_core/voice_context.h"
#include "core/memory.h"

namespace AudioCore {
namespace {
constexpr std::size_t MIX_BUFFER_SIZE = 0x3f00;
constexpr std::size_t SCALED_MIX_BUFFER_SIZE = MIX_BUFFER_SIZE << 15ULL;

template <std::size_t N>
void ApplyMix(s32* output, const s32* input, s32 gain, s32 sample_count) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(sample_count); i += N) {
        for (std::size_t j = 0; j < N; j++) {
            output[i + j] +=
                static_cast<s32>((static_cast<s64>(input[i + j]) * gain + 0x4000) >> 15);
        }
    }
}

s32 ApplyMixRamp(s32* output, const s32* input, float gain, float delta, s32 sample_count) {
    s32 x = 0;
    for (s32 i = 0; i < sample_count; i++) {
        x = static_cast<s32>(static_cast<float>(input[i]) * gain);
        output[i] += x;
        gain += delta;
    }
    return x;
}

void ApplyGain(s32* output, const s32* input, s32 gain, s32 delta, s32 sample_count) {
    for (s32 i = 0; i < sample_count; i++) {
        output[i] = static_cast<s32>((static_cast<s64>(input[i]) * gain + 0x4000) >> 15);
        gain += delta;
    }
}

void ApplyGainWithoutDelta(s32* output, const s32* input, s32 gain, s32 sample_count) {
    for (s32 i = 0; i < sample_count; i++) {
        output[i] = static_cast<s32>((static_cast<s64>(input[i]) * gain + 0x4000) >> 15);
    }
}

s32 ApplyMixDepop(s32* output, s32 first_sample, s32 delta, s32 sample_count) {
    const bool positive = first_sample > 0;
    auto final_sample = std::abs(first_sample);
    for (s32 i = 0; i < sample_count; i++) {
        final_sample = static_cast<s32>((static_cast<s64>(final_sample) * delta) >> 15);
        if (positive) {
            output[i] += final_sample;
        } else {
            output[i] -= final_sample;
        }
    }
    if (positive) {
        return final_sample;
    } else {
        return -final_sample;
    }
}

} // namespace

CommandGenerator::CommandGenerator(AudioCommon::AudioRendererParameter& worker_params,
                                   VoiceContext& voice_context, MixContext& mix_context,
                                   SplitterContext& splitter_context, EffectContext& effect_context,
                                   Core::Memory::Memory& memory)
    : worker_params(worker_params), voice_context(voice_context), mix_context(mix_context),
      splitter_context(splitter_context), effect_context(effect_context), memory(memory),
      mix_buffer((worker_params.mix_buffer_count + AudioCommon::MAX_CHANNEL_COUNT) *
                 worker_params.sample_count),
      sample_buffer(MIX_BUFFER_SIZE),
      depop_buffer((worker_params.mix_buffer_count + AudioCommon::MAX_CHANNEL_COUNT) *
                   worker_params.sample_count) {}
CommandGenerator::~CommandGenerator() = default;

void CommandGenerator::ClearMixBuffers() {
    std::fill(mix_buffer.begin(), mix_buffer.end(), 0);
    std::fill(sample_buffer.begin(), sample_buffer.end(), 0);
    // std::fill(depop_buffer.begin(), depop_buffer.end(), 0);
}

void CommandGenerator::GenerateVoiceCommands() {
    if (dumping_frame) {
        LOG_DEBUG(Audio, "(DSP_TRACE) GenerateVoiceCommands");
    }
    // Grab all our voices
    const auto voice_count = voice_context.GetVoiceCount();
    for (std::size_t i = 0; i < voice_count; i++) {
        auto& voice_info = voice_context.GetSortedInfo(i);
        // Update voices and check if we should queue them
        if (voice_info.ShouldSkip() || !voice_info.UpdateForCommandGeneration(voice_context)) {
            continue;
        }

        // Queue our voice
        GenerateVoiceCommand(voice_info);
    }
    // Update our splitters
    splitter_context.UpdateInternalState();
}

void CommandGenerator::GenerateVoiceCommand(ServerVoiceInfo& voice_info) {
    auto& in_params = voice_info.GetInParams();
    const auto channel_count = in_params.channel_count;

    for (s32 channel = 0; channel < channel_count; channel++) {
        const auto resource_id = in_params.voice_channel_resource_id[channel];
        auto& dsp_state = voice_context.GetDspSharedState(resource_id);
        auto& channel_resource = voice_context.GetChannelResource(resource_id);

        // Decode our samples for our channel
        GenerateDataSourceCommand(voice_info, dsp_state, channel);

        if (in_params.should_depop) {
            in_params.last_volume = 0.0f;
        } else if (in_params.splitter_info_id != AudioCommon::NO_SPLITTER ||
                   in_params.mix_id != AudioCommon::NO_MIX) {
            // Apply a biquad filter if needed
            GenerateBiquadFilterCommandForVoice(voice_info, dsp_state,
                                                worker_params.mix_buffer_count, channel);
            // Base voice volume ramping
            GenerateVolumeRampCommand(in_params.last_volume, in_params.volume, channel,
                                      in_params.node_id);
            in_params.last_volume = in_params.volume;

            if (in_params.mix_id != AudioCommon::NO_MIX) {
                // If we're using a mix id
                auto& mix_info = mix_context.GetInfo(in_params.mix_id);
                const auto& dest_mix_params = mix_info.GetInParams();

                // Voice Mixing
                GenerateVoiceMixCommand(
                    channel_resource.GetCurrentMixVolume(), channel_resource.GetLastMixVolume(),
                    dsp_state, dest_mix_params.buffer_offset, dest_mix_params.buffer_count,
                    worker_params.mix_buffer_count + channel, in_params.node_id);

                // Update last mix volumes
                channel_resource.UpdateLastMixVolumes();
            } else if (in_params.splitter_info_id != AudioCommon::NO_SPLITTER) {
                s32 base = channel;
                while (auto* destination_data =
                           GetDestinationData(in_params.splitter_info_id, base)) {
                    base += channel_count;

                    if (!destination_data->IsConfigured()) {
                        continue;
                    }
                    if (destination_data->GetMixId() >= static_cast<int>(mix_context.GetCount())) {
                        continue;
                    }

                    const auto& mix_info = mix_context.GetInfo(destination_data->GetMixId());
                    const auto& dest_mix_params = mix_info.GetInParams();
                    GenerateVoiceMixCommand(
                        destination_data->CurrentMixVolumes(), destination_data->LastMixVolumes(),
                        dsp_state, dest_mix_params.buffer_offset, dest_mix_params.buffer_count,
                        worker_params.mix_buffer_count + channel, in_params.node_id);
                    destination_data->MarkDirty();
                }
            }
            // Update biquad filter enabled states
            for (std::size_t i = 0; i < AudioCommon::MAX_BIQUAD_FILTERS; i++) {
                in_params.was_biquad_filter_enabled[i] = in_params.biquad_filter[i].enabled;
            }
        }
    }
}

void CommandGenerator::GenerateSubMixCommands() {
    const auto mix_count = mix_context.GetCount();
    for (std::size_t i = 0; i < mix_count; i++) {
        auto& mix_info = mix_context.GetSortedInfo(i);
        const auto& in_params = mix_info.GetInParams();
        if (!in_params.in_use || in_params.mix_id == AudioCommon::FINAL_MIX) {
            continue;
        }
        GenerateSubMixCommand(mix_info);
    }
}

void CommandGenerator::GenerateFinalMixCommands() {
    GenerateFinalMixCommand();
}

void CommandGenerator::PreCommand() {
    if (!dumping_frame) {
        return;
    }
    for (std::size_t i = 0; i < splitter_context.GetInfoCount(); i++) {
        const auto& base = splitter_context.GetInfo(i);
        std::string graph = fmt::format("b[{}]", i);
        const auto* head = base.GetHead();
        while (head != nullptr) {
            graph += fmt::format("->{}", head->GetMixId());
            head = head->GetNextDestination();
        }
        LOG_DEBUG(Audio, "(DSP_TRACE) SplitterGraph splitter_info={}, {}", i, graph);
    }
}

void CommandGenerator::PostCommand() {
    if (!dumping_frame) {
        return;
    }
    dumping_frame = false;
}

void CommandGenerator::GenerateDataSourceCommand(ServerVoiceInfo& voice_info, VoiceState& dsp_state,
                                                 s32 channel) {
    const auto& in_params = voice_info.GetInParams();
    const auto depop = in_params.should_depop;

    if (depop) {
        if (in_params.mix_id != AudioCommon::NO_MIX) {
            auto& mix_info = mix_context.GetInfo(in_params.mix_id);
            const auto& mix_in = mix_info.GetInParams();
            GenerateDepopPrepareCommand(dsp_state, mix_in.buffer_count, mix_in.buffer_offset);
        } else if (in_params.splitter_info_id != AudioCommon::NO_SPLITTER) {
            s32 index{};
            while (const auto* destination =
                       GetDestinationData(in_params.splitter_info_id, index++)) {
                if (!destination->IsConfigured()) {
                    continue;
                }
                auto& mix_info = mix_context.GetInfo(destination->GetMixId());
                const auto& mix_in = mix_info.GetInParams();
                GenerateDepopPrepareCommand(dsp_state, mix_in.buffer_count, mix_in.buffer_offset);
            }
        }
    } else {
        switch (in_params.sample_format) {
        case SampleFormat::Pcm16:
            DecodeFromWaveBuffers(voice_info, GetChannelMixBuffer(channel), dsp_state, channel,
                                  worker_params.sample_rate, worker_params.sample_count,
                                  in_params.node_id);
            break;
        case SampleFormat::Adpcm:
            ASSERT(channel == 0 && in_params.channel_count == 1);
            DecodeFromWaveBuffers(voice_info, GetChannelMixBuffer(0), dsp_state, 0,
                                  worker_params.sample_rate, worker_params.sample_count,
                                  in_params.node_id);
            break;
        default:
            UNREACHABLE_MSG("Unimplemented sample format={}", in_params.sample_format);
        }
    }
}

void CommandGenerator::GenerateBiquadFilterCommandForVoice(ServerVoiceInfo& voice_info,
                                                           VoiceState& dsp_state,
                                                           s32 mix_buffer_count, s32 channel) {
    for (std::size_t i = 0; i < AudioCommon::MAX_BIQUAD_FILTERS; i++) {
        const auto& in_params = voice_info.GetInParams();
        auto& biquad_filter = in_params.biquad_filter[i];
        // Check if biquad filter is actually used
        if (!biquad_filter.enabled) {
            continue;
        }

        // Reinitialize our biquad filter state if it was enabled previously
        if (!in_params.was_biquad_filter_enabled[i]) {
            dsp_state.biquad_filter_state.fill(0);
        }

        // Generate biquad filter
        GenerateBiquadFilterCommand(mix_buffer_count, biquad_filter, dsp_state.biquad_filter_state,
                                    mix_buffer_count + channel, mix_buffer_count + channel,
                                    worker_params.sample_count, voice_info.GetInParams().node_id);
    }
}

void AudioCore::CommandGenerator::GenerateBiquadFilterCommand(
    s32 mix_buffer, const BiquadFilterParameter& params, std::array<s64, 2>& state,
    std::size_t input_offset, std::size_t output_offset, s32 sample_count, s32 node_id) {
    if (dumping_frame) {
        LOG_DEBUG(Audio,
                  "(DSP_TRACE) GenerateBiquadFilterCommand node_id={}, "
                  "input_mix_buffer={}, output_mix_buffer={}",
                  node_id, input_offset, output_offset);
    }
    const auto* input = GetMixBuffer(input_offset);
    auto* output = GetMixBuffer(output_offset);

    // Biquad filter parameters
    const auto [n0, n1, n2] = params.numerator;
    const auto [d0, d1] = params.denominator;

    // Biquad filter states
    auto [s0, s1] = state;

    constexpr s64 int32_min = std::numeric_limits<s32>::min();
    constexpr s64 int32_max = std::numeric_limits<s32>::max();

    for (int i = 0; i < sample_count; ++i) {
        const auto sample = static_cast<s64>(input[i]);
        const auto f = (sample * n0 + s0 + 0x2000) >> 14;
        const auto y = static_cast<s64>(std::clamp(f, int32_min, int32_max));

        s0 = sample * n1 + y * d0 + s1;
        s1 = sample * n2 + y * d1;
        output[i] = static_cast<s32>(y);
    }

    state = {s0, s1};
}

void CommandGenerator::GenerateDepopPrepareCommand(VoiceState& dsp_state,
                                                   std::size_t mix_buffer_count,
                                                   std::size_t mix_buffer_offset) {
    for (std::size_t i = 0; i < mix_buffer_count; i++) {
        auto& sample = dsp_state.previous_samples[i];
        if (sample != 0) {
            depop_buffer[mix_buffer_offset + i] += sample;
            sample = 0;
        }
    }
}

void CommandGenerator::GenerateDepopForMixBuffersCommand(std::size_t mix_buffer_count,
                                                         std::size_t mix_buffer_offset,
                                                         s32 sample_rate) {
    const std::size_t end_offset =
        std::min(mix_buffer_offset + mix_buffer_count, GetTotalMixBufferCount());
    const s32 delta = sample_rate == 48000 ? 0x7B29 : 0x78CB;
    for (std::size_t i = mix_buffer_offset; i < end_offset; i++) {
        if (depop_buffer[i] == 0) {
            continue;
        }

        depop_buffer[i] =
            ApplyMixDepop(GetMixBuffer(i), depop_buffer[i], delta, worker_params.sample_count);
    }
}

void CommandGenerator::GenerateEffectCommand(ServerMixInfo& mix_info) {
    const std::size_t effect_count = effect_context.GetCount();
    const auto buffer_offset = mix_info.GetInParams().buffer_offset;
    for (std::size_t i = 0; i < effect_count; i++) {
        const auto index = mix_info.GetEffectOrder(i);
        if (index == AudioCommon::NO_EFFECT_ORDER) {
            break;
        }
        auto* info = effect_context.GetInfo(index);
        const auto type = info->GetType();

        // TODO(ogniK): Finish remaining effects
        switch (type) {
        case EffectType::Aux:
            GenerateAuxCommand(buffer_offset, info, info->IsEnabled());
            break;
        case EffectType::I3dl2Reverb:
            GenerateI3dl2ReverbEffectCommand(buffer_offset, info, info->IsEnabled());
            break;
        case EffectType::BiquadFilter:
            GenerateBiquadFilterEffectCommand(buffer_offset, info, info->IsEnabled());
            break;
        default:
            break;
        }

        info->UpdateForCommandGeneration();
    }
}

void CommandGenerator::GenerateI3dl2ReverbEffectCommand(s32 mix_buffer_offset, EffectBase* info,
                                                        bool enabled) {
    if (!enabled) {
        return;
    }
    const auto& params = dynamic_cast<EffectI3dl2Reverb*>(info)->GetParams();
    const auto channel_count = params.channel_count;
    for (s32 i = 0; i < channel_count; i++) {
        // TODO(ogniK): Actually implement reverb
        if (params.input[i] != params.output[i]) {
            const auto* input = GetMixBuffer(mix_buffer_offset + params.input[i]);
            auto* output = GetMixBuffer(mix_buffer_offset + params.output[i]);
            ApplyMix<1>(output, input, 32768, worker_params.sample_count);
        }
    }
}

void CommandGenerator::GenerateBiquadFilterEffectCommand(s32 mix_buffer_offset, EffectBase* info,
                                                         bool enabled) {
    auto* effect = dynamic_cast<EffectBiquadFilter*>(info);
    const auto& params = effect->GetParams();
    const auto channel_count = params.channel_count;

    if (!enabled) {
        for (s32 i = 0; i < channel_count; i++) {
            const auto* input = GetMixBuffer(mix_buffer_offset + params.input[i]);
            auto* output = GetMixBuffer(mix_buffer_offset + params.output[i]);
            ApplyMix<1>(output, input, 32768, worker_params.sample_count);
        }
    } else {
        const BiquadFilterParameter filter_params{
            .enabled = true,
            .numerator = params.numerator,
            .denominator = params.denominator,
        };
        auto& states = effect->GetState();
        const auto need_init = params.status == ParameterStatus::Initialized ||
                               params.status == ParameterStatus::Updating;
        for (s32 i = 0; i < channel_count; i++) {
            if (need_init) {
                states[i].fill(0);
            }
            GenerateBiquadFilterCommand(mix_buffer_offset, filter_params, states[i],
                                        params.input[i], params.output[i],
                                        worker_params.sample_count, 0);
        }
    }
}

void CommandGenerator::GenerateAuxCommand(s32 mix_buffer_offset, EffectBase* info, bool enabled) {
    auto* aux = dynamic_cast<EffectAuxInfo*>(info);
    const auto& params = aux->GetParams();
    if (aux->GetSendBuffer() != 0 && aux->GetRecvBuffer() != 0) {
        const auto max_channels = params.count;
        u32 offset{};
        for (u32 channel = 0; channel < max_channels; channel++) {
            u32 write_count = 0;
            if (channel == (max_channels - 1)) {
                write_count = offset + worker_params.sample_count;
            }

            const auto input_index = params.input_mix_buffers[channel] + mix_buffer_offset;
            const auto output_index = params.output_mix_buffers[channel] + mix_buffer_offset;

            if (enabled) {
                AuxInfoDSP send_info{};
                AuxInfoDSP recv_info{};
                memory.ReadBlock(aux->GetSendInfo(), &send_info, sizeof(AuxInfoDSP));
                memory.ReadBlock(aux->GetRecvInfo(), &recv_info, sizeof(AuxInfoDSP));

                WriteAuxBuffer(send_info, aux->GetSendBuffer(), params.sample_count,
                               GetMixBuffer(input_index), worker_params.sample_count, offset,
                               write_count);
                memory.WriteBlock(aux->GetSendInfo(), &send_info, sizeof(AuxInfoDSP));

                const auto samples_read = ReadAuxBuffer(
                    recv_info, aux->GetRecvBuffer(), params.sample_count,
                    GetMixBuffer(output_index), worker_params.sample_count, offset, write_count);
                memory.WriteBlock(aux->GetRecvInfo(), &recv_info, sizeof(AuxInfoDSP));

                if (samples_read != static_cast<int>(worker_params.sample_count) &&
                    samples_read <= params.sample_count) {
                    std::memset(GetMixBuffer(output_index), 0, params.sample_count - samples_read);
                }
            } else {
                AuxInfoDSP empty{};
                memory.WriteBlock(aux->GetSendInfo(), &empty, sizeof(AuxInfoDSP));
                memory.WriteBlock(aux->GetRecvInfo(), &empty, sizeof(AuxInfoDSP));
                if (output_index != input_index) {
                    std::memcpy(GetMixBuffer(output_index), GetMixBuffer(input_index),
                                worker_params.sample_count * sizeof(s32));
                }
            }

            offset += worker_params.sample_count;
        }
    }
}

ServerSplitterDestinationData* CommandGenerator::GetDestinationData(s32 splitter_id, s32 index) {
    if (splitter_id == AudioCommon::NO_SPLITTER) {
        return nullptr;
    }
    return splitter_context.GetDestinationData(splitter_id, index);
}

s32 CommandGenerator::WriteAuxBuffer(AuxInfoDSP& dsp_info, VAddr send_buffer, u32 max_samples,
                                     const s32* data, u32 sample_count, u32 write_offset,
                                     u32 write_count) {
    if (max_samples == 0) {
        return 0;
    }
    u32 offset = dsp_info.write_offset + write_offset;
    if (send_buffer == 0 || offset > max_samples) {
        return 0;
    }

    std::size_t data_offset{};
    u32 remaining = sample_count;
    while (remaining > 0) {
        // Get position in buffer
        const auto base = send_buffer + (offset * sizeof(u32));
        const auto samples_to_grab = std::min(max_samples - offset, remaining);
        // Write to output
        memory.WriteBlock(base, (data + data_offset), samples_to_grab * sizeof(u32));
        offset = (offset + samples_to_grab) % max_samples;
        remaining -= samples_to_grab;
        data_offset += samples_to_grab;
    }

    if (write_count != 0) {
        dsp_info.write_offset = (dsp_info.write_offset + write_count) % max_samples;
    }
    return sample_count;
}

s32 CommandGenerator::ReadAuxBuffer(AuxInfoDSP& recv_info, VAddr recv_buffer, u32 max_samples,
                                    s32* out_data, u32 sample_count, u32 read_offset,
                                    u32 read_count) {
    if (max_samples == 0) {
        return 0;
    }

    u32 offset = recv_info.read_offset + read_offset;
    if (recv_buffer == 0 || offset > max_samples) {
        return 0;
    }

    u32 remaining = sample_count;
    while (remaining > 0) {
        const auto base = recv_buffer + (offset * sizeof(u32));
        const auto samples_to_grab = std::min(max_samples - offset, remaining);
        std::vector<s32> buffer(samples_to_grab);
        memory.ReadBlock(base, buffer.data(), buffer.size() * sizeof(u32));
        std::memcpy(out_data, buffer.data(), buffer.size() * sizeof(u32));
        out_data += samples_to_grab;
        offset = (offset + samples_to_grab) % max_samples;
        remaining -= samples_to_grab;
    }

    if (read_count != 0) {
        recv_info.read_offset = (recv_info.read_offset + read_count) % max_samples;
    }
    return sample_count;
}

void CommandGenerator::GenerateVolumeRampCommand(float last_volume, float current_volume,
                                                 s32 channel, s32 node_id) {
    const auto last = static_cast<s32>(last_volume * 32768.0f);
    const auto current = static_cast<s32>(current_volume * 32768.0f);
    const auto delta = static_cast<s32>((static_cast<float>(current) - static_cast<float>(last)) /
                                        static_cast<float>(worker_params.sample_count));

    if (dumping_frame) {
        LOG_DEBUG(Audio,
                  "(DSP_TRACE) GenerateVolumeRampCommand node_id={}, input={}, output={}, "
                  "last_volume={}, current_volume={}",
                  node_id, GetMixChannelBufferOffset(channel), GetMixChannelBufferOffset(channel),
                  last_volume, current_volume);
    }
    // Apply generic gain on samples
    ApplyGain(GetChannelMixBuffer(channel), GetChannelMixBuffer(channel), last, delta,
              worker_params.sample_count);
}

void CommandGenerator::GenerateVoiceMixCommand(const MixVolumeBuffer& mix_volumes,
                                               const MixVolumeBuffer& last_mix_volumes,
                                               VoiceState& dsp_state, s32 mix_buffer_offset,
                                               s32 mix_buffer_count, s32 voice_index, s32 node_id) {
    // Loop all our mix buffers
    for (s32 i = 0; i < mix_buffer_count; i++) {
        if (last_mix_volumes[i] != 0.0f || mix_volumes[i] != 0.0f) {
            const auto delta = static_cast<float>((mix_volumes[i] - last_mix_volumes[i])) /
                               static_cast<float>(worker_params.sample_count);

            if (dumping_frame) {
                LOG_DEBUG(Audio,
                          "(DSP_TRACE) GenerateVoiceMixCommand node_id={}, input={}, "
                          "output={}, last_volume={}, current_volume={}",
                          node_id, voice_index, mix_buffer_offset + i, last_mix_volumes[i],
                          mix_volumes[i]);
            }

            dsp_state.previous_samples[i] =
                ApplyMixRamp(GetMixBuffer(mix_buffer_offset + i), GetMixBuffer(voice_index),
                             last_mix_volumes[i], delta, worker_params.sample_count);
        } else {
            dsp_state.previous_samples[i] = 0;
        }
    }
}

void CommandGenerator::GenerateSubMixCommand(ServerMixInfo& mix_info) {
    if (dumping_frame) {
        LOG_DEBUG(Audio, "(DSP_TRACE) GenerateSubMixCommand");
    }
    const auto& in_params = mix_info.GetInParams();
    GenerateDepopForMixBuffersCommand(in_params.buffer_count, in_params.buffer_offset,
                                      in_params.sample_rate);

    GenerateEffectCommand(mix_info);

    GenerateMixCommands(mix_info);
}

void CommandGenerator::GenerateMixCommands(ServerMixInfo& mix_info) {
    if (!mix_info.HasAnyConnection()) {
        return;
    }
    const auto& in_params = mix_info.GetInParams();
    if (in_params.dest_mix_id != AudioCommon::NO_MIX) {
        const auto& dest_mix = mix_context.GetInfo(in_params.dest_mix_id);
        const auto& dest_in_params = dest_mix.GetInParams();

        const auto buffer_count = in_params.buffer_count;

        for (s32 i = 0; i < buffer_count; i++) {
            for (s32 j = 0; j < dest_in_params.buffer_count; j++) {
                const auto mixed_volume = in_params.volume * in_params.mix_volume[i][j];
                if (mixed_volume != 0.0f) {
                    GenerateMixCommand(dest_in_params.buffer_offset + j,
                                       in_params.buffer_offset + i, mixed_volume,
                                       in_params.node_id);
                }
            }
        }
    } else if (in_params.splitter_id != AudioCommon::NO_SPLITTER) {
        s32 base{};
        while (const auto* destination_data = GetDestinationData(in_params.splitter_id, base++)) {
            if (!destination_data->IsConfigured()) {
                continue;
            }

            const auto& dest_mix = mix_context.GetInfo(destination_data->GetMixId());
            const auto& dest_in_params = dest_mix.GetInParams();
            const auto mix_index = (base - 1) % in_params.buffer_count + in_params.buffer_offset;
            for (std::size_t i = 0; i < static_cast<std::size_t>(dest_in_params.buffer_count);
                 i++) {
                const auto mixed_volume = in_params.volume * destination_data->GetMixVolume(i);
                if (mixed_volume != 0.0f) {
                    GenerateMixCommand(dest_in_params.buffer_offset + i, mix_index, mixed_volume,
                                       in_params.node_id);
                }
            }
        }
    }
}

void CommandGenerator::GenerateMixCommand(std::size_t output_offset, std::size_t input_offset,
                                          float volume, s32 node_id) {

    if (dumping_frame) {
        LOG_DEBUG(Audio,
                  "(DSP_TRACE) GenerateMixCommand node_id={}, input={}, output={}, volume={}",
                  node_id, input_offset, output_offset, volume);
    }

    auto* output = GetMixBuffer(output_offset);
    const auto* input = GetMixBuffer(input_offset);

    const s32 gain = static_cast<s32>(volume * 32768.0f);
    // Mix with loop unrolling
    if (worker_params.sample_count % 4 == 0) {
        ApplyMix<4>(output, input, gain, worker_params.sample_count);
    } else if (worker_params.sample_count % 2 == 0) {
        ApplyMix<2>(output, input, gain, worker_params.sample_count);
    } else {
        ApplyMix<1>(output, input, gain, worker_params.sample_count);
    }
}

void CommandGenerator::GenerateFinalMixCommand() {
    if (dumping_frame) {
        LOG_DEBUG(Audio, "(DSP_TRACE) GenerateFinalMixCommand");
    }
    auto& mix_info = mix_context.GetFinalMixInfo();
    const auto& in_params = mix_info.GetInParams();

    GenerateDepopForMixBuffersCommand(in_params.buffer_count, in_params.buffer_offset,
                                      in_params.sample_rate);

    GenerateEffectCommand(mix_info);

    for (s32 i = 0; i < in_params.buffer_count; i++) {
        const s32 gain = static_cast<s32>(in_params.volume * 32768.0f);
        if (dumping_frame) {
            LOG_DEBUG(
                Audio,
                "(DSP_TRACE) ApplyGainWithoutDelta node_id={}, input={}, output={}, volume={}",
                in_params.node_id, in_params.buffer_offset + i, in_params.buffer_offset + i,
                in_params.volume);
        }
        ApplyGainWithoutDelta(GetMixBuffer(in_params.buffer_offset + i),
                              GetMixBuffer(in_params.buffer_offset + i), gain,
                              worker_params.sample_count);
    }
}

s32 CommandGenerator::DecodePcm16(ServerVoiceInfo& voice_info, VoiceState& dsp_state,
                                  s32 sample_count, s32 channel, std::size_t mix_offset) {
    const auto& in_params = voice_info.GetInParams();
    const auto& wave_buffer = in_params.wave_buffer[dsp_state.wave_buffer_index];
    if (wave_buffer.buffer_address == 0) {
        return 0;
    }
    if (wave_buffer.buffer_size == 0) {
        return 0;
    }
    if (wave_buffer.end_sample_offset < wave_buffer.start_sample_offset) {
        return 0;
    }
    const auto samples_remaining =
        (wave_buffer.end_sample_offset - wave_buffer.start_sample_offset) - dsp_state.offset;
    const auto start_offset =
        ((wave_buffer.start_sample_offset + dsp_state.offset) * in_params.channel_count) *
        sizeof(s16);
    const auto buffer_pos = wave_buffer.buffer_address + start_offset;
    const auto samples_processed = std::min(sample_count, samples_remaining);

    if (in_params.channel_count == 1) {
        std::vector<s16> buffer(samples_processed);
        memory.ReadBlock(buffer_pos, buffer.data(), buffer.size() * sizeof(s16));
        for (std::size_t i = 0; i < buffer.size(); i++) {
            sample_buffer[mix_offset + i] = buffer[i];
        }
    } else {
        const auto channel_count = in_params.channel_count;
        std::vector<s16> buffer(samples_processed * channel_count);
        memory.ReadBlock(buffer_pos, buffer.data(), buffer.size() * sizeof(s16));

        for (std::size_t i = 0; i < static_cast<std::size_t>(samples_processed); i++) {
            sample_buffer[mix_offset + i] = buffer[i * channel_count + channel];
        }
    }

    return samples_processed;
}

s32 CommandGenerator::DecodeAdpcm(ServerVoiceInfo& voice_info, VoiceState& dsp_state,
                                  s32 sample_count, s32 channel, std::size_t mix_offset) {
    const auto& in_params = voice_info.GetInParams();
    const auto& wave_buffer = in_params.wave_buffer[dsp_state.wave_buffer_index];
    if (wave_buffer.buffer_address == 0) {
        return 0;
    }
    if (wave_buffer.buffer_size == 0) {
        return 0;
    }
    if (wave_buffer.end_sample_offset < wave_buffer.start_sample_offset) {
        return 0;
    }

    static constexpr std::array<int, 16> SIGNED_NIBBLES{
        0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1,
    };

    constexpr std::size_t FRAME_LEN = 8;
    constexpr std::size_t NIBBLES_PER_SAMPLE = 16;
    constexpr std::size_t SAMPLES_PER_FRAME = 14;

    auto frame_header = dsp_state.context.header;
    s32 idx = (frame_header >> 4) & 0xf;
    s32 scale = frame_header & 0xf;
    s16 yn1 = dsp_state.context.yn1;
    s16 yn2 = dsp_state.context.yn2;

    Codec::ADPCM_Coeff coeffs;
    memory.ReadBlock(in_params.additional_params_address, coeffs.data(),
                     sizeof(Codec::ADPCM_Coeff));

    s32 coef1 = coeffs[idx * 2];
    s32 coef2 = coeffs[idx * 2 + 1];

    const auto samples_remaining =
        (wave_buffer.end_sample_offset - wave_buffer.start_sample_offset) - dsp_state.offset;
    const auto samples_processed = std::min(sample_count, samples_remaining);
    const auto sample_pos = wave_buffer.start_sample_offset + dsp_state.offset;

    const auto samples_remaining_in_frame = sample_pos % SAMPLES_PER_FRAME;
    auto position_in_frame = ((sample_pos / SAMPLES_PER_FRAME) * NIBBLES_PER_SAMPLE) +
                             samples_remaining_in_frame + (samples_remaining_in_frame != 0 ? 2 : 0);

    const auto decode_sample = [&](const int nibble) -> s16 {
        const int xn = nibble * (1 << scale);
        // We first transform everything into 11 bit fixed point, perform the second order
        // digital filter, then transform back.
        // 0x400 == 0.5 in 11 bit fixed point.
        // Filter: y[n] = x[n] + 0.5 + c1 * y[n-1] + c2 * y[n-2]
        int val = ((xn << 11) + 0x400 + coef1 * yn1 + coef2 * yn2) >> 11;
        // Clamp to output range.
        val = std::clamp<s32>(val, -32768, 32767);
        // Advance output feedback.
        yn2 = yn1;
        yn1 = static_cast<s16>(val);
        return yn1;
    };

    std::size_t buffer_offset{};
    std::vector<u8> buffer(
        std::max((samples_processed / FRAME_LEN) * SAMPLES_PER_FRAME, FRAME_LEN));
    memory.ReadBlock(wave_buffer.buffer_address + (position_in_frame / 2), buffer.data(),
                     buffer.size());
    std::size_t cur_mix_offset = mix_offset;

    auto remaining_samples = samples_processed;
    while (remaining_samples > 0) {
        if (position_in_frame % NIBBLES_PER_SAMPLE == 0) {
            // Read header
            frame_header = buffer[buffer_offset++];
            idx = (frame_header >> 4) & 0xf;
            scale = frame_header & 0xf;
            coef1 = coeffs[idx * 2];
            coef2 = coeffs[idx * 2 + 1];
            position_in_frame += 2;

            // Decode entire frame
            if (remaining_samples >= static_cast<int>(SAMPLES_PER_FRAME)) {
                for (std::size_t i = 0; i < SAMPLES_PER_FRAME / 2; i++) {
                    // Sample 1
                    const s32 s0 = SIGNED_NIBBLES[buffer[buffer_offset] >> 4];
                    const s32 s1 = SIGNED_NIBBLES[buffer[buffer_offset++] & 0xf];
                    const s16 sample_1 = decode_sample(s0);
                    const s16 sample_2 = decode_sample(s1);
                    sample_buffer[cur_mix_offset++] = sample_1;
                    sample_buffer[cur_mix_offset++] = sample_2;
                }
                remaining_samples -= static_cast<int>(SAMPLES_PER_FRAME);
                position_in_frame += SAMPLES_PER_FRAME;
                continue;
            }
        }
        // Decode mid frame
        s32 current_nibble = buffer[buffer_offset];
        if (position_in_frame++ & 0x1) {
            current_nibble &= 0xf;
            buffer_offset++;
        } else {
            current_nibble >>= 4;
        }
        const s16 sample = decode_sample(SIGNED_NIBBLES[current_nibble]);
        sample_buffer[cur_mix_offset++] = sample;
        remaining_samples--;
    }

    dsp_state.context.header = frame_header;
    dsp_state.context.yn1 = yn1;
    dsp_state.context.yn2 = yn2;

    return samples_processed;
}

s32* CommandGenerator::GetMixBuffer(std::size_t index) {
    return mix_buffer.data() + (index * worker_params.sample_count);
}

const s32* CommandGenerator::GetMixBuffer(std::size_t index) const {
    return mix_buffer.data() + (index * worker_params.sample_count);
}

std::size_t CommandGenerator::GetMixChannelBufferOffset(s32 channel) const {
    return worker_params.mix_buffer_count + channel;
}

std::size_t CommandGenerator::GetTotalMixBufferCount() const {
    return worker_params.mix_buffer_count + AudioCommon::MAX_CHANNEL_COUNT;
}

s32* CommandGenerator::GetChannelMixBuffer(s32 channel) {
    return GetMixBuffer(worker_params.mix_buffer_count + channel);
}

const s32* CommandGenerator::GetChannelMixBuffer(s32 channel) const {
    return GetMixBuffer(worker_params.mix_buffer_count + channel);
}

void CommandGenerator::DecodeFromWaveBuffers(ServerVoiceInfo& voice_info, s32* output,
                                             VoiceState& dsp_state, s32 channel,
                                             s32 target_sample_rate, s32 sample_count,
                                             s32 node_id) {
    const auto& in_params = voice_info.GetInParams();
    if (dumping_frame) {
        LOG_DEBUG(Audio,
                  "(DSP_TRACE) DecodeFromWaveBuffers, node_id={}, channel={}, "
                  "format={}, sample_count={}, sample_rate={}, mix_id={}, splitter_id={}",
                  node_id, channel, in_params.sample_format, sample_count, in_params.sample_rate,
                  in_params.mix_id, in_params.splitter_info_id);
    }
    ASSERT_OR_EXECUTE(output != nullptr, { return; });

    const auto resample_rate = static_cast<s32>(
        static_cast<float>(in_params.sample_rate) / static_cast<float>(target_sample_rate) *
        static_cast<float>(static_cast<s32>(in_params.pitch * 32768.0f)));
    if (dsp_state.fraction + sample_count * resample_rate >
        static_cast<s32>(SCALED_MIX_BUFFER_SIZE - 4ULL)) {
        return;
    }

    auto min_required_samples =
        std::min(static_cast<s32>(SCALED_MIX_BUFFER_SIZE) - dsp_state.fraction, resample_rate);
    if (min_required_samples >= sample_count) {
        min_required_samples = sample_count;
    }

    std::size_t temp_mix_offset{};
    bool is_buffer_completed{false};
    auto samples_remaining = sample_count;
    while (samples_remaining > 0 && !is_buffer_completed) {
        const auto samples_to_output = std::min(samples_remaining, min_required_samples);
        const auto samples_to_read = (samples_to_output * resample_rate + dsp_state.fraction) >> 15;

        if (!in_params.behavior_flags.is_pitch_and_src_skipped) {
            // Append sample histtory for resampler
            for (std::size_t i = 0; i < AudioCommon::MAX_SAMPLE_HISTORY; i++) {
                sample_buffer[temp_mix_offset + i] = dsp_state.sample_history[i];
            }
            temp_mix_offset += 4;
        }

        s32 samples_read{};
        while (samples_read < samples_to_read) {
            const auto& wave_buffer = in_params.wave_buffer[dsp_state.wave_buffer_index];
            // No more data can be read
            if (!dsp_state.is_wave_buffer_valid[dsp_state.wave_buffer_index]) {
                is_buffer_completed = true;
                break;
            }

            if (in_params.sample_format == SampleFormat::Adpcm && dsp_state.offset == 0 &&
                wave_buffer.context_address != 0 && wave_buffer.context_size != 0) {
                // TODO(ogniK): ADPCM loop context
            }

            s32 samples_decoded{0};
            switch (in_params.sample_format) {
            case SampleFormat::Pcm16:
                samples_decoded = DecodePcm16(voice_info, dsp_state, samples_to_read - samples_read,
                                              channel, temp_mix_offset);
                break;
            case SampleFormat::Adpcm:
                samples_decoded = DecodeAdpcm(voice_info, dsp_state, samples_to_read - samples_read,
                                              channel, temp_mix_offset);
                break;
            default:
                UNREACHABLE_MSG("Unimplemented sample format={}", in_params.sample_format);
            }

            temp_mix_offset += samples_decoded;
            samples_read += samples_decoded;
            dsp_state.offset += samples_decoded;
            dsp_state.played_sample_count += samples_decoded;

            if (dsp_state.offset >=
                    (wave_buffer.end_sample_offset - wave_buffer.start_sample_offset) ||
                samples_decoded == 0) {
                // Reset our sample offset
                dsp_state.offset = 0;
                if (wave_buffer.is_looping) {
                    if (samples_decoded == 0) {
                        // End of our buffer
                        is_buffer_completed = true;
                        break;
                    }

                    if (in_params.behavior_flags.is_played_samples_reset_at_loop_point.Value()) {
                        dsp_state.played_sample_count = 0;
                    }
                } else {

                    // Update our wave buffer states
                    dsp_state.is_wave_buffer_valid[dsp_state.wave_buffer_index] = false;
                    dsp_state.wave_buffer_consumed++;
                    dsp_state.wave_buffer_index =
                        (dsp_state.wave_buffer_index + 1) % AudioCommon::MAX_WAVE_BUFFERS;
                    if (wave_buffer.end_of_stream) {
                        dsp_state.played_sample_count = 0;
                    }
                }
            }
        }

        if (in_params.behavior_flags.is_pitch_and_src_skipped.Value()) {
            // No need to resample
            std::memcpy(output, sample_buffer.data(), samples_read * sizeof(s32));
        } else {
            std::fill(sample_buffer.begin() + temp_mix_offset,
                      sample_buffer.begin() + temp_mix_offset + (samples_to_read - samples_read),
                      0);
            AudioCore::Resample(output, sample_buffer.data(), resample_rate, dsp_state.fraction,
                                samples_to_output);
            // Resample
            for (std::size_t i = 0; i < AudioCommon::MAX_SAMPLE_HISTORY; i++) {
                dsp_state.sample_history[i] = sample_buffer[samples_to_read + i];
            }
        }
        output += samples_to_output;
        samples_remaining -= samples_to_output;
    }
}

} // namespace AudioCore
