// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <list>
#include <mutex>
#include <utility>
#include "common/assert.h"
#include "common/threadsafe_queue.h"
#include "input_common/joycon/jc_adapter.h"
#include "input_common/joycon/jc_poller.h"

namespace InputCommon {

class JCButton final : public Input::ButtonDevice {
public:
    explicit JCButton(int port_, int button_, JCAdapter::Joycons* adapter)
        : port(port_), button(button_), jcadapter(adapter) {}

    ~JCButton() override;

    bool GetStatus() const override {
        if (jcadapter->DeviceConnected(port)) {
            return jcadapter->GetPadState(port).buttons.at(button);
        }
        return false;
    }

    bool SetRumblePlay(f32 amp_high, f32 amp_low, f32 freq_high, f32 freq_low) const override {
        if (jcadapter->DeviceConnected(port)) {
            jcadapter->SetRumble(port, amp_high, amp_low, freq_high, freq_low);
            return true;
        }
        return false;
    }

private:
    const int port;
    const int button;
    JCAdapter::Joycons* jcadapter;
};

class JCAxisButton final : public Input::ButtonDevice {
public:
    explicit JCAxisButton(int port_, int axis_, float threshold_, bool trigger_if_greater_,
                          JCAdapter::Joycons* adapter)
        : port(port_), axis(axis_), threshold(threshold_), trigger_if_greater(trigger_if_greater_),
          jcadapter(adapter) {}

    bool GetStatus() const override {
        if (jcadapter->DeviceConnected(port)) {
            const float axis_value = jcadapter->GetPadState(port).axes.at(axis);
            if (trigger_if_greater) {
                // TODO: Might be worthwile to set a slider for the trigger threshold. It is
                // currently always set to 0.5 in configure_input_player.cpp ZL/ZR HandleClick
                return axis_value > threshold;
            }
            return axis_value < -threshold;
        }
        return false;
    }

    bool SetRumblePlay(f32 amp_high, f32 amp_low, f32 freq_high, f32 freq_low) const override {
        if (jcadapter->DeviceConnected(port)) {
            jcadapter->SetRumble(port, amp_high, amp_low, freq_high, freq_low);
            return true;
        }
        return false;
    }

private:
    const int port;
    const int axis;
    float threshold;
    bool trigger_if_greater;
    JCAdapter::Joycons* jcadapter;
};
// Motion buttons are a temporary way to test motion
class JCMotionButton final : public Input::ButtonDevice {
public:
    explicit JCMotionButton(int port_, int axis_, float threshold_, bool trigger_if_greater_,
                            JCAdapter::Joycons* adapter)
        : port(port_), axis(axis_), threshold(threshold_), trigger_if_greater(trigger_if_greater_),
          jcadapter(adapter) {}

    bool GetStatus() const override {
        if (jcadapter->DeviceConnected(port)) {
            const float axis_value = jcadapter->GetPadState(port).motion.at(axis);
            if (trigger_if_greater) {
                // TODO: Might be worthwile to set a slider for the trigger threshold. It is
                // currently always set to 0.5 in configure_input_player.cpp ZL/ZR HandleClick
                return axis_value > threshold;
            }
            return axis_value < -threshold;
        }
        return false;
    }

    bool SetRumblePlay(f32 amp_high, f32 amp_low, f32 freq_high, f32 freq_low) const override {
        if (jcadapter->DeviceConnected(port)) {
            jcadapter->SetRumble(port, amp_high, amp_low, freq_high, freq_low);
            return true;
        }
        return false;
    }

private:
    const int port;
    const int axis;
    float threshold;
    bool trigger_if_greater;
    JCAdapter::Joycons* jcadapter;
};

JCButtonFactory::JCButtonFactory(std::shared_ptr<JCAdapter::Joycons> adapter_)
    : adapter(std::move(adapter_)) {}

JCButton::~JCButton() = default;

std::unique_ptr<Input::ButtonDevice> JCButtonFactory::Create(const Common::ParamPackage& params) {
    const int button_id = params.Get("button", 0);
    const int port = params.Get("port", 0);

    constexpr int PAD_STICK_ID = static_cast<u32>(JCAdapter::PadButton::STICK);
    constexpr int PAD_MOTION_ID = static_cast<u32>(JCAdapter::PadButton::MOTION);
    // button is not an axis/stick button
    if (button_id != PAD_STICK_ID && button_id != PAD_MOTION_ID) {
        auto button = std::make_unique<JCButton>(port, button_id, adapter.get());
        return std::move(button);
    }

    // For Axis buttons, used by the binary sticks.
    if (button_id == PAD_STICK_ID && button_id != PAD_MOTION_ID) {
        const int axis = params.Get("axis", 0);
        const float threshold = params.Get("threshold", 0.25f);
        const std::string direction_name = params.Get("direction", "");
        bool trigger_if_greater;
        if (direction_name == "+") {
            trigger_if_greater = true;
        } else if (direction_name == "-") {
            trigger_if_greater = false;
        } else {
            trigger_if_greater = true;
            LOG_ERROR(Input, "Unknown direction {}", direction_name);
        }
        return std::make_unique<JCAxisButton>(port, axis, threshold, trigger_if_greater,
                                              adapter.get());
    }

    // For Motion buttons, used by the binary motion.
    if (button_id != PAD_STICK_ID && button_id == PAD_MOTION_ID) {
        const int axis = params.Get("motion", 0);
        const float threshold = params.Get("threshold", 0.5f);
        const std::string direction_name = params.Get("direction", "");
        bool trigger_if_greater;
        if (direction_name == "+") {
            trigger_if_greater = true;
        } else if (direction_name == "-") {
            trigger_if_greater = false;
        } else {
            trigger_if_greater = true;
            LOG_ERROR(Input, "Unknown direction {}", direction_name);
        }
        return std::make_unique<JCMotionButton>(port, axis, threshold, trigger_if_greater,
                                                adapter.get());
    }

    UNREACHABLE();
    return nullptr;
}

Common::ParamPackage JCButtonFactory::GetNextInput() {
    Common::ParamPackage params;
    JCAdapter::JCPadStatus pad;
    auto& queue = adapter->GetPadQueue();
    for (std::size_t port = 0; port < queue.size(); ++port) {
        while (queue[port].Pop(pad)) {
            // This while loop will break on the earliest detected button
            params.Set("engine", "jcpad");
            params.Set("port", static_cast<int>(port));
            for (const auto& button : JCAdapter::PadButtonArray) {
                const int button_value = static_cast<u32>(button);
                if (pad.button & button_value) {
                    params.Set("button", button_value);
                    break;
                }
            }

            // For Axis button implementation
            if (pad.axis != JCAdapter::PadAxes::Undefined) {
                params.Set("axis", static_cast<u16>(pad.axis));
                params.Set("button", static_cast<int>(JCAdapter::PadButton::STICK));
                if (pad.axis_value > 0) {
                    params.Set("direction", "+");
                    params.Set("threshold", "0.25");
                } else {
                    params.Set("direction", "-");
                    params.Set("threshold", "0.25");
                }
                break;
            }

            // For Motion button implementation
            if (pad.motion != JCAdapter::PadMotion::Undefined) {
                params.Set("motion", static_cast<u16>(pad.motion));
                params.Set("button", static_cast<int>(JCAdapter::PadButton::MOTION));
                if (pad.motion_value > 0) {
                    params.Set("direction", "+");
                    params.Set("threshold", "0.5");
                } else {
                    params.Set("direction", "-");
                    params.Set("threshold", "0.5");
                }
                break;
            }
        }
    }
    return params;
}

void JCButtonFactory::BeginConfiguration() {
    polling = true;
    adapter->BeginConfiguration();
}

void JCButtonFactory::EndConfiguration() {
    polling = false;
    adapter->EndConfiguration();
}

class JCAnalog final : public Input::AnalogDevice {
public:
    JCAnalog(int port_, int axis_x_, int axis_y_, bool invert_x_, bool invert_y_, float deadzone_,
             float range_, JCAdapter::Joycons* adapter)
        : port(port_), axis_x(axis_x_), axis_y(axis_y_), invert_x(invert_x_), invert_y(invert_y_),
          deadzone(deadzone_), range(range_), jcadapter(adapter) {}

    float GetAxis(int axis) const {
        if (jcadapter->DeviceConnected(port)) {
            std::lock_guard lock{mutex};
            if (axis < 10) {
                return jcadapter->GetPadState(port).axes.at(axis);
            } else {
                return jcadapter->GetPadState(port).motion.at(axis - 10);
            }
        }
        return 0.0f;
    }

    std::pair<float, float> GetAnalog(u32 analog_axis_x, u32 analog_axis_y) const {
        float x = GetAxis(analog_axis_x);
        float y = GetAxis(analog_axis_y);
        if (invert_x) {
            x = -x;
        }
        if (invert_y) {
            y = -y;
        }
        // Make sure the coordinates are in the unit circle,
        // otherwise normalize it.
        float r = x * x + y * y;
        if (r > 1.0f) {
            r = std::sqrt(r);
            x /= r;
            y /= r;
        }

        return {x, y};
    }

    std::tuple<float, float> GetStatus() const override {
        const auto [x, y] = GetAnalog(axis_x, axis_y);
        const float r = std::sqrt((x * x) + (y * y));
        if (r > deadzone) {
            return {x / r * (r - deadzone) / (1 - deadzone),
                    y / r * (r - deadzone) / (1 - deadzone)};
        }
        return {0.0f, 0.0f};
    }

    bool GetAnalogDirectionStatus(Input::AnalogDirection direction) const override {
        const auto [x, y] = GetStatus();
        const float directional_deadzone = 0.4f;
        switch (direction) {
        case Input::AnalogDirection::RIGHT:
            return x > directional_deadzone;
        case Input::AnalogDirection::LEFT:
            return x < -directional_deadzone;
        case Input::AnalogDirection::UP:
            return y > directional_deadzone;
        case Input::AnalogDirection::DOWN:
            return y < -directional_deadzone;
        }
        return false;
    }

private:
    const int port;
    const int axis_x;
    const int axis_y;
    const bool invert_x;
    const bool invert_y;
    const float deadzone;
    const float range;
    JCAdapter::Joycons* jcadapter;
    mutable std::mutex mutex;
};

/// An analog device factory that creates analog devices from JC Adapter
JCAnalogFactory::JCAnalogFactory(std::shared_ptr<JCAdapter::Joycons> adapter_)
    : adapter(std::move(adapter_)) {}

/**
 * Creates analog device from joystick axes
 * @param params contains parameters for creating the device:
 *     - "port": the nth jcpad on the adapter
 *     - "axis_x": the index of the axis to be bind as x-axis
 *     - "axis_y": the index of the axis to be bind as y-axis
 */
std::unique_ptr<Input::AnalogDevice> JCAnalogFactory::Create(const Common::ParamPackage& params) {
    const auto port = static_cast<u32>(params.Get("port", 0));
    const auto axis_x = static_cast<u32>(params.Get("axis_x", 0));
    const auto axis_y = static_cast<u32>(params.Get("axis_y", 1));
    const auto deadzone = std::clamp(params.Get("deadzone", 0.0f), 0.0f, 1.0f);
    const auto range = std::clamp(params.Get("range", 1.0f), 0.50f, 1.50f);
    const std::string invert_x_value = params.Get("invert_x", "+");
    const std::string invert_y_value = params.Get("invert_y", "+");
    const bool invert_x = invert_x_value == "-";
    const bool invert_y = invert_y_value == "-";

    return std::make_unique<JCAnalog>(port, axis_x, axis_y, invert_x, invert_y, deadzone, range,
                                      adapter.get());
}

void JCAnalogFactory::BeginConfiguration() {
    polling = true;
    adapter->BeginConfiguration();
}

void JCAnalogFactory::EndConfiguration() {
    polling = false;
    adapter->EndConfiguration();
}

Common::ParamPackage JCAnalogFactory::GetNextInput() {
    JCAdapter::JCPadStatus pad;
    auto& queue = adapter->GetPadQueue();
    for (std::size_t port = 0; port < queue.size(); ++port) {
        while (queue[port].Pop(pad)) {
            if (pad.axis != JCAdapter::PadAxes::Undefined && std::abs(pad.axis_value) > 0.1) {
                // An analog device needs two axes, so we need to store the axis for later and wait
                // for a second input event. The axes also must be from the same joystick.
                const u16 axis = static_cast<u16>(pad.axis);
                if (analog_x_axis == -1) {
                    analog_x_axis = axis;
                    controller_number = static_cast<int>(port);
                } else if (analog_y_axis == -1 && analog_x_axis != axis &&
                           controller_number == static_cast<int>(port)) {
                    analog_y_axis = axis;
                }
            } else if (pad.motion != JCAdapter::PadMotion::Undefined &&
                       std::abs(pad.motion_value) > 1) {
                const u16 axis = 10 + static_cast<u16>(pad.motion);
                if (analog_x_axis == -1) {
                    analog_x_axis = axis;
                    controller_number = static_cast<int>(port);
                } else if (analog_y_axis == -1 && analog_x_axis != axis &&
                           controller_number == static_cast<int>(port)) {
                    analog_y_axis = axis;
                }
            }
        }
    }
    Common::ParamPackage params;
    if (analog_x_axis != -1 && analog_y_axis != -1) {
        params.Set("engine", "jcpad");
        params.Set("port", controller_number);
        params.Set("axis_x", analog_x_axis);
        params.Set("axis_y", analog_y_axis);
        analog_x_axis = -1;
        analog_y_axis = -1;
        controller_number = -1;
        return params;
    }
    return params;
}

class JCMotion final : public Input::MotionDevice {
public:
    JCMotion(int port_, JCAdapter::Joycons* adapter) : port(port_), jcadapter(adapter) {}

    std::tuple<Common::Vec3<float>, Common::Vec3<float>, Common::Vec3<float>,
               std::array<Common::Vec3f, 3>>
    GetStatus() const override {
        auto motion = jcadapter->GetPadState(port).motion;
        if (motion.size() == 18) {
            auto gyroscope = Common::MakeVec(motion.at(0), motion.at(1), motion.at(2));
            auto accelerometer = Common::MakeVec(motion.at(3), motion.at(4), motion.at(5));
            auto rotation = Common::MakeVec(motion.at(6), motion.at(7), motion.at(8));
            std::array<Common::Vec3f, 3> orientation{};
            orientation[0] = Common::MakeVec(motion.at(9), motion.at(10), motion.at(11));
            orientation[1] = Common::MakeVec(motion.at(12), motion.at(13), motion.at(14));
            orientation[2] = Common::MakeVec(motion.at(15), motion.at(16), motion.at(17));
            return std::make_tuple(accelerometer, gyroscope, rotation, orientation);
        }
        return {};
    }

private:
    const int port;
    JCAdapter::Joycons* jcadapter;
    mutable std::mutex mutex;
};

/// A motion device factory that creates motion devices from JC Adapter
JCMotionFactory::JCMotionFactory(std::shared_ptr<JCAdapter::Joycons> adapter_)
    : adapter(std::move(adapter_)) {}

/**
 * Creates motion device
 * @param params contains parameters for creating the device:
 *     - "port": the nth jcpad on the adapter
 */
std::unique_ptr<Input::MotionDevice> JCMotionFactory::Create(const Common::ParamPackage& params) {
    const int port = params.Get("port", 0);

    return std::make_unique<JCMotion>(port, adapter.get());
}

void JCMotionFactory::BeginConfiguration() {
    polling = true;
    adapter->BeginConfiguration();
}

void JCMotionFactory::EndConfiguration() {
    polling = false;
    adapter->EndConfiguration();
}

Common::ParamPackage JCMotionFactory::GetNextInput() {
    Common::ParamPackage params;
    JCAdapter::JCPadStatus pad;
    auto& queue = adapter->GetPadQueue();
    for (std::size_t port = 0; port < queue.size(); ++port) {
        while (queue[port].Pop(pad)) {
            if (pad.motion == JCAdapter::PadMotion::Undefined || std::abs(pad.motion_value) < 1) {
                continue;
            }
            params.Set("engine", "jcpad");
            params.Set("motion", static_cast<u16>(pad.motion));
            params.Set("port", static_cast<int>(port));
            return params;
        }
    }
    return params;
}

} // namespace InputCommon
