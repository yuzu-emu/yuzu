// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Common {
class ParamPackage;
}

namespace Settings::NativeAnalog {
enum Values : int;
}

namespace Settings::NativeButton {
enum Values : int;
}

namespace Settings::NativeMotion {
enum Values : int;
}

namespace MouseInput {
class Mouse;
}

namespace TasInput {
class Tas;
}

namespace InputCommon {
namespace Polling {

enum class DeviceType { Button, AnalogPreferred, Motion };

/**
 * A class that can be used to get inputs from an input device like controllers without having to
 * poll the device's status yourself
 */
class DevicePoller {
public:
    virtual ~DevicePoller() = default;
    /// Setup and start polling for inputs, should be called before GetNextInput
    /// If a device_id is provided, events should be filtered to only include events from this
    /// device id
    virtual void Start(const std::string& device_id = "") = 0;
    /// Stop polling
    virtual void Stop() = 0;
    /**
     * Every call to this function returns the next input recorded since calling Start
     * @return A ParamPackage of the recorded input, which can be used to create an InputDevice.
     *         If there has been no input, the package is empty
     */
    virtual Common::ParamPackage GetNextInput() = 0;
};
} // namespace Polling

class GCAnalogFactory;
class GCButtonFactory;
class UDPMotionFactory;
class UDPTouchFactory;
class MouseButtonFactory;
class MouseAnalogFactory;
class MouseMotionFactory;
class MouseTouchFactory;
class TasButtonFactory;
class TasAnalogFactory;
class Keyboard;

/**
 * Given a ParamPackage for a Device returned from `GetInputDevices`, attempt to get the default
 * mapping for the device. This is currently only implemented for the SDL backend devices.
 */
using AnalogMapping = std::unordered_map<Settings::NativeAnalog::Values, Common::ParamPackage>;
using ButtonMapping = std::unordered_map<Settings::NativeButton::Values, Common::ParamPackage>;
using MotionMapping = std::unordered_map<Settings::NativeMotion::Values, Common::ParamPackage>;

class InputSubsystem {
public:
    explicit InputSubsystem();
    ~InputSubsystem();

    InputSubsystem(const InputSubsystem&) = delete;
    InputSubsystem& operator=(const InputSubsystem&) = delete;

    InputSubsystem(InputSubsystem&&) = delete;
    InputSubsystem& operator=(InputSubsystem&&) = delete;

    /// Initializes and registers all built-in input device factories.
    void Initialize();

    /// Unregisters all built-in input device factories and shuts them down.
    void Shutdown();

    /// Retrieves the underlying keyboard device.
    [[nodiscard]] Keyboard* GetKeyboard();

    /// Retrieves the underlying keyboard device.
    [[nodiscard]] const Keyboard* GetKeyboard() const;

    /// Retrieves the underlying mouse device.
    [[nodiscard]] MouseInput::Mouse* GetMouse();

    /// Retrieves the underlying mouse device.
    [[nodiscard]] const MouseInput::Mouse* GetMouse() const;

    /// Retrieves the underlying tas device.
    [[nodiscard]] TasInput::Tas* GetTas();

    /// Retrieves the underlying tas device.
    [[nodiscard]] const TasInput::Tas* GetTas() const;
    /**
     * Returns all available input devices that this Factory can create a new device with.
     * Each returned ParamPackage should have a `display` field used for display, a class field for
     * backends to determine if this backend is meant to service the request and any other
     * information needed to identify this in the backend later.
     */
    [[nodiscard]] std::vector<Common::ParamPackage> GetInputDevices() const;

    /// Retrieves the analog mappings for the given device.
    [[nodiscard]] AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage& device) const;

    /// Retrieves the button mappings for the given device.
    [[nodiscard]] ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage& device) const;

    /// Retrieves the motion mappings for the given device.
    [[nodiscard]] MotionMapping GetMotionMappingForDevice(const Common::ParamPackage& device) const;

    /// Retrieves the underlying GameCube analog handler.
    [[nodiscard]] GCAnalogFactory* GetGCAnalogs();

    /// Retrieves the underlying GameCube analog handler.
    [[nodiscard]] const GCAnalogFactory* GetGCAnalogs() const;

    /// Retrieves the underlying GameCube button handler.
    [[nodiscard]] GCButtonFactory* GetGCButtons();

    /// Retrieves the underlying GameCube button handler.
    [[nodiscard]] const GCButtonFactory* GetGCButtons() const;

    /// Retrieves the underlying udp motion handler.
    [[nodiscard]] UDPMotionFactory* GetUDPMotions();

    /// Retrieves the underlying udp motion handler.
    [[nodiscard]] const UDPMotionFactory* GetUDPMotions() const;

    /// Retrieves the underlying udp touch handler.
    [[nodiscard]] UDPTouchFactory* GetUDPTouch();

    /// Retrieves the underlying udp touch handler.
    [[nodiscard]] const UDPTouchFactory* GetUDPTouch() const;

    /// Retrieves the underlying mouse button handler.
    [[nodiscard]] MouseButtonFactory* GetMouseButtons();

    /// Retrieves the underlying mouse button handler.
    [[nodiscard]] const MouseButtonFactory* GetMouseButtons() const;

    /// Retrieves the underlying mouse analog handler.
    [[nodiscard]] MouseAnalogFactory* GetMouseAnalogs();

    /// Retrieves the underlying mouse analog handler.
    [[nodiscard]] const MouseAnalogFactory* GetMouseAnalogs() const;

    /// Retrieves the underlying mouse motion handler.
    [[nodiscard]] MouseMotionFactory* GetMouseMotions();

    /// Retrieves the underlying mouse motion handler.
    [[nodiscard]] const MouseMotionFactory* GetMouseMotions() const;

    /// Retrieves the underlying mouse touch handler.
    [[nodiscard]] MouseTouchFactory* GetMouseTouch();

    /// Retrieves the underlying mouse touch handler.
    [[nodiscard]] const MouseTouchFactory* GetMouseTouch() const;

    /// Retrieves the underlying tas button handler.
    [[nodiscard]] TasButtonFactory* GetTasButtons();

    /// Retrieves the underlying tas button handler.
    [[nodiscard]] const TasButtonFactory* GetTasButtons() const;

    /// Retrieves the underlying tas analogs handler.
    [[nodiscard]] TasAnalogFactory* GetTasAnalogs();

    /// Retrieves the underlying tas analogs handler.
    [[nodiscard]] const TasAnalogFactory* GetTasAnalogs() const;

    /// Reloads the input devices
    void ReloadInputDevices();

    /// Get all DevicePoller from all backends for a specific device type
    [[nodiscard]] std::vector<std::unique_ptr<Polling::DevicePoller>> GetPollers(
        Polling::DeviceType type) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// Generates a serialized param package for creating a keyboard button device
std::string GenerateKeyboardParam(int key_code);

/// Generates a serialized param package for creating an analog device taking input from keyboard
std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale);

} // namespace InputCommon
