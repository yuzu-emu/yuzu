// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <mutex>

#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/quaternion.h"
#include "common/settings.h"
#include "core/frontend/input.h"
#include "core/hle/service/hid/controllers/controller_base.h"

namespace Kernel {
class KEvent;
class KReadableEvent;
} // namespace Kernel

namespace Service::KernelHelpers {
class ServiceContext;
}

namespace Service::HID {

constexpr u32 NPAD_HANDHELD = 32;
constexpr u32 NPAD_UNKNOWN = 16; // TODO(ogniK): What is this?

class Controller_NPad final : public ControllerBase {
public:
    explicit Controller_NPad(Core::System& system_,
                             KernelHelpers::ServiceContext& service_context_);
    ~Controller_NPad() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing, u8* data, std::size_t size) override;

    // When the controller is requesting a motion update for the shared memory
    void OnMotionUpdate(const Core::Timing::CoreTiming& core_timing, u8* data,
                        std::size_t size) override;

    // Called when input devices should be loaded
    void OnLoadInputDevices() override;

    enum class NPadControllerType {
        None,
        ProController,
        Handheld,
        JoyDual,
        JoyLeft,
        JoyRight,
        GameCube,
        Pokeball,
    };

    enum class NpadType : u8 {
        ProController = 3,
        Handheld = 4,
        JoyconDual = 5,
        JoyconLeft = 6,
        JoyconRight = 7,
        GameCube = 8,
        Pokeball = 9,
        MaxNpadType = 10,
    };

    enum class DeviceIndex : u8 {
        Left = 0,
        Right = 1,
        None = 2,
        MaxDeviceIndex = 3,
    };

    enum class GyroscopeZeroDriftMode : u32 {
        Loose = 0,
        Standard = 1,
        Tight = 2,
    };

    enum class NpadHoldType : u64 {
        Vertical = 0,
        Horizontal = 1,
    };

    enum class NpadAssignments : u32 {
        Dual = 0,
        Single = 1,
    };

    enum class NpadHandheldActivationMode : u64 {
        Dual = 0,
        Single = 1,
        None = 2,
    };

    enum class NpadCommunicationMode : u64 {
        Mode_5ms = 0,
        Mode_10ms = 1,
        Mode_15ms = 2,
        Default = 3,
    };

    struct DeviceHandle {
        NpadType npad_type;
        u8 npad_id;
        DeviceIndex device_index;
        INSERT_PADDING_BYTES_NOINIT(1);
    };
    static_assert(sizeof(DeviceHandle) == 4, "DeviceHandle is an invalid size");

    struct NpadStyleSet {
        union {
            u32_le raw{};

            BitField<0, 1, u32> fullkey;
            BitField<1, 1, u32> handheld;
            BitField<2, 1, u32> joycon_dual;
            BitField<3, 1, u32> joycon_left;
            BitField<4, 1, u32> joycon_right;
            BitField<5, 1, u32> gamecube;
            BitField<6, 1, u32> palma;
            BitField<7, 1, u32> lark;
            BitField<8, 1, u32> handheld_lark;
            BitField<9, 1, u32> lucia;
            BitField<29, 1, u32> system_ext;
            BitField<30, 1, u32> system;
        };
    };
    static_assert(sizeof(NpadStyleSet) == 4, "NpadStyleSet is an invalid size");

    struct VibrationValue {
        f32 amp_low;
        f32 freq_low;
        f32 amp_high;
        f32 freq_high;
    };
    static_assert(sizeof(VibrationValue) == 0x10, "Vibration is an invalid size");

    static constexpr VibrationValue DEFAULT_VIBRATION_VALUE{
        .amp_low = 0.0f,
        .freq_low = 160.0f,
        .amp_high = 0.0f,
        .freq_high = 320.0f,
    };

    struct LedPattern {
        explicit LedPattern(u64 light1, u64 light2, u64 light3, u64 light4) {
            position1.Assign(light1);
            position2.Assign(light2);
            position3.Assign(light3);
            position4.Assign(light4);
        }
        union {
            u64 raw{};
            BitField<0, 1, u64> position1;
            BitField<1, 1, u64> position2;
            BitField<2, 1, u64> position3;
            BitField<3, 1, u64> position4;
        };
    };

    void SetSupportedStyleSet(NpadStyleSet style_set);
    NpadStyleSet GetSupportedStyleSet() const;

    void SetSupportedNpadIdTypes(u8* data, std::size_t length);
    void GetSupportedNpadIdTypes(u32* data, std::size_t max_length);
    std::size_t GetSupportedNpadIdTypesSize() const;

    void SetHoldType(NpadHoldType joy_hold_type);
    NpadHoldType GetHoldType() const;

    void SetNpadHandheldActivationMode(NpadHandheldActivationMode activation_mode);
    NpadHandheldActivationMode GetNpadHandheldActivationMode() const;

    void SetNpadCommunicationMode(NpadCommunicationMode communication_mode_);
    NpadCommunicationMode GetNpadCommunicationMode() const;

    void SetNpadMode(u32 npad_id, NpadAssignments assignment_mode);

    bool VibrateControllerAtIndex(std::size_t npad_index, std::size_t device_index,
                                  const VibrationValue& vibration_value);

    void VibrateController(const DeviceHandle& vibration_device_handle,
                           const VibrationValue& vibration_value);

    void VibrateControllers(const std::vector<DeviceHandle>& vibration_device_handles,
                            const std::vector<VibrationValue>& vibration_values);

    VibrationValue GetLastVibration(const DeviceHandle& vibration_device_handle) const;

    void InitializeVibrationDevice(const DeviceHandle& vibration_device_handle);

    void InitializeVibrationDeviceAtIndex(std::size_t npad_index, std::size_t device_index);

    void SetPermitVibrationSession(bool permit_vibration_session);

    bool IsVibrationDeviceMounted(const DeviceHandle& vibration_device_handle) const;

    Kernel::KReadableEvent& GetStyleSetChangedEvent(u32 npad_id);
    void SignalStyleSetChangedEvent(u32 npad_id) const;

    // Adds a new controller at an index.
    void AddNewControllerAt(NPadControllerType controller, std::size_t npad_index);
    // Adds a new controller at an index with connection status.
    void UpdateControllerAt(NPadControllerType controller, std::size_t npad_index, bool connected);

    void DisconnectNpad(u32 npad_id);
    void DisconnectNpadAtIndex(std::size_t index);

    void SetGyroscopeZeroDriftMode(GyroscopeZeroDriftMode drift_mode);
    GyroscopeZeroDriftMode GetGyroscopeZeroDriftMode() const;
    bool IsSixAxisSensorAtRest() const;
    void SetSixAxisEnabled(bool six_axis_status);
    void SetSixAxisFusionParameters(f32 parameter1, f32 parameter2);
    std::pair<f32, f32> GetSixAxisFusionParameters();
    void ResetSixAxisFusionParameters();
    LedPattern GetLedPattern(u32 npad_id);
    bool IsUnintendedHomeButtonInputProtectionEnabled(u32 npad_id) const;
    void SetUnintendedHomeButtonInputProtectionEnabled(bool is_protection_enabled, u32 npad_id);
    void SetAnalogStickUseCenterClamp(bool use_center_clamp);
    void ClearAllConnectedControllers();
    void DisconnectAllConnectedControllers();
    void ConnectAllDisconnectedControllers();
    void ClearAllControllers();

    void MergeSingleJoyAsDualJoy(u32 npad_id_1, u32 npad_id_2);
    void StartLRAssignmentMode();
    void StopLRAssignmentMode();
    bool SwapNpadAssignment(u32 npad_id_1, u32 npad_id_2);

    // Logical OR for all buttons presses on all controllers
    // Specifically for cheat engine and other features.
    u32 GetAndResetPressState();

    static Controller_NPad::NPadControllerType MapSettingsTypeToNPad(Settings::ControllerType type);
    static Settings::ControllerType MapNPadToSettingsType(Controller_NPad::NPadControllerType type);
    static std::size_t NPadIdToIndex(u32 npad_id);
    static u32 IndexToNPad(std::size_t index);
    static bool IsNpadIdValid(u32 npad_id);
    static bool IsDeviceHandleValid(const DeviceHandle& device_handle);

private:
    struct CommonHeader {
        s64_le timestamp;
        s64_le total_entry_count;
        s64_le last_entry_index;
        s64_le entry_count;
    };
    static_assert(sizeof(CommonHeader) == 0x20, "CommonHeader is an invalid size");

    enum class ColorAttributes : u32_le {
        Ok = 0,
        ReadError = 1,
        NoController = 2,
    };
    static_assert(sizeof(ColorAttributes) == 4, "ColorAttributes is an invalid size");

    struct ControllerColor {
        u32_le body;
        u32_le button;
    };
    static_assert(sizeof(ControllerColor) == 8, "ControllerColor is an invalid size");

    struct FullKeyColor {
        ColorAttributes attribute;
        ControllerColor fullkey;
    };
    static_assert(sizeof(FullKeyColor) == 0xC, "FullKeyColor is an invalid size");

    struct JoyconColor {
        ColorAttributes attribute;
        ControllerColor left;
        ControllerColor right;
    };
    static_assert(sizeof(JoyconColor) == 0x14, "JoyconColor is an invalid size");

    struct ControllerPadState {
        union {
            u64_le raw{};
            // Button states
            BitField<0, 1, u64> a;
            BitField<1, 1, u64> b;
            BitField<2, 1, u64> x;
            BitField<3, 1, u64> y;
            BitField<4, 1, u64> l_stick;
            BitField<5, 1, u64> r_stick;
            BitField<6, 1, u64> l;
            BitField<7, 1, u64> r;
            BitField<8, 1, u64> zl;
            BitField<9, 1, u64> zr;
            BitField<10, 1, u64> plus;
            BitField<11, 1, u64> minus;

            // D-Pad
            BitField<12, 1, u64> d_left;
            BitField<13, 1, u64> d_up;
            BitField<14, 1, u64> d_right;
            BitField<15, 1, u64> d_down;

            // Left JoyStick
            BitField<16, 1, u64> l_stick_left;
            BitField<17, 1, u64> l_stick_up;
            BitField<18, 1, u64> l_stick_right;
            BitField<19, 1, u64> l_stick_down;

            // Right JoyStick
            BitField<20, 1, u64> r_stick_left;
            BitField<21, 1, u64> r_stick_up;
            BitField<22, 1, u64> r_stick_right;
            BitField<23, 1, u64> r_stick_down;

            // Not always active?
            BitField<24, 1, u64> left_sl;
            BitField<25, 1, u64> left_sr;

            BitField<26, 1, u64> right_sl;
            BitField<27, 1, u64> right_sr;

            BitField<28, 1, u64> palma;
            BitField<30, 1, u64> handheld_left_b;
        };
    };
    static_assert(sizeof(ControllerPadState) == 8, "ControllerPadState is an invalid size");

    struct AnalogPosition {
        s32_le x;
        s32_le y;
    };
    static_assert(sizeof(AnalogPosition) == 8, "AnalogPosition is an invalid size");

    struct ConnectionState {
        union {
            u32_le raw{};
            BitField<0, 1, u32> is_connected;
            BitField<1, 1, u32> is_wired;
            BitField<2, 1, u32> is_left_connected;
            BitField<3, 1, u32> is_left_wired;
            BitField<4, 1, u32> is_right_connected;
            BitField<5, 1, u32> is_right_wired;
        };
    };
    static_assert(sizeof(ConnectionState) == 4, "ConnectionState is an invalid size");

    struct ControllerPad {
        ControllerPadState pad_states;
        AnalogPosition l_stick;
        AnalogPosition r_stick;
    };
    static_assert(sizeof(ControllerPad) == 0x18, "ControllerPad is an invalid size");

    struct GenericStates {
        s64_le timestamp;
        s64_le timestamp2;
        ControllerPad pad;
        ConnectionState connection_status;
    };
    static_assert(sizeof(GenericStates) == 0x30, "NPadGenericStates is an invalid size");

    struct NPadGeneric {
        CommonHeader common;
        std::array<GenericStates, 17> npad;
    };
    static_assert(sizeof(NPadGeneric) == 0x350, "NPadGeneric is an invalid size");

    struct SixAxisAttributes {
        union {
            u32_le raw{};
            BitField<0, 1, u32> is_connected;
            BitField<1, 1, u32> is_interpolated;
        };
    };
    static_assert(sizeof(SixAxisAttributes) == 4, "SixAxisAttributes is an invalid size");

    struct SixAxisStates {
        s64_le timestamp{};
        INSERT_PADDING_WORDS(2);
        s64_le timestamp2{};
        Common::Vec3f accel{};
        Common::Vec3f gyro{};
        Common::Vec3f rotation{};
        std::array<Common::Vec3f, 3> orientation{};
        SixAxisAttributes attribute;
        INSERT_PADDING_BYTES(4); // Reserved
    };
    static_assert(sizeof(SixAxisStates) == 0x68, "SixAxisStates is an invalid size");

    struct SixAxisGeneric {
        CommonHeader common{};
        std::array<SixAxisStates, 17> sixaxis{};
    };
    static_assert(sizeof(SixAxisGeneric) == 0x708, "SixAxisGeneric is an invalid size");

    struct TriggerState {
        s64_le timestamp{};
        s64_le timestamp2{};
        s32_le l_analog{};
        s32_le r_analog{};
    };
    static_assert(sizeof(TriggerState) == 0x18, "TriggerState is an invalid size");

    struct TriggerGeneric {
        INSERT_PADDING_BYTES(0x4);
        s64_le timestamp;
        INSERT_PADDING_BYTES(0x4);
        s64_le total_entry_count;
        s64_le last_entry_index;
        s64_le entry_count;
        std::array<TriggerState, 17> trigger{};
    };
    static_assert(sizeof(TriggerGeneric) == 0x1C8, "TriggerGeneric is an invalid size");

    struct NPadSystemProperties {
        union {
            s64_le raw{};
            BitField<0, 1, s64> is_charging_joy_dual;
            BitField<1, 1, s64> is_charging_joy_left;
            BitField<2, 1, s64> is_charging_joy_right;
            BitField<3, 1, s64> is_powered_joy_dual;
            BitField<4, 1, s64> is_powered_joy_left;
            BitField<5, 1, s64> is_powered_joy_right;
            BitField<9, 1, s64> is_system_unsupported_button;
            BitField<10, 1, s64> is_system_ext_unsupported_button;
            BitField<11, 1, s64> is_vertical;
            BitField<12, 1, s64> is_horizontal;
            BitField<13, 1, s64> use_plus;
            BitField<14, 1, s64> use_minus;
            BitField<15, 1, s64> use_directional_buttons;
        };
    };
    static_assert(sizeof(NPadSystemProperties) == 0x8, "NPadSystemProperties is an invalid size");

    struct NPadButtonProperties {
        union {
            s32_le raw{};
            BitField<0, 1, s32> is_home_button_protection_enabled;
        };
    };
    static_assert(sizeof(NPadButtonProperties) == 0x4, "NPadButtonProperties is an invalid size");

    struct NPadDevice {
        union {
            u32_le raw{};
            BitField<0, 1, s32> fullkey;
            BitField<1, 1, s32> debug_pad;
            BitField<2, 1, s32> handheld_left;
            BitField<3, 1, s32> handheld_right;
            BitField<4, 1, s32> joycon_left;
            BitField<5, 1, s32> joycon_right;
            BitField<6, 1, s32> palma;
            BitField<7, 1, s32> lark_hvc_left;
            BitField<8, 1, s32> lark_hvc_right;
            BitField<9, 1, s32> lark_nes_left;
            BitField<10, 1, s32> lark_nes_right;
            BitField<11, 1, s32> handheld_lark_hvc_left;
            BitField<12, 1, s32> handheld_lark_hvc_right;
            BitField<13, 1, s32> handheld_lark_nes_left;
            BitField<14, 1, s32> handheld_lark_nes_right;
            BitField<15, 1, s32> lucia;
            BitField<31, 1, s32> system;
        };
    };

    struct MotionDevice {
        Common::Vec3f accel;
        Common::Vec3f gyro;
        Common::Vec3f rotation;
        std::array<Common::Vec3f, 3> orientation;
        Common::Quaternion<f32> quaternion;
    };

    struct NfcXcdHandle {
        INSERT_PADDING_BYTES(0x60);
    };

    struct AppletFooterUiAttributes {
        INSERT_PADDING_BYTES(0x4);
    };

    enum class AppletFooterUiType : u8 {
        None = 0,
        HandheldNone = 1,
        HandheldJoyConLeftOnly = 1,
        HandheldJoyConRightOnly = 3,
        HandheldJoyConLeftJoyConRight = 4,
        JoyDual = 5,
        JoyDualLeftOnly = 6,
        JoyDualRightOnly = 7,
        JoyLeftHorizontal = 8,
        JoyLeftVertical = 9,
        JoyRightHorizontal = 10,
        JoyRightVertical = 11,
        SwitchProController = 12,
        CompatibleProController = 13,
        CompatibleJoyCon = 14,
        LarkHvc1 = 15,
        LarkHvc2 = 16,
        LarkNesLeft = 17,
        LarkNesRight = 18,
        Lucia = 19,
        Verification = 20,
    };

    struct NPadEntry {
        NpadStyleSet style_set;
        NpadAssignments assignment_mode;
        FullKeyColor fullkey_color;
        JoyconColor joycon_color;

        NPadGeneric fullkey_states;
        NPadGeneric handheld_states;
        NPadGeneric joy_dual_states;
        NPadGeneric joy_left_states;
        NPadGeneric joy_right_states;
        NPadGeneric palma_states;
        NPadGeneric system_ext_states;
        SixAxisGeneric sixaxis_fullkey;
        SixAxisGeneric sixaxis_handheld;
        SixAxisGeneric sixaxis_dual_left;
        SixAxisGeneric sixaxis_dual_right;
        SixAxisGeneric sixaxis_left;
        SixAxisGeneric sixaxis_right;
        NPadDevice device_type;
        INSERT_PADDING_BYTES(0x4); // reserved
        NPadSystemProperties system_properties;
        NPadButtonProperties button_properties;
        u32 battery_level_dual;
        u32 battery_level_left;
        u32 battery_level_right;
        AppletFooterUiAttributes footer_attributes;
        AppletFooterUiType footer_type;
        // nfc_states needs to be checked switchbrew does not match with HW
        NfcXcdHandle nfc_states;
        INSERT_PADDING_BYTES(0x8); // Mutex
        TriggerGeneric gc_trigger_states;
        INSERT_PADDING_BYTES(0xc1f);
    };
    static_assert(sizeof(NPadEntry) == 0x5000, "NPadEntry is an invalid size");

    struct ControllerHolder {
        NPadControllerType type;
        bool is_connected;
    };

    void InitNewlyAddedController(std::size_t controller_idx);
    bool IsControllerSupported(NPadControllerType controller) const;
    void RequestPadStateUpdate(u32 npad_id);

    std::atomic<u32> press_state{};

    NpadStyleSet style{};
    std::array<NPadEntry, 10> shared_memory_entries{};
    using ButtonArray = std::array<
        std::array<std::unique_ptr<Input::ButtonDevice>, Settings::NativeButton::NUM_BUTTONS_HID>,
        10>;
    using StickArray = std::array<
        std::array<std::unique_ptr<Input::AnalogDevice>, Settings::NativeAnalog::NUM_STICKS_HID>,
        10>;
    using VibrationArray = std::array<std::array<std::unique_ptr<Input::VibrationDevice>,
                                                 Settings::NativeVibration::NUM_VIBRATIONS_HID>,
                                      10>;
    using MotionArray = std::array<
        std::array<std::unique_ptr<Input::MotionDevice>, Settings::NativeMotion::NUM_MOTIONS_HID>,
        10>;

    KernelHelpers::ServiceContext& service_context;
    std::mutex mutex;
    ButtonArray buttons;
    StickArray sticks;
    VibrationArray vibrations;
    MotionArray motions;
    std::vector<u32> supported_npad_id_types{};
    NpadHoldType hold_type{NpadHoldType::Vertical};
    NpadHandheldActivationMode handheld_activation_mode{NpadHandheldActivationMode::Dual};
    NpadCommunicationMode communication_mode{NpadCommunicationMode::Default};
    // Each controller should have their own styleset changed event
    std::array<Kernel::KEvent*, 10> styleset_changed_events{};
    std::array<std::array<std::chrono::steady_clock::time_point, 2>, 10>
        last_vibration_timepoints{};
    std::array<std::array<VibrationValue, 2>, 10> latest_vibration_values{};
    bool permit_vibration_session_enabled{false};
    std::array<std::array<bool, 2>, 10> vibration_devices_mounted{};
    std::array<ControllerHolder, 10> connected_controllers{};
    std::array<bool, 10> unintended_home_button_input_protection{};
    bool analog_stick_use_center_clamp{};
    GyroscopeZeroDriftMode gyroscope_zero_drift_mode{GyroscopeZeroDriftMode::Standard};
    bool sixaxis_sensors_enabled{true};
    f32 sixaxis_fusion_parameter1{};
    f32 sixaxis_fusion_parameter2{};
    bool sixaxis_at_rest{true};
    std::array<ControllerPad, 10> npad_pad_states{};
    std::array<TriggerState, 10> npad_trigger_states{};
    bool is_in_lr_assignment_mode{false};
};
} // namespace Service::HID
