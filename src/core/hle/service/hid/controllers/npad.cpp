// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/input.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "core/hle/service/kernel_helpers.h"

namespace Service::HID {
constexpr s32 HID_JOYSTICK_MAX = 0x7fff;
constexpr s32 HID_TRIGGER_MAX = 0x7fff;
[[maybe_unused]] constexpr s32 HID_JOYSTICK_MIN = -0x7fff;
constexpr std::size_t NPAD_OFFSET = 0x9A00;
constexpr u32 BATTERY_FULL = 2;
constexpr u32 MAX_NPAD_ID = 7;
constexpr std::size_t HANDHELD_INDEX = 8;
constexpr std::array<u32, 10> npad_id_list{
    0, 1, 2, 3, 4, 5, 6, 7, NPAD_HANDHELD, NPAD_UNKNOWN,
};

enum class JoystickId : std::size_t {
    Joystick_Left,
    Joystick_Right,
};

Controller_NPad::NPadControllerType Controller_NPad::MapSettingsTypeToNPad(
    Settings::ControllerType type) {
    switch (type) {
    case Settings::ControllerType::ProController:
        return NPadControllerType::ProController;
    case Settings::ControllerType::DualJoyconDetached:
        return NPadControllerType::JoyDual;
    case Settings::ControllerType::LeftJoycon:
        return NPadControllerType::JoyLeft;
    case Settings::ControllerType::RightJoycon:
        return NPadControllerType::JoyRight;
    case Settings::ControllerType::Handheld:
        return NPadControllerType::Handheld;
    case Settings::ControllerType::GameCube:
        return NPadControllerType::GameCube;
    default:
        UNREACHABLE();
        return NPadControllerType::ProController;
    }
}

Settings::ControllerType Controller_NPad::MapNPadToSettingsType(
    Controller_NPad::NPadControllerType type) {
    switch (type) {
    case NPadControllerType::ProController:
        return Settings::ControllerType::ProController;
    case NPadControllerType::JoyDual:
        return Settings::ControllerType::DualJoyconDetached;
    case NPadControllerType::JoyLeft:
        return Settings::ControllerType::LeftJoycon;
    case NPadControllerType::JoyRight:
        return Settings::ControllerType::RightJoycon;
    case NPadControllerType::Handheld:
        return Settings::ControllerType::Handheld;
    case NPadControllerType::GameCube:
        return Settings::ControllerType::GameCube;
    default:
        UNREACHABLE();
        return Settings::ControllerType::ProController;
    }
}

std::size_t Controller_NPad::NPadIdToIndex(u32 npad_id) {
    switch (npad_id) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        return npad_id;
    case HANDHELD_INDEX:
    case NPAD_HANDHELD:
        return HANDHELD_INDEX;
    case 9:
    case NPAD_UNKNOWN:
        return 9;
    default:
        UNIMPLEMENTED_MSG("Unknown npad id {}", npad_id);
        return 0;
    }
}

u32 Controller_NPad::IndexToNPad(std::size_t index) {
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        return static_cast<u32>(index);
    case HANDHELD_INDEX:
        return NPAD_HANDHELD;
    case 9:
        return NPAD_UNKNOWN;
    default:
        UNIMPLEMENTED_MSG("Unknown npad index {}", index);
        return 0;
    }
}

bool Controller_NPad::IsNpadIdValid(u32 npad_id) {
    switch (npad_id) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case NPAD_UNKNOWN:
    case NPAD_HANDHELD:
        return true;
    default:
        LOG_ERROR(Service_HID, "Invalid npad id {}", npad_id);
        return false;
    }
}

bool Controller_NPad::IsDeviceHandleValid(const DeviceHandle& device_handle) {
    return IsNpadIdValid(device_handle.npad_id) &&
           device_handle.npad_type < NpadType::MaxNpadType &&
           device_handle.device_index < DeviceIndex::MaxDeviceIndex;
}

Controller_NPad::Controller_NPad(Core::System& system_,
                                 KernelHelpers::ServiceContext& service_context_)
    : ControllerBase{system_}, service_context{service_context_} {
    latest_vibration_values.fill({DEFAULT_VIBRATION_VALUE, DEFAULT_VIBRATION_VALUE});
}

Controller_NPad::~Controller_NPad() {
    OnRelease();
}

void Controller_NPad::InitNewlyAddedController(std::size_t controller_idx) {
    const auto controller_type = connected_controllers[controller_idx].type;
    auto& controller = shared_memory_entries[controller_idx];
    if (controller_type == NPadControllerType::None) {
        styleset_changed_events[controller_idx]->GetWritableEvent().Signal();
        return;
    }
    controller.style_set.raw = 0; // Zero out
    controller.device_type.raw = 0;
    controller.system_properties.raw = 0;
    switch (controller_type) {
    case NPadControllerType::None:
        UNREACHABLE();
        break;
    case NPadControllerType::ProController:
        controller.style_set.fullkey.Assign(1);
        controller.device_type.fullkey.Assign(1);
        controller.system_properties.is_vertical.Assign(1);
        controller.system_properties.use_plus.Assign(1);
        controller.system_properties.use_minus.Assign(1);
        controller.assignment_mode = NpadAssignments::Single;
        controller.footer_type = AppletFooterUiType::SwitchProController;
        break;
    case NPadControllerType::Handheld:
        controller.style_set.handheld.Assign(1);
        controller.device_type.handheld_left.Assign(1);
        controller.device_type.handheld_right.Assign(1);
        controller.system_properties.is_vertical.Assign(1);
        controller.system_properties.use_plus.Assign(1);
        controller.system_properties.use_minus.Assign(1);
        controller.assignment_mode = NpadAssignments::Dual;
        controller.footer_type = AppletFooterUiType::HandheldJoyConLeftJoyConRight;
        break;
    case NPadControllerType::JoyDual:
        controller.style_set.joycon_dual.Assign(1);
        controller.device_type.joycon_left.Assign(1);
        controller.device_type.joycon_right.Assign(1);
        controller.system_properties.is_vertical.Assign(1);
        controller.system_properties.use_plus.Assign(1);
        controller.system_properties.use_minus.Assign(1);
        controller.assignment_mode = NpadAssignments::Dual;
        controller.footer_type = AppletFooterUiType::JoyDual;
        break;
    case NPadControllerType::JoyLeft:
        controller.style_set.joycon_left.Assign(1);
        controller.device_type.joycon_left.Assign(1);
        controller.system_properties.is_horizontal.Assign(1);
        controller.system_properties.use_minus.Assign(1);
        controller.assignment_mode = NpadAssignments::Single;
        controller.footer_type = AppletFooterUiType::JoyLeftHorizontal;
        break;
    case NPadControllerType::JoyRight:
        controller.style_set.joycon_right.Assign(1);
        controller.device_type.joycon_right.Assign(1);
        controller.system_properties.is_horizontal.Assign(1);
        controller.system_properties.use_plus.Assign(1);
        controller.assignment_mode = NpadAssignments::Single;
        controller.footer_type = AppletFooterUiType::JoyRightHorizontal;
        break;
    case NPadControllerType::GameCube:
        controller.style_set.gamecube.Assign(1);
        // The GC Controller behaves like a wired Pro Controller
        controller.device_type.fullkey.Assign(1);
        controller.system_properties.is_vertical.Assign(1);
        controller.system_properties.use_plus.Assign(1);
        break;
    case NPadControllerType::Pokeball:
        controller.style_set.palma.Assign(1);
        controller.device_type.palma.Assign(1);
        controller.assignment_mode = NpadAssignments::Single;
        break;
    }

    controller.fullkey_color.attribute = ColorAttributes::Ok;
    controller.fullkey_color.fullkey.body = 0;
    controller.fullkey_color.fullkey.button = 0;

    controller.joycon_color.attribute = ColorAttributes::Ok;
    controller.joycon_color.left.body =
        Settings::values.players.GetValue()[controller_idx].body_color_left;
    controller.joycon_color.left.button =
        Settings::values.players.GetValue()[controller_idx].button_color_left;
    controller.joycon_color.right.body =
        Settings::values.players.GetValue()[controller_idx].body_color_right;
    controller.joycon_color.right.button =
        Settings::values.players.GetValue()[controller_idx].button_color_right;

    // TODO: Investigate when we should report all batery types
    controller.battery_level_dual = BATTERY_FULL;
    controller.battery_level_left = BATTERY_FULL;
    controller.battery_level_right = BATTERY_FULL;

    SignalStyleSetChangedEvent(IndexToNPad(controller_idx));
}

void Controller_NPad::OnInit() {
    auto& kernel = system.Kernel();
    for (std::size_t i = 0; i < styleset_changed_events.size(); ++i) {
        styleset_changed_events[i] =
            service_context.CreateEvent(fmt::format("npad:NpadStyleSetChanged_{}", i));
    }

    if (!IsControllerActivated()) {
        return;
    }

    OnLoadInputDevices();

    if (style.raw == 0) {
        // We want to support all controllers
        style.handheld.Assign(1);
        style.joycon_left.Assign(1);
        style.joycon_right.Assign(1);
        style.joycon_dual.Assign(1);
        style.fullkey.Assign(1);
        style.gamecube.Assign(1);
        style.palma.Assign(1);
    }

    std::transform(Settings::values.players.GetValue().begin(),
                   Settings::values.players.GetValue().end(), connected_controllers.begin(),
                   [](const Settings::PlayerInput& player) {
                       return ControllerHolder{MapSettingsTypeToNPad(player.controller_type),
                                               player.connected};
                   });

    // Connect the Player 1 or Handheld controller if none are connected.
    if (std::none_of(connected_controllers.begin(), connected_controllers.end(),
                     [](const ControllerHolder& controller) { return controller.is_connected; })) {
        const auto controller =
            MapSettingsTypeToNPad(Settings::values.players.GetValue()[0].controller_type);
        if (controller == NPadControllerType::Handheld) {
            Settings::values.players.GetValue()[HANDHELD_INDEX].connected = true;
            connected_controllers[HANDHELD_INDEX] = {controller, true};
        } else {
            Settings::values.players.GetValue()[0].connected = true;
            connected_controllers[0] = {controller, true};
        }
    }

    // Account for handheld
    if (connected_controllers[HANDHELD_INDEX].is_connected) {
        connected_controllers[HANDHELD_INDEX].type = NPadControllerType::Handheld;
    }

    supported_npad_id_types.resize(npad_id_list.size());
    std::memcpy(supported_npad_id_types.data(), npad_id_list.data(),
                npad_id_list.size() * sizeof(u32));

    for (std::size_t i = 0; i < connected_controllers.size(); ++i) {
        const auto& controller = connected_controllers[i];
        if (controller.is_connected) {
            AddNewControllerAt(controller.type, i);
        }
    }
}

void Controller_NPad::OnLoadInputDevices() {
    const auto& players = Settings::values.players.GetValue();

    std::lock_guard lock{mutex};
    for (std::size_t i = 0; i < players.size(); ++i) {
        std::transform(players[i].buttons.begin() + Settings::NativeButton::BUTTON_HID_BEGIN,
                       players[i].buttons.begin() + Settings::NativeButton::BUTTON_HID_END,
                       buttons[i].begin(), Input::CreateDevice<Input::ButtonDevice>);
        std::transform(players[i].analogs.begin() + Settings::NativeAnalog::STICK_HID_BEGIN,
                       players[i].analogs.begin() + Settings::NativeAnalog::STICK_HID_END,
                       sticks[i].begin(), Input::CreateDevice<Input::AnalogDevice>);
        std::transform(players[i].vibrations.begin() +
                           Settings::NativeVibration::VIBRATION_HID_BEGIN,
                       players[i].vibrations.begin() + Settings::NativeVibration::VIBRATION_HID_END,
                       vibrations[i].begin(), Input::CreateDevice<Input::VibrationDevice>);
        std::transform(players[i].motions.begin() + Settings::NativeMotion::MOTION_HID_BEGIN,
                       players[i].motions.begin() + Settings::NativeMotion::MOTION_HID_END,
                       motions[i].begin(), Input::CreateDevice<Input::MotionDevice>);
        for (std::size_t device_idx = 0; device_idx < vibrations[i].size(); ++device_idx) {
            InitializeVibrationDeviceAtIndex(i, device_idx);
        }
    }
}

void Controller_NPad::OnRelease() {
    for (std::size_t npad_idx = 0; npad_idx < vibrations.size(); ++npad_idx) {
        for (std::size_t device_idx = 0; device_idx < vibrations[npad_idx].size(); ++device_idx) {
            VibrateControllerAtIndex(npad_idx, device_idx, {});
        }
    }

    for (std::size_t i = 0; i < styleset_changed_events.size(); ++i) {
        service_context.CloseEvent(styleset_changed_events[i]);
    }
}

void Controller_NPad::RequestPadStateUpdate(u32 npad_id) {
    std::lock_guard lock{mutex};

    const auto controller_idx = NPadIdToIndex(npad_id);
    const auto controller_type = connected_controllers[controller_idx].type;
    if (!connected_controllers[controller_idx].is_connected) {
        return;
    }
    auto& pad_state = npad_pad_states[controller_idx].pad_states;
    auto& lstick_entry = npad_pad_states[controller_idx].l_stick;
    auto& rstick_entry = npad_pad_states[controller_idx].r_stick;
    auto& trigger_entry = npad_trigger_states[controller_idx];
    const auto& button_state = buttons[controller_idx];
    const auto& analog_state = sticks[controller_idx];
    const auto [stick_l_x_f, stick_l_y_f] =
        analog_state[static_cast<std::size_t>(JoystickId::Joystick_Left)]->GetStatus();
    const auto [stick_r_x_f, stick_r_y_f] =
        analog_state[static_cast<std::size_t>(JoystickId::Joystick_Right)]->GetStatus();

    using namespace Settings::NativeButton;
    if (controller_type != NPadControllerType::JoyLeft) {
        pad_state.a.Assign(button_state[A - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.b.Assign(button_state[B - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.x.Assign(button_state[X - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.y.Assign(button_state[Y - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.r_stick.Assign(button_state[RStick - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.r.Assign(button_state[R - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.zr.Assign(button_state[ZR - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.plus.Assign(button_state[Plus - BUTTON_HID_BEGIN]->GetStatus());

        pad_state.r_stick_right.Assign(
            analog_state[static_cast<std::size_t>(JoystickId::Joystick_Right)]
                ->GetAnalogDirectionStatus(Input::AnalogDirection::RIGHT));
        pad_state.r_stick_left.Assign(
            analog_state[static_cast<std::size_t>(JoystickId::Joystick_Right)]
                ->GetAnalogDirectionStatus(Input::AnalogDirection::LEFT));
        pad_state.r_stick_up.Assign(
            analog_state[static_cast<std::size_t>(JoystickId::Joystick_Right)]
                ->GetAnalogDirectionStatus(Input::AnalogDirection::UP));
        pad_state.r_stick_down.Assign(
            analog_state[static_cast<std::size_t>(JoystickId::Joystick_Right)]
                ->GetAnalogDirectionStatus(Input::AnalogDirection::DOWN));
        rstick_entry.x = static_cast<s32>(stick_r_x_f * HID_JOYSTICK_MAX);
        rstick_entry.y = static_cast<s32>(stick_r_y_f * HID_JOYSTICK_MAX);
    }

    if (controller_type != NPadControllerType::JoyRight) {
        pad_state.d_left.Assign(button_state[DLeft - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.d_up.Assign(button_state[DUp - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.d_right.Assign(button_state[DRight - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.d_down.Assign(button_state[DDown - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.l_stick.Assign(button_state[LStick - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.l.Assign(button_state[L - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.zl.Assign(button_state[ZL - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.minus.Assign(button_state[Minus - BUTTON_HID_BEGIN]->GetStatus());

        pad_state.l_stick_right.Assign(
            analog_state[static_cast<std::size_t>(JoystickId::Joystick_Left)]
                ->GetAnalogDirectionStatus(Input::AnalogDirection::RIGHT));
        pad_state.l_stick_left.Assign(
            analog_state[static_cast<std::size_t>(JoystickId::Joystick_Left)]
                ->GetAnalogDirectionStatus(Input::AnalogDirection::LEFT));
        pad_state.l_stick_up.Assign(
            analog_state[static_cast<std::size_t>(JoystickId::Joystick_Left)]
                ->GetAnalogDirectionStatus(Input::AnalogDirection::UP));
        pad_state.l_stick_down.Assign(
            analog_state[static_cast<std::size_t>(JoystickId::Joystick_Left)]
                ->GetAnalogDirectionStatus(Input::AnalogDirection::DOWN));
        lstick_entry.x = static_cast<s32>(stick_l_x_f * HID_JOYSTICK_MAX);
        lstick_entry.y = static_cast<s32>(stick_l_y_f * HID_JOYSTICK_MAX);
    }

    if (controller_type == NPadControllerType::JoyLeft) {
        pad_state.left_sl.Assign(button_state[SL - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.left_sr.Assign(button_state[SR - BUTTON_HID_BEGIN]->GetStatus());
    }

    if (controller_type == NPadControllerType::JoyRight) {
        pad_state.right_sl.Assign(button_state[SL - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.right_sr.Assign(button_state[SR - BUTTON_HID_BEGIN]->GetStatus());
    }

    if (controller_type == NPadControllerType::GameCube) {
        trigger_entry.l_analog = static_cast<s32>(
            button_state[ZL - BUTTON_HID_BEGIN]->GetStatus() ? HID_TRIGGER_MAX : 0);
        trigger_entry.r_analog = static_cast<s32>(
            button_state[ZR - BUTTON_HID_BEGIN]->GetStatus() ? HID_TRIGGER_MAX : 0);
        pad_state.zl.Assign(false);
        pad_state.zr.Assign(button_state[R - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.l.Assign(button_state[ZL - BUTTON_HID_BEGIN]->GetStatus());
        pad_state.r.Assign(button_state[ZR - BUTTON_HID_BEGIN]->GetStatus());
    }
}

void Controller_NPad::OnUpdate(const Core::Timing::CoreTiming& core_timing, u8* data,
                               std::size_t data_len) {
    if (!IsControllerActivated()) {
        return;
    }
    for (std::size_t i = 0; i < shared_memory_entries.size(); ++i) {
        auto& npad = shared_memory_entries[i];
        const std::array<NPadGeneric*, 7> controller_npads{
            &npad.fullkey_states,   &npad.handheld_states,  &npad.joy_dual_states,
            &npad.joy_left_states,  &npad.joy_right_states, &npad.palma_states,
            &npad.system_ext_states};

        // There is the posibility to have more controllers with analog triggers
        const std::array<TriggerGeneric*, 1> controller_triggers{
            &npad.gc_trigger_states,
        };

        for (auto* main_controller : controller_npads) {
            main_controller->common.entry_count = 16;
            main_controller->common.total_entry_count = 17;

            const auto& last_entry =
                main_controller->npad[main_controller->common.last_entry_index];

            main_controller->common.timestamp = core_timing.GetCPUTicks();
            main_controller->common.last_entry_index =
                (main_controller->common.last_entry_index + 1) % 17;

            auto& cur_entry = main_controller->npad[main_controller->common.last_entry_index];

            cur_entry.timestamp = last_entry.timestamp + 1;
            cur_entry.timestamp2 = cur_entry.timestamp;
        }

        for (auto* analog_trigger : controller_triggers) {
            analog_trigger->entry_count = 16;
            analog_trigger->total_entry_count = 17;

            const auto& last_entry = analog_trigger->trigger[analog_trigger->last_entry_index];

            analog_trigger->timestamp = core_timing.GetCPUTicks();
            analog_trigger->last_entry_index = (analog_trigger->last_entry_index + 1) % 17;

            auto& cur_entry = analog_trigger->trigger[analog_trigger->last_entry_index];

            cur_entry.timestamp = last_entry.timestamp + 1;
            cur_entry.timestamp2 = cur_entry.timestamp;
        }

        const auto& controller_type = connected_controllers[i].type;

        if (controller_type == NPadControllerType::None || !connected_controllers[i].is_connected) {
            continue;
        }
        const u32 npad_index = static_cast<u32>(i);

        RequestPadStateUpdate(npad_index);
        auto& pad_state = npad_pad_states[npad_index];
        auto& trigger_state = npad_trigger_states[npad_index];

        auto& main_controller =
            npad.fullkey_states.npad[npad.fullkey_states.common.last_entry_index];
        auto& handheld_entry =
            npad.handheld_states.npad[npad.handheld_states.common.last_entry_index];
        auto& dual_entry = npad.joy_dual_states.npad[npad.joy_dual_states.common.last_entry_index];
        auto& left_entry = npad.joy_left_states.npad[npad.joy_left_states.common.last_entry_index];
        auto& right_entry =
            npad.joy_right_states.npad[npad.joy_right_states.common.last_entry_index];
        auto& pokeball_entry = npad.palma_states.npad[npad.palma_states.common.last_entry_index];
        auto& libnx_entry =
            npad.system_ext_states.npad[npad.system_ext_states.common.last_entry_index];
        auto& trigger_entry =
            npad.gc_trigger_states.trigger[npad.gc_trigger_states.last_entry_index];

        libnx_entry.connection_status.raw = 0;
        libnx_entry.connection_status.is_connected.Assign(1);

        switch (controller_type) {
        case NPadControllerType::None:
            UNREACHABLE();
            break;
        case NPadControllerType::ProController:
            main_controller.connection_status.raw = 0;
            main_controller.connection_status.is_connected.Assign(1);
            main_controller.connection_status.is_wired.Assign(1);
            main_controller.pad.pad_states.raw = pad_state.pad_states.raw;
            main_controller.pad.l_stick = pad_state.l_stick;
            main_controller.pad.r_stick = pad_state.r_stick;

            libnx_entry.connection_status.is_wired.Assign(1);
            break;
        case NPadControllerType::Handheld:
            handheld_entry.connection_status.raw = 0;
            handheld_entry.connection_status.is_connected.Assign(1);
            handheld_entry.connection_status.is_wired.Assign(1);
            handheld_entry.connection_status.is_left_connected.Assign(1);
            handheld_entry.connection_status.is_right_connected.Assign(1);
            handheld_entry.connection_status.is_left_wired.Assign(1);
            handheld_entry.connection_status.is_right_wired.Assign(1);
            handheld_entry.pad.pad_states.raw = pad_state.pad_states.raw;
            handheld_entry.pad.l_stick = pad_state.l_stick;
            handheld_entry.pad.r_stick = pad_state.r_stick;

            libnx_entry.connection_status.is_wired.Assign(1);
            libnx_entry.connection_status.is_left_connected.Assign(1);
            libnx_entry.connection_status.is_right_connected.Assign(1);
            libnx_entry.connection_status.is_left_wired.Assign(1);
            libnx_entry.connection_status.is_right_wired.Assign(1);
            break;
        case NPadControllerType::JoyDual:
            dual_entry.connection_status.raw = 0;
            dual_entry.connection_status.is_connected.Assign(1);
            dual_entry.connection_status.is_left_connected.Assign(1);
            dual_entry.connection_status.is_right_connected.Assign(1);
            dual_entry.pad.pad_states.raw = pad_state.pad_states.raw;
            dual_entry.pad.l_stick = pad_state.l_stick;
            dual_entry.pad.r_stick = pad_state.r_stick;

            libnx_entry.connection_status.is_left_connected.Assign(1);
            libnx_entry.connection_status.is_right_connected.Assign(1);
            break;
        case NPadControllerType::JoyLeft:
            left_entry.connection_status.raw = 0;
            left_entry.connection_status.is_connected.Assign(1);
            left_entry.connection_status.is_left_connected.Assign(1);
            left_entry.pad.pad_states.raw = pad_state.pad_states.raw;
            left_entry.pad.l_stick = pad_state.l_stick;
            left_entry.pad.r_stick = pad_state.r_stick;

            libnx_entry.connection_status.is_left_connected.Assign(1);
            break;
        case NPadControllerType::JoyRight:
            right_entry.connection_status.raw = 0;
            right_entry.connection_status.is_connected.Assign(1);
            right_entry.connection_status.is_right_connected.Assign(1);
            right_entry.pad.pad_states.raw = pad_state.pad_states.raw;
            right_entry.pad.l_stick = pad_state.l_stick;
            right_entry.pad.r_stick = pad_state.r_stick;

            libnx_entry.connection_status.is_right_connected.Assign(1);
            break;
        case NPadControllerType::GameCube:
            main_controller.connection_status.raw = 0;
            main_controller.connection_status.is_connected.Assign(1);
            main_controller.connection_status.is_wired.Assign(1);
            main_controller.pad.pad_states.raw = pad_state.pad_states.raw;
            main_controller.pad.l_stick = pad_state.l_stick;
            main_controller.pad.r_stick = pad_state.r_stick;
            trigger_entry.l_analog = trigger_state.l_analog;
            trigger_entry.r_analog = trigger_state.r_analog;

            libnx_entry.connection_status.is_wired.Assign(1);
            break;
        case NPadControllerType::Pokeball:
            pokeball_entry.connection_status.raw = 0;
            pokeball_entry.connection_status.is_connected.Assign(1);
            pokeball_entry.pad.pad_states.raw = pad_state.pad_states.raw;
            pokeball_entry.pad.l_stick = pad_state.l_stick;
            pokeball_entry.pad.r_stick = pad_state.r_stick;
            break;
        }

        // LibNX exclusively uses this section, so we always update it since LibNX doesn't activate
        // any controllers.
        libnx_entry.pad.pad_states.raw = pad_state.pad_states.raw;
        libnx_entry.pad.l_stick = pad_state.l_stick;
        libnx_entry.pad.r_stick = pad_state.r_stick;

        press_state |= static_cast<u32>(pad_state.pad_states.raw);
    }
    std::memcpy(data + NPAD_OFFSET, shared_memory_entries.data(),
                shared_memory_entries.size() * sizeof(NPadEntry));
}

void Controller_NPad::OnMotionUpdate(const Core::Timing::CoreTiming& core_timing, u8* data,
                                     std::size_t data_len) {
    if (!IsControllerActivated()) {
        return;
    }
    for (std::size_t i = 0; i < shared_memory_entries.size(); ++i) {
        auto& npad = shared_memory_entries[i];

        const auto& controller_type = connected_controllers[i].type;

        if (controller_type == NPadControllerType::None || !connected_controllers[i].is_connected) {
            continue;
        }

        const std::array<SixAxisGeneric*, 6> controller_sixaxes{
            &npad.sixaxis_fullkey,    &npad.sixaxis_handheld, &npad.sixaxis_dual_left,
            &npad.sixaxis_dual_right, &npad.sixaxis_left,     &npad.sixaxis_right,
        };

        for (auto* sixaxis_sensor : controller_sixaxes) {
            sixaxis_sensor->common.entry_count = 16;
            sixaxis_sensor->common.total_entry_count = 17;

            const auto& last_entry =
                sixaxis_sensor->sixaxis[sixaxis_sensor->common.last_entry_index];

            sixaxis_sensor->common.timestamp = core_timing.GetCPUTicks();
            sixaxis_sensor->common.last_entry_index =
                (sixaxis_sensor->common.last_entry_index + 1) % 17;

            auto& cur_entry = sixaxis_sensor->sixaxis[sixaxis_sensor->common.last_entry_index];

            cur_entry.timestamp = last_entry.timestamp + 1;
            cur_entry.timestamp2 = cur_entry.timestamp;
        }

        // Try to read sixaxis sensor states
        std::array<MotionDevice, 2> motion_devices;

        if (sixaxis_sensors_enabled && Settings::values.motion_enabled.GetValue()) {
            sixaxis_at_rest = true;
            for (std::size_t e = 0; e < motion_devices.size(); ++e) {
                const auto& device = motions[i][e];
                if (device) {
                    std::tie(motion_devices[e].accel, motion_devices[e].gyro,
                             motion_devices[e].rotation, motion_devices[e].orientation,
                             motion_devices[e].quaternion) = device->GetStatus();
                    sixaxis_at_rest = sixaxis_at_rest && motion_devices[e].gyro.Length2() < 0.0001f;
                }
            }
        }

        auto& full_sixaxis_entry =
            npad.sixaxis_fullkey.sixaxis[npad.sixaxis_fullkey.common.last_entry_index];
        auto& handheld_sixaxis_entry =
            npad.sixaxis_handheld.sixaxis[npad.sixaxis_handheld.common.last_entry_index];
        auto& dual_left_sixaxis_entry =
            npad.sixaxis_dual_left.sixaxis[npad.sixaxis_dual_left.common.last_entry_index];
        auto& dual_right_sixaxis_entry =
            npad.sixaxis_dual_right.sixaxis[npad.sixaxis_dual_right.common.last_entry_index];
        auto& left_sixaxis_entry =
            npad.sixaxis_left.sixaxis[npad.sixaxis_left.common.last_entry_index];
        auto& right_sixaxis_entry =
            npad.sixaxis_right.sixaxis[npad.sixaxis_right.common.last_entry_index];

        switch (controller_type) {
        case NPadControllerType::None:
            UNREACHABLE();
            break;
        case NPadControllerType::ProController:
            full_sixaxis_entry.attribute.raw = 0;
            if (sixaxis_sensors_enabled && motions[i][0]) {
                full_sixaxis_entry.attribute.is_connected.Assign(1);
                full_sixaxis_entry.accel = motion_devices[0].accel;
                full_sixaxis_entry.gyro = motion_devices[0].gyro;
                full_sixaxis_entry.rotation = motion_devices[0].rotation;
                full_sixaxis_entry.orientation = motion_devices[0].orientation;
            }
            break;
        case NPadControllerType::Handheld:
            handheld_sixaxis_entry.attribute.raw = 0;
            if (sixaxis_sensors_enabled && motions[i][0]) {
                handheld_sixaxis_entry.attribute.is_connected.Assign(1);
                handheld_sixaxis_entry.accel = motion_devices[0].accel;
                handheld_sixaxis_entry.gyro = motion_devices[0].gyro;
                handheld_sixaxis_entry.rotation = motion_devices[0].rotation;
                handheld_sixaxis_entry.orientation = motion_devices[0].orientation;
            }
            break;
        case NPadControllerType::JoyDual:
            dual_left_sixaxis_entry.attribute.raw = 0;
            dual_right_sixaxis_entry.attribute.raw = 0;
            if (sixaxis_sensors_enabled && motions[i][0]) {
                // Set motion for the left joycon
                dual_left_sixaxis_entry.attribute.is_connected.Assign(1);
                dual_left_sixaxis_entry.accel = motion_devices[0].accel;
                dual_left_sixaxis_entry.gyro = motion_devices[0].gyro;
                dual_left_sixaxis_entry.rotation = motion_devices[0].rotation;
                dual_left_sixaxis_entry.orientation = motion_devices[0].orientation;
            }
            if (sixaxis_sensors_enabled && motions[i][1]) {
                // Set motion for the right joycon
                dual_right_sixaxis_entry.attribute.is_connected.Assign(1);
                dual_right_sixaxis_entry.accel = motion_devices[1].accel;
                dual_right_sixaxis_entry.gyro = motion_devices[1].gyro;
                dual_right_sixaxis_entry.rotation = motion_devices[1].rotation;
                dual_right_sixaxis_entry.orientation = motion_devices[1].orientation;
            }
            break;
        case NPadControllerType::JoyLeft:
            left_sixaxis_entry.attribute.raw = 0;
            if (sixaxis_sensors_enabled && motions[i][0]) {
                left_sixaxis_entry.attribute.is_connected.Assign(1);
                left_sixaxis_entry.accel = motion_devices[0].accel;
                left_sixaxis_entry.gyro = motion_devices[0].gyro;
                left_sixaxis_entry.rotation = motion_devices[0].rotation;
                left_sixaxis_entry.orientation = motion_devices[0].orientation;
            }
            break;
        case NPadControllerType::JoyRight:
            right_sixaxis_entry.attribute.raw = 0;
            if (sixaxis_sensors_enabled && motions[i][1]) {
                right_sixaxis_entry.attribute.is_connected.Assign(1);
                right_sixaxis_entry.accel = motion_devices[1].accel;
                right_sixaxis_entry.gyro = motion_devices[1].gyro;
                right_sixaxis_entry.rotation = motion_devices[1].rotation;
                right_sixaxis_entry.orientation = motion_devices[1].orientation;
            }
            break;
        case NPadControllerType::GameCube:
        case NPadControllerType::Pokeball:
            break;
        }
    }
    std::memcpy(data + NPAD_OFFSET, shared_memory_entries.data(),
                shared_memory_entries.size() * sizeof(NPadEntry));
}

void Controller_NPad::SetSupportedStyleSet(NpadStyleSet style_set) {
    style.raw = style_set.raw;
}

Controller_NPad::NpadStyleSet Controller_NPad::GetSupportedStyleSet() const {
    return style;
}

void Controller_NPad::SetSupportedNpadIdTypes(u8* data, std::size_t length) {
    ASSERT(length > 0 && (length % sizeof(u32)) == 0);
    supported_npad_id_types.clear();
    supported_npad_id_types.resize(length / sizeof(u32));
    std::memcpy(supported_npad_id_types.data(), data, length);
}

void Controller_NPad::GetSupportedNpadIdTypes(u32* data, std::size_t max_length) {
    ASSERT(max_length < supported_npad_id_types.size());
    std::memcpy(data, supported_npad_id_types.data(), supported_npad_id_types.size());
}

std::size_t Controller_NPad::GetSupportedNpadIdTypesSize() const {
    return supported_npad_id_types.size();
}

void Controller_NPad::SetHoldType(NpadHoldType joy_hold_type) {
    hold_type = joy_hold_type;
}

Controller_NPad::NpadHoldType Controller_NPad::GetHoldType() const {
    return hold_type;
}

void Controller_NPad::SetNpadHandheldActivationMode(NpadHandheldActivationMode activation_mode) {
    handheld_activation_mode = activation_mode;
}

Controller_NPad::NpadHandheldActivationMode Controller_NPad::GetNpadHandheldActivationMode() const {
    return handheld_activation_mode;
}

void Controller_NPad::SetNpadCommunicationMode(NpadCommunicationMode communication_mode_) {
    communication_mode = communication_mode_;
}

Controller_NPad::NpadCommunicationMode Controller_NPad::GetNpadCommunicationMode() const {
    return communication_mode;
}

void Controller_NPad::SetNpadMode(u32 npad_id, NpadAssignments assignment_mode) {
    const std::size_t npad_index = NPadIdToIndex(npad_id);
    ASSERT(npad_index < shared_memory_entries.size());
    if (shared_memory_entries[npad_index].assignment_mode != assignment_mode) {
        shared_memory_entries[npad_index].assignment_mode = assignment_mode;
    }
}

bool Controller_NPad::VibrateControllerAtIndex(std::size_t npad_index, std::size_t device_index,
                                               const VibrationValue& vibration_value) {
    if (!connected_controllers[npad_index].is_connected || !vibrations[npad_index][device_index]) {
        return false;
    }

    const auto& player = Settings::values.players.GetValue()[npad_index];

    if (!player.vibration_enabled) {
        if (latest_vibration_values[npad_index][device_index].amp_low != 0.0f ||
            latest_vibration_values[npad_index][device_index].amp_high != 0.0f) {
            // Send an empty vibration to stop any vibrations.
            vibrations[npad_index][device_index]->SetRumblePlay(0.0f, 160.0f, 0.0f, 320.0f);
            // Then reset the vibration value to its default value.
            latest_vibration_values[npad_index][device_index] = DEFAULT_VIBRATION_VALUE;
        }

        return false;
    }

    if (!Settings::values.enable_accurate_vibrations.GetValue()) {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        using std::chrono::steady_clock;

        const auto now = steady_clock::now();

        // Filter out non-zero vibrations that are within 10ms of each other.
        if ((vibration_value.amp_low != 0.0f || vibration_value.amp_high != 0.0f) &&
            duration_cast<milliseconds>(now - last_vibration_timepoints[npad_index][device_index]) <
                milliseconds(10)) {
            return false;
        }

        last_vibration_timepoints[npad_index][device_index] = now;
    }

    auto& vibration = vibrations[npad_index][device_index];
    const auto player_vibration_strength = static_cast<f32>(player.vibration_strength);
    const auto amp_low =
        std::min(vibration_value.amp_low * player_vibration_strength / 100.0f, 1.0f);
    const auto amp_high =
        std::min(vibration_value.amp_high * player_vibration_strength / 100.0f, 1.0f);
    return vibration->SetRumblePlay(amp_low, vibration_value.freq_low, amp_high,
                                    vibration_value.freq_high);
}

void Controller_NPad::VibrateController(const DeviceHandle& vibration_device_handle,
                                        const VibrationValue& vibration_value) {
    if (!IsDeviceHandleValid(vibration_device_handle)) {
        return;
    }

    if (!Settings::values.vibration_enabled.GetValue() && !permit_vibration_session_enabled) {
        return;
    }

    const auto npad_index = NPadIdToIndex(vibration_device_handle.npad_id);
    const auto device_index = static_cast<std::size_t>(vibration_device_handle.device_index);

    if (!vibration_devices_mounted[npad_index][device_index] ||
        !connected_controllers[npad_index].is_connected) {
        return;
    }

    if (vibration_device_handle.device_index == DeviceIndex::None) {
        UNREACHABLE_MSG("DeviceIndex should never be None!");
        return;
    }

    // Some games try to send mismatched parameters in the device handle, block these.
    if ((connected_controllers[npad_index].type == NPadControllerType::JoyLeft &&
         (vibration_device_handle.npad_type == NpadType::JoyconRight ||
          vibration_device_handle.device_index == DeviceIndex::Right)) ||
        (connected_controllers[npad_index].type == NPadControllerType::JoyRight &&
         (vibration_device_handle.npad_type == NpadType::JoyconLeft ||
          vibration_device_handle.device_index == DeviceIndex::Left))) {
        return;
    }

    // Filter out vibrations with equivalent values to reduce unnecessary state changes.
    if (vibration_value.amp_low == latest_vibration_values[npad_index][device_index].amp_low &&
        vibration_value.amp_high == latest_vibration_values[npad_index][device_index].amp_high) {
        return;
    }

    if (VibrateControllerAtIndex(npad_index, device_index, vibration_value)) {
        latest_vibration_values[npad_index][device_index] = vibration_value;
    }
}

void Controller_NPad::VibrateControllers(const std::vector<DeviceHandle>& vibration_device_handles,
                                         const std::vector<VibrationValue>& vibration_values) {
    if (!Settings::values.vibration_enabled.GetValue() && !permit_vibration_session_enabled) {
        return;
    }

    ASSERT_OR_EXECUTE_MSG(
        vibration_device_handles.size() == vibration_values.size(), { return; },
        "The amount of device handles does not match with the amount of vibration values,"
        "this is undefined behavior!");

    for (std::size_t i = 0; i < vibration_device_handles.size(); ++i) {
        VibrateController(vibration_device_handles[i], vibration_values[i]);
    }
}

Controller_NPad::VibrationValue Controller_NPad::GetLastVibration(
    const DeviceHandle& vibration_device_handle) const {
    if (!IsDeviceHandleValid(vibration_device_handle)) {
        return {};
    }

    const auto npad_index = NPadIdToIndex(vibration_device_handle.npad_id);
    const auto device_index = static_cast<std::size_t>(vibration_device_handle.device_index);
    return latest_vibration_values[npad_index][device_index];
}

void Controller_NPad::InitializeVibrationDevice(const DeviceHandle& vibration_device_handle) {
    if (!IsDeviceHandleValid(vibration_device_handle)) {
        return;
    }

    const auto npad_index = NPadIdToIndex(vibration_device_handle.npad_id);
    const auto device_index = static_cast<std::size_t>(vibration_device_handle.device_index);
    InitializeVibrationDeviceAtIndex(npad_index, device_index);
}

void Controller_NPad::InitializeVibrationDeviceAtIndex(std::size_t npad_index,
                                                       std::size_t device_index) {
    if (vibrations[npad_index][device_index]) {
        vibration_devices_mounted[npad_index][device_index] =
            vibrations[npad_index][device_index]->GetStatus() == 1;
    } else {
        vibration_devices_mounted[npad_index][device_index] = false;
    }
}

void Controller_NPad::SetPermitVibrationSession(bool permit_vibration_session) {
    permit_vibration_session_enabled = permit_vibration_session;
}

bool Controller_NPad::IsVibrationDeviceMounted(const DeviceHandle& vibration_device_handle) const {
    if (!IsDeviceHandleValid(vibration_device_handle)) {
        return false;
    }

    const auto npad_index = NPadIdToIndex(vibration_device_handle.npad_id);
    const auto device_index = static_cast<std::size_t>(vibration_device_handle.device_index);
    return vibration_devices_mounted[npad_index][device_index];
}

Kernel::KReadableEvent& Controller_NPad::GetStyleSetChangedEvent(u32 npad_id) {
    return styleset_changed_events[NPadIdToIndex(npad_id)]->GetReadableEvent();
}

void Controller_NPad::SignalStyleSetChangedEvent(u32 npad_id) const {
    styleset_changed_events[NPadIdToIndex(npad_id)]->GetWritableEvent().Signal();
}

void Controller_NPad::AddNewControllerAt(NPadControllerType controller, std::size_t npad_index) {
    UpdateControllerAt(controller, npad_index, true);
}

void Controller_NPad::UpdateControllerAt(NPadControllerType controller, std::size_t npad_index,
                                         bool connected) {
    if (!connected) {
        DisconnectNpadAtIndex(npad_index);
        return;
    }

    if (controller == NPadControllerType::Handheld && npad_index == HANDHELD_INDEX) {
        Settings::values.players.GetValue()[HANDHELD_INDEX].controller_type =
            MapNPadToSettingsType(controller);
        Settings::values.players.GetValue()[HANDHELD_INDEX].connected = true;
        connected_controllers[HANDHELD_INDEX] = {controller, true};
        InitNewlyAddedController(HANDHELD_INDEX);
        return;
    }

    Settings::values.players.GetValue()[npad_index].controller_type =
        MapNPadToSettingsType(controller);
    Settings::values.players.GetValue()[npad_index].connected = true;
    connected_controllers[npad_index] = {controller, true};
    InitNewlyAddedController(npad_index);
}

void Controller_NPad::DisconnectNpad(u32 npad_id) {
    DisconnectNpadAtIndex(NPadIdToIndex(npad_id));
}

void Controller_NPad::DisconnectNpadAtIndex(std::size_t npad_index) {
    for (std::size_t device_idx = 0; device_idx < vibrations[npad_index].size(); ++device_idx) {
        // Send an empty vibration to stop any vibrations.
        VibrateControllerAtIndex(npad_index, device_idx, {});
        vibration_devices_mounted[npad_index][device_idx] = false;
    }

    Settings::values.players.GetValue()[npad_index].connected = false;
    connected_controllers[npad_index].is_connected = false;

    auto& controller = shared_memory_entries[npad_index];
    controller.style_set.raw = 0; // Zero out
    controller.device_type.raw = 0;
    controller.system_properties.raw = 0;
    controller.button_properties.raw = 0;
    controller.battery_level_dual = 0;
    controller.battery_level_left = 0;
    controller.battery_level_right = 0;
    controller.fullkey_color = {};
    controller.joycon_color = {};
    controller.assignment_mode = NpadAssignments::Dual;
    controller.footer_type = AppletFooterUiType::None;

    SignalStyleSetChangedEvent(IndexToNPad(npad_index));
}

void Controller_NPad::SetGyroscopeZeroDriftMode(GyroscopeZeroDriftMode drift_mode) {
    gyroscope_zero_drift_mode = drift_mode;
}

Controller_NPad::GyroscopeZeroDriftMode Controller_NPad::GetGyroscopeZeroDriftMode() const {
    return gyroscope_zero_drift_mode;
}

bool Controller_NPad::IsSixAxisSensorAtRest() const {
    return sixaxis_at_rest;
}

void Controller_NPad::SetSixAxisEnabled(bool six_axis_status) {
    sixaxis_sensors_enabled = six_axis_status;
}

void Controller_NPad::SetSixAxisFusionParameters(f32 parameter1, f32 parameter2) {
    sixaxis_fusion_parameter1 = parameter1;
    sixaxis_fusion_parameter2 = parameter2;
}

std::pair<f32, f32> Controller_NPad::GetSixAxisFusionParameters() {
    return {
        sixaxis_fusion_parameter1,
        sixaxis_fusion_parameter2,
    };
}

void Controller_NPad::ResetSixAxisFusionParameters() {
    sixaxis_fusion_parameter1 = 0.0f;
    sixaxis_fusion_parameter2 = 0.0f;
}

void Controller_NPad::MergeSingleJoyAsDualJoy(u32 npad_id_1, u32 npad_id_2) {
    const auto npad_index_1 = NPadIdToIndex(npad_id_1);
    const auto npad_index_2 = NPadIdToIndex(npad_id_2);

    // If the controllers at both npad indices form a pair of left and right joycons, merge them.
    // Otherwise, do nothing.
    if ((connected_controllers[npad_index_1].type == NPadControllerType::JoyLeft &&
         connected_controllers[npad_index_2].type == NPadControllerType::JoyRight) ||
        (connected_controllers[npad_index_2].type == NPadControllerType::JoyLeft &&
         connected_controllers[npad_index_1].type == NPadControllerType::JoyRight)) {
        // Disconnect the joycon at the second id and connect the dual joycon at the first index.
        DisconnectNpad(npad_id_2);
        AddNewControllerAt(NPadControllerType::JoyDual, npad_index_1);
    }
}

void Controller_NPad::StartLRAssignmentMode() {
    // Nothing internally is used for lr assignment mode. Since we have the ability to set the
    // controller types from boot, it doesn't really matter about showing a selection screen
    is_in_lr_assignment_mode = true;
}

void Controller_NPad::StopLRAssignmentMode() {
    is_in_lr_assignment_mode = false;
}

bool Controller_NPad::SwapNpadAssignment(u32 npad_id_1, u32 npad_id_2) {
    if (npad_id_1 == NPAD_HANDHELD || npad_id_2 == NPAD_HANDHELD || npad_id_1 == NPAD_UNKNOWN ||
        npad_id_2 == NPAD_UNKNOWN) {
        return true;
    }
    const auto npad_index_1 = NPadIdToIndex(npad_id_1);
    const auto npad_index_2 = NPadIdToIndex(npad_id_2);

    if (!IsControllerSupported(connected_controllers[npad_index_1].type) ||
        !IsControllerSupported(connected_controllers[npad_index_2].type)) {
        return false;
    }

    std::swap(connected_controllers[npad_index_1].type, connected_controllers[npad_index_2].type);

    AddNewControllerAt(connected_controllers[npad_index_1].type, npad_index_1);
    AddNewControllerAt(connected_controllers[npad_index_2].type, npad_index_2);

    return true;
}

Controller_NPad::LedPattern Controller_NPad::GetLedPattern(u32 npad_id) {
    if (npad_id == npad_id_list.back() || npad_id == npad_id_list[npad_id_list.size() - 2]) {
        // These are controllers without led patterns
        return LedPattern{0, 0, 0, 0};
    }
    switch (npad_id) {
    case 0:
        return LedPattern{1, 0, 0, 0};
    case 1:
        return LedPattern{1, 1, 0, 0};
    case 2:
        return LedPattern{1, 1, 1, 0};
    case 3:
        return LedPattern{1, 1, 1, 1};
    case 4:
        return LedPattern{1, 0, 0, 1};
    case 5:
        return LedPattern{1, 0, 1, 0};
    case 6:
        return LedPattern{1, 0, 1, 1};
    case 7:
        return LedPattern{0, 1, 1, 0};
    default:
        return LedPattern{0, 0, 0, 0};
    }
}

bool Controller_NPad::IsUnintendedHomeButtonInputProtectionEnabled(u32 npad_id) const {
    return unintended_home_button_input_protection[NPadIdToIndex(npad_id)];
}

void Controller_NPad::SetUnintendedHomeButtonInputProtectionEnabled(bool is_protection_enabled,
                                                                    u32 npad_id) {
    unintended_home_button_input_protection[NPadIdToIndex(npad_id)] = is_protection_enabled;
}

void Controller_NPad::SetAnalogStickUseCenterClamp(bool use_center_clamp) {
    analog_stick_use_center_clamp = use_center_clamp;
}

void Controller_NPad::ClearAllConnectedControllers() {
    for (auto& controller : connected_controllers) {
        if (controller.is_connected && controller.type != NPadControllerType::None) {
            controller.type = NPadControllerType::None;
            controller.is_connected = false;
        }
    }
}

void Controller_NPad::DisconnectAllConnectedControllers() {
    for (auto& controller : connected_controllers) {
        controller.is_connected = false;
    }
}

void Controller_NPad::ConnectAllDisconnectedControllers() {
    for (auto& controller : connected_controllers) {
        if (controller.type != NPadControllerType::None && !controller.is_connected) {
            controller.is_connected = true;
        }
    }
}

void Controller_NPad::ClearAllControllers() {
    for (auto& controller : connected_controllers) {
        controller.type = NPadControllerType::None;
        controller.is_connected = false;
    }
}

u32 Controller_NPad::GetAndResetPressState() {
    return press_state.exchange(0);
}

bool Controller_NPad::IsControllerSupported(NPadControllerType controller) const {
    if (controller == NPadControllerType::Handheld) {
        const bool support_handheld =
            std::find(supported_npad_id_types.begin(), supported_npad_id_types.end(),
                      NPAD_HANDHELD) != supported_npad_id_types.end();
        // Handheld is not even a supported type, lets stop here
        if (!support_handheld) {
            return false;
        }
        // Handheld should not be supported in docked mode
        if (Settings::values.use_docked_mode.GetValue()) {
            return false;
        }

        return true;
    }

    if (std::any_of(supported_npad_id_types.begin(), supported_npad_id_types.end(),
                    [](u32 npad_id) { return npad_id <= MAX_NPAD_ID; })) {
        switch (controller) {
        case NPadControllerType::ProController:
            return style.fullkey;
        case NPadControllerType::JoyDual:
            return style.joycon_dual;
        case NPadControllerType::JoyLeft:
            return style.joycon_left;
        case NPadControllerType::JoyRight:
            return style.joycon_right;
        case NPadControllerType::GameCube:
            return style.gamecube;
        case NPadControllerType::Pokeball:
            return style.palma;
        default:
            return false;
        }
    }

    return false;
}

} // namespace Service::HID
