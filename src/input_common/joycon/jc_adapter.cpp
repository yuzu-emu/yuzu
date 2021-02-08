// Copyright 2020 Yuzu Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <span>
#include <thread>
#include "common/logging/log.h"
#include "input_common/joycon/jc_adapter.h"

namespace JCAdapter {

/// Used to loop through and assign button in poller
constexpr std::array<PadButton, 22> PadButtonArray{
    PadButton::BUTTON_DOWN,    PadButton::BUTTON_UP,    PadButton::BUTTON_RIGHT,
    PadButton::BUTTON_LEFT,    PadButton::BUTTON_L_SR,  PadButton::BUTTON_L_SL,
    PadButton::TRIGGER_L,      PadButton::TRIGGER_ZL,   PadButton::BUTTON_Y,
    PadButton::BUTTON_X,       PadButton::BUTTON_B,     PadButton::BUTTON_A,
    PadButton::BUTTON_R_SL,    PadButton::BUTTON_R_SR,  PadButton::TRIGGER_R,
    PadButton::TRIGGER_ZR,     PadButton::BUTTON_MINUS, PadButton::BUTTON_PLUS,
    PadButton::STICK_R,        PadButton::STICK_L,      PadButton::BUTTON_HOME,
    PadButton::BUTTON_CAPTURE,
};

Joycons::Joycons() {
    LOG_INFO(Input, "JC Adapter Initialization started");
    global_counter = 0;
    adapter_thread_running = false;
    const int init_res = hid_init();
    if (init_res == 0) {
        Setup();
    } else {
        LOG_ERROR(Input, "HIDapi could not be initialized. failed with error = {}", init_res);
    }
}

void Joycons::SendWrite(hid_device* handle, std::vector<u8> buffer, int size) {
    hid_write(handle, buffer.data(), size);
}

u8 Joycons::GetCounter() {
    const u8 counter = global_counter;
    global_counter = (global_counter + 1) & 0x0F;
    return counter;
}

std::vector<u8> Joycons::SubCommand(hid_device* handle, SubComamnd sc, std::vector<u8> buffer,
                                    int size) {
    std::vector<u8> local_buffer(size + 11);

    local_buffer[0] = static_cast<u8>(Output::RUMBLE_AND_SUBCMD);
    local_buffer[1] = GetCounter();
    for (int i = 0; i < 8; ++i) {
        local_buffer[i + 2] = default_buffer[i];
    }
    local_buffer[10] = static_cast<u8>(sc);
    for (int i = 0; i < size; ++i) {
        local_buffer[11 + i] = buffer[i];
    }

    SendWrite(handle, local_buffer, size + 11);
    return GetResponse(handle, sc);
}

std::vector<u8> Joycons::GetResponse(hid_device* handle, SubComamnd sc) {
    int tries = 0;
    std::vector<u8> buffer(max_resp_size);
    do {
        int result = hid_read_timeout(handle, buffer.data(), max_resp_size, 100);
        if (result < 1) {
            LOG_ERROR(Input, "No response from joystick");
        }
        tries++;
    } while (tries < 10 && buffer[0] != 0x21 && buffer[14] != static_cast<u8>(sc));
    return buffer;
}

std::vector<u8> Joycons::ReadSPI(hid_device* handle, CalAddr addr, u8 size) {
    std::vector<u8> buffer = {0x00, 0x00, 0x00, 0x00, size};
    std::vector<u8> local_buffer(size + 20);

    buffer[0] = static_cast<u8>(static_cast<u16>(addr) & 0x00FF);
    buffer[1] = static_cast<u8>((static_cast<u16>(addr) & 0xFF00) >> 8);
    for (int i = 0; i < 100; ++i) {
        local_buffer = SubCommand(handle, SubComamnd::SPI_FLASH_READ, buffer, 5);
        if (local_buffer[15] == buffer[0] && local_buffer[16] == buffer[1]) {
            break;
        }
    }
    return std::vector<u8>(local_buffer.begin() + 20, local_buffer.end());
}

void Joycons::SetJoyStickCal(std::vector<u8> buffer, JoyStick& axis1, JoyStick& axis2, bool left) {
    if (left) {
        axis1.max = (u16)(((buffer[1] & 0x0F) << 8) | buffer[0]);
        axis2.max = (u16)((buffer[2] << 4) | (buffer[1] >> 4));
        axis1.center = (u16)(((buffer[4] & 0x0F) << 8) | buffer[3]);
        axis2.center = (u16)((buffer[5] << 4) | (buffer[4] >> 4));
        axis1.min = (u16)(((buffer[7] & 0x0F) << 8) | buffer[6]);
        axis2.min = (u16)((buffer[8] << 4) | (buffer[7] >> 4));
    } else {
        axis1.center = (u16)(((buffer[1] & 0x0F) << 8) | buffer[0]);
        axis2.center = (u16)((buffer[2] << 4) | (buffer[1] >> 4));
        axis1.min = (u16)(((buffer[4] & 0x0F) << 8) | buffer[3]);
        axis2.min = (u16)((buffer[5] << 4) | (buffer[4] >> 4));
        axis1.max = (u16)(((buffer[7] & 0x0F) << 8) | buffer[6]);
        axis2.max = (u16)((buffer[8] << 4) | (buffer[7] >> 4));
    }

    /* Nintendo fix for drifting stick
    buffer = ReadSPI(0x60, (isLeft ? 0x86 : 0x98), 16);
    joycon[0].joystick.deadzone = (u16)((buffer[4] << 8) & 0xF00 | buffer[3]);*/
}

void Joycons::SetImuCal(Joycon& jc, std::vector<u8> buffer) {
    for (std::size_t i = 0; i < jc.imu.size(); ++i) {
        jc.imu[i].acc.offset = (u16)(buffer[0 + (i * 2)] | (buffer[1 + (i * 2)] << 8));
        jc.imu[i].acc.scale = (u16)(buffer[0 + 6 + (i * 2)] | (buffer[1 + 6 + (i * 2)] << 8));
        jc.imu[i].gyr.offset = (u16)(buffer[0 + 12 + (i * 2)] | (buffer[1 + 12 + (i * 2)] << 8));
        jc.imu[i].gyr.scale = (u16)(buffer[0 + 18 + (i * 2)] | (buffer[1 + 18 + (i * 2)] << 8));
    }
}

void Joycons::GetUserCalibrationData(Joycon& jc) {
    std::vector<u8> buffer;
    hid_set_nonblocking(jc.handle, 0);

    switch (jc.type) {
    case JoyControllerTypes::Left:
        buffer = ReadSPI(jc.handle, CalAddr::USER_LEFT_DATA, 9);
        SetJoyStickCal(buffer, jc.axis[0], jc.axis[1], true);
        break;
    case JoyControllerTypes::Right:
        buffer = ReadSPI(jc.handle, CalAddr::USER_RIGHT_DATA, 9);
        SetJoyStickCal(buffer, jc.axis[2], jc.axis[3], false);
        break;
    case JoyControllerTypes::Pro:
        buffer = ReadSPI(jc.handle, CalAddr::USER_LEFT_DATA, 9);
        SetJoyStickCal(buffer, jc.axis[0], jc.axis[1], true);
        buffer = ReadSPI(jc.handle, CalAddr::USER_RIGHT_DATA, 9);
        SetJoyStickCal(buffer, jc.axis[2], jc.axis[3], false);
        break;
    case JoyControllerTypes::None:
        break;
    }

    buffer = ReadSPI(jc.handle, CalAddr::USER_IMU_DATA, 24);
    SetImuCal(jc, buffer);
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::GetFactoryCalibrationData(Joycon& jc) {
    std::vector<u8> buffer;
    hid_set_nonblocking(jc.handle, 0);

    switch (jc.type) {
    case JoyControllerTypes::Left:
        buffer = ReadSPI(jc.handle, CalAddr::FACT_LEFT_DATA, 9);
        SetJoyStickCal(buffer, jc.axis[0], jc.axis[1], true);
        break;
    case JoyControllerTypes::Right:
        buffer = ReadSPI(jc.handle, CalAddr::FACT_RIGHT_DATA, 9);
        SetJoyStickCal(buffer, jc.axis[2], jc.axis[3], false);
        break;
    case JoyControllerTypes::Pro:
        buffer = ReadSPI(jc.handle, CalAddr::FACT_LEFT_DATA, 9);
        SetJoyStickCal(buffer, jc.axis[0], jc.axis[1], true);
        buffer = ReadSPI(jc.handle, CalAddr::FACT_RIGHT_DATA, 9);
        SetJoyStickCal(buffer, jc.axis[2], jc.axis[3], false);
        break;
    case JoyControllerTypes::None:
        break;
    }

    buffer = ReadSPI(jc.handle, CalAddr::FACT_IMU_DATA, 24);
    SetImuCal(jc, buffer);
    hid_set_nonblocking(jc.handle, 1);
}

s16 Joycons::GetRawIMUValues(std::size_t sensor, size_t axis, std::vector<u8> buffer) {
    const size_t offset = (sensor * 6) + (axis * 2);
    return static_cast<s16>(buffer[13 + offset] | (buffer[14 + offset] << 8));
}

f32 Joycons::TransformAccValue(s16 raw, ImuData cal, AccSensitivity sen) {
    // const f32 value = (raw - cal.offset) * cal.scale / 65535.0f / 1000.0f;
    const f32 value = raw * (1.0f / (cal.scale - cal.offset)) * 4;
    switch (sen) {
    case AccSensitivity::G2:
        return value / 4.0f;
    case AccSensitivity::G4:
        return value / 2.0f;
    case AccSensitivity::G8:
        return value;
    case AccSensitivity::G16:
        return value * 2.0f;
    }
    return value;
}

f32 Joycons::TransformGyrValue(s16 raw, ImuData cal, GyrSensitivity sen) {
    // const f32 value = (raw - cal.offset) * cal.scale / 65535.0f / 360.0f / 3.8f;
    const f32 value = (raw - cal.offset) * (936.0f / (cal.scale - cal.offset)) / 360.0f;
    switch (sen) {
    case GyrSensitivity::DPS250:
        return value / 8.0f;
    case GyrSensitivity::DPS500:
        return value / 4.0f;
    case GyrSensitivity::DPS1000:
        return value / 2.0f;
    case GyrSensitivity::DPS2000:
        return value;
    }
    return value;
}

void Joycons::GetIMUValues(Joycon& jc, std::vector<u8> buffer) {
    for (std::size_t i = 0; i < jc.imu.size(); ++i) {
        jc.imu[i].gyr.value = 0;
        jc.imu[i].acc.value = 0;
    }
    for (std::size_t i = 0; i < jc.imu.size(); ++i) {
        for (std::size_t sample = 0; sample < 3; ++sample) {
            const s16 raw_gyr = GetRawIMUValues((sample * 2) + 1, i, buffer);
            const s16 raw_acc = GetRawIMUValues(sample * 2, i, buffer);
            switch (i) {
            case 0:
                jc.gyro[sample].y = TransformGyrValue(raw_gyr, jc.imu[i].gyr, jc.gsen);
                jc.imu[1].gyr.value += TransformGyrValue(raw_gyr, jc.imu[i].gyr, jc.gsen) * 0.3333f;
                jc.imu[1].acc.value += TransformAccValue(raw_acc, jc.imu[i].acc, jc.asen) * 0.3333f;
                break;
            case 1:
                jc.gyro[sample].x = TransformGyrValue(raw_gyr, jc.imu[i].gyr, jc.gsen);
                jc.imu[0].gyr.value += TransformGyrValue(raw_gyr, jc.imu[i].gyr, jc.gsen) * 0.3333f;
                jc.imu[0].acc.value += TransformAccValue(raw_acc, jc.imu[i].acc, jc.asen) * 0.3333f;
                break;
            case 2:
                jc.gyro[sample].z = TransformGyrValue(raw_gyr, jc.imu[i].gyr, jc.gsen);
                jc.imu[2].gyr.value += TransformGyrValue(raw_gyr, jc.imu[i].gyr, jc.gsen) * 0.3333f;
                jc.imu[2].acc.value += TransformAccValue(raw_acc, jc.imu[i].acc, jc.asen) * 0.3333f;
                break;
            }
        }
    }
}

void Joycons::SetImuConfig(Joycon& jc, GyrSensitivity gsen, GyrPerformance gfrec,
                           AccSensitivity asen, AccPerformance afrec) {
    hid_set_nonblocking(jc.handle, 0);
    jc.gsen = gsen;
    jc.gfrec = gfrec;
    jc.asen = asen;
    jc.afrec = afrec;
    const std::vector<u8> buffer{static_cast<u8>(gsen), static_cast<u8>(asen),
                                 static_cast<u8>(gfrec), static_cast<u8>(afrec)};
    SubCommand(jc.handle, SubComamnd::SET_IMU_SENSITIVITY, buffer, 4);
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::GetLeftPadInput(Joycon& jc, std::vector<u8> buffer) {
    jc.button = static_cast<u32>(buffer[5]);
    jc.button |= static_cast<u32>((buffer[4] & 0b00101001) << 16);
    jc.axis[0].value = static_cast<u16>(buffer[6] | ((buffer[7] & 0xf) << 8));
    jc.axis[1].value = static_cast<u16>((buffer[7] >> 4) | (buffer[8] << 4));
}

void Joycons::GetRightPadInput(Joycon& jc, std::vector<u8> buffer) {
    jc.button = static_cast<u32>(buffer[3] << 8);
    jc.button |= static_cast<u32>((buffer[4] & 0b00010110) << 16);
    jc.axis[2].value = static_cast<u16>(buffer[9] | ((buffer[10] & 0xf) << 8));
    jc.axis[3].value = static_cast<u16>((buffer[10] >> 4) | (buffer[11] << 4));
}

void Joycons::GetProPadInput(Joycon& jc, std::vector<u8> buffer) {
    jc.button = static_cast<u32>(buffer[5] & 0b11001111);
    jc.button |= static_cast<u32>((buffer[3] & 0b11001111) << 8);
    jc.button |= static_cast<u32>((buffer[4] & 0b00111111) << 16);
    jc.axis[0].value = static_cast<u16>(buffer[6] | ((buffer[7] & 0xf) << 8));
    jc.axis[1].value = static_cast<u16>((buffer[7] >> 4) | (buffer[8] << 4));
    jc.axis[2].value = static_cast<u16>(buffer[9] | ((buffer[10] & 0xf) << 8));
    jc.axis[3].value = static_cast<u16>((buffer[10] >> 4) | (buffer[11] << 4));
}

void Joycons::SetRumble(std::size_t port, f32 amp_high, f32 amp_low, f32 freq_high, f32 freq_low) {
    if (!DeviceConnected(port) || !joycon[port].rumble_enabled) {
        return;
    }
    freq_high = std::clamp(freq_high, 81.75177f, 1252.572266f);
    freq_low = std::clamp(freq_low, 40.875885f, 626.286133f);
    amp_high = std::clamp(amp_high, 0.0f, 1.0f);
    amp_low = std::clamp(amp_low, 0.0f, 1.0f);
    if (freq_high != joycon[port].hd_rumble.freq_high ||
        freq_low != joycon[port].hd_rumble.freq_low ||
        amp_high != joycon[port].hd_rumble.amp_high || amp_low != joycon[port].hd_rumble.amp_low) {
        joycon[port].hd_rumble.freq_high = freq_high;
        joycon[port].hd_rumble.freq_low = freq_low;
        joycon[port].hd_rumble.amp_high = amp_high;
        joycon[port].hd_rumble.amp_low = amp_low;
        SendRumble(port);
    }
}

void Joycons::SendRumble(std::size_t port) {
    std::vector<u8> buffer(max_resp_size);

    buffer[port] = static_cast<u8>(Output::RUMBLE_ONLY);
    buffer[1] = GetCounter();
    for (int i = 0; i < 8; ++i) {
        buffer[2 + i] = default_buffer[i];
    }

    if (joycon[port].hd_rumble.amp_low > 0 || joycon[port].hd_rumble.amp_high > 0) {
        u16 encoded_hf =
            static_cast<u16>(roundf(128 * log2f(joycon[port].hd_rumble.freq_high * 0.1f)) - 0x180);
        u8 encoded_lf =
            static_cast<u8>(roundf(32 * log2f(joycon[port].hd_rumble.freq_low * 0.1f)) - 0x40);
        u8 encoded_hamp =
            static_cast<u8>(EncodeRumbleAmplification(joycon[port].hd_rumble.amp_high));
        u8 encoded_lamp =
            static_cast<u8>(EncodeRumbleAmplification(joycon[port].hd_rumble.amp_low));

        for (u8 i = 0; i < 2; ++i) {
            const u8 amplitude = i == 0 ? encoded_lamp : encoded_hamp;
            const u8 offset = i * 4;
            u32 encoded_amp = amplitude >> 1;
            const u32 parity = (encoded_amp % 2) * 0x80;
            encoded_amp >>= 1;
            encoded_amp += 0x40;

            buffer[2 + offset] = static_cast<u8>(encoded_hf & 0xff);
            buffer[3 + offset] = static_cast<u8>((encoded_hf >> 8) & 0xff);
            buffer[4 + offset] = static_cast<u8>(encoded_lf & 0xff);

            buffer[3 + offset] |= static_cast<u8>(amplitude);
            buffer[4 + offset] |= static_cast<u8>(parity);
            buffer[5 + offset] = static_cast<u8>(encoded_amp);
        }
    }

    SendWrite(joycon[port].handle, buffer, max_resp_size);
}

const f32 Joycons::EncodeRumbleAmplification(f32 amplification) {
    if (amplification < 0.01182) {
        return roundf(pow(amplification, 1.7f) * 7561);
    } else if (amplification < 0.11249) {
        return roundf((log(amplification) * 11.556f) + 55.3f);
    } else if (amplification < 0.22498) {
        return roundf((log2f(amplification) * 32) + 131);
    }

    return roundf((log2f(amplification) * 64) + 200);
}

const f32 Joycons::GetTemperatureCelcius(std::size_t port) {
    if (!DeviceConnected(port)) {
        return 0.0f;
    }
    return 25.0f + joycon[port].temperature * 0.0625f;
}

const f32 Joycons::GetTemperatureFahrenheit(std::size_t port) {
    if (!DeviceConnected(port)) {
        return 0.0f;
    }
    return GetTemperatureCelcius(port) * 1.8f + 32;
}

const u8 Joycons::GetBatteryLevel(std::size_t port) {
    if (!DeviceConnected(port)) {
        return 0x0;
    }
    return joycon[port].battery;
}

const std::array<u8, 15> Joycons::GetSerialNumber(std::size_t port) {
    if (!DeviceConnected(port)) {
        return {};
    }
    return joycon[port].serial_number;
}

const f32 Joycons::GetVersion(std::size_t port) {
    if (!DeviceConnected(port)) {
        return 0x0;
    }
    return joycon[port].version;
}

const JoyControllerTypes Joycons::GetDeviceType(std::size_t port) {
    if (!DeviceConnected(port)) {
        return JoyControllerTypes::None;
    }
    return joycon[port].type;
}

const std::array<u8, 6> Joycons::GetMac(std::size_t port) {
    if (!DeviceConnected(port)) {
        return {};
    }
    return joycon[port].mac;
}

const u32 Joycons::GetBodyColor(std::size_t port) {
    if (!DeviceConnected(port)) {
        return 0x0;
    }
    return joycon[port].color.body;
}

const u32 Joycons::GetButtonColor(std::size_t port) {
    if (!DeviceConnected(port)) {
        return 0x0;
    }
    return joycon[port].color.buttons;
}

const u32 Joycons::GetLeftGripColor(std::size_t port) {
    if (!DeviceConnected(port)) {
        return 0x0;
    }
    return joycon[port].color.left_grip;
}
const u32 Joycons::GetRightGripColor(std::size_t port) {
    if (!DeviceConnected(port)) {
        return 0x0;
    }
    return joycon[port].color.right_grip;
}

void Joycons::SetSerialNumber(Joycon& jc) {
    std::vector<u8> buffer;
    hid_set_nonblocking(jc.handle, 0);
    buffer = ReadSPI(jc.handle, CalAddr::SERIAL_NUMBER, 16);
    for (int i = 0; i < 15; ++i) {
        jc.serial_number[i] = buffer[i + 1];
    }
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::SetDeviceType(Joycon& jc) {
    std::vector<u8> buffer;
    hid_set_nonblocking(jc.handle, 0);
    buffer = ReadSPI(jc.handle, CalAddr::DEVICE_TYPE, 1);
    jc.type = static_cast<JoyControllerTypes>(buffer[0]);
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::GetColor(Joycon& jc) {
    std::vector<u8> buffer;
    hid_set_nonblocking(jc.handle, 0);
    buffer = ReadSPI(jc.handle, CalAddr::COLOR_DATA, 12);
    jc.color.body = (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
    jc.color.buttons = (buffer[5] << 16) | (buffer[4] << 8) | buffer[3];
    jc.color.left_grip = (buffer[8] << 16) | (buffer[7] << 8) | buffer[6];
    jc.color.right_grip = (buffer[11] << 16) | (buffer[10] << 8) | buffer[9];
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::SetMac(Joycon& jc) {
    wchar_t mac[255];
    hid_get_serial_number_string(jc.handle, mac, std::size(mac));
    for (int i = 0; i < 6; ++i) {
        wchar_t value[3] = {mac[i * 2], mac[(i * 2) + 1]};
        jc.mac[i] = static_cast<u8>(std::stoi(value, 0, 16));
    }
}

void Joycons::SetVersionNumber(Joycon& jc) {
    jc.version = 0.0f;
}

void Joycons::SetLedConfig(Joycon& jc, u8 leds) {
    hid_set_nonblocking(jc.handle, 0);
    jc.leds = leds;
    const std::vector<u8> buffer{leds};
    SubCommand(jc.handle, SubComamnd::SET_PLAYER_LIGHTS, buffer, 1);
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::EnableImu(Joycon& jc, bool enable) {
    hid_set_nonblocking(jc.handle, 0);
    jc.imu_enabled = enable;
    const std::vector<u8> buffer{static_cast<u8>(enable ? 1 : 0)};
    SubCommand(jc.handle, SubComamnd::ENABLE_IMU, buffer, 1);
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::EnableRumble(Joycon& jc, bool enable) {
    hid_set_nonblocking(jc.handle, 0);
    jc.rumble_enabled = enable;
    const std::vector<u8> buffer{static_cast<u8>(enable ? 1 : 0)};
    SubCommand(jc.handle, SubComamnd::ENABLE_VIBRATION, buffer, 1);
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::SetReportMode(Joycon& jc, ReportMode mode) {
    hid_set_nonblocking(jc.handle, 0);
    jc.mode = mode;
    const std::vector<u8> buffer{static_cast<u8>(mode)};
    SubCommand(jc.handle, SubComamnd::SET_REPORT_MODE, buffer, 1);
    hid_set_nonblocking(jc.handle, 1);
}

void Joycons::UpdateJoyconData(Joycon& jc, std::vector<u8> buffer) {
    switch (jc.type) {
    case JoyControllerTypes::Left:
        GetLeftPadInput(jc, buffer);
        if (jc.imu_enabled) {
            GetIMUValues(jc, buffer);
            jc.imu[1].acc.value = -jc.imu[1].acc.value;
            jc.imu[0].gyr.value = -jc.imu[0].gyr.value;
            jc.gyro[0].x = -jc.gyro[0].x;
            jc.gyro[1].x = -jc.gyro[1].x;
            jc.gyro[2].x = -jc.gyro[2].x;

            jc.imu[2].acc.value = -jc.imu[2].acc.value;
        }
        break;
    case JoyControllerTypes::Right:
        GetRightPadInput(jc, buffer);
        if (jc.imu_enabled) {
            GetIMUValues(jc, buffer);
            jc.imu[0].acc.value = -jc.imu[0].acc.value;
            jc.imu[1].acc.value = -jc.imu[1].acc.value;
            jc.imu[2].gyr.value = -jc.imu[2].gyr.value;
            jc.gyro[0].z = -jc.gyro[0].z;
            jc.gyro[1].z = -jc.gyro[1].z;
            jc.gyro[2].z = -jc.gyro[2].z;
        }
        break;
    case JoyControllerTypes::Pro:
        GetProPadInput(jc, buffer);
        if (jc.imu_enabled) {
            GetIMUValues(jc, buffer);
            jc.imu[1].acc.value = -jc.imu[1].acc.value;
            jc.imu[2].acc.value = -jc.imu[2].acc.value;
        }
        break;
    case JoyControllerTypes::None:
        break;
    }
    const auto now = std::chrono::system_clock::now();
    u64 difference =
        std::chrono::duration_cast<std::chrono::microseconds>(now - jc.last_motion_update).count();
    jc.last_motion_update = now;
    Common::Vec3f acceleration =
        Common::Vec3f(jc.imu[0].acc.value, jc.imu[1].acc.value, jc.imu[2].acc.value);
    jc.motion->SetAcceleration(acceleration);
    for (int i = 0; i < 3; ++i) {
        jc.motion->SetGyroscope(jc.gyro[i]);
        jc.motion->UpdateRotation(difference / 3);
        jc.motion->UpdateOrientation(difference / 3);
    }
    jc.battery = buffer[2] >> 4;
}

void Joycons::UpdateYuzuSettings(Joycon& jc, std::size_t port) {
    if (DeviceConnected(port) && configuring) {
        JCPadStatus pad;
        if (jc.button != 0) {
            pad.button = jc.button;
            pad_queue[port].Push(pad);
        }
        for (std::size_t i = 0; i < jc.axis.size(); ++i) {
            const u16 value = jc.axis[i].value;
            const u16 origin = jc.axis[i].center;
            if (value != 0) {
                if (value > origin + 500 || value < origin - 500) {
                    pad.axis = static_cast<PadAxes>(i);
                    pad.axis_value = value - origin;
                    pad_queue[port].Push(pad);
                }
            }
        }
        for (std::size_t i = 0; i < jc.imu.size(); ++i) {
            const f32 value = jc.imu[i].gyr.value;
            const f32 value2 = jc.imu[i].acc.value;
            if (value > 6.0f || value < -6.0f) {
                pad.motion = static_cast<PadMotion>(i);
                pad.motion_value = value;
                pad_queue[port].Push(pad);
            }
            if (value2 > 2.0f || value2 < -2.0f) {
                pad.motion = static_cast<PadMotion>(i + 3);
                pad.motion_value = value;
                pad_queue[port].Push(pad);
            }
        }
    }
}

void Joycons::JoyconToState(Joycon& jc, JCState& state) {
    for (const auto& button : PadButtonArray) {
        const u32 button_value = static_cast<u32>(button);
        state.buttons.insert_or_assign(button_value, jc.button & button_value);
    }

    for (std::size_t i = 0; i < jc.axis.size(); ++i) {
        f32 axis_value = static_cast<f32>(jc.axis[i].value - jc.axis[i].center);
        if (axis_value > 0) {
            axis_value = axis_value / jc.axis[i].max;
        } else {
            axis_value = axis_value / jc.axis[i].min;
        }
        axis_value = std::clamp(axis_value, -1.0f, 1.0f);
        state.axes.insert_or_assign(static_cast<u16>(i), axis_value);
    }

    Common::Vec3f gyroscope = jc.motion->GetGyroscope();
    Common::Vec3f accelerometer = jc.motion->GetAcceleration();
    Common::Vec3f rotation = jc.motion->GetRotations();
    std::array<Common::Vec3f, 3> orientation = jc.motion->GetOrientation();

    state.motion.insert_or_assign(static_cast<u16>(0), gyroscope.x);
    state.motion.insert_or_assign(static_cast<u16>(1), gyroscope.y);
    state.motion.insert_or_assign(static_cast<u16>(2), gyroscope.z);

    state.motion.insert_or_assign(static_cast<u16>(3), accelerometer.x);
    state.motion.insert_or_assign(static_cast<u16>(4), accelerometer.y);
    state.motion.insert_or_assign(static_cast<u16>(5), accelerometer.z);

    state.motion.insert_or_assign(static_cast<u16>(6), rotation.x);
    state.motion.insert_or_assign(static_cast<u16>(7), rotation.y);
    state.motion.insert_or_assign(static_cast<u16>(8), rotation.z);

    state.motion.insert_or_assign(static_cast<u16>(9), orientation[0].x);
    state.motion.insert_or_assign(static_cast<u16>(10), orientation[0].y);
    state.motion.insert_or_assign(static_cast<u16>(11), orientation[0].z);
    state.motion.insert_or_assign(static_cast<u16>(12), orientation[1].x);
    state.motion.insert_or_assign(static_cast<u16>(13), orientation[1].y);
    state.motion.insert_or_assign(static_cast<u16>(14), orientation[1].z);
    state.motion.insert_or_assign(static_cast<u16>(15), orientation[2].x);
    state.motion.insert_or_assign(static_cast<u16>(16), orientation[2].y);
    state.motion.insert_or_assign(static_cast<u16>(17), orientation[2].z);

    if (jc.button & 0x1 || jc.button & 0x100) {
        jc.motion->ResetRotations();
        jc.motion->SetQuaternion({{0, 0, -1}, 0});
    }
}

void Joycons::ReadLoop() {
    LOG_DEBUG(Input, "JC Adapter Read() thread started");

    std::vector<u8> buffer(max_resp_size);

    while (adapter_thread_running) {
        for (std::size_t port = 0; port < joycon.size(); ++port) {
            if (joycon[port].type != JoyControllerTypes::None) {

                const int status =
                    hid_read_timeout(joycon[port].handle, buffer.data(), max_resp_size, 15);
                if (status > 0) {
                    if (buffer[0] == 0x00) {
                        // Invalid response
                        LOG_ERROR(Input, "error reading buffer");
                        adapter_thread_running = false;
                        joycon[port].type = JoyControllerTypes::None;
                        break;
                    }
                    UpdateJoyconData(joycon[port], buffer);
                    UpdateYuzuSettings(joycon[port], port);
                    JoyconToState(joycon[port], joycon[port].state);
                    /*
                    printf("ctrl:%d\tbtn:%d\tx1:%d\ty1:%d\tx2:%d\ty2:%d\tax:%.3f\tay:%.3f\taz:%.3f\tgx:%.3f\tgy:%.3f,
                    " "gz:%.3f\tbuffer:",port, joycon[port].button, joycon[port].axis[0].value -
                    joycon[port].axis[0].center, joycon[port].axis[1].value -
                    joycon[port].axis[1].center, joycon[port].axis[2].value -
                    joycon[port].axis[2].center, joycon[port].axis[3].value -
                    joycon[port].axis[3].center, joycon[port].imu[0].acc.value,
                           joycon[port].imu[1].acc.value, joycon[port].imu[2].acc.value,
                           joycon[port].imu[0].gyr.value, joycon[port].imu[1].gyr.value,
                           joycon[port].imu[2].gyr.value);
                    for (int i = 0; i < 7; ++i) {
                        printf("%02hx ", buffer[i]);
                    }
                    printf("\n");*/
                }
            }
        }
        std::this_thread::yield();
    }
}

void Joycons::Setup() {
    // Initialize all controllers as unplugged
    for (std::size_t port = 0; port < joycon.size(); ++port) {
        joycon[port].type = JoyControllerTypes::None;
        for (std::size_t i = 0; i < joycon[port].axis.size(); ++i) {
            joycon[port].axis[i].value = 0;
        }
        for (std::size_t i = 0; i < joycon[port].imu.size(); ++i) {
            joycon[port].imu[i].acc.value = 0;
            joycon[port].imu[i].gyr.value = 0;
        }
        joycon[port].hd_rumble.amp_high = 0;
        joycon[port].hd_rumble.amp_low = 0;
        joycon[port].hd_rumble.freq_high = 160.0f;
        joycon[port].hd_rumble.freq_low = 80.0f;
    }
    std::size_t port = 0;

    hid_device_info* devs = hid_enumerate(0x057e, 0x2006);
    hid_device_info* cur_dev = devs;

    while (cur_dev) {
        printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
               cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
        printf("\n");
        printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
        printf("  Product:      %ls\n", cur_dev->product_string);
        printf("  Release:      %hx\n", cur_dev->release_number);
        printf("  Interface:    %d\n", cur_dev->interface_number);
        printf("  Usage (page): 0x%hx (0x%hx)\n", cur_dev->usage, cur_dev->usage_page);
        printf("\n");

        if (CheckDeviceAccess(port, cur_dev)) {
            // JC Adapter found and accessible, registering it
            ++port;
        }
        cur_dev = cur_dev->next;
    }

    devs = hid_enumerate(0x057e, 0x2007);
    cur_dev = devs;

    while (cur_dev) {
        printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
               cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
        printf("\n");
        printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
        printf("  Product:      %ls\n", cur_dev->product_string);
        printf("  Release:      %hx\n", cur_dev->release_number);
        printf("  Interface:    %d\n", cur_dev->interface_number);
        printf("  Usage (page): 0x%hx (0x%hx)\n", cur_dev->usage, cur_dev->usage_page);
        printf("\n");

        if (CheckDeviceAccess(port, cur_dev)) {
            // JC Adapter found and accessible, registering it
            ++port;
        }
        cur_dev = cur_dev->next;
    } //*/
    /*
    devs = hid_enumerate(0x057e, 0x2009);
    cur_dev = devs;

    while (cur_dev) {
        printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
               cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
        printf("\n");
        printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
        printf("  Product:      %ls\n", cur_dev->product_string);
        printf("  Release:      %hx\n", cur_dev->release_number);
        printf("  Interface:    %d\n", cur_dev->interface_number);
        printf("  Usage (page): 0x%hx (0x%hx)\n", cur_dev->usage, cur_dev->usage_page);
        printf("\n");

        if (CheckDeviceAccess(port, cur_dev)) {
            // JC Adapter found and accessible, registering it
            ++port;
        }
        cur_dev = cur_dev->next;
    }
    */
    GetJCEndpoint();
}

bool Joycons::CheckDeviceAccess(std::size_t port, hid_device_info* device) {
    if (device->vendor_id != 0x057e ||
        !(device->product_id == 0x2006 || device->product_id == 0x2007 ||
          device->product_id == 0x2009)) {
        // This isn't the device we are looking for.
        return false;
    }
    joycon[port].handle = hid_open(device->vendor_id, device->product_id, device->serial_number);
    if (!joycon[port].handle) {
        LOG_ERROR(Input, "Yuzu can not gain access to this device: ID {:04X}:{:04X}.",
                  device->vendor_id, device->product_id);
        return false;
    }
    hid_set_nonblocking(joycon[port].handle, 1);
    switch (device->product_id) {
    case 0x2006:
        joycon[port].type = JoyControllerTypes::Left;
        break;
    case 0x2007:
        joycon[port].type = JoyControllerTypes::Right;
        break;
    case 0x2009:
        joycon[port].type = JoyControllerTypes::Pro;
        break;
    }
    return true;
}

void Joycons::GetJCEndpoint() {
    // init joycons
    for (u8 port = 0; port < joycon.size(); ++port) {
        joycon[port].port = port;
        if (joycon[port].type != JoyControllerTypes::None) {
            SetMac(joycon[port]);
            SetSerialNumber(joycon[port]);
            SetVersionNumber(joycon[port]);
            SetDeviceType(joycon[port]);
            GetFactoryCalibrationData(joycon[port]);
            GetColor(joycon[port]);

            SetLedConfig(joycon[port], port + 1);
            EnableImu(joycon[port], true);
            SetImuConfig(joycon[port], GyrSensitivity::DPS2000, GyrPerformance::HZ833,
                         AccSensitivity::G8, AccPerformance::HZ100);
            EnableRumble(joycon[port], true);
            SetReportMode(joycon[port], ReportMode::STANDARD_FULL_60HZ);
            joycon[port].motion = new InputCommon::MotionInput(0.3f, 0.005f, 0.0f);
            joycon[port].motion->SetGyroThreshold(0.001f);
            // joycon[port].motion->EnableReset(false);
        }
    }

    adapter_thread_running = true;
    adapter_input_thread = std::thread(&Joycons::ReadLoop, this); // Read input
}

Joycons::~Joycons() {
    Reset();
}

void Joycons::Reset() {
    if (adapter_thread_running) {
        adapter_thread_running = false;
    }
    if (adapter_input_thread.joinable()) {
        adapter_input_thread.join();
    }

    for (u8 port = 0; port < joycon.size(); ++port) {
        joycon[port].type = JoyControllerTypes::None;
        if (joycon[port].handle) {
            joycon[port].handle = nullptr;
        }
    }

    hid_exit();
}
std::string Joycons::JoyconName(std::size_t port) const {
    switch (joycon[port].type) {
    case JoyControllerTypes::Left:
        return "Left Joycon";
        break;
    case JoyControllerTypes::Right:
        return "Right Joycon";
        break;
    case JoyControllerTypes::Pro:
        return "Pro Controller";
        break;
    case JoyControllerTypes::None:
        return "Unknow Joycon";
        break;
    }
    return "Unknow Joycon";
}

std::vector<Common::ParamPackage> Joycons::GetInputDevices() const {
    // std::scoped_lock lock(joystick_map_mutex);
    std::vector<Common::ParamPackage> devices;
    for (const auto& controller : joycon) {
        if (DeviceConnected(controller.port)) {
            std::string name = fmt::format("{} {}", JoyconName(controller.port), controller.port);
            devices.emplace_back(Common::ParamPackage{
                {"class", "jcpad"},
                {"display", std::move(name)},
                {"port", std::to_string(controller.port)},
            });
        }
    }
    return devices;
}

InputCommon::ButtonMapping Joycons::GetButtonMappingForDevice(const Common::ParamPackage& params) {
    // This list is missing ZL/ZR since those are not considered buttons in SDL GameController.
    // We will add those afterwards
    // This list also excludes Screenshot since theres not really a mapping for that
    static constexpr std::array<std::pair<Settings::NativeButton::Values, PadButton>, 20>
        switch_to_jcadapter_button = {
            std::pair{Settings::NativeButton::A, PadButton::BUTTON_B},
            {Settings::NativeButton::B, PadButton::BUTTON_A},
            {Settings::NativeButton::X, PadButton::BUTTON_Y},
            {Settings::NativeButton::Y, PadButton::BUTTON_X},
            {Settings::NativeButton::LStick, PadButton::STICK_L},
            {Settings::NativeButton::RStick, PadButton::STICK_R},
            {Settings::NativeButton::L, PadButton::TRIGGER_L},
            {Settings::NativeButton::R, PadButton::TRIGGER_R},
            {Settings::NativeButton::Plus, PadButton::BUTTON_PLUS},
            {Settings::NativeButton::Minus, PadButton::BUTTON_MINUS},
            {Settings::NativeButton::DLeft, PadButton::BUTTON_LEFT},
            {Settings::NativeButton::DUp, PadButton::BUTTON_UP},
            {Settings::NativeButton::DRight, PadButton::BUTTON_RIGHT},
            {Settings::NativeButton::DDown, PadButton::BUTTON_DOWN},
            {Settings::NativeButton::SL, PadButton::BUTTON_L_SL},
            {Settings::NativeButton::SR, PadButton::BUTTON_L_SR},
            {Settings::NativeButton::Screenshot, PadButton::BUTTON_CAPTURE},
            {Settings::NativeButton::Home, PadButton::BUTTON_HOME},
            {Settings::NativeButton::ZL, PadButton::TRIGGER_ZL},
            {Settings::NativeButton::ZR, PadButton::TRIGGER_ZR},
        };
    if (!params.Has("port")) {
        return {};
    }

    InputCommon::ButtonMapping mapping{};
    for (const auto& [switch_button, jcadapter_button] : switch_to_jcadapter_button) {
        Common::ParamPackage button_params({{"engine", "jcpad"}});
        button_params.Set("port", params.Get("port", 0));
        button_params.Set("button", static_cast<int>(jcadapter_button));
        mapping[switch_button] = button_params;
    }

    return mapping;
}

InputCommon::AnalogMapping Joycons::GetAnalogMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("port")) {
        return {};
    }

    InputCommon::AnalogMapping mapping = {};
    Common::ParamPackage left_analog_params;
    left_analog_params.Set("engine", "jcpad");
    left_analog_params.Set("port", params.Get("port", 0));
    left_analog_params.Set("axis_x", 0);
    left_analog_params.Set("axis_y", 1);
    mapping[Settings::NativeAnalog::LStick] = left_analog_params;
    Common::ParamPackage right_analog_params;
    right_analog_params.Set("engine", "jcpad");
    right_analog_params.Set("port", params.Get("port", 0));
    right_analog_params.Set("axis_x", 2);
    right_analog_params.Set("axis_y", 3);
    mapping[Settings::NativeAnalog::RStick] = right_analog_params;
    return mapping;
}

bool Joycons::DeviceConnected(std::size_t port) const {
    if (port > 4) {
        return false;
    }
    return joycon[port].type != JoyControllerTypes::None;
}

void Joycons::ResetDeviceType(std::size_t port) {
    if (port > 4) {
        return;
    }
    joycon[port].type = JoyControllerTypes::None;
}

void Joycons::BeginConfiguration() {
    for (auto& pq : pad_queue) {
        pq.Clear();
    }
    configuring = true;
}

void Joycons::EndConfiguration() {
    for (auto& pq : pad_queue) {
        pq.Clear();
    }
    configuring = false;
}

JCState& Joycons::GetPadState(std::size_t port) {
    return joycon[port].state;
}

const JCState& Joycons::GetPadState(std::size_t port) const {
    return joycon[port].state;
}

std::array<Common::SPSCQueue<JCPadStatus>, 4>& Joycons::GetPadQueue() {
    return pad_queue;
}

const std::array<Common::SPSCQueue<JCPadStatus>, 4>& Joycons::GetPadQueue() const {
    return pad_queue;
}
} // namespace JCAdapter
