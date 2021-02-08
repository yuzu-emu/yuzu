// Copyright 2020 Yuzu Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <hidapi.h>
#include "common/param_package.h"
#include "common/threadsafe_queue.h"
#include "common/vector_math.h"
#include "input_common/main.h"
#include "input_common/motion_input.h"
#include "input_common/settings.h"

namespace JCAdapter {

enum class PadButton {
    BUTTON_DOWN = 0x000001,
    BUTTON_UP = 0x000002,
    BUTTON_RIGHT = 0x000004,
    BUTTON_LEFT = 0x000008,
    BUTTON_L_SR = 0x000010,
    BUTTON_L_SL = 0x000020,
    TRIGGER_L = 0x000040,
    TRIGGER_ZL = 0x000080,
    BUTTON_Y = 0x000100,
    BUTTON_X = 0x000200,
    BUTTON_B = 0x000400,
    BUTTON_A = 0x000800,
    BUTTON_R_SL = 0x001000,
    BUTTON_R_SR = 0x002000,
    TRIGGER_R = 0x004000,
    TRIGGER_ZR = 0x008000,
    BUTTON_MINUS = 0x010000,
    BUTTON_PLUS = 0x020000,
    STICK_R = 0x040000,
    STICK_L = 0x080000,
    BUTTON_HOME = 0x100000,
    BUTTON_CAPTURE = 0x200000,
    // Below is for compatibility with "AxisButton" and "MotionButton" type
    STICK = 0x400000,
    MOTION = 0x800000,
};

enum class Output {
    RUMBLE_AND_SUBCMD = 0x01,
    FW_UPDATE_PKT = 0x03,
    RUMBLE_ONLY = 0x10,
    MCU_DATA = 0x11,
    USB_CMD = 0x80,
};

enum class SubComamnd {
    STATE = 0x00,
    MANUAL_BT_PAIRING = 0x01,
    REQ_DEV_INFO = 0x02,
    SET_REPORT_MODE = 0x03,
    TRIGGERS_ELAPSED = 0x04,
    GET_PAGE_LIST_STATE = 0x05,
    SET_HCI_STATE = 0x06,
    RESET_PAIRING_INFO = 0x07,
    LOW_POWER_MODE = 0x08,
    SPI_FLASH_READ = 0x10,
    SPI_FLASH_WRITE = 0x11,
    RESET_MCU = 0x20,
    SET_MCU_CONFIG = 0x21,
    SET_MCU_STATE = 0x22,
    SET_PLAYER_LIGHTS = 0x30,
    GET_PLAYER_LIGHTS = 0x31,
    SET_HOME_LIGHT = 0x38,
    ENABLE_IMU = 0x40,
    SET_IMU_SENSITIVITY = 0x41,
    WRITE_IMU_REG = 0x42,
    READ_IMU_REG = 0x43,
    ENABLE_VIBRATION = 0x48,
    GET_REGULATED_VOLTAGE = 0x50,
};

enum class CalAddr {
    USER_LEFT_MAGIC = 0X8010,
    USER_LEFT_DATA = 0X8012,
    USER_RIGHT_MAGIC = 0X801B,
    USER_RIGHT_DATA = 0X801D,
    USER_IMU_MAGIC = 0X8026,
    USER_IMU_DATA = 0X8028,
    SERIAL_NUMBER = 0X6000,
    DEVICE_TYPE = 0X6012,
    COLOR_EXIST = 0X601B,
    FACT_LEFT_DATA = 0X603d,
    FACT_RIGHT_DATA = 0X6046,
    COLOR_DATA = 0X6050,
    FACT_IMU_DATA = 0X6020,
};

enum class ReportMode {
    ACTIVE_POLLING_NFC_IR_CAMERA_DATA = 0x00,
    ACTIVE_POLLING_NFC_IR_CAMERA_CONFIGURATION = 0x01,
    ACTIVE_POLLING_NFC_IR_CAMERA_DATA_CONFIGURATION = 0x02,
    ACTIVE_POLLING_IR_CAMERA_DATA = 0x03,
    MCU_UPDATE_STATE = 0x23,
    STANDARD_FULL_60HZ = 0x30,
    NFC_IR_MODE_60HZ = 0x31,
    SIMPLE_HID_MODE = 0x3F,
};

enum class GyrSensitivity {
    DPS250,
    DPS500,
    DPS1000,
    DPS2000,
};

enum class AccSensitivity {
    G8,
    G4,
    G2,
    G16,
};

enum class GyrPerformance {
    HZ833,
    HZ208,
};

enum class AccPerformance {
    HZ200,
    HZ100,
};

enum class PadAxes {
    StickX,
    StickY,
    SubstickX,
    SubstickY,
    Undefined,
};

enum class PadMotion {
    GyrX,
    GyrY,
    GyrZ,
    AccX,
    AccY,
    AccZ,
    Undefined,
};

enum class JoyControllerTypes {
    None,
    Left,
    Right,
    Pro,
};

extern const std::array<PadButton, 22> PadButtonArray;

struct JCPadStatus {
    u32 button{};

    std::array<u16, 4> axis_values{};
    std::array<f32, 6> imu_values{};
    std::array<f32, 9> orientation{};

    u8 port{};
    PadAxes axis{PadAxes::Undefined};
    s16 axis_value{0};
    PadMotion motion{PadMotion::Undefined};
    f32 motion_value{0.0f};
};

struct JCState {
    std::unordered_map<int, bool> buttons;
    std::unordered_map<int, float> axes;
    std::unordered_map<int, float> motion;
};

class Joycons {
public:
    /// Initialize the JC Adapter capture and read sequence
    Joycons();

    /// Close the adapter read thread and release the adapter
    ~Joycons();
    /// Used for polling
    void BeginConfiguration();
    void EndConfiguration();

    std::vector<Common::ParamPackage> GetInputDevices() const;
    InputCommon::ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage& params);
    InputCommon::AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage& params);

    bool DeviceConnected(std::size_t port) const;

    void SetRumble(std::size_t port, f32 amp_high, f32 amp_low, f32 freq_high, f32 freq_low);

    const f32 GetTemperatureCelcius(std::size_t port);
    const f32 GetTemperatureFahrenheit(std::size_t port);
    const u8 GetBatteryLevel(std::size_t port);
    const std::array<u8, 15> GetSerialNumber(std::size_t port);
    const f32 GetVersion(std::size_t port);
    const JoyControllerTypes GetDeviceType(std::size_t port);
    const std::array<u8, 6> GetMac(std::size_t port);
    const u32 GetBodyColor(std::size_t port);
    const u32 GetButtonColor(std::size_t port);
    const u32 GetLeftGripColor(std::size_t port);
    const u32 GetRightGripColor(std::size_t port);

    std::array<Common::SPSCQueue<JCPadStatus>, 4>& GetPadQueue();
    const std::array<Common::SPSCQueue<JCPadStatus>, 4>& GetPadQueue() const;

    JCState& GetPadState(std::size_t port);
    const JCState& GetPadState(std::size_t port) const;

private:
    struct HDRumble {
        f32 freq_high;
        f32 freq_low;
        f32 amp_high;
        f32 amp_low;
    };

    struct ImuData {
        s16 offset;
        s16 scale;
        f32 value;
    };

    struct Imu {
        ImuData acc;
        ImuData gyr;
    };

    struct JoyStick {
        u16 max;
        u16 min;
        u16 center;
        u16 deadzone;
        u16 value;
    };

    struct Color {
        u32 body;
        u32 buttons;
        u32 left_grip;
        u32 right_grip;
    };

    struct Joycon {
        hid_device* handle = nullptr;
        JCState state;

        // Harware config
        GyrSensitivity gsen;
        GyrPerformance gfrec;
        AccSensitivity asen;
        AccPerformance afrec;
        ReportMode mode;
        bool imu_enabled;
        bool rumble_enabled;
        u8 leds;

        // Fixed value info
        f32 version;
        std::array<u8, 6> mac;
        JoyControllerTypes type;
        std::array<u8, 15> serial_number;
        Color color;
        u8 port;

        // Realtime values
        InputCommon::MotionInput* motion = nullptr;

        std::array<Common::Vec3f, 3> gyro;
        std::array<JoyStick, 4> axis;
        std::array<Imu, 3> imu;
        HDRumble hd_rumble;
        u32 button;
        u32 temperature;
        u8 battery;

        std::chrono::time_point<std::chrono::system_clock> last_motion_update;
    };

    // Low level commands for communication
    void SendWrite(hid_device* handle, std::vector<u8> buffer, int size);
    u8 GetCounter();
    std::vector<u8> SubCommand(hid_device* handle, SubComamnd sc, std::vector<u8> buffer, int size);
    std::vector<u8> GetResponse(hid_device* handle, SubComamnd sc);
    std::vector<u8> ReadSPI(hid_device* handle, CalAddr addr, u8 size);

    // Reads calibration values
    void SetJoyStickCal(std::vector<u8> buffer, JoyStick& axis1, JoyStick& axis2, bool left);
    void SetImuCal(Joycon& jc, std::vector<u8> buffer);
    void GetUserCalibrationData(Joycon& jc);
    void GetFactoryCalibrationData(Joycon& jc);

    // Reads buttons and stick values
    void GetLeftPadInput(Joycon& jc, std::vector<u8> buffer);
    void GetRightPadInput(Joycon& jc, std::vector<u8> buffer);
    void GetProPadInput(Joycon& jc, std::vector<u8> buffer);

    // Reads gyro and accelerometer values
    f32 TransformAccValue(s16 raw, ImuData cal, AccSensitivity sen);
    f32 TransformGyrValue(s16 raw, ImuData cal, GyrSensitivity sen);
    s16 GetRawIMUValues(size_t sensor, size_t axis, std::vector<u8> buffer);
    void GetIMUValues(Joycon& jc, std::vector<u8> buffer);
    void SetImuConfig(Joycon& jc, GyrSensitivity gsen, GyrPerformance gfrec, AccSensitivity asen,
                      AccPerformance afrec);

    // Sends rumble state
    void SendRumble(std::size_t port);
    const f32 EncodeRumbleAmplification(f32 amplification);

    // Reads color values
    void GetColor(Joycon& jc);

    // Reads device hardware values
    void SetMac(Joycon& jc);
    void SetSerialNumber(Joycon& jc);
    void SetDeviceType(Joycon& jc);
    void SetVersionNumber(Joycon& jc);

    // Write device hardware configuration
    void SetLedConfig(Joycon& jc, u8 leds);
    void EnableImu(Joycon& jc, bool enable);
    void EnableRumble(Joycon& jc, bool enable);
    void SetReportMode(Joycon& jc, ReportMode mode);

    void UpdateJoyconData(Joycon& jc, std::vector<u8> buffer);
    void UpdateYuzuSettings(Joycon& jc, std::size_t port);
    void JoyconToState(Joycon& jc, JCState& state);
    std::string JoyconName(std::size_t port) const;
    void ReadLoop();

    /// Resets status of device connected to port
    void ResetDeviceType(std::size_t port);

    /// Returns true if we successfully gain access to JC Adapter
    bool CheckDeviceAccess(std::size_t port, hid_device_info* device);

    /// Captures JC Adapter endpoint address,
    void GetJCEndpoint();

    /// For shutting down, clear all data, join all threads, release usb
    void Reset();

    /// For use in initialization, querying devices to find the adapter
    void Setup();

    // void UpdateOrientation(Joycon& jc, u64 time, std::size_t iteration);

    std::thread adapter_input_thread;
    bool adapter_thread_running;

    bool configuring = false;

    std::array<Joycon, 4> joycon;
    std::array<Common::SPSCQueue<JCPadStatus>, 4> pad_queue;

    u8 global_counter;
    static constexpr u32 max_resp_size = 49;
    static constexpr std::array<u8, 8> default_buffer{0x0, 0x1, 0x40, 0x40, 0x0, 0x1, 0x40, 0x40};
};

} // namespace JCAdapter
