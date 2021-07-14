// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <thread>
#include "common/param_package.h"
#include "common/settings.h"
#include "input_common/analog_from_button.h"
#include "input_common/gcadapter/gc_adapter.h"
#include "input_common/gcadapter/gc_poller.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_from_button.h"
#include "input_common/mouse/mouse_input.h"
#include "input_common/mouse/mouse_poller.h"
#include "input_common/tas/tas_input.h"
#include "input_common/tas/tas_poller.h"
#include "input_common/touch_from_button.h"
#include "input_common/udp/client.h"
#include "input_common/udp/udp.h"
#ifdef HAVE_SDL2
#include "input_common/sdl/sdl.h"
#endif

namespace InputCommon {

struct InputSubsystem::Impl {
    void Initialize() {
        gcadapter = std::make_shared<GCAdapter::Adapter>();
        gcbuttons = std::make_shared<GCButtonFactory>(gcadapter);
        Input::RegisterFactory<Input::ButtonDevice>("gcpad", gcbuttons);
        gcanalog = std::make_shared<GCAnalogFactory>(gcadapter);
        Input::RegisterFactory<Input::AnalogDevice>("gcpad", gcanalog);
        gcvibration = std::make_shared<GCVibrationFactory>(gcadapter);
        Input::RegisterFactory<Input::VibrationDevice>("gcpad", gcvibration);

        keyboard = std::make_shared<Keyboard>();
        Input::RegisterFactory<Input::ButtonDevice>("keyboard", keyboard);
        Input::RegisterFactory<Input::AnalogDevice>("analog_from_button",
                                                    std::make_shared<AnalogFromButton>());
        Input::RegisterFactory<Input::MotionDevice>("keyboard",
                                                    std::make_shared<MotionFromButton>());
        Input::RegisterFactory<Input::TouchDevice>("touch_from_button",
                                                   std::make_shared<TouchFromButtonFactory>());

#ifdef HAVE_SDL2
        sdl = SDL::Init();
#endif

        udp = std::make_shared<InputCommon::CemuhookUDP::Client>();
        udpmotion = std::make_shared<UDPMotionFactory>(udp);
        Input::RegisterFactory<Input::MotionDevice>("cemuhookudp", udpmotion);
        udptouch = std::make_shared<UDPTouchFactory>(udp);
        Input::RegisterFactory<Input::TouchDevice>("cemuhookudp", udptouch);

        mouse = std::make_shared<MouseInput::Mouse>();
        mousebuttons = std::make_shared<MouseButtonFactory>(mouse);
        Input::RegisterFactory<Input::ButtonDevice>("mouse", mousebuttons);
        mouseanalog = std::make_shared<MouseAnalogFactory>(mouse);
        Input::RegisterFactory<Input::AnalogDevice>("mouse", mouseanalog);
        mousemotion = std::make_shared<MouseMotionFactory>(mouse);
        Input::RegisterFactory<Input::MotionDevice>("mouse", mousemotion);
        mousetouch = std::make_shared<MouseTouchFactory>(mouse);
        Input::RegisterFactory<Input::TouchDevice>("mouse", mousetouch);

        tas = std::make_shared<TasInput::Tas>();
        tasbuttons = std::make_shared<TasButtonFactory>(tas);
        Input::RegisterFactory<Input::ButtonDevice>("tas", tasbuttons);
        tasanalog = std::make_shared<TasAnalogFactory>(tas);
        Input::RegisterFactory<Input::AnalogDevice>("tas", tasanalog);
    }

    void Shutdown() {
        Input::UnregisterFactory<Input::ButtonDevice>("keyboard");
        Input::UnregisterFactory<Input::MotionDevice>("keyboard");
        keyboard.reset();
        Input::UnregisterFactory<Input::AnalogDevice>("analog_from_button");
        Input::UnregisterFactory<Input::TouchDevice>("touch_from_button");
#ifdef HAVE_SDL2
        sdl.reset();
#endif
        Input::UnregisterFactory<Input::ButtonDevice>("gcpad");
        Input::UnregisterFactory<Input::AnalogDevice>("gcpad");
        Input::UnregisterFactory<Input::VibrationDevice>("gcpad");

        gcbuttons.reset();
        gcanalog.reset();
        gcvibration.reset();

        Input::UnregisterFactory<Input::MotionDevice>("cemuhookudp");
        Input::UnregisterFactory<Input::TouchDevice>("cemuhookudp");

        udpmotion.reset();
        udptouch.reset();

        Input::UnregisterFactory<Input::ButtonDevice>("mouse");
        Input::UnregisterFactory<Input::AnalogDevice>("mouse");
        Input::UnregisterFactory<Input::MotionDevice>("mouse");
        Input::UnregisterFactory<Input::TouchDevice>("mouse");

        mousebuttons.reset();
        mouseanalog.reset();
        mousemotion.reset();
        mousetouch.reset();

        Input::UnregisterFactory<Input::ButtonDevice>("tas");
        Input::UnregisterFactory<Input::AnalogDevice>("tas");

        tasbuttons.reset();
        tasanalog.reset();
    }

    [[nodiscard]] std::vector<Common::ParamPackage> GetInputDevices() const {
        std::vector<Common::ParamPackage> devices = {
            Common::ParamPackage{{"display", "Any"}, {"class", "any"}},
            Common::ParamPackage{{"display", "Keyboard/Mouse"}, {"class", "keyboard"}},
        };
        if (Settings::values.tas_enable) {
            devices.emplace_back(
                Common::ParamPackage{{"display", "TAS Controller"}, {"class", "tas"}});
        }
#ifdef HAVE_SDL2
        auto sdl_devices = sdl->GetInputDevices();
        devices.insert(devices.end(), sdl_devices.begin(), sdl_devices.end());
#endif
        auto udp_devices = udp->GetInputDevices();
        devices.insert(devices.end(), udp_devices.begin(), udp_devices.end());
        auto gcpad_devices = gcadapter->GetInputDevices();
        devices.insert(devices.end(), gcpad_devices.begin(), gcpad_devices.end());
        return devices;
    }

    [[nodiscard]] AnalogMapping GetAnalogMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("class") || params.Get("class", "") == "any") {
            return {};
        }
        if (params.Get("class", "") == "gcpad") {
            return gcadapter->GetAnalogMappingForDevice(params);
        }
        if (params.Get("class", "") == "tas") {
            return tas->GetAnalogMappingForDevice(params);
        }
#ifdef HAVE_SDL2
        if (params.Get("class", "") == "sdl") {
            return sdl->GetAnalogMappingForDevice(params);
        }
#endif
        return {};
    }

    [[nodiscard]] ButtonMapping GetButtonMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("class") || params.Get("class", "") == "any") {
            return {};
        }
        if (params.Get("class", "") == "gcpad") {
            return gcadapter->GetButtonMappingForDevice(params);
        }
        if (params.Get("class", "") == "tas") {
            return tas->GetButtonMappingForDevice(params);
        }
#ifdef HAVE_SDL2
        if (params.Get("class", "") == "sdl") {
            return sdl->GetButtonMappingForDevice(params);
        }
#endif
        return {};
    }

    [[nodiscard]] MotionMapping GetMotionMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("class") || params.Get("class", "") == "any") {
            return {};
        }
        if (params.Get("class", "") == "cemuhookudp") {
            // TODO return the correct motion device
            return {};
        }
#ifdef HAVE_SDL2
        if (params.Get("class", "") == "sdl") {
            return sdl->GetMotionMappingForDevice(params);
        }
#endif
        return {};
    }

    std::shared_ptr<Keyboard> keyboard;
#ifdef HAVE_SDL2
    std::unique_ptr<SDL::State> sdl;
#endif
    std::shared_ptr<GCButtonFactory> gcbuttons;
    std::shared_ptr<GCAnalogFactory> gcanalog;
    std::shared_ptr<GCVibrationFactory> gcvibration;
    std::shared_ptr<UDPMotionFactory> udpmotion;
    std::shared_ptr<UDPTouchFactory> udptouch;
    std::shared_ptr<MouseButtonFactory> mousebuttons;
    std::shared_ptr<MouseAnalogFactory> mouseanalog;
    std::shared_ptr<MouseMotionFactory> mousemotion;
    std::shared_ptr<MouseTouchFactory> mousetouch;
    std::shared_ptr<TasButtonFactory> tasbuttons;
    std::shared_ptr<TasAnalogFactory> tasanalog;
    std::shared_ptr<CemuhookUDP::Client> udp;
    std::shared_ptr<GCAdapter::Adapter> gcadapter;
    std::shared_ptr<MouseInput::Mouse> mouse;
    std::shared_ptr<TasInput::Tas> tas;
};

InputSubsystem::InputSubsystem() : impl{std::make_unique<Impl>()} {}

InputSubsystem::~InputSubsystem() = default;

void InputSubsystem::Initialize() {
    impl->Initialize();
}

void InputSubsystem::Shutdown() {
    impl->Shutdown();
}

Keyboard* InputSubsystem::GetKeyboard() {
    return impl->keyboard.get();
}

const Keyboard* InputSubsystem::GetKeyboard() const {
    return impl->keyboard.get();
}

MouseInput::Mouse* InputSubsystem::GetMouse() {
    return impl->mouse.get();
}

const MouseInput::Mouse* InputSubsystem::GetMouse() const {
    return impl->mouse.get();
}

TasInput::Tas* InputSubsystem::GetTas() {
    return impl->tas.get();
}

const TasInput::Tas* InputSubsystem::GetTas() const {
    return impl->tas.get();
}

std::vector<Common::ParamPackage> InputSubsystem::GetInputDevices() const {
    return impl->GetInputDevices();
}

AnalogMapping InputSubsystem::GetAnalogMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetAnalogMappingForDevice(device);
}

ButtonMapping InputSubsystem::GetButtonMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetButtonMappingForDevice(device);
}

MotionMapping InputSubsystem::GetMotionMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetMotionMappingForDevice(device);
}

GCAnalogFactory* InputSubsystem::GetGCAnalogs() {
    return impl->gcanalog.get();
}

const GCAnalogFactory* InputSubsystem::GetGCAnalogs() const {
    return impl->gcanalog.get();
}

GCButtonFactory* InputSubsystem::GetGCButtons() {
    return impl->gcbuttons.get();
}

const GCButtonFactory* InputSubsystem::GetGCButtons() const {
    return impl->gcbuttons.get();
}

UDPMotionFactory* InputSubsystem::GetUDPMotions() {
    return impl->udpmotion.get();
}

const UDPMotionFactory* InputSubsystem::GetUDPMotions() const {
    return impl->udpmotion.get();
}

UDPTouchFactory* InputSubsystem::GetUDPTouch() {
    return impl->udptouch.get();
}

const UDPTouchFactory* InputSubsystem::GetUDPTouch() const {
    return impl->udptouch.get();
}

MouseButtonFactory* InputSubsystem::GetMouseButtons() {
    return impl->mousebuttons.get();
}

const MouseButtonFactory* InputSubsystem::GetMouseButtons() const {
    return impl->mousebuttons.get();
}

MouseAnalogFactory* InputSubsystem::GetMouseAnalogs() {
    return impl->mouseanalog.get();
}

const MouseAnalogFactory* InputSubsystem::GetMouseAnalogs() const {
    return impl->mouseanalog.get();
}

MouseMotionFactory* InputSubsystem::GetMouseMotions() {
    return impl->mousemotion.get();
}

const MouseMotionFactory* InputSubsystem::GetMouseMotions() const {
    return impl->mousemotion.get();
}

MouseTouchFactory* InputSubsystem::GetMouseTouch() {
    return impl->mousetouch.get();
}

const MouseTouchFactory* InputSubsystem::GetMouseTouch() const {
    return impl->mousetouch.get();
}

TasButtonFactory* InputSubsystem::GetTasButtons() {
    return impl->tasbuttons.get();
}

const TasButtonFactory* InputSubsystem::GetTasButtons() const {
    return impl->tasbuttons.get();
}

TasAnalogFactory* InputSubsystem::GetTasAnalogs() {
    return impl->tasanalog.get();
}

const TasAnalogFactory* InputSubsystem::GetTasAnalogs() const {
    return impl->tasanalog.get();
}

void InputSubsystem::ReloadInputDevices() {
    if (!impl->udp) {
        return;
    }
    impl->udp->ReloadSockets();
}

std::vector<std::unique_ptr<Polling::DevicePoller>> InputSubsystem::GetPollers([
    [maybe_unused]] Polling::DeviceType type) const {
#ifdef HAVE_SDL2
    return impl->sdl->GetPollers(type);
#else
    return {};
#endif
}

std::string GenerateKeyboardParam(int key_code) {
    Common::ParamPackage param{
        {"engine", "keyboard"},
        {"code", std::to_string(key_code)},
    };
    return param.Serialize();
}

std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale) {
    Common::ParamPackage circle_pad_param{
        {"engine", "analog_from_button"},
        {"up", GenerateKeyboardParam(key_up)},
        {"down", GenerateKeyboardParam(key_down)},
        {"left", GenerateKeyboardParam(key_left)},
        {"right", GenerateKeyboardParam(key_right)},
        {"modifier", GenerateKeyboardParam(key_modifier)},
        {"modifier_scale", std::to_string(modifier_scale)},
    };
    return circle_pad_param.Serialize();
}
} // namespace InputCommon
