// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <SDL.h>

#include "common/common_types.h"
#include "common/threadsafe_queue.h"
#include "input_common/sdl/sdl.h"

union SDL_Event;
using SDL_GameController = struct _SDL_GameController;
using SDL_Joystick = struct _SDL_Joystick;
using SDL_JoystickID = s32;

using ButtonBindings =
    std::array<std::pair<Settings::NativeButton::Values, SDL_GameControllerButton>, 17>;
using ZButtonBindings =
    std::array<std::pair<Settings::NativeButton::Values, SDL_GameControllerAxis>, 2>;

namespace InputCommon::SDL {

class SDLAnalogFactory;
class SDLButtonFactory;
class SDLMotionFactory;
class SDLVibrationFactory;
class SDLJoystick;

class SDLState : public State {
public:
    /// Initializes and registers SDL device factories
    SDLState();

    /// Unregisters SDL device factories and shut them down.
    ~SDLState() override;

    /// Handle SDL_Events for joysticks from SDL_PollEvent
    void HandleGameControllerEvent(const SDL_Event& event);

    /// Get the nth joystick with the corresponding GUID
    std::shared_ptr<SDLJoystick> GetSDLJoystickBySDLID(SDL_JoystickID sdl_id);

    /**
     * Check how many identical joysticks (by guid) were connected before the one with sdl_id and so
     * tie it to a SDLJoystick with the same guid and that port
     */
    std::shared_ptr<SDLJoystick> GetSDLJoystickByGUID(const std::string& guid, int port);

    /// Get all DevicePoller that use the SDL backend for a specific device type
    Pollers GetPollers(Polling::DeviceType type) override;

    /// Used by the Pollers during config
    std::atomic<bool> polling = false;
    Common::SPSCQueue<SDL_Event> event_queue;

    std::vector<Common::ParamPackage> GetInputDevices() override;

    ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage& params) override;
    AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage& params) override;
    MotionMapping GetMotionMappingForDevice(const Common::ParamPackage& params) override;

private:
    void InitJoystick(int joystick_index);
    void CloseJoystick(SDL_Joystick* sdl_joystick);

    /// Needs to be called before SDL_QuitSubSystem.
    void CloseJoysticks();

    /// Returns the default button bindings list for generic controllers
    ButtonBindings GetDefaultButtonBinding() const;

    /// Returns the default button bindings list for nintendo controllers
    ButtonBindings GetNintendoButtonBinding(const std::shared_ptr<SDLJoystick>& joystick) const;

    /// Returns the button mappings from a single controller
    ButtonMapping GetSingleControllerMapping(const std::shared_ptr<SDLJoystick>& joystick,
                                             const ButtonBindings& switch_to_sdl_button,
                                             const ZButtonBindings& switch_to_sdl_axis) const;

    /// Returns the button mappings from two different controllers
    ButtonMapping GetDualControllerMapping(const std::shared_ptr<SDLJoystick>& joystick,
                                           const std::shared_ptr<SDLJoystick>& joystick2,
                                           const ButtonBindings& switch_to_sdl_button,
                                           const ZButtonBindings& switch_to_sdl_axis) const;

    /// Returns true if the button is on the left joycon
    bool IsButtonOnLeftSide(Settings::NativeButton::Values button) const;

    // Set to true if SDL supports game controller subsystem
    bool has_gamecontroller = false;

    /// Map of GUID of a list of corresponding virtual Joysticks
    std::unordered_map<std::string, std::vector<std::shared_ptr<SDLJoystick>>> joystick_map;
    std::mutex joystick_map_mutex;

    std::shared_ptr<SDLButtonFactory> button_factory;
    std::shared_ptr<SDLAnalogFactory> analog_factory;
    std::shared_ptr<SDLVibrationFactory> vibration_factory;
    std::shared_ptr<SDLMotionFactory> motion_factory;

    bool start_thread = false;
    std::atomic<bool> initialized = false;

    std::thread poll_thread;
};
} // namespace InputCommon::SDL
