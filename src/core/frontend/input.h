// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include "common/logging/log.h"
#include "common/param_package.h"
#include "common/quaternion.h"
#include "common/vector_math.h"

namespace Input {

enum class AnalogDirection : u8 {
    RIGHT,
    LEFT,
    UP,
    DOWN,
};
struct AnalogProperties {
    float deadzone;
    float range;
    float threshold;
};
template <typename StatusType>
struct InputCallback {
    std::function<void(StatusType)> on_change;
};

/// An abstract class template for an input device (a button, an analog input, etc.).
template <typename StatusType>
class InputDevice {
public:
    virtual ~InputDevice() = default;
    virtual StatusType GetStatus() const {
        return {};
    }
    virtual StatusType GetRawStatus() const {
        return GetStatus();
    }
    virtual AnalogProperties GetAnalogProperties() const {
        return {};
    }
    virtual bool GetAnalogDirectionStatus([[maybe_unused]] AnalogDirection direction) const {
        return {};
    }
    virtual bool SetRumblePlay([[maybe_unused]] f32 amp_low, [[maybe_unused]] f32 freq_low,
                               [[maybe_unused]] f32 amp_high,
                               [[maybe_unused]] f32 freq_high) const {
        return {};
    }
    void SetCallback(InputCallback<StatusType> callback_) {
        callback = std::move(callback_);
    }
    void TriggerOnChange() {
        if (callback.on_change) {
            callback.on_change(GetStatus());
        }
    }

private:
    InputCallback<StatusType> callback;
};

/// An abstract class template for a factory that can create input devices.
template <typename InputDeviceType>
class Factory {
public:
    virtual ~Factory() = default;
    virtual std::unique_ptr<InputDeviceType> Create(const Common::ParamPackage&) = 0;
};

namespace Impl {

template <typename InputDeviceType>
using FactoryListType = std::unordered_map<std::string, std::shared_ptr<Factory<InputDeviceType>>>;

template <typename InputDeviceType>
struct FactoryList {
    static FactoryListType<InputDeviceType> list;
};

template <typename InputDeviceType>
FactoryListType<InputDeviceType> FactoryList<InputDeviceType>::list;

} // namespace Impl

/**
 * Registers an input device factory.
 * @tparam InputDeviceType the type of input devices the factory can create
 * @param name the name of the factory. Will be used to match the "engine" parameter when creating
 *     a device
 * @param factory the factory object to register
 */
template <typename InputDeviceType>
void RegisterFactory(const std::string& name, std::shared_ptr<Factory<InputDeviceType>> factory) {
    auto pair = std::make_pair(name, std::move(factory));
    if (!Impl::FactoryList<InputDeviceType>::list.insert(std::move(pair)).second) {
        LOG_ERROR(Input, "Factory '{}' already registered", name);
    }
}

/**
 * Unregisters an input device factory.
 * @tparam InputDeviceType the type of input devices the factory can create
 * @param name the name of the factory to unregister
 */
template <typename InputDeviceType>
void UnregisterFactory(const std::string& name) {
    if (Impl::FactoryList<InputDeviceType>::list.erase(name) == 0) {
        LOG_ERROR(Input, "Factory '{}' not registered", name);
    }
}

/**
 * Create an input device from given paramters.
 * @tparam InputDeviceType the type of input devices to create
 * @param params a serialized ParamPackage string contains all parameters for creating the device
 */
template <typename InputDeviceType>
std::unique_ptr<InputDeviceType> CreateDevice(const std::string& params) {
    const Common::ParamPackage package(params);
    const std::string engine = package.Get("engine", "null");
    const auto& factory_list = Impl::FactoryList<InputDeviceType>::list;
    const auto pair = factory_list.find(engine);
    if (pair == factory_list.end()) {
        if (engine != "null") {
            LOG_ERROR(Input, "Unknown engine name: {}", engine);
        }
        return std::make_unique<InputDeviceType>();
    }
    return pair->second->Create(package);
}

/**
 * A button device is an input device that returns bool as status.
 * true for pressed; false for released.
 */
using ButtonDevice = InputDevice<bool>;

/**
 * An analog device is an input device that returns a tuple of x and y coordinates as status. The
 * coordinates are within the unit circle. x+ is defined as right direction, and y+ is defined as up
 * direction
 */
using AnalogDevice = InputDevice<std::tuple<float, float>>;

/**
 * A vibration device is an input device that returns an unsigned byte as status.
 * It represents whether the vibration device supports vibration or not.
 * If the status returns 1, it supports vibration. Otherwise, it does not support vibration.
 */
using VibrationDevice = InputDevice<u8>;

/**
 * A motion status is an object that returns a tuple of accelerometer state vector,
 * gyroscope state vector, rotation state vector, orientation state matrix and quaterion state
 * vector.
 *
 * For both 3D vectors:
 *   x+ is the same direction as RIGHT on D-pad.
 *   y+ is normal to the touch screen, pointing outward.
 *   z+ is the same direction as UP on D-pad.
 *
 * For accelerometer state vector
 *   Units: g (gravitational acceleration)
 *
 * For gyroscope state vector:
 *   Orientation is determined by right-hand rule.
 *   Units: deg/sec
 *
 * For rotation state vector
 *   Units: rotations
 *
 * For orientation state matrix
 *   x vector
 *   y vector
 *   z vector
 *
 * For quaternion state vector
 *   xyz vector
 *   w float
 */
using MotionStatus = std::tuple<Common::Vec3<float>, Common::Vec3<float>, Common::Vec3<float>,
                                std::array<Common::Vec3f, 3>, Common::Quaternion<f32>>;

/**
 * A motion device is an input device that returns a motion status object
 */
using MotionDevice = InputDevice<MotionStatus>;

/**
 * A touch status is an object that returns an array of 16 tuple elements of two floats and a bool.
 * The floats are x and y coordinates in the range 0.0 - 1.0, and the bool indicates whether it is
 * pressed.
 */
using TouchStatus = std::array<std::tuple<float, float, bool>, 16>;

/**
 * A touch device is an input device that returns a touch status object
 */
using TouchDevice = InputDevice<TouchStatus>;

/**
 * A mouse device is an input device that returns a tuple of two floats and four ints.
 * The first two floats are X and Y device coordinates of the mouse (from 0-1).
 * The s32s are the mouse wheel.
 */
using MouseDevice = InputDevice<std::tuple<float, float, s32, s32>>;

} // namespace Input
