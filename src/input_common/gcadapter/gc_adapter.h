// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>

#include "common/common_types.h"
#include "common/threadsafe_queue.h"
#include "input_common/main.h"

struct libusb_context;
struct libusb_device;
struct libusb_device_handle;

namespace GCAdapter {

class LibUSBContext;
class LibUSBDeviceHandle;

enum class PadButton {
    Undefined = 0x0000,
    ButtonLeft = 0x0001,
    ButtonRight = 0x0002,
    ButtonDown = 0x0004,
    ButtonUp = 0x0008,
    TriggerZ = 0x0010,
    TriggerR = 0x0020,
    TriggerL = 0x0040,
    ButtonA = 0x0100,
    ButtonB = 0x0200,
    ButtonX = 0x0400,
    ButtonY = 0x0800,
    ButtonStart = 0x1000,
    // Below is for compatibility with "AxisButton" type
    Stick = 0x2000,
};

enum class PadAxes : u8 {
    StickX,
    StickY,
    SubstickX,
    SubstickY,
    TriggerLeft,
    TriggerRight,
    Undefined,
};

enum class ControllerTypes {
    None,
    Wired,
    Wireless,
};

struct GCPadStatus {
    std::size_t port{};

    PadButton button{PadButton::Undefined}; // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits

    PadAxes axis{PadAxes::Undefined};
    s16 axis_value{};
    u8 axis_threshold{50};
};

struct GCController {
    ControllerTypes type = ControllerTypes::None;
    bool enable_vibration = false;
    u8 rumble_amplitude = 0;
    u16 buttons = 0;
    PadButton last_button = PadButton::Undefined;
    std::array<s16, 6> axis_values{};
    std::array<u8, 6> axis_origin{};
    u8 reset_origin_counter{};
};

class Adapter {
public:
    Adapter();
    ~Adapter();

    /// Request a vibration for a controller
    bool RumblePlay(std::size_t port, u8 amplitude);

    /// Used for polling
    void BeginConfiguration();
    void EndConfiguration();

    Common::SPSCQueue<GCPadStatus>& GetPadQueue();
    const Common::SPSCQueue<GCPadStatus>& GetPadQueue() const;

    GCController& GetPadState(std::size_t port);
    const GCController& GetPadState(std::size_t port) const;

    /// Returns true if there is a device connected to port
    bool DeviceConnected(std::size_t port) const;

    /// Used for automapping features
    std::vector<Common::ParamPackage> GetInputDevices() const;
    InputCommon::ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage& params) const;
    InputCommon::AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage& params) const;

private:
    using AdapterPayload = std::array<u8, 37>;

    void UpdatePadType(std::size_t port, ControllerTypes pad_type);
    void UpdateControllers(const AdapterPayload& adapter_payload);
    void UpdateYuzuSettings(std::size_t port);
    void UpdateStateButtons(std::size_t port, u8 b1, u8 b2);
    void UpdateStateAxes(std::size_t port, const AdapterPayload& adapter_payload);
    void UpdateVibrations();

    void AdapterInputThread(std::stop_token stop_token);

    void AdapterScanThread(std::stop_token stop_token);

    bool IsPayloadCorrect(const AdapterPayload& adapter_payload, s32 payload_size);

    // Updates vibration state of all controllers
    void SendVibrations();

    /// For use in initialization, querying devices to find the adapter
    bool Setup();

    /// Returns true if we successfully gain access to GC Adapter
    bool CheckDeviceAccess();

    /// Captures GC Adapter endpoint address
    /// Returns true if the endpoint was set correctly
    bool GetGCEndpoint(libusb_device* device);

    /// For shutting down, clear all data, join all threads, release usb
    void Reset();

    std::unique_ptr<LibUSBDeviceHandle> usb_adapter_handle;
    std::array<GCController, 4> pads;
    Common::SPSCQueue<GCPadStatus> pad_queue;

    std::jthread adapter_input_thread;
    std::jthread adapter_scan_thread;
    bool restart_scan_thread{};

    std::unique_ptr<LibUSBContext> libusb_ctx;

    u8 input_endpoint{0};
    u8 output_endpoint{0};
    u8 input_error_counter{0};
    u8 output_error_counter{0};
    int vibration_counter{0};

    bool configuring{false};
    bool rumble_enabled{true};
    bool vibration_changed{true};
};
} // namespace GCAdapter
