// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/param_package.h"
#include "common/settings_input.h"
#include "common/threadsafe_queue.h"
#include "core/frontend/input.h"
#include "input_common/motion_input.h"
#include "input_common/sdl/sdl_impl.h"

namespace InputCommon::SDL {

namespace {
std::string GetGUID(SDL_Joystick* joystick) {
    const SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    char guid_str[33];
    SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
    return guid_str;
}

/// Creates a ParamPackage from an SDL_Event that can directly be used to create a ButtonDevice
Common::ParamPackage SDLEventToButtonParamPackage(SDLState& state, const SDL_Event& event);
} // Anonymous namespace

static int SDLEventWatcher(void* user_data, SDL_Event* event) {
    auto* const sdl_state = static_cast<SDLState*>(user_data);

    // Don't handle the event if we are configuring
    if (sdl_state->polling) {
        sdl_state->event_queue.Push(*event);
    } else {
        sdl_state->HandleGameControllerEvent(*event);
    }

    return 0;
}

class SDLJoystick {
public:
    SDLJoystick(std::string guid_, int port_, SDL_Joystick* joystick,
                SDL_GameController* game_controller)
        : guid{std::move(guid_)}, port{port_}, sdl_joystick{joystick, &SDL_JoystickClose},
          sdl_controller{game_controller, &SDL_GameControllerClose} {
        EnableMotion();
    }

    void EnableMotion() {
        if (sdl_controller) {
            SDL_GameController* controller = sdl_controller.get();
            if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) && !has_accel) {
                SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE);
                has_accel = true;
            }
            if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) && !has_gyro) {
                SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE);
                has_gyro = true;
            }
        }
    }

    void SetButton(int button, bool value) {
        std::lock_guard lock{mutex};
        state.buttons.insert_or_assign(button, value);
    }

    void SetMotion(SDL_ControllerSensorEvent event) {
        constexpr float gravity_constant = 9.80665f;
        std::lock_guard lock{mutex};
        u64 time_difference = event.timestamp - last_motion_update;
        last_motion_update = event.timestamp;
        switch (event.sensor) {
        case SDL_SENSOR_ACCEL: {
            const Common::Vec3f acceleration = {-event.data[0], event.data[2], -event.data[1]};
            motion.SetAcceleration(acceleration / gravity_constant);
            break;
        }
        case SDL_SENSOR_GYRO: {
            const Common::Vec3f gyroscope = {event.data[0], -event.data[2], event.data[1]};
            motion.SetGyroscope(gyroscope / (Common::PI * 2));
            break;
        }
        }

        // Ignore duplicated timestamps
        if (time_difference == 0) {
            return;
        }

        motion.SetGyroThreshold(0.0001f);
        motion.UpdateRotation(time_difference * 1000);
        motion.UpdateOrientation(time_difference * 1000);
    }

    bool GetButton(int button) const {
        std::lock_guard lock{mutex};
        return state.buttons.at(button);
    }

    bool ToggleButton(int button) {
        std::lock_guard lock{mutex};

        if (!state.toggle_buttons.contains(button) || !state.lock_buttons.contains(button)) {
            state.toggle_buttons.insert_or_assign(button, false);
            state.lock_buttons.insert_or_assign(button, false);
        }

        const bool button_state = state.toggle_buttons.at(button);
        const bool button_lock = state.lock_buttons.at(button);

        if (button_lock) {
            return button_state;
        }

        state.lock_buttons.insert_or_assign(button, true);

        if (button_state) {
            state.toggle_buttons.insert_or_assign(button, false);
        } else {
            state.toggle_buttons.insert_or_assign(button, true);
        }

        return !button_state;
    }

    bool UnlockButton(int button) {
        std::lock_guard lock{mutex};
        if (!state.toggle_buttons.contains(button)) {
            return false;
        }
        state.lock_buttons.insert_or_assign(button, false);
        return state.toggle_buttons.at(button);
    }

    void SetAxis(int axis, Sint16 value) {
        std::lock_guard lock{mutex};
        state.axes.insert_or_assign(axis, value);
    }

    float GetAxis(int axis, float range) const {
        std::lock_guard lock{mutex};
        return static_cast<float>(state.axes.at(axis)) / (32767.0f * range);
    }

    bool RumblePlay(u16 amp_low, u16 amp_high) {
        constexpr u32 rumble_max_duration_ms = 1000;

        if (sdl_controller) {
            return SDL_GameControllerRumble(sdl_controller.get(), amp_low, amp_high,
                                            rumble_max_duration_ms) == 0;
        } else if (sdl_joystick) {
            return SDL_JoystickRumble(sdl_joystick.get(), amp_low, amp_high,
                                      rumble_max_duration_ms) == 0;
        }

        return false;
    }

    std::tuple<float, float> GetAnalog(int axis_x, int axis_y, float range) const {
        float x = GetAxis(axis_x, range);
        float y = GetAxis(axis_y, range);
        y = -y; // 3DS uses an y-axis inverse from SDL

        // Make sure the coordinates are in the unit circle,
        // otherwise normalize it.
        float r = x * x + y * y;
        if (r > 1.0f) {
            r = std::sqrt(r);
            x /= r;
            y /= r;
        }

        return std::make_tuple(x, y);
    }

    bool HasGyro() const {
        return has_gyro;
    }

    bool HasAccel() const {
        return has_accel;
    }

    const MotionInput& GetMotion() const {
        return motion;
    }

    void SetHat(int hat, Uint8 direction) {
        std::lock_guard lock{mutex};
        state.hats.insert_or_assign(hat, direction);
    }

    bool GetHatDirection(int hat, Uint8 direction) const {
        std::lock_guard lock{mutex};
        return (state.hats.at(hat) & direction) != 0;
    }
    /**
     * The guid of the joystick
     */
    const std::string& GetGUID() const {
        return guid;
    }

    /**
     * The number of joystick from the same type that were connected before this joystick
     */
    int GetPort() const {
        return port;
    }

    SDL_Joystick* GetSDLJoystick() const {
        return sdl_joystick.get();
    }

    SDL_GameController* GetSDLGameController() const {
        return sdl_controller.get();
    }

    void SetSDLJoystick(SDL_Joystick* joystick, SDL_GameController* controller) {
        sdl_joystick.reset(joystick);
        sdl_controller.reset(controller);
    }

    bool IsJoyconLeft() const {
        return std::strstr(GetControllerName().c_str(), "Joy-Con Left") != nullptr;
    }

    bool IsJoyconRight() const {
        return std::strstr(GetControllerName().c_str(), "Joy-Con Right") != nullptr;
    }

    std::string GetControllerName() const {
        if (sdl_controller) {
            switch (SDL_GameControllerGetType(sdl_controller.get())) {
            case SDL_CONTROLLER_TYPE_XBOX360:
                return "XBox 360 Controller";
            case SDL_CONTROLLER_TYPE_XBOXONE:
                return "XBox One Controller";
            default:
                break;
            }
            const auto name = SDL_GameControllerName(sdl_controller.get());
            if (name) {
                return name;
            }
        }

        if (sdl_joystick) {
            const auto name = SDL_JoystickName(sdl_joystick.get());
            if (name) {
                return name;
            }
        }

        return "Unknown";
    }

private:
    struct State {
        std::unordered_map<int, bool> buttons;
        std::unordered_map<int, bool> toggle_buttons{};
        std::unordered_map<int, bool> lock_buttons{};
        std::unordered_map<int, Sint16> axes;
        std::unordered_map<int, Uint8> hats;
    } state;
    std::string guid;
    int port;
    std::unique_ptr<SDL_Joystick, decltype(&SDL_JoystickClose)> sdl_joystick;
    std::unique_ptr<SDL_GameController, decltype(&SDL_GameControllerClose)> sdl_controller;
    mutable std::mutex mutex;

    // Motion is initialized with the PID values
    MotionInput motion{0.3f, 0.005f, 0.0f};
    u64 last_motion_update{};
    bool has_gyro{false};
    bool has_accel{false};
};

std::shared_ptr<SDLJoystick> SDLState::GetSDLJoystickByGUID(const std::string& guid, int port) {
    std::lock_guard lock{joystick_map_mutex};
    const auto it = joystick_map.find(guid);

    if (it != joystick_map.end()) {
        while (it->second.size() <= static_cast<std::size_t>(port)) {
            auto joystick = std::make_shared<SDLJoystick>(guid, static_cast<int>(it->second.size()),
                                                          nullptr, nullptr);
            it->second.emplace_back(std::move(joystick));
        }

        return it->second[static_cast<std::size_t>(port)];
    }

    auto joystick = std::make_shared<SDLJoystick>(guid, 0, nullptr, nullptr);

    return joystick_map[guid].emplace_back(std::move(joystick));
}

std::shared_ptr<SDLJoystick> SDLState::GetSDLJoystickBySDLID(SDL_JoystickID sdl_id) {
    auto sdl_joystick = SDL_JoystickFromInstanceID(sdl_id);
    const std::string guid = GetGUID(sdl_joystick);

    std::lock_guard lock{joystick_map_mutex};
    const auto map_it = joystick_map.find(guid);

    if (map_it == joystick_map.end()) {
        return nullptr;
    }

    const auto vec_it = std::find_if(map_it->second.begin(), map_it->second.end(),
                                     [&sdl_joystick](const auto& joystick) {
                                         return joystick->GetSDLJoystick() == sdl_joystick;
                                     });

    if (vec_it == map_it->second.end()) {
        return nullptr;
    }

    return *vec_it;
}

void SDLState::InitJoystick(int joystick_index) {
    SDL_Joystick* sdl_joystick = SDL_JoystickOpen(joystick_index);
    SDL_GameController* sdl_gamecontroller = nullptr;

    if (SDL_IsGameController(joystick_index)) {
        sdl_gamecontroller = SDL_GameControllerOpen(joystick_index);
    }

    if (!sdl_joystick) {
        LOG_ERROR(Input, "Failed to open joystick {}", joystick_index);
        return;
    }

    const std::string guid = GetGUID(sdl_joystick);

    std::lock_guard lock{joystick_map_mutex};
    if (joystick_map.find(guid) == joystick_map.end()) {
        auto joystick = std::make_shared<SDLJoystick>(guid, 0, sdl_joystick, sdl_gamecontroller);
        joystick_map[guid].emplace_back(std::move(joystick));
        return;
    }

    auto& joystick_guid_list = joystick_map[guid];
    const auto joystick_it =
        std::find_if(joystick_guid_list.begin(), joystick_guid_list.end(),
                     [](const auto& joystick) { return !joystick->GetSDLJoystick(); });

    if (joystick_it != joystick_guid_list.end()) {
        (*joystick_it)->SetSDLJoystick(sdl_joystick, sdl_gamecontroller);
        return;
    }

    const int port = static_cast<int>(joystick_guid_list.size());
    auto joystick = std::make_shared<SDLJoystick>(guid, port, sdl_joystick, sdl_gamecontroller);
    joystick_guid_list.emplace_back(std::move(joystick));
}

void SDLState::CloseJoystick(SDL_Joystick* sdl_joystick) {
    const std::string guid = GetGUID(sdl_joystick);

    std::lock_guard lock{joystick_map_mutex};
    // This call to guid is safe since the joystick is guaranteed to be in the map
    const auto& joystick_guid_list = joystick_map[guid];
    const auto joystick_it = std::find_if(joystick_guid_list.begin(), joystick_guid_list.end(),
                                          [&sdl_joystick](const auto& joystick) {
                                              return joystick->GetSDLJoystick() == sdl_joystick;
                                          });

    if (joystick_it != joystick_guid_list.end()) {
        (*joystick_it)->SetSDLJoystick(nullptr, nullptr);
    }
}

void SDLState::HandleGameControllerEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_JOYBUTTONUP: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jbutton.which)) {
            joystick->SetButton(event.jbutton.button, false);
        }
        break;
    }
    case SDL_JOYBUTTONDOWN: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jbutton.which)) {
            joystick->SetButton(event.jbutton.button, true);
        }
        break;
    }
    case SDL_JOYHATMOTION: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jhat.which)) {
            joystick->SetHat(event.jhat.hat, event.jhat.value);
        }
        break;
    }
    case SDL_JOYAXISMOTION: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jaxis.which)) {
            joystick->SetAxis(event.jaxis.axis, event.jaxis.value);
        }
        break;
    }
    case SDL_CONTROLLERSENSORUPDATE: {
        if (auto joystick = GetSDLJoystickBySDLID(event.csensor.which)) {
            joystick->SetMotion(event.csensor);
        }
        break;
    }
    case SDL_JOYDEVICEREMOVED:
        LOG_DEBUG(Input, "Controller removed with Instance_ID {}", event.jdevice.which);
        CloseJoystick(SDL_JoystickFromInstanceID(event.jdevice.which));
        break;
    case SDL_JOYDEVICEADDED:
        LOG_DEBUG(Input, "Controller connected with device index {}", event.jdevice.which);
        InitJoystick(event.jdevice.which);
        break;
    }
}

void SDLState::CloseJoysticks() {
    std::lock_guard lock{joystick_map_mutex};
    joystick_map.clear();
}

class SDLButton final : public Input::ButtonDevice {
public:
    explicit SDLButton(std::shared_ptr<SDLJoystick> joystick_, int button_, bool toggle_)
        : joystick(std::move(joystick_)), button(button_), toggle(toggle_) {}

    bool GetStatus() const override {
        const bool button_state = joystick->GetButton(button);
        if (!toggle) {
            return button_state;
        }

        if (button_state) {
            return joystick->ToggleButton(button);
        }
        return joystick->UnlockButton(button);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int button;
    bool toggle;
};

class SDLDirectionButton final : public Input::ButtonDevice {
public:
    explicit SDLDirectionButton(std::shared_ptr<SDLJoystick> joystick_, int hat_, Uint8 direction_)
        : joystick(std::move(joystick_)), hat(hat_), direction(direction_) {}

    bool GetStatus() const override {
        return joystick->GetHatDirection(hat, direction);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int hat;
    Uint8 direction;
};

class SDLAxisButton final : public Input::ButtonDevice {
public:
    explicit SDLAxisButton(std::shared_ptr<SDLJoystick> joystick_, int axis_, float threshold_,
                           bool trigger_if_greater_)
        : joystick(std::move(joystick_)), axis(axis_), threshold(threshold_),
          trigger_if_greater(trigger_if_greater_) {}

    bool GetStatus() const override {
        const float axis_value = joystick->GetAxis(axis, 1.0f);
        if (trigger_if_greater) {
            return axis_value > threshold;
        }
        return axis_value < threshold;
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int axis;
    float threshold;
    bool trigger_if_greater;
};

class SDLAnalog final : public Input::AnalogDevice {
public:
    explicit SDLAnalog(std::shared_ptr<SDLJoystick> joystick_, int axis_x_, int axis_y_,
                       bool invert_x_, bool invert_y_, float deadzone_, float range_)
        : joystick(std::move(joystick_)), axis_x(axis_x_), axis_y(axis_y_), invert_x(invert_x_),
          invert_y(invert_y_), deadzone(deadzone_), range(range_) {}

    std::tuple<float, float> GetStatus() const override {
        auto [x, y] = joystick->GetAnalog(axis_x, axis_y, range);
        const float r = std::sqrt((x * x) + (y * y));
        if (invert_x) {
            x = -x;
        }
        if (invert_y) {
            y = -y;
        }

        if (r > deadzone) {
            return std::make_tuple(x / r * (r - deadzone) / (1 - deadzone),
                                   y / r * (r - deadzone) / (1 - deadzone));
        }
        return {};
    }

    std::tuple<float, float> GetRawStatus() const override {
        const float x = joystick->GetAxis(axis_x, range);
        const float y = joystick->GetAxis(axis_y, range);
        return {x, -y};
    }

    Input::AnalogProperties GetAnalogProperties() const override {
        return {deadzone, range, 0.5f};
    }

    bool GetAnalogDirectionStatus(Input::AnalogDirection direction) const override {
        const auto [x, y] = GetStatus();
        const float directional_deadzone = 0.5f;
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
    std::shared_ptr<SDLJoystick> joystick;
    const int axis_x;
    const int axis_y;
    const bool invert_x;
    const bool invert_y;
    const float deadzone;
    const float range;
};

class SDLVibration final : public Input::VibrationDevice {
public:
    explicit SDLVibration(std::shared_ptr<SDLJoystick> joystick_)
        : joystick(std::move(joystick_)) {}

    u8 GetStatus() const override {
        joystick->RumblePlay(1, 1);
        return joystick->RumblePlay(0, 0);
    }

    bool SetRumblePlay(f32 amp_low, [[maybe_unused]] f32 freq_low, f32 amp_high,
                       [[maybe_unused]] f32 freq_high) const override {
        const auto process_amplitude = [](f32 amplitude) {
            return static_cast<u16>((amplitude + std::pow(amplitude, 0.3f)) * 0.5f * 0xFFFF);
        };

        const auto processed_amp_low = process_amplitude(amp_low);
        const auto processed_amp_high = process_amplitude(amp_high);

        return joystick->RumblePlay(processed_amp_low, processed_amp_high);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
};

class SDLMotion final : public Input::MotionDevice {
public:
    explicit SDLMotion(std::shared_ptr<SDLJoystick> joystick_) : joystick(std::move(joystick_)) {}

    Input::MotionStatus GetStatus() const override {
        return joystick->GetMotion().GetMotion();
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
};

class SDLDirectionMotion final : public Input::MotionDevice {
public:
    explicit SDLDirectionMotion(std::shared_ptr<SDLJoystick> joystick_, int hat_, Uint8 direction_)
        : joystick(std::move(joystick_)), hat(hat_), direction(direction_) {}

    Input::MotionStatus GetStatus() const override {
        if (joystick->GetHatDirection(hat, direction)) {
            return joystick->GetMotion().GetRandomMotion(2, 6);
        }
        return joystick->GetMotion().GetRandomMotion(0, 0);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int hat;
    Uint8 direction;
};

class SDLAxisMotion final : public Input::MotionDevice {
public:
    explicit SDLAxisMotion(std::shared_ptr<SDLJoystick> joystick_, int axis_, float threshold_,
                           bool trigger_if_greater_)
        : joystick(std::move(joystick_)), axis(axis_), threshold(threshold_),
          trigger_if_greater(trigger_if_greater_) {}

    Input::MotionStatus GetStatus() const override {
        const float axis_value = joystick->GetAxis(axis, 1.0f);
        bool trigger = axis_value < threshold;
        if (trigger_if_greater) {
            trigger = axis_value > threshold;
        }

        if (trigger) {
            return joystick->GetMotion().GetRandomMotion(2, 6);
        }
        return joystick->GetMotion().GetRandomMotion(0, 0);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int axis;
    float threshold;
    bool trigger_if_greater;
};

class SDLButtonMotion final : public Input::MotionDevice {
public:
    explicit SDLButtonMotion(std::shared_ptr<SDLJoystick> joystick_, int button_)
        : joystick(std::move(joystick_)), button(button_) {}

    Input::MotionStatus GetStatus() const override {
        if (joystick->GetButton(button)) {
            return joystick->GetMotion().GetRandomMotion(2, 6);
        }
        return joystick->GetMotion().GetRandomMotion(0, 0);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int button;
};

/// A button device factory that creates button devices from SDL joystick
class SDLButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    explicit SDLButtonFactory(SDLState& state_) : state(state_) {}

    /**
     * Creates a button device from a joystick button
     * @param params contains parameters for creating the device:
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type to bind
     *     - "button"(optional): the index of the button to bind
     *     - "hat"(optional): the index of the hat to bind as direction buttons
     *     - "axis"(optional): the index of the axis to bind
     *     - "direction"(only used for hat): the direction name of the hat to bind. Can be "up",
     *         "down", "left" or "right"
     *     - "threshold"(only used for axis): a float value in (-1.0, 1.0) which the button is
     *         triggered if the axis value crosses
     *     - "direction"(only used for axis): "+" means the button is triggered when the axis
     * value is greater than the threshold; "-" means the button is triggered when the axis
     * value is smaller than the threshold
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);
        const auto toggle = params.Get("toggle", false);

        auto joystick = state.GetSDLJoystickByGUID(guid, port);

        if (params.Has("hat")) {
            const int hat = params.Get("hat", 0);
            const std::string direction_name = params.Get("direction", "");
            Uint8 direction;
            if (direction_name == "up") {
                direction = SDL_HAT_UP;
            } else if (direction_name == "down") {
                direction = SDL_HAT_DOWN;
            } else if (direction_name == "left") {
                direction = SDL_HAT_LEFT;
            } else if (direction_name == "right") {
                direction = SDL_HAT_RIGHT;
            } else {
                direction = 0;
            }
            // This is necessary so accessing GetHat with hat won't crash
            joystick->SetHat(hat, SDL_HAT_CENTERED);
            return std::make_unique<SDLDirectionButton>(joystick, hat, direction);
        }

        if (params.Has("axis")) {
            const int axis = params.Get("axis", 0);
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
            // This is necessary so accessing GetAxis with axis won't crash
            joystick->SetAxis(axis, 0);
            return std::make_unique<SDLAxisButton>(joystick, axis, threshold, trigger_if_greater);
        }

        const int button = params.Get("button", 0);
        // This is necessary so accessing GetButton with button won't crash
        joystick->SetButton(button, false);
        return std::make_unique<SDLButton>(joystick, button, toggle);
    }

private:
    SDLState& state;
};

/// An analog device factory that creates analog devices from SDL joystick
class SDLAnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    explicit SDLAnalogFactory(SDLState& state_) : state(state_) {}
    /**
     * Creates an analog device from joystick axes
     * @param params contains parameters for creating the device:
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type
     *     - "axis_x": the index of the axis to be bind as x-axis
     *     - "axis_y": the index of the axis to be bind as y-axis
     */
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);
        const int axis_x = params.Get("axis_x", 0);
        const int axis_y = params.Get("axis_y", 1);
        const float deadzone = std::clamp(params.Get("deadzone", 0.0f), 0.0f, 1.0f);
        const float range = std::clamp(params.Get("range", 1.0f), 0.50f, 1.50f);
        const std::string invert_x_value = params.Get("invert_x", "+");
        const std::string invert_y_value = params.Get("invert_y", "+");
        const bool invert_x = invert_x_value == "-";
        const bool invert_y = invert_y_value == "-";
        auto joystick = state.GetSDLJoystickByGUID(guid, port);

        // This is necessary so accessing GetAxis with axis_x and axis_y won't crash
        joystick->SetAxis(axis_x, 0);
        joystick->SetAxis(axis_y, 0);
        return std::make_unique<SDLAnalog>(joystick, axis_x, axis_y, invert_x, invert_y, deadzone,
                                           range);
    }

private:
    SDLState& state;
};

/// An vibration device factory that creates vibration devices from SDL joystick
class SDLVibrationFactory final : public Input::Factory<Input::VibrationDevice> {
public:
    explicit SDLVibrationFactory(SDLState& state_) : state(state_) {}
    /**
     * Creates a vibration device from a joystick
     * @param params contains parameters for creating the device:
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type
     */
    std::unique_ptr<Input::VibrationDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);
        return std::make_unique<SDLVibration>(state.GetSDLJoystickByGUID(guid, port));
    }

private:
    SDLState& state;
};

/// A motion device factory that creates motion devices from SDL joystick
class SDLMotionFactory final : public Input::Factory<Input::MotionDevice> {
public:
    explicit SDLMotionFactory(SDLState& state_) : state(state_) {}
    /**
     * Creates motion device from joystick axes
     * @param params contains parameters for creating the device:
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type
     */
    std::unique_ptr<Input::MotionDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);

        auto joystick = state.GetSDLJoystickByGUID(guid, port);

        if (params.Has("motion")) {
            return std::make_unique<SDLMotion>(joystick);
        }

        if (params.Has("hat")) {
            const int hat = params.Get("hat", 0);
            const std::string direction_name = params.Get("direction", "");
            Uint8 direction;
            if (direction_name == "up") {
                direction = SDL_HAT_UP;
            } else if (direction_name == "down") {
                direction = SDL_HAT_DOWN;
            } else if (direction_name == "left") {
                direction = SDL_HAT_LEFT;
            } else if (direction_name == "right") {
                direction = SDL_HAT_RIGHT;
            } else {
                direction = 0;
            }
            // This is necessary so accessing GetHat with hat won't crash
            joystick->SetHat(hat, SDL_HAT_CENTERED);
            return std::make_unique<SDLDirectionMotion>(joystick, hat, direction);
        }

        if (params.Has("axis")) {
            const int axis = params.Get("axis", 0);
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
            // This is necessary so accessing GetAxis with axis won't crash
            joystick->SetAxis(axis, 0);
            return std::make_unique<SDLAxisMotion>(joystick, axis, threshold, trigger_if_greater);
        }

        const int button = params.Get("button", 0);
        // This is necessary so accessing GetButton with button won't crash
        joystick->SetButton(button, false);
        return std::make_unique<SDLButtonMotion>(joystick, button);
    }

private:
    SDLState& state;
};

SDLState::SDLState() {
    using namespace Input;
    button_factory = std::make_shared<SDLButtonFactory>(*this);
    analog_factory = std::make_shared<SDLAnalogFactory>(*this);
    vibration_factory = std::make_shared<SDLVibrationFactory>(*this);
    motion_factory = std::make_shared<SDLMotionFactory>(*this);
    RegisterFactory<ButtonDevice>("sdl", button_factory);
    RegisterFactory<AnalogDevice>("sdl", analog_factory);
    RegisterFactory<VibrationDevice>("sdl", vibration_factory);
    RegisterFactory<MotionDevice>("sdl", motion_factory);

    // Enable HIDAPI rumble. This prevents SDL from disabling motion on PS4 and PS5 controllers
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

    // Tell SDL2 to use the hidapi driver. This will allow joycons to be detected as a
    // GameController and not a generic one
    SDL_SetHint("SDL_JOYSTICK_HIDAPI_JOY_CONS", "1");

    // Turn off Pro controller home led
    SDL_SetHint("SDL_JOYSTICK_HIDAPI_SWITCH_HOME_LED", "0");

    // If the frontend is going to manage the event loop, then we don't start one here
    start_thread = SDL_WasInit(SDL_INIT_JOYSTICK) == 0;
    if (start_thread && SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        LOG_CRITICAL(Input, "SDL_Init(SDL_INIT_JOYSTICK) failed with: {}", SDL_GetError());
        return;
    }
    has_gamecontroller = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0;
    if (SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1") == SDL_FALSE) {
        LOG_ERROR(Input, "Failed to set hint for background events with: {}", SDL_GetError());
    }

    SDL_AddEventWatch(&SDLEventWatcher, this);

    initialized = true;
    if (start_thread) {
        poll_thread = std::thread([this] {
            using namespace std::chrono_literals;
            while (initialized) {
                SDL_PumpEvents();
                std::this_thread::sleep_for(1ms);
            }
        });
    }
    // Because the events for joystick connection happens before we have our event watcher added, we
    // can just open all the joysticks right here
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        InitJoystick(i);
    }
}

SDLState::~SDLState() {
    using namespace Input;
    UnregisterFactory<ButtonDevice>("sdl");
    UnregisterFactory<AnalogDevice>("sdl");
    UnregisterFactory<VibrationDevice>("sdl");
    UnregisterFactory<MotionDevice>("sdl");

    CloseJoysticks();
    SDL_DelEventWatch(&SDLEventWatcher, this);

    initialized = false;
    if (start_thread) {
        poll_thread.join();
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }
}

std::vector<Common::ParamPackage> SDLState::GetInputDevices() {
    std::scoped_lock lock(joystick_map_mutex);
    std::vector<Common::ParamPackage> devices;
    std::unordered_map<int, std::shared_ptr<SDLJoystick>> joycon_pairs;
    for (const auto& [key, value] : joystick_map) {
        for (const auto& joystick : value) {
            if (!joystick->GetSDLJoystick()) {
                continue;
            }
            std::string name =
                fmt::format("{} {}", joystick->GetControllerName(), joystick->GetPort());
            devices.emplace_back(Common::ParamPackage{
                {"class", "sdl"},
                {"display", std::move(name)},
                {"guid", joystick->GetGUID()},
                {"port", std::to_string(joystick->GetPort())},
            });
            if (joystick->IsJoyconLeft()) {
                joycon_pairs.insert_or_assign(joystick->GetPort(), joystick);
            }
        }
    }

    // Add dual controllers
    for (const auto& [key, value] : joystick_map) {
        for (const auto& joystick : value) {
            if (joystick->IsJoyconRight()) {
                if (!joycon_pairs.contains(joystick->GetPort())) {
                    continue;
                }
                const auto joystick2 = joycon_pairs.at(joystick->GetPort());

                std::string name =
                    fmt::format("{} {}", "Nintendo Dual Joy-Con", joystick->GetPort());
                devices.emplace_back(Common::ParamPackage{
                    {"class", "sdl"},
                    {"display", std::move(name)},
                    {"guid", joystick->GetGUID()},
                    {"guid2", joystick2->GetGUID()},
                    {"port", std::to_string(joystick->GetPort())},
                });
            }
        }
    }
    return devices;
}

namespace {
Common::ParamPackage BuildAnalogParamPackageForButton(int port, std::string guid, s32 axis,
                                                      float value = 0.1f) {
    Common::ParamPackage params({{"engine", "sdl"}});
    params.Set("port", port);
    params.Set("guid", std::move(guid));
    params.Set("axis", axis);
    if (value > 0) {
        params.Set("direction", "+");
        params.Set("threshold", "0.5");
    } else {
        params.Set("direction", "-");
        params.Set("threshold", "-0.5");
    }
    return params;
}

Common::ParamPackage BuildButtonParamPackageForButton(int port, std::string guid, s32 button) {
    Common::ParamPackage params({{"engine", "sdl"}});
    params.Set("port", port);
    params.Set("guid", std::move(guid));
    params.Set("button", button);
    return params;
}

Common::ParamPackage BuildHatParamPackageForButton(int port, std::string guid, s32 hat, s32 value) {
    Common::ParamPackage params({{"engine", "sdl"}});

    params.Set("port", port);
    params.Set("guid", std::move(guid));
    params.Set("hat", hat);
    switch (value) {
    case SDL_HAT_UP:
        params.Set("direction", "up");
        break;
    case SDL_HAT_DOWN:
        params.Set("direction", "down");
        break;
    case SDL_HAT_LEFT:
        params.Set("direction", "left");
        break;
    case SDL_HAT_RIGHT:
        params.Set("direction", "right");
        break;
    default:
        return {};
    }
    return params;
}

Common::ParamPackage BuildMotionParam(int port, std::string guid) {
    Common::ParamPackage params({{"engine", "sdl"}, {"motion", "0"}});
    params.Set("port", port);
    params.Set("guid", std::move(guid));
    return params;
}

Common::ParamPackage SDLEventToButtonParamPackage(SDLState& state, const SDL_Event& event) {
    switch (event.type) {
    case SDL_JOYAXISMOTION: {
        if (const auto joystick = state.GetSDLJoystickBySDLID(event.jaxis.which)) {
            return BuildAnalogParamPackageForButton(joystick->GetPort(), joystick->GetGUID(),
                                                    static_cast<s32>(event.jaxis.axis),
                                                    event.jaxis.value);
        }
        break;
    }
    case SDL_JOYBUTTONUP: {
        if (const auto joystick = state.GetSDLJoystickBySDLID(event.jbutton.which)) {
            return BuildButtonParamPackageForButton(joystick->GetPort(), joystick->GetGUID(),
                                                    static_cast<s32>(event.jbutton.button));
        }
        break;
    }
    case SDL_JOYHATMOTION: {
        if (const auto joystick = state.GetSDLJoystickBySDLID(event.jhat.which)) {
            return BuildHatParamPackageForButton(joystick->GetPort(), joystick->GetGUID(),
                                                 static_cast<s32>(event.jhat.hat),
                                                 static_cast<s32>(event.jhat.value));
        }
        break;
    }
    }
    return {};
}

Common::ParamPackage SDLEventToMotionParamPackage(SDLState& state, const SDL_Event& event) {
    switch (event.type) {
    case SDL_JOYAXISMOTION: {
        if (const auto joystick = state.GetSDLJoystickBySDLID(event.jaxis.which)) {
            return BuildAnalogParamPackageForButton(joystick->GetPort(), joystick->GetGUID(),
                                                    static_cast<s32>(event.jaxis.axis),
                                                    event.jaxis.value);
        }
        break;
    }
    case SDL_JOYBUTTONUP: {
        if (const auto joystick = state.GetSDLJoystickBySDLID(event.jbutton.which)) {
            return BuildButtonParamPackageForButton(joystick->GetPort(), joystick->GetGUID(),
                                                    static_cast<s32>(event.jbutton.button));
        }
        break;
    }
    case SDL_JOYHATMOTION: {
        if (const auto joystick = state.GetSDLJoystickBySDLID(event.jhat.which)) {
            return BuildHatParamPackageForButton(joystick->GetPort(), joystick->GetGUID(),
                                                 static_cast<s32>(event.jhat.hat),
                                                 static_cast<s32>(event.jhat.value));
        }
        break;
    }
    case SDL_CONTROLLERSENSORUPDATE: {
        bool is_motion_shaking = false;
        constexpr float gyro_threshold = 5.0f;
        constexpr float accel_threshold = 11.0f;
        if (event.csensor.sensor == SDL_SENSOR_ACCEL) {
            const Common::Vec3f acceleration = {-event.csensor.data[0], event.csensor.data[2],
                                                -event.csensor.data[1]};
            if (acceleration.Length() > accel_threshold) {
                is_motion_shaking = true;
            }
        }

        if (event.csensor.sensor == SDL_SENSOR_GYRO) {
            const Common::Vec3f gyroscope = {event.csensor.data[0], -event.csensor.data[2],
                                             event.csensor.data[1]};
            if (gyroscope.Length() > gyro_threshold) {
                is_motion_shaking = true;
            }
        }

        if (!is_motion_shaking) {
            break;
        }

        if (const auto joystick = state.GetSDLJoystickBySDLID(event.csensor.which)) {
            return BuildMotionParam(joystick->GetPort(), joystick->GetGUID());
        }
        break;
    }
    }
    return {};
}

Common::ParamPackage BuildParamPackageForBinding(int port, const std::string& guid,
                                                 const SDL_GameControllerButtonBind& binding) {
    switch (binding.bindType) {
    case SDL_CONTROLLER_BINDTYPE_NONE:
        break;
    case SDL_CONTROLLER_BINDTYPE_AXIS:
        return BuildAnalogParamPackageForButton(port, guid, binding.value.axis);
    case SDL_CONTROLLER_BINDTYPE_BUTTON:
        return BuildButtonParamPackageForButton(port, guid, binding.value.button);
    case SDL_CONTROLLER_BINDTYPE_HAT:
        return BuildHatParamPackageForButton(port, guid, binding.value.hat.hat,
                                             binding.value.hat.hat_mask);
    }
    return {};
}

Common::ParamPackage BuildParamPackageForAnalog(int port, const std::string& guid, int axis_x,
                                                int axis_y) {
    Common::ParamPackage params;
    params.Set("engine", "sdl");
    params.Set("port", port);
    params.Set("guid", guid);
    params.Set("axis_x", axis_x);
    params.Set("axis_y", axis_y);
    params.Set("invert_x", "+");
    params.Set("invert_y", "+");
    return params;
}
} // Anonymous namespace

ButtonMapping SDLState::GetButtonMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("guid") || !params.Has("port")) {
        return {};
    }
    const auto joystick = GetSDLJoystickByGUID(params.Get("guid", ""), params.Get("port", 0));

    auto* controller = joystick->GetSDLGameController();
    if (controller == nullptr) {
        return {};
    }

    // This list is missing ZL/ZR since those are not considered buttons in SDL GameController.
    // We will add those afterwards
    // This list also excludes Screenshot since theres not really a mapping for that
    ButtonBindings switch_to_sdl_button;

    if (SDL_GameControllerGetType(controller) == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO) {
        switch_to_sdl_button = GetNintendoButtonBinding(joystick);
    } else {
        switch_to_sdl_button = GetDefaultButtonBinding();
    }

    // Add the missing bindings for ZL/ZR
    static constexpr ZButtonBindings switch_to_sdl_axis{{
        {Settings::NativeButton::ZL, SDL_CONTROLLER_AXIS_TRIGGERLEFT},
        {Settings::NativeButton::ZR, SDL_CONTROLLER_AXIS_TRIGGERRIGHT},
    }};

    // Parameters contain two joysticks return dual
    if (params.Has("guid2")) {
        const auto joystick2 = GetSDLJoystickByGUID(params.Get("guid2", ""), params.Get("port", 0));

        if (joystick2->GetSDLGameController() != nullptr) {
            return GetDualControllerMapping(joystick, joystick2, switch_to_sdl_button,
                                            switch_to_sdl_axis);
        }
    }

    return GetSingleControllerMapping(joystick, switch_to_sdl_button, switch_to_sdl_axis);
}

ButtonBindings SDLState::GetDefaultButtonBinding() const {
    return {
        std::pair{Settings::NativeButton::A, SDL_CONTROLLER_BUTTON_B},
        {Settings::NativeButton::B, SDL_CONTROLLER_BUTTON_A},
        {Settings::NativeButton::X, SDL_CONTROLLER_BUTTON_Y},
        {Settings::NativeButton::Y, SDL_CONTROLLER_BUTTON_X},
        {Settings::NativeButton::LStick, SDL_CONTROLLER_BUTTON_LEFTSTICK},
        {Settings::NativeButton::RStick, SDL_CONTROLLER_BUTTON_RIGHTSTICK},
        {Settings::NativeButton::L, SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
        {Settings::NativeButton::R, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
        {Settings::NativeButton::Plus, SDL_CONTROLLER_BUTTON_START},
        {Settings::NativeButton::Minus, SDL_CONTROLLER_BUTTON_BACK},
        {Settings::NativeButton::DLeft, SDL_CONTROLLER_BUTTON_DPAD_LEFT},
        {Settings::NativeButton::DUp, SDL_CONTROLLER_BUTTON_DPAD_UP},
        {Settings::NativeButton::DRight, SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
        {Settings::NativeButton::DDown, SDL_CONTROLLER_BUTTON_DPAD_DOWN},
        {Settings::NativeButton::SL, SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
        {Settings::NativeButton::SR, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
        {Settings::NativeButton::Home, SDL_CONTROLLER_BUTTON_GUIDE},
    };
}

ButtonBindings SDLState::GetNintendoButtonBinding(
    const std::shared_ptr<SDLJoystick>& joystick) const {
    // Default SL/SR mapping for pro controllers
    auto sl_button = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    auto sr_button = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;

    if (joystick->IsJoyconLeft()) {
        sl_button = SDL_CONTROLLER_BUTTON_PADDLE2;
        sr_button = SDL_CONTROLLER_BUTTON_PADDLE4;
    }
    if (joystick->IsJoyconRight()) {
        sl_button = SDL_CONTROLLER_BUTTON_PADDLE3;
        sr_button = SDL_CONTROLLER_BUTTON_PADDLE1;
    }

    return {
        std::pair{Settings::NativeButton::A, SDL_CONTROLLER_BUTTON_A},
        {Settings::NativeButton::B, SDL_CONTROLLER_BUTTON_B},
        {Settings::NativeButton::X, SDL_CONTROLLER_BUTTON_X},
        {Settings::NativeButton::Y, SDL_CONTROLLER_BUTTON_Y},
        {Settings::NativeButton::LStick, SDL_CONTROLLER_BUTTON_LEFTSTICK},
        {Settings::NativeButton::RStick, SDL_CONTROLLER_BUTTON_RIGHTSTICK},
        {Settings::NativeButton::L, SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
        {Settings::NativeButton::R, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
        {Settings::NativeButton::Plus, SDL_CONTROLLER_BUTTON_START},
        {Settings::NativeButton::Minus, SDL_CONTROLLER_BUTTON_BACK},
        {Settings::NativeButton::DLeft, SDL_CONTROLLER_BUTTON_DPAD_LEFT},
        {Settings::NativeButton::DUp, SDL_CONTROLLER_BUTTON_DPAD_UP},
        {Settings::NativeButton::DRight, SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
        {Settings::NativeButton::DDown, SDL_CONTROLLER_BUTTON_DPAD_DOWN},
        {Settings::NativeButton::SL, sl_button},
        {Settings::NativeButton::SR, sr_button},
        {Settings::NativeButton::Home, SDL_CONTROLLER_BUTTON_GUIDE},
    };
}

ButtonMapping SDLState::GetSingleControllerMapping(
    const std::shared_ptr<SDLJoystick>& joystick, const ButtonBindings& switch_to_sdl_button,
    const ZButtonBindings& switch_to_sdl_axis) const {
    ButtonMapping mapping;
    mapping.reserve(switch_to_sdl_button.size() + switch_to_sdl_axis.size());
    auto* controller = joystick->GetSDLGameController();

    for (const auto& [switch_button, sdl_button] : switch_to_sdl_button) {
        const auto& binding = SDL_GameControllerGetBindForButton(controller, sdl_button);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(joystick->GetPort(), joystick->GetGUID(), binding));
    }
    for (const auto& [switch_button, sdl_axis] : switch_to_sdl_axis) {
        const auto& binding = SDL_GameControllerGetBindForAxis(controller, sdl_axis);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(joystick->GetPort(), joystick->GetGUID(), binding));
    }

    return mapping;
}

ButtonMapping SDLState::GetDualControllerMapping(const std::shared_ptr<SDLJoystick>& joystick,
                                                 const std::shared_ptr<SDLJoystick>& joystick2,
                                                 const ButtonBindings& switch_to_sdl_button,
                                                 const ZButtonBindings& switch_to_sdl_axis) const {
    ButtonMapping mapping;
    mapping.reserve(switch_to_sdl_button.size() + switch_to_sdl_axis.size());
    auto* controller = joystick->GetSDLGameController();
    auto* controller2 = joystick2->GetSDLGameController();

    for (const auto& [switch_button, sdl_button] : switch_to_sdl_button) {
        if (IsButtonOnLeftSide(switch_button)) {
            const auto& binding = SDL_GameControllerGetBindForButton(controller2, sdl_button);
            mapping.insert_or_assign(
                switch_button,
                BuildParamPackageForBinding(joystick2->GetPort(), joystick2->GetGUID(), binding));
            continue;
        }
        const auto& binding = SDL_GameControllerGetBindForButton(controller, sdl_button);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(joystick->GetPort(), joystick->GetGUID(), binding));
    }
    for (const auto& [switch_button, sdl_axis] : switch_to_sdl_axis) {
        if (IsButtonOnLeftSide(switch_button)) {
            const auto& binding = SDL_GameControllerGetBindForAxis(controller2, sdl_axis);
            mapping.insert_or_assign(
                switch_button,
                BuildParamPackageForBinding(joystick2->GetPort(), joystick2->GetGUID(), binding));
            continue;
        }
        const auto& binding = SDL_GameControllerGetBindForAxis(controller, sdl_axis);
        mapping.insert_or_assign(
            switch_button,
            BuildParamPackageForBinding(joystick->GetPort(), joystick->GetGUID(), binding));
    }

    return mapping;
}

bool SDLState::IsButtonOnLeftSide(Settings::NativeButton::Values button) const {
    switch (button) {
    case Settings::NativeButton::DDown:
    case Settings::NativeButton::DLeft:
    case Settings::NativeButton::DRight:
    case Settings::NativeButton::DUp:
    case Settings::NativeButton::L:
    case Settings::NativeButton::LStick:
    case Settings::NativeButton::Minus:
    case Settings::NativeButton::Screenshot:
    case Settings::NativeButton::ZL:
        return true;
    default:
        return false;
    }
}

AnalogMapping SDLState::GetAnalogMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("guid") || !params.Has("port")) {
        return {};
    }
    const auto joystick = GetSDLJoystickByGUID(params.Get("guid", ""), params.Get("port", 0));
    const auto joystick2 = GetSDLJoystickByGUID(params.Get("guid2", ""), params.Get("port", 0));
    auto* controller = joystick->GetSDLGameController();
    if (controller == nullptr) {
        return {};
    }

    AnalogMapping mapping = {};
    const auto& binding_left_x =
        SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    const auto& binding_left_y =
        SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    if (params.Has("guid2")) {
        mapping.insert_or_assign(
            Settings::NativeAnalog::LStick,
            BuildParamPackageForAnalog(joystick2->GetPort(), joystick2->GetGUID(),
                                       binding_left_x.value.axis, binding_left_y.value.axis));
    } else {
        mapping.insert_or_assign(
            Settings::NativeAnalog::LStick,
            BuildParamPackageForAnalog(joystick->GetPort(), joystick->GetGUID(),
                                       binding_left_x.value.axis, binding_left_y.value.axis));
    }
    const auto& binding_right_x =
        SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
    const auto& binding_right_y =
        SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);
    mapping.insert_or_assign(Settings::NativeAnalog::RStick,
                             BuildParamPackageForAnalog(joystick->GetPort(), joystick->GetGUID(),
                                                        binding_right_x.value.axis,
                                                        binding_right_y.value.axis));
    return mapping;
}

MotionMapping SDLState::GetMotionMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("guid") || !params.Has("port")) {
        return {};
    }
    const auto joystick = GetSDLJoystickByGUID(params.Get("guid", ""), params.Get("port", 0));
    const auto joystick2 = GetSDLJoystickByGUID(params.Get("guid2", ""), params.Get("port", 0));
    auto* controller = joystick->GetSDLGameController();
    if (controller == nullptr) {
        return {};
    }

    MotionMapping mapping = {};
    joystick->EnableMotion();

    if (joystick->HasGyro() || joystick->HasAccel()) {
        mapping.insert_or_assign(Settings::NativeMotion::MotionRight,
                                 BuildMotionParam(joystick->GetPort(), joystick->GetGUID()));
    }
    if (params.Has("guid2")) {
        joystick2->EnableMotion();
        if (joystick2->HasGyro() || joystick2->HasAccel()) {
            mapping.insert_or_assign(Settings::NativeMotion::MotionLeft,
                                     BuildMotionParam(joystick2->GetPort(), joystick2->GetGUID()));
        }
    } else {
        if (joystick->HasGyro() || joystick->HasAccel()) {
            mapping.insert_or_assign(Settings::NativeMotion::MotionLeft,
                                     BuildMotionParam(joystick->GetPort(), joystick->GetGUID()));
        }
    }

    return mapping;
}
namespace Polling {
class SDLPoller : public InputCommon::Polling::DevicePoller {
public:
    explicit SDLPoller(SDLState& state_) : state(state_) {}

    void Start([[maybe_unused]] const std::string& device_id) override {
        state.event_queue.Clear();
        state.polling = true;
    }

    void Stop() override {
        state.polling = false;
    }

protected:
    SDLState& state;
};

class SDLButtonPoller final : public SDLPoller {
public:
    explicit SDLButtonPoller(SDLState& state_) : SDLPoller(state_) {}

    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        while (state.event_queue.Pop(event)) {
            const auto package = FromEvent(event);
            if (package) {
                return *package;
            }
        }
        return {};
    }
    [[nodiscard]] std::optional<Common::ParamPackage> FromEvent(SDL_Event& event) {
        switch (event.type) {
        case SDL_JOYAXISMOTION:
            if (!axis_memory.count(event.jaxis.which) ||
                !axis_memory[event.jaxis.which].count(event.jaxis.axis)) {
                axis_memory[event.jaxis.which][event.jaxis.axis] = event.jaxis.value;
                axis_event_count[event.jaxis.which][event.jaxis.axis] = 1;
                break;
            } else {
                axis_event_count[event.jaxis.which][event.jaxis.axis]++;
                // The joystick and axis exist in our map if we take this branch, so no checks
                // needed
                if (std::abs(
                        (event.jaxis.value - axis_memory[event.jaxis.which][event.jaxis.axis]) /
                        32767.0) < 0.5) {
                    break;
                } else {
                    if (axis_event_count[event.jaxis.which][event.jaxis.axis] == 2 &&
                        IsAxisAtPole(event.jaxis.value) &&
                        IsAxisAtPole(axis_memory[event.jaxis.which][event.jaxis.axis])) {
                        // If we have exactly two events and both are near a pole, this is
                        // likely a digital input masquerading as an analog axis; Instead of
                        // trying to look at the direction the axis travelled, assume the first
                        // event was press and the second was release; This should handle most
                        // digital axes while deferring to the direction of travel for analog
                        // axes
                        event.jaxis.value = static_cast<Sint16>(
                            std::copysign(32767, axis_memory[event.jaxis.which][event.jaxis.axis]));
                    } else {
                        // There are more than two events, so this is likely a true analog axis,
                        // check the direction it travelled
                        event.jaxis.value = static_cast<Sint16>(std::copysign(
                            32767,
                            event.jaxis.value - axis_memory[event.jaxis.which][event.jaxis.axis]));
                    }
                    axis_memory.clear();
                    axis_event_count.clear();
                }
            }
            [[fallthrough]];
        case SDL_JOYBUTTONUP:
        case SDL_JOYHATMOTION:
            return {SDLEventToButtonParamPackage(state, event)};
        }
        return std::nullopt;
    }

private:
    // Determine whether an axis value is close to an extreme or center
    // Some controllers have a digital D-Pad as a pair of analog sticks, with 3 possible values per
    // axis, which is why the center must be considered a pole
    bool IsAxisAtPole(int16_t value) const {
        return std::abs(value) >= 32767 || std::abs(value) < 327;
    }
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, int16_t>> axis_memory;
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, uint32_t>> axis_event_count;
};

class SDLMotionPoller final : public SDLPoller {
public:
    explicit SDLMotionPoller(SDLState& state_) : SDLPoller(state_) {}

    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        while (state.event_queue.Pop(event)) {
            const auto package = FromEvent(event);
            if (package) {
                return *package;
            }
        }
        return {};
    }
    [[nodiscard]] std::optional<Common::ParamPackage> FromEvent(const SDL_Event& event) const {
        switch (event.type) {
        case SDL_JOYAXISMOTION:
            if (std::abs(event.jaxis.value / 32767.0) < 0.5) {
                break;
            }
            [[fallthrough]];
        case SDL_JOYBUTTONUP:
        case SDL_JOYHATMOTION:
        case SDL_CONTROLLERSENSORUPDATE:
            return {SDLEventToMotionParamPackage(state, event)};
        }
        return std::nullopt;
    }
};

/**
 * Attempts to match the press to a controller joy axis (left/right stick) and if a match
 * isn't found, checks if the event matches anything from SDLButtonPoller and uses that
 * instead
 */
class SDLAnalogPreferredPoller final : public SDLPoller {
public:
    explicit SDLAnalogPreferredPoller(SDLState& state_)
        : SDLPoller(state_), button_poller(state_) {}

    void Start(const std::string& device_id) override {
        SDLPoller::Start(device_id);
        // Reset stored axes
        first_axis = -1;
    }

    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        while (state.event_queue.Pop(event)) {
            if (event.type != SDL_JOYAXISMOTION) {
                // Check for a button press
                auto button_press = button_poller.FromEvent(event);
                if (button_press) {
                    return *button_press;
                }
                continue;
            }
            const auto axis = event.jaxis.axis;

            // Filter out axis events that are below a threshold
            if (std::abs(event.jaxis.value / 32767.0) < 0.5) {
                continue;
            }

            // Filter out axis events that are the same
            if (first_axis == axis) {
                continue;
            }

            // In order to return a complete analog param, we need inputs for both axes.
            // If the first axis isn't set we set the value then wait till next event
            if (first_axis == -1) {
                first_axis = axis;
                continue;
            }

            if (const auto joystick = state.GetSDLJoystickBySDLID(event.jaxis.which)) {
                auto params = BuildParamPackageForAnalog(joystick->GetPort(), joystick->GetGUID(),
                                                         first_axis, axis);
                first_axis = -1;
                return params;
            }
        }
        return {};
    }

private:
    int first_axis = -1;
    SDLButtonPoller button_poller;
};
} // namespace Polling

SDLState::Pollers SDLState::GetPollers(InputCommon::Polling::DeviceType type) {
    Pollers pollers;

    switch (type) {
    case InputCommon::Polling::DeviceType::AnalogPreferred:
        pollers.emplace_back(std::make_unique<Polling::SDLAnalogPreferredPoller>(*this));
        break;
    case InputCommon::Polling::DeviceType::Button:
        pollers.emplace_back(std::make_unique<Polling::SDLButtonPoller>(*this));
        break;
    case InputCommon::Polling::DeviceType::Motion:
        pollers.emplace_back(std::make_unique<Polling::SDLMotionPoller>(*this));
        break;
    }

    return pollers;
}

} // namespace InputCommon::SDL
