// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <type_traits>

#include "common/common_types.h"
#include "core/core_timing.h"
#include "core/hle/service/hid/hidbus/ringcon.h"

namespace Service::HID {
// These values are hardcoded from a real joycon
constexpr u8 DEVICE_ID = 0x20;
constexpr RingController::FirmwareVersion version = {0x0, 0x2c};
constexpr RingController::FactoryCalibration factory_calibration = {5469, 742, 2159, 2444};
constexpr RingController::UserCalibration user_calibration = {-13570, 134, -13570, 134, 2188, 62};

RingController::RingController() {
    OnLoadInputDevices();
}
RingController::~RingController() = default;

void RingController::OnLoadInputDevices() {
    // Use the horizontal axis of left stick for emulating input
    // There is no point on adding a frontend implementation since Ring Fit Adventure doesn't work
    const auto& player = Settings::values.players.GetValue()[0];
    stick =
        Input::CreateDevice<Input::AnalogDevice>(player.analogs[Settings::NativeAnalog::LStick]);
}

void RingController::Update() {
    if (!ringcon_enabled) {
        return;
    }

    if (!polling_mode_enabled || !is_transfer_memory_set) {
        return;
    }

    switch (polling_mode) {
    case JoyPollingMode::SixAxisSensorEnable: {
        sixaxis_data.header.total_entries = 10;
        sixaxis_data.header.result = ResultSuccess;
        const auto& last_entry = sixaxis_data.entries[sixaxis_data.header.latest_entry];

        sixaxis_data.header.latest_entry = (sixaxis_data.header.latest_entry + 1) % 10;
        auto& curr_entry = sixaxis_data.entries[sixaxis_data.header.latest_entry];

        curr_entry.sampling_number = last_entry.sampling_number + 1;
        curr_entry.polling_data.sampling_number = curr_entry.sampling_number;

        const RingConData ringcon_value = GetSensorValue();
        curr_entry.polling_data.out_size = sizeof(ringcon_value);
        std::memcpy(curr_entry.polling_data.data.data(), &ringcon_value, sizeof(ringcon_value));

        std::memcpy(transfer_memory, &sixaxis_data, sizeof(sixaxis_data));
        break;
    }
    default:
        LOG_ERROR(Service_HID, "Polling mode not supported {}", polling_mode);
        break;
    }
}

RingController::RingConData RingController::GetSensorValue() const {
    RingConData ringcon_sensor_value{
        .status = DataValid::Valid,
        .data = 0,
    };

    if (!stick) {
        LOG_ERROR(Service_HID, "Input not initialized");
        return ringcon_sensor_value;
    }

    const auto [stick_x_f, stick_y_f] = stick->GetStatus();
    constexpr s16 idle_value = 2280;
    constexpr s16 range = 2500;

    ringcon_sensor_value.data = static_cast<s16>(stick_x_f * range) + idle_value;

    return ringcon_sensor_value;
}

u8 RingController::GetDeviceId() const {
    return DEVICE_ID;
}

std::vector<u8> RingController::GetReply() {
    const RingConCommands current_command = command;
    command = RingConCommands::Error;

    switch (current_command) {
    case RingConCommands::GetFirmwareVersion:
        return GetFirmwareVersionReply();
    case RingConCommands::c20105:
        return GetC020105Reply();
    case RingConCommands::ReadTotalPushCount:
        return GetReadTotalPushCountReply();
    case RingConCommands::ReadUnkCal:
        return GetReadUnkCalReply();
    case RingConCommands::ReadFactoryCal:
        return GetReadFactoryCalReply();
    case RingConCommands::ReadUserCal:
        return GetReadUserCalReply();
    case RingConCommands::ReadId:
        return GetReadIdReply();
    default:
        return GetErrorReply();
    }
}

bool RingController::SetCommand(const std::vector<u8>& data) {
    if (data.size() != 4) {
        LOG_ERROR(Service_HID, "Command size not supported {}", data.size());
        command = RingConCommands::Error;
        return false;
    }

    // There must be a better way to do this
    const u32 command_id =
        u32{data[0]} + (u32{data[1]} << 8) + (u32{data[2]} << 16) + (u32{data[3]} << 24);
    static constexpr std::array supported_commands = {
        RingConCommands::GetFirmwareVersion,
        RingConCommands::ReadId,
        RingConCommands::c20105,
        RingConCommands::ReadUnkCal,
        RingConCommands::ReadFactoryCal,
        RingConCommands::ReadUserCal,
        RingConCommands::ReadTotalPushCount,
    };

    for (RingConCommands cmd : supported_commands) {
        if (command_id == static_cast<u32>(cmd)) {
            command = cmd;
            return true;
        }
    }

    LOG_ERROR(Service_HID, "Command not implemented {}", command_id);
    command = RingConCommands::Error;
    return false;
}

void RingController::Enable(bool enable) {
    ringcon_enabled = enable;
}

bool RingController::IsEnabled() const {
    return ringcon_enabled;
}

bool RingController::IsPollingMode() const {
    return polling_mode_enabled;
}

JoyPollingMode RingController::GetPollingMode() const {
    return polling_mode;
}

void RingController::SetPollingMode(JoyPollingMode mode) {
    polling_mode = mode;
    polling_mode_enabled = true;
}

void RingController::DisablePollingMode() {
    polling_mode_enabled = false;
}

void RingController::SetTransferMemoryPointer(u8* t_mem) {
    is_transfer_memory_set = true;
    transfer_memory = t_mem;
}

u8 RingController::GetCrcValue(const std::vector<u8>& data) const {
    u8 crc = 0;
    for (std::size_t index = 0; index < data.size(); index++) {
        for (u8 i = 0x80; i > 0; i >>= 1) {
            bool bit = (crc & 0x80) != 0;
            if ((data[index] & i) != 0) {
                bit = !bit;
            }
            crc <<= 1;
            if (bit) {
                crc ^= 0x8d;
            }
        }
    }
    return crc;
}

std::vector<u8> RingController::GetFirmwareVersionReply() const {
    const FirmwareVersionReply reply{
        .status = DataValid::Valid,
        .firmware = version,
    };

    return GetDataVector(reply);
}

std::vector<u8> RingController::GetReadIdReply() const {
    // The values are hardcoded from a real joycon
    const ReadIdReply reply{
        .status = DataValid::Valid,
        .id_l_x0 = 8,
        .id_l_x0_2 = 41,
        .id_l_x4 = 22294,
        .id_h_x0 = 19777,
        .id_h_x0_2 = 13621,
        .id_h_x4 = 8245,
    };

    return GetDataVector(reply);
}

std::vector<u8> RingController::GetC020105Reply() const {
    const Cmd020105Reply reply{
        .status = DataValid::Valid,
        .data = 1,
    };

    return GetDataVector(reply);
}

std::vector<u8> RingController::GetReadUnkCalReply() const {
    const ReadUnkCalReply reply{
        .status = DataValid::Valid,
        .data = 0,
    };

    return GetDataVector(reply);
}

std::vector<u8> RingController::GetReadFactoryCalReply() const {
    const ReadFactoryCalReply reply{
        .status = DataValid::Valid,
        .calibration = factory_calibration,
    };

    return GetDataVector(reply);
}

std::vector<u8> RingController::GetReadUserCalReply() const {
    const ReadUserCalReply reply{
        .status = DataValid::Valid,
        .calibration = user_calibration,
    };

    return GetDataVector(reply);
}

std::vector<u8> RingController::GetReadTotalPushCountReply() const {
    // The values are hardcoded from a real joycon
    const GetThreeByteReply reply{
        .status = DataValid::Valid,
        .data = {30, 0, 0},
        .crc = GetCrcValue({30, 0, 0, 0}),
    };

    return GetDataVector(reply);
}

std::vector<u8> RingController::GetErrorReply() const {
    const ErrorReply reply{
        .status = DataValid::BadCRC,
    };

    return GetDataVector(reply);
}

template <typename T>
std::vector<u8> RingController::GetDataVector(const T& reply) const {
    static_assert(std::is_trivially_copyable_v<T>);
    std::vector<u8> data;
    data.resize(sizeof(reply));
    std::memcpy(data.data(), &reply, sizeof(reply));
    return data;
}

} // namespace Service::HID
