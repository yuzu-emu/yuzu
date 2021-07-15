// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <thread>

#include <libusb.h>

#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/settings_input.h"
#include "input_common/gcadapter/gc_adapter.h"

namespace GCAdapter {

class LibUSBContext {
public:
    explicit LibUSBContext() {
        init_result = libusb_init(&ctx);
    }

    ~LibUSBContext() {
        libusb_exit(ctx);
    }

    LibUSBContext& operator=(const LibUSBContext&) = delete;
    LibUSBContext(const LibUSBContext&) = delete;

    LibUSBContext& operator=(LibUSBContext&&) noexcept = delete;
    LibUSBContext(LibUSBContext&&) noexcept = delete;

    [[nodiscard]] int InitResult() const noexcept {
        return init_result;
    }

    [[nodiscard]] libusb_context* get() noexcept {
        return ctx;
    }

private:
    libusb_context* ctx;
    int init_result{};
};

class LibUSBDeviceHandle {
public:
    explicit LibUSBDeviceHandle(libusb_context* ctx, uint16_t vid, uint16_t pid) noexcept {
        handle = libusb_open_device_with_vid_pid(ctx, vid, pid);
    }

    ~LibUSBDeviceHandle() noexcept {
        if (handle) {
            libusb_release_interface(handle, 1);
            libusb_close(handle);
        }
    }

    LibUSBDeviceHandle& operator=(const LibUSBDeviceHandle&) = delete;
    LibUSBDeviceHandle(const LibUSBDeviceHandle&) = delete;

    LibUSBDeviceHandle& operator=(LibUSBDeviceHandle&&) noexcept = delete;
    LibUSBDeviceHandle(LibUSBDeviceHandle&&) noexcept = delete;

    [[nodiscard]] libusb_device_handle* get() noexcept {
        return handle;
    }

private:
    libusb_device_handle* handle{};
};

Adapter::Adapter() {
    if (usb_adapter_handle) {
        return;
    }
    LOG_INFO(Input, "GC Adapter Initialization started");

    libusb_ctx = std::make_unique<LibUSBContext>();
    const int init_res = libusb_ctx->InitResult();
    if (init_res == LIBUSB_SUCCESS) {
        adapter_scan_thread =
            std::jthread([this](std::stop_token stop_token) { AdapterScanThread(stop_token); });
    } else {
        LOG_ERROR(Input, "libusb could not be initialized. failed with error = {}", init_res);
    }
}

Adapter::~Adapter() {
    Reset();
}

void Adapter::AdapterInputThread(std::stop_token stop_token) {
    LOG_DEBUG(Input, "GC Adapter input thread started");
    s32 payload_size{};
    AdapterPayload adapter_payload{};

    adapter_scan_thread = {};

    while (!stop_token.stop_requested()) {
        libusb_interrupt_transfer(usb_adapter_handle->get(), input_endpoint, adapter_payload.data(),
                                  static_cast<s32>(adapter_payload.size()), &payload_size, 16);
        if (IsPayloadCorrect(adapter_payload, payload_size)) {
            UpdateControllers(adapter_payload);
            UpdateVibrations();
        }
        std::this_thread::yield();
    }

    if (restart_scan_thread) {
        adapter_scan_thread =
            std::jthread([this](std::stop_token token) { AdapterScanThread(token); });
        restart_scan_thread = false;
    }
}

bool Adapter::IsPayloadCorrect(const AdapterPayload& adapter_payload, s32 payload_size) {
    if (payload_size != static_cast<s32>(adapter_payload.size()) ||
        adapter_payload[0] != LIBUSB_DT_HID) {
        LOG_DEBUG(Input, "Error reading payload (size: {}, type: {:02x})", payload_size,
                  adapter_payload[0]);
        if (input_error_counter++ > 20) {
            LOG_ERROR(Input, "GC adapter timeout, Is the adapter connected?");
            adapter_input_thread.request_stop();
            restart_scan_thread = true;
        }
        return false;
    }

    input_error_counter = 0;
    return true;
}

void Adapter::UpdateControllers(const AdapterPayload& adapter_payload) {
    for (std::size_t port = 0; port < pads.size(); ++port) {
        const std::size_t offset = 1 + (9 * port);
        const auto type = static_cast<ControllerTypes>(adapter_payload[offset] >> 4);
        UpdatePadType(port, type);
        if (DeviceConnected(port)) {
            const u8 b1 = adapter_payload[offset + 1];
            const u8 b2 = adapter_payload[offset + 2];
            UpdateStateButtons(port, b1, b2);
            UpdateStateAxes(port, adapter_payload);
            if (configuring) {
                UpdateYuzuSettings(port);
            }
        }
    }
}

void Adapter::UpdatePadType(std::size_t port, ControllerTypes pad_type) {
    if (pads[port].type == pad_type) {
        return;
    }
    // Device changed reset device and set new type
    pads[port] = {};
    pads[port].type = pad_type;
}

void Adapter::UpdateStateButtons(std::size_t port, u8 b1, u8 b2) {
    if (port >= pads.size()) {
        return;
    }

    static constexpr std::array<PadButton, 8> b1_buttons{
        PadButton::ButtonA,    PadButton::ButtonB,     PadButton::ButtonX,    PadButton::ButtonY,
        PadButton::ButtonLeft, PadButton::ButtonRight, PadButton::ButtonDown, PadButton::ButtonUp,
    };

    static constexpr std::array<PadButton, 4> b2_buttons{
        PadButton::ButtonStart,
        PadButton::TriggerZ,
        PadButton::TriggerR,
        PadButton::TriggerL,
    };
    pads[port].buttons = 0;
    for (std::size_t i = 0; i < b1_buttons.size(); ++i) {
        if ((b1 & (1U << i)) != 0) {
            pads[port].buttons =
                static_cast<u16>(pads[port].buttons | static_cast<u16>(b1_buttons[i]));
            pads[port].last_button = b1_buttons[i];
        }
    }

    for (std::size_t j = 0; j < b2_buttons.size(); ++j) {
        if ((b2 & (1U << j)) != 0) {
            pads[port].buttons =
                static_cast<u16>(pads[port].buttons | static_cast<u16>(b2_buttons[j]));
            pads[port].last_button = b2_buttons[j];
        }
    }
}

void Adapter::UpdateStateAxes(std::size_t port, const AdapterPayload& adapter_payload) {
    if (port >= pads.size()) {
        return;
    }

    const std::size_t offset = 1 + (9 * port);
    static constexpr std::array<PadAxes, 6> axes{
        PadAxes::StickX,    PadAxes::StickY,      PadAxes::SubstickX,
        PadAxes::SubstickY, PadAxes::TriggerLeft, PadAxes::TriggerRight,
    };

    for (const PadAxes axis : axes) {
        const auto index = static_cast<std::size_t>(axis);
        const u8 axis_value = adapter_payload[offset + 3 + index];
        if (pads[port].reset_origin_counter <= 18) {
            if (pads[port].axis_origin[index] != axis_value) {
                pads[port].reset_origin_counter = 0;
            }
            pads[port].axis_origin[index] = axis_value;
            pads[port].reset_origin_counter++;
        }
        pads[port].axis_values[index] =
            static_cast<s16>(axis_value - pads[port].axis_origin[index]);
    }
}

void Adapter::UpdateYuzuSettings(std::size_t port) {
    if (port >= pads.size()) {
        return;
    }

    constexpr u8 axis_threshold = 50;
    GCPadStatus pad_status = {.port = port};

    if (pads[port].buttons != 0) {
        pad_status.button = pads[port].last_button;
        pad_queue.Push(pad_status);
    }

    // Accounting for a threshold here to ensure an intentional press
    for (std::size_t i = 0; i < pads[port].axis_values.size(); ++i) {
        const s16 value = pads[port].axis_values[i];

        if (value > axis_threshold || value < -axis_threshold) {
            pad_status.axis = static_cast<PadAxes>(i);
            pad_status.axis_value = value;
            pad_status.axis_threshold = axis_threshold;
            pad_queue.Push(pad_status);
        }
    }
}

void Adapter::UpdateVibrations() {
    // Use 8 states to keep the switching between on/off fast enough for
    // a human to not notice the difference between switching from on/off
    // More states = more rumble strengths = slower update time
    constexpr u8 vibration_states = 8;

    vibration_counter = (vibration_counter + 1) % vibration_states;

    for (GCController& pad : pads) {
        const bool vibrate = pad.rumble_amplitude > vibration_counter;
        vibration_changed |= vibrate != pad.enable_vibration;
        pad.enable_vibration = vibrate;
    }
    SendVibrations();
}

void Adapter::SendVibrations() {
    if (!rumble_enabled || !vibration_changed) {
        return;
    }
    s32 size{};
    constexpr u8 rumble_command = 0x11;
    const u8 p1 = pads[0].enable_vibration;
    const u8 p2 = pads[1].enable_vibration;
    const u8 p3 = pads[2].enable_vibration;
    const u8 p4 = pads[3].enable_vibration;
    std::array<u8, 5> payload = {rumble_command, p1, p2, p3, p4};
    const int err =
        libusb_interrupt_transfer(usb_adapter_handle->get(), output_endpoint, payload.data(),
                                  static_cast<s32>(payload.size()), &size, 16);
    if (err) {
        LOG_DEBUG(Input, "Adapter libusb write failed: {}", libusb_error_name(err));
        if (output_error_counter++ > 5) {
            LOG_ERROR(Input, "GC adapter output timeout, Rumble disabled");
            rumble_enabled = false;
        }
        return;
    }
    output_error_counter = 0;
    vibration_changed = false;
}

bool Adapter::RumblePlay(std::size_t port, u8 amplitude) {
    pads[port].rumble_amplitude = amplitude;

    return rumble_enabled;
}

void Adapter::AdapterScanThread(std::stop_token stop_token) {
    usb_adapter_handle = nullptr;
    pads = {};
    while (!stop_token.stop_requested() && !Setup()) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

bool Adapter::Setup() {
    constexpr u16 nintendo_vid = 0x057e;
    constexpr u16 gc_adapter_pid = 0x0337;
    usb_adapter_handle =
        std::make_unique<LibUSBDeviceHandle>(libusb_ctx->get(), nintendo_vid, gc_adapter_pid);
    if (!usb_adapter_handle->get()) {
        return false;
    }
    if (!CheckDeviceAccess()) {
        usb_adapter_handle = nullptr;
        return false;
    }

    libusb_device* const device = libusb_get_device(usb_adapter_handle->get());

    LOG_INFO(Input, "GC adapter is now connected");
    // GC Adapter found and accessible, registering it
    if (GetGCEndpoint(device)) {
        rumble_enabled = true;
        input_error_counter = 0;
        output_error_counter = 0;
        adapter_input_thread =
            std::jthread([this](std::stop_token stop_token) { AdapterInputThread(stop_token); });
        return true;
    }
    return false;
}

bool Adapter::CheckDeviceAccess() {
    // This fixes payload problems from offbrand GCAdapters
    const s32 control_transfer_error =
        libusb_control_transfer(usb_adapter_handle->get(), 0x21, 11, 0x0001, 0, nullptr, 0, 1000);
    if (control_transfer_error < 0) {
        LOG_ERROR(Input, "libusb_control_transfer failed with error= {}", control_transfer_error);
    }

    s32 kernel_driver_error = libusb_kernel_driver_active(usb_adapter_handle->get(), 0);
    if (kernel_driver_error == 1) {
        kernel_driver_error = libusb_detach_kernel_driver(usb_adapter_handle->get(), 0);
        if (kernel_driver_error != 0 && kernel_driver_error != LIBUSB_ERROR_NOT_SUPPORTED) {
            LOG_ERROR(Input, "libusb_detach_kernel_driver failed with error = {}",
                      kernel_driver_error);
        }
    }

    if (kernel_driver_error && kernel_driver_error != LIBUSB_ERROR_NOT_SUPPORTED) {
        usb_adapter_handle = nullptr;
        return false;
    }

    const int interface_claim_error = libusb_claim_interface(usb_adapter_handle->get(), 0);
    if (interface_claim_error) {
        LOG_ERROR(Input, "libusb_claim_interface failed with error = {}", interface_claim_error);
        usb_adapter_handle = nullptr;
        return false;
    }

    return true;
}

bool Adapter::GetGCEndpoint(libusb_device* device) {
    libusb_config_descriptor* config = nullptr;
    const int config_descriptor_return = libusb_get_config_descriptor(device, 0, &config);
    if (config_descriptor_return != LIBUSB_SUCCESS) {
        LOG_ERROR(Input, "libusb_get_config_descriptor failed with error = {}",
                  config_descriptor_return);
        return false;
    }

    for (u8 ic = 0; ic < config->bNumInterfaces; ic++) {
        const libusb_interface* interfaceContainer = &config->interface[ic];
        for (int i = 0; i < interfaceContainer->num_altsetting; i++) {
            const libusb_interface_descriptor* interface = &interfaceContainer->altsetting[i];
            for (u8 e = 0; e < interface->bNumEndpoints; e++) {
                const libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];
                if ((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) != 0) {
                    input_endpoint = endpoint->bEndpointAddress;
                } else {
                    output_endpoint = endpoint->bEndpointAddress;
                }
            }
        }
    }
    // This transfer seems to be responsible for clearing the state of the adapter
    // Used to clear the "busy" state of when the device is unexpectedly unplugged
    unsigned char clear_payload = 0x13;
    libusb_interrupt_transfer(usb_adapter_handle->get(), output_endpoint, &clear_payload,
                              sizeof(clear_payload), nullptr, 16);
    return true;
}

void Adapter::Reset() {
    adapter_scan_thread = {};
    adapter_input_thread = {};
    usb_adapter_handle = nullptr;
    pads = {};
    libusb_ctx = nullptr;
}

std::vector<Common::ParamPackage> Adapter::GetInputDevices() const {
    std::vector<Common::ParamPackage> devices;
    for (std::size_t port = 0; port < pads.size(); ++port) {
        if (!DeviceConnected(port)) {
            continue;
        }
        std::string name = fmt::format("Gamecube Controller {}", port + 1);
        devices.emplace_back(Common::ParamPackage{
            {"class", "gcpad"},
            {"display", std::move(name)},
            {"port", std::to_string(port)},
        });
    }
    return devices;
}

InputCommon::ButtonMapping Adapter::GetButtonMappingForDevice(
    const Common::ParamPackage& params) const {
    // This list is missing ZL/ZR since those are not considered buttons.
    // We will add those afterwards
    // This list also excludes any button that can't be really mapped
    static constexpr std::array<std::pair<Settings::NativeButton::Values, PadButton>, 12>
        switch_to_gcadapter_button = {
            std::pair{Settings::NativeButton::A, PadButton::ButtonA},
            {Settings::NativeButton::B, PadButton::ButtonB},
            {Settings::NativeButton::X, PadButton::ButtonX},
            {Settings::NativeButton::Y, PadButton::ButtonY},
            {Settings::NativeButton::Plus, PadButton::ButtonStart},
            {Settings::NativeButton::DLeft, PadButton::ButtonLeft},
            {Settings::NativeButton::DUp, PadButton::ButtonUp},
            {Settings::NativeButton::DRight, PadButton::ButtonRight},
            {Settings::NativeButton::DDown, PadButton::ButtonDown},
            {Settings::NativeButton::SL, PadButton::TriggerL},
            {Settings::NativeButton::SR, PadButton::TriggerR},
            {Settings::NativeButton::R, PadButton::TriggerZ},
        };
    if (!params.Has("port")) {
        return {};
    }

    InputCommon::ButtonMapping mapping{};
    for (const auto& [switch_button, gcadapter_button] : switch_to_gcadapter_button) {
        Common::ParamPackage button_params({{"engine", "gcpad"}});
        button_params.Set("port", params.Get("port", 0));
        button_params.Set("button", static_cast<int>(gcadapter_button));
        mapping.insert_or_assign(switch_button, std::move(button_params));
    }

    // Add the missing bindings for ZL/ZR
    static constexpr std::array<std::pair<Settings::NativeButton::Values, PadAxes>, 2>
        switch_to_gcadapter_axis = {
            std::pair{Settings::NativeButton::ZL, PadAxes::TriggerLeft},
            {Settings::NativeButton::ZR, PadAxes::TriggerRight},
        };
    for (const auto& [switch_button, gcadapter_axis] : switch_to_gcadapter_axis) {
        Common::ParamPackage button_params({{"engine", "gcpad"}});
        button_params.Set("port", params.Get("port", 0));
        button_params.Set("button", static_cast<s32>(PadButton::Stick));
        button_params.Set("axis", static_cast<s32>(gcadapter_axis));
        button_params.Set("threshold", 0.5f);
        button_params.Set("direction", "+");
        mapping.insert_or_assign(switch_button, std::move(button_params));
    }
    return mapping;
}

InputCommon::AnalogMapping Adapter::GetAnalogMappingForDevice(
    const Common::ParamPackage& params) const {
    if (!params.Has("port")) {
        return {};
    }

    InputCommon::AnalogMapping mapping = {};
    Common::ParamPackage left_analog_params;
    left_analog_params.Set("engine", "gcpad");
    left_analog_params.Set("port", params.Get("port", 0));
    left_analog_params.Set("axis_x", static_cast<int>(PadAxes::StickX));
    left_analog_params.Set("axis_y", static_cast<int>(PadAxes::StickY));
    mapping.insert_or_assign(Settings::NativeAnalog::LStick, std::move(left_analog_params));
    Common::ParamPackage right_analog_params;
    right_analog_params.Set("engine", "gcpad");
    right_analog_params.Set("port", params.Get("port", 0));
    right_analog_params.Set("axis_x", static_cast<int>(PadAxes::SubstickX));
    right_analog_params.Set("axis_y", static_cast<int>(PadAxes::SubstickY));
    mapping.insert_or_assign(Settings::NativeAnalog::RStick, std::move(right_analog_params));
    return mapping;
}

bool Adapter::DeviceConnected(std::size_t port) const {
    return pads[port].type != ControllerTypes::None;
}

void Adapter::BeginConfiguration() {
    pad_queue.Clear();
    configuring = true;
}

void Adapter::EndConfiguration() {
    pad_queue.Clear();
    configuring = false;
}

Common::SPSCQueue<GCPadStatus>& Adapter::GetPadQueue() {
    return pad_queue;
}

const Common::SPSCQueue<GCPadStatus>& Adapter::GetPadQueue() const {
    return pad_queue;
}

GCController& Adapter::GetPadState(std::size_t port) {
    return pads.at(port);
}

const GCController& Adapter::GetPadState(std::size_t port) const {
    return pads.at(port);
}

} // namespace GCAdapter
