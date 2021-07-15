// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <utility>

#include "common/settings.h"
#include "common/threadsafe_queue.h"
#include "input_common/tas/tas_input.h"
#include "input_common/tas/tas_poller.h"

namespace InputCommon {

class TasButton final : public Input::ButtonDevice {
public:
    explicit TasButton(u32 button_, u32 pad_, const TasInput::Tas* tas_input_)
        : button(button_), pad(pad_), tas_input(tas_input_) {}

    bool GetStatus() const override {
        return (tas_input->GetTasState(pad).buttons & button) != 0;
    }

private:
    const u32 button;
    const u32 pad;
    const TasInput::Tas* tas_input;
};

TasButtonFactory::TasButtonFactory(std::shared_ptr<TasInput::Tas> tas_input_)
    : tas_input(std::move(tas_input_)) {}

std::unique_ptr<Input::ButtonDevice> TasButtonFactory::Create(const Common::ParamPackage& params) {
    const auto button_id = params.Get("button", 0);
    const auto pad = params.Get("pad", 0);

    return std::make_unique<TasButton>(button_id, pad, tas_input.get());
}

class TasAnalog final : public Input::AnalogDevice {
public:
    explicit TasAnalog(u32 pad_, u32 axis_x_, u32 axis_y_, const TasInput::Tas* tas_input_)
        : pad(pad_), axis_x(axis_x_), axis_y(axis_y_), tas_input(tas_input_) {}

    float GetAxis(u32 axis) const {
        std::lock_guard lock{mutex};
        return tas_input->GetTasState(pad).axis.at(axis);
    }

    std::pair<float, float> GetAnalog(u32 analog_axis_x, u32 analog_axis_y) const {
        float x = GetAxis(analog_axis_x);
        float y = GetAxis(analog_axis_y);

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
        return GetAnalog(axis_x, axis_y);
    }

    Input::AnalogProperties GetAnalogProperties() const override {
        return {0.0f, 1.0f, 0.5f};
    }

private:
    const u32 pad;
    const u32 axis_x;
    const u32 axis_y;
    const TasInput::Tas* tas_input;
    mutable std::mutex mutex;
};

/// An analog device factory that creates analog devices from GC Adapter
TasAnalogFactory::TasAnalogFactory(std::shared_ptr<TasInput::Tas> tas_input_)
    : tas_input(std::move(tas_input_)) {}

/**
 * Creates analog device from joystick axes
 * @param params contains parameters for creating the device:
 *     - "port": the nth gcpad on the adapter
 *     - "axis_x": the index of the axis to be bind as x-axis
 *     - "axis_y": the index of the axis to be bind as y-axis
 */
std::unique_ptr<Input::AnalogDevice> TasAnalogFactory::Create(const Common::ParamPackage& params) {
    const auto pad = static_cast<u32>(params.Get("pad", 0));
    const auto axis_x = static_cast<u32>(params.Get("axis_x", 0));
    const auto axis_y = static_cast<u32>(params.Get("axis_y", 1));

    return std::make_unique<TasAnalog>(pad, axis_x, axis_y, tas_input.get());
}

} // namespace InputCommon
