// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/frontend/input.h"
#include "input_common/joycon/jc_adapter.h"

namespace InputCommon {

/**
 * A button device factory representing a jcpad. It receives jcpad events and forward them
 * to all button devices it created.
 */
class JCButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    explicit JCButtonFactory(std::shared_ptr<JCAdapter::Joycons> adapter_);

    /**
     * Creates a button device from a button press
     * @param params contains parameters for creating the device:
     *     - "code": the code of the key to bind with the button
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override;

    Common::ParamPackage GetNextInput();

    /// For device input configuration/polling
    void BeginConfiguration();
    void EndConfiguration();

    bool IsPolling() const {
        return polling;
    }

private:
    std::shared_ptr<JCAdapter::Joycons> adapter;
    bool polling = false;
};

/// An analog device factory that creates analog devices from JC Adapter
class JCAnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    explicit JCAnalogFactory(std::shared_ptr<JCAdapter::Joycons> adapter_);

    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override;
    Common::ParamPackage GetNextInput();

    /// For device input configuration/polling
    void BeginConfiguration();
    void EndConfiguration();

    bool IsPolling() const {
        return polling;
    }

private:
    std::shared_ptr<JCAdapter::Joycons> adapter;
    int analog_x_axis = -1;
    int analog_y_axis = -1;
    int controller_number = -1;
    bool polling = false;
};

/// A motion device factory that creates motion devices from JC Adapter
class JCMotionFactory final : public Input::Factory<Input::MotionDevice> {
public:
    explicit JCMotionFactory(std::shared_ptr<JCAdapter::Joycons> adapter_);

    std::unique_ptr<Input::MotionDevice> Create(const Common::ParamPackage& params) override;

    Common::ParamPackage GetNextInput();

    /// For device input configuration/polling
    void BeginConfiguration();
    void EndConfiguration();

    bool IsPolling() const {
        return polling;
    }

private:
    std::shared_ptr<JCAdapter::Joycons> adapter;
    bool polling = false;
};

} // namespace InputCommon
