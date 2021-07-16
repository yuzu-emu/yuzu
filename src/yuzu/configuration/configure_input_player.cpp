// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>
#include <QGridLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include "common/param_package.h"
#include "core/core.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/sm/sm.h"
#include "input_common/gcadapter/gc_poller.h"
#include "input_common/main.h"
#include "input_common/mouse/mouse_poller.h"
#include "input_common/udp/udp.h"
#include "ui_configure_input_player.h"
#include "yuzu/bootmanager.h"
#include "yuzu/configuration/config.h"
#include "yuzu/configuration/configure_input_player.h"
#include "yuzu/configuration/configure_input_player_widget.h"
#include "yuzu/configuration/configure_vibration.h"
#include "yuzu/configuration/input_profiles.h"
#include "yuzu/util/limitable_input_dialog.h"

using namespace Service::HID;

const std::array<std::string, ConfigureInputPlayer::ANALOG_SUB_BUTTONS_NUM>
    ConfigureInputPlayer::analog_sub_buttons{{
        "up",
        "down",
        "left",
        "right",
    }};

namespace {

constexpr std::size_t HANDHELD_INDEX = 8;

void UpdateController(Settings::ControllerType controller_type, std::size_t npad_index,
                      bool connected) {
    Core::System& system{Core::System::GetInstance()};
    if (!system.IsPoweredOn()) {
        return;
    }
    Service::SM::ServiceManager& sm = system.ServiceManager();

    auto& npad = sm.GetService<Hid>("hid")->GetAppletResource()->GetController<Controller_NPad>(
        HidController::NPad);

    npad.UpdateControllerAt(npad.MapSettingsTypeToNPad(controller_type), npad_index, connected);
}

QString GetKeyName(int key_code) {
    switch (key_code) {
    case Qt::LeftButton:
        return QObject::tr("Click 0");
    case Qt::RightButton:
        return QObject::tr("Click 1");
    case Qt::MiddleButton:
        return QObject::tr("Click 2");
    case Qt::BackButton:
        return QObject::tr("Click 3");
    case Qt::ForwardButton:
        return QObject::tr("Click 4");
    case Qt::Key_Shift:
        return QObject::tr("Shift");
    case Qt::Key_Control:
        return QObject::tr("Ctrl");
    case Qt::Key_Alt:
        return QObject::tr("Alt");
    case Qt::Key_Meta:
        return {};
    default:
        return QKeySequence(key_code).toString();
    }
}

void SetAnalogParam(const Common::ParamPackage& input_param, Common::ParamPackage& analog_param,
                    const std::string& button_name) {
    // The poller returned a complete axis, so set all the buttons
    if (input_param.Has("axis_x") && input_param.Has("axis_y")) {
        analog_param = input_param;
        return;
    }
    // Check if the current configuration has either no engine or an axis binding.
    // Clears out the old binding and adds one with analog_from_button.
    if (!analog_param.Has("engine") || analog_param.Has("axis_x") || analog_param.Has("axis_y")) {
        analog_param = {
            {"engine", "analog_from_button"},
        };
    }
    analog_param.Set(button_name, input_param.Serialize());
}

QString ButtonToText(const Common::ParamPackage& param) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    }

    if (param.Get("engine", "") == "keyboard") {
        const QString button_str = GetKeyName(param.Get("code", 0));
        const QString toggle = QString::fromStdString(param.Get("toggle", false) ? "~" : "");
        return QObject::tr("%1%2").arg(toggle, button_str);
    }

    if (param.Get("engine", "") == "gcpad") {
        if (param.Has("axis")) {
            const QString axis_str = QString::fromStdString(param.Get("axis", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("GC Axis %1%2").arg(axis_str, direction_str);
        }
        if (param.Has("button")) {
            const QString button_str = QString::number(int(std::log2(param.Get("button", 0))));
            return QObject::tr("GC Button %1").arg(button_str);
        }
        return GetKeyName(param.Get("code", 0));
    }

    if (param.Get("engine", "") == "cemuhookudp") {
        if (param.Has("pad_index")) {
            const QString motion_str = QString::fromStdString(param.Get("pad_index", ""));
            return QObject::tr("Motion %1").arg(motion_str);
        }
        return GetKeyName(param.Get("code", 0));
    }

    if (param.Get("engine", "") == "sdl") {
        if (param.Has("hat")) {
            const QString hat_str = QString::fromStdString(param.Get("hat", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("Hat %1 %2").arg(hat_str, direction_str);
        }

        if (param.Has("axis")) {
            const QString axis_str = QString::fromStdString(param.Get("axis", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("Axis %1%2").arg(axis_str, direction_str);
        }

        if (param.Has("button")) {
            const QString button_str = QString::fromStdString(param.Get("button", ""));

            return QObject::tr("Button %1").arg(button_str);
        }

        if (param.Has("motion")) {
            return QObject::tr("SDL Motion");
        }

        return {};
    }

    if (param.Get("engine", "") == "mouse") {
        if (param.Has("button")) {
            const QString button_str = QString::number(int(param.Get("button", 0)));
            const QString toggle = QString::fromStdString(param.Get("toggle", false) ? "~" : "");
            return QObject::tr("%1Click %2").arg(toggle, button_str);
        }
        return GetKeyName(param.Get("code", 0));
    }

    return QObject::tr("[unknown]");
}

QString AnalogToText(const Common::ParamPackage& param, const std::string& dir) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    }

    if (param.Get("engine", "") == "analog_from_button") {
        return ButtonToText(Common::ParamPackage{param.Get(dir, "")});
    }

    const auto engine_str = param.Get("engine", "");
    const QString axis_x_str = QString::fromStdString(param.Get("axis_x", ""));
    const QString axis_y_str = QString::fromStdString(param.Get("axis_y", ""));
    const bool invert_x = param.Get("invert_x", "+") == "-";
    const bool invert_y = param.Get("invert_y", "+") == "-";
    if (engine_str == "sdl" || engine_str == "gcpad" || engine_str == "mouse") {
        if (dir == "modifier") {
            return QObject::tr("[unused]");
        }

        if (dir == "left") {
            const QString invert_x_str = QString::fromStdString(invert_x ? "+" : "-");
            return QObject::tr("Axis %1%2").arg(axis_x_str, invert_x_str);
        }
        if (dir == "right") {
            const QString invert_x_str = QString::fromStdString(invert_x ? "-" : "+");
            return QObject::tr("Axis %1%2").arg(axis_x_str, invert_x_str);
        }
        if (dir == "up") {
            const QString invert_y_str = QString::fromStdString(invert_y ? "-" : "+");
            return QObject::tr("Axis %1%2").arg(axis_y_str, invert_y_str);
        }
        if (dir == "down") {
            const QString invert_y_str = QString::fromStdString(invert_y ? "+" : "-");
            return QObject::tr("Axis %1%2").arg(axis_y_str, invert_y_str);
        }

        return {};
    }
    return QObject::tr("[unknown]");
}
} // namespace

ConfigureInputPlayer::ConfigureInputPlayer(QWidget* parent, std::size_t player_index,
                                           QWidget* bottom_row,
                                           InputCommon::InputSubsystem* input_subsystem_,
                                           InputProfiles* profiles_, bool debug)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureInputPlayer>()), player_index(player_index),
      debug(debug), input_subsystem{input_subsystem_}, profiles(profiles_),
      timeout_timer(std::make_unique<QTimer>()), poll_timer(std::make_unique<QTimer>()),
      bottom_row(bottom_row) {
    ui->setupUi(this);

    setFocusPolicy(Qt::ClickFocus);

    button_map = {
        ui->buttonA,        ui->buttonB,      ui->buttonX,         ui->buttonY,
        ui->buttonLStick,   ui->buttonRStick, ui->buttonL,         ui->buttonR,
        ui->buttonZL,       ui->buttonZR,     ui->buttonPlus,      ui->buttonMinus,
        ui->buttonDpadLeft, ui->buttonDpadUp, ui->buttonDpadRight, ui->buttonDpadDown,
        ui->buttonSL,       ui->buttonSR,     ui->buttonHome,      ui->buttonScreenshot,
    };

    analog_map_buttons = {{
        {
            ui->buttonLStickUp,
            ui->buttonLStickDown,
            ui->buttonLStickLeft,
            ui->buttonLStickRight,
        },
        {
            ui->buttonRStickUp,
            ui->buttonRStickDown,
            ui->buttonRStickLeft,
            ui->buttonRStickRight,
        },
    }};

    motion_map = {
        ui->buttonMotionLeft,
        ui->buttonMotionRight,
    };

    analog_map_deadzone_label = {ui->labelLStickDeadzone, ui->labelRStickDeadzone};
    analog_map_deadzone_slider = {ui->sliderLStickDeadzone, ui->sliderRStickDeadzone};
    analog_map_modifier_groupbox = {ui->buttonLStickModGroup, ui->buttonRStickModGroup};
    analog_map_modifier_button = {ui->buttonLStickMod, ui->buttonRStickMod};
    analog_map_modifier_label = {ui->labelLStickModifierRange, ui->labelRStickModifierRange};
    analog_map_modifier_slider = {ui->sliderLStickModifierRange, ui->sliderRStickModifierRange};
    analog_map_range_groupbox = {ui->buttonLStickRangeGroup, ui->buttonRStickRangeGroup};
    analog_map_range_spinbox = {ui->spinboxLStickRange, ui->spinboxRStickRange};

    const auto ConfigureButtonClick = [&](QPushButton* button, std::size_t button_id,
                                          Common::ParamPackage* param, int default_val,
                                          InputCommon::Polling::DeviceType type) {
        connect(button, &QPushButton::clicked, [=, this] {
            HandleClick(
                button, button_id,
                [=, this](Common::ParamPackage params) {
                    // Workaround for ZL & ZR for analog triggers like on XBOX
                    // controllers. Analog triggers (from controllers like the XBOX
                    // controller) would not work due to a different range of their
                    // signals (from 0 to 255 on analog triggers instead of -32768 to
                    // 32768 on analog joysticks). The SDL driver misinterprets analog
                    // triggers as analog joysticks.
                    // TODO: reinterpret the signal range for analog triggers to map the
                    // values correctly. This is required for the correct emulation of
                    // the analog triggers of the GameCube controller.
                    if (button == ui->buttonZL || button == ui->buttonZR) {
                        params.Set("direction", "+");
                        params.Set("threshold", "0.5");
                    }
                    *param = std::move(params);
                },
                type);
        });
    };

    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; ++button_id) {
        auto* const button = button_map[button_id];

        if (button == nullptr) {
            continue;
        }

        ConfigureButtonClick(button_map[button_id], button_id, &buttons_param[button_id],
                             Config::default_buttons[button_id],
                             InputCommon::Polling::DeviceType::Button);

        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QPushButton::customContextMenuRequested,
                [=, this](const QPoint& menu_location) {
                    QMenu context_menu;
                    context_menu.addAction(tr("Clear"), [&] {
                        buttons_param[button_id].Clear();
                        button_map[button_id]->setText(tr("[not set]"));
                    });
                    context_menu.addAction(tr("Toggle button"), [&] {
                        const bool toggle_value = !buttons_param[button_id].Get("toggle", false);
                        buttons_param[button_id].Set("toggle", toggle_value);
                        button_map[button_id]->setText(ButtonToText(buttons_param[button_id]));
                    });
                    if (buttons_param[button_id].Has("threshold")) {
                        context_menu.addAction(tr("Set threshold"), [&] {
                            const int button_threshold = static_cast<int>(
                                buttons_param[button_id].Get("threshold", 0.5f) * 100.0f);
                            const int new_threshold = QInputDialog::getInt(
                                this, tr("Set threshold"), tr("Choose a value between 0% and 100%"),
                                button_threshold, 0, 100);
                            buttons_param[button_id].Set("threshold", new_threshold / 100.0f);
                        });
                    }
                    context_menu.exec(button_map[button_id]->mapToGlobal(menu_location));
                    ui->controllerFrame->SetPlayerInput(player_index, buttons_param, analogs_param);
                });
    }

    for (int motion_id = 0; motion_id < Settings::NativeMotion::NumMotions; ++motion_id) {
        auto* const button = motion_map[motion_id];
        if (button == nullptr) {
            continue;
        }

        ConfigureButtonClick(motion_map[motion_id], motion_id, &motions_param[motion_id],
                             Config::default_motions[motion_id],
                             InputCommon::Polling::DeviceType::Motion);

        button->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(button, &QPushButton::customContextMenuRequested,
                [=, this](const QPoint& menu_location) {
                    QMenu context_menu;
                    context_menu.addAction(tr("Clear"), [&] {
                        motions_param[motion_id].Clear();
                        motion_map[motion_id]->setText(tr("[not set]"));
                    });
                    context_menu.exec(motion_map[motion_id]->mapToGlobal(menu_location));
                });
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; ++analog_id) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; ++sub_button_id) {
            auto* const analog_button = analog_map_buttons[analog_id][sub_button_id];

            if (analog_button == nullptr) {
                continue;
            }

            connect(analog_button, &QPushButton::clicked, [=, this] {
                if (!map_analog_stick_accepted) {
                    map_analog_stick_accepted =
                        QMessageBox::information(
                            this, tr("Map Analog Stick"),
                            tr("After pressing OK, first move your joystick horizontally, and then "
                               "vertically.\nTo invert the axes, first move your joystick "
                               "vertically, and then horizontally."),
                            QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok;
                    if (!map_analog_stick_accepted) {
                        return;
                    }
                }
                HandleClick(
                    analog_map_buttons[analog_id][sub_button_id], analog_id,
                    [=, this](const Common::ParamPackage& params) {
                        SetAnalogParam(params, analogs_param[analog_id],
                                       analog_sub_buttons[sub_button_id]);
                    },
                    InputCommon::Polling::DeviceType::AnalogPreferred);
            });

            analog_button->setContextMenuPolicy(Qt::CustomContextMenu);

            connect(
                analog_button, &QPushButton::customContextMenuRequested,
                [=, this](const QPoint& menu_location) {
                    QMenu context_menu;
                    context_menu.addAction(tr("Clear"), [&] {
                        analogs_param[analog_id].Clear();
                        analog_map_buttons[analog_id][sub_button_id]->setText(tr("[not set]"));
                    });
                    context_menu.addAction(tr("Invert axis"), [&] {
                        if (sub_button_id == 2 || sub_button_id == 3) {
                            const bool invert_value =
                                analogs_param[analog_id].Get("invert_x", "+") == "-";
                            const std::string invert_str = invert_value ? "+" : "-";
                            analogs_param[analog_id].Set("invert_x", invert_str);
                        }
                        if (sub_button_id == 0 || sub_button_id == 1) {
                            const bool invert_value =
                                analogs_param[analog_id].Get("invert_y", "+") == "-";
                            const std::string invert_str = invert_value ? "+" : "-";
                            analogs_param[analog_id].Set("invert_y", invert_str);
                        }
                        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM;
                             ++sub_button_id) {
                            analog_map_buttons[analog_id][sub_button_id]->setText(AnalogToText(
                                analogs_param[analog_id], analog_sub_buttons[sub_button_id]));
                        }
                    });
                    context_menu.exec(
                        analog_map_buttons[analog_id][sub_button_id]->mapToGlobal(menu_location));
                    ui->controllerFrame->SetPlayerInput(player_index, buttons_param, analogs_param);
                });
        }

        // Handle clicks for the modifier buttons as well.
        connect(analog_map_modifier_button[analog_id], &QPushButton::clicked, [=, this] {
            HandleClick(
                analog_map_modifier_button[analog_id], analog_id,
                [=, this](const Common::ParamPackage& params) {
                    analogs_param[analog_id].Set("modifier", params.Serialize());
                },
                InputCommon::Polling::DeviceType::Button);
        });

        analog_map_modifier_button[analog_id]->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(analog_map_modifier_button[analog_id], &QPushButton::customContextMenuRequested,
                [=, this](const QPoint& menu_location) {
                    QMenu context_menu;
                    context_menu.addAction(tr("Clear"), [&] {
                        analogs_param[analog_id].Set("modifier", "");
                        analog_map_modifier_button[analog_id]->setText(tr("[not set]"));
                    });
                    context_menu.addAction(tr("Toggle button"), [&] {
                        Common::ParamPackage modifier_param =
                            Common::ParamPackage{analogs_param[analog_id].Get("modifier", "")};
                        const bool toggle_value = !modifier_param.Get("toggle", false);
                        modifier_param.Set("toggle", toggle_value);
                        analogs_param[analog_id].Set("modifier", modifier_param.Serialize());
                        analog_map_modifier_button[analog_id]->setText(
                            ButtonToText(modifier_param));
                    });
                    context_menu.exec(
                        analog_map_modifier_button[analog_id]->mapToGlobal(menu_location));
                });

        connect(analog_map_range_spinbox[analog_id], qOverload<int>(&QSpinBox::valueChanged),
                [=, this] {
                    const auto spinbox_value = analog_map_range_spinbox[analog_id]->value();
                    analogs_param[analog_id].Set("range", spinbox_value / 100.0f);
                    ui->controllerFrame->SetPlayerInput(player_index, buttons_param, analogs_param);
                });

        connect(analog_map_deadzone_slider[analog_id], &QSlider::valueChanged, [=, this] {
            const auto slider_value = analog_map_deadzone_slider[analog_id]->value();
            analog_map_deadzone_label[analog_id]->setText(tr("Deadzone: %1%").arg(slider_value));
            analogs_param[analog_id].Set("deadzone", slider_value / 100.0f);
            ui->controllerFrame->SetPlayerInput(player_index, buttons_param, analogs_param);
        });

        connect(analog_map_modifier_slider[analog_id], &QSlider::valueChanged, [=, this] {
            const auto slider_value = analog_map_modifier_slider[analog_id]->value();
            analog_map_modifier_label[analog_id]->setText(
                tr("Modifier Range: %1%").arg(slider_value));
            analogs_param[analog_id].Set("modifier_scale", slider_value / 100.0f);
        });
    }

    // Player Connected checkbox
    connect(ui->groupConnectedController, &QGroupBox::toggled, [this](bool checked) {
        emit Connected(checked);
        ui->controllerFrame->SetConnectedStatus(checked);
    });

    if (player_index == 0) {
        connect(ui->comboControllerType, qOverload<int>(&QComboBox::currentIndexChanged),
                [this](int index) {
                    emit HandheldStateChanged(GetControllerTypeFromIndex(index) ==
                                              Settings::ControllerType::Handheld);
                });
    }

    if (debug || player_index == 9) {
        ui->groupConnectedController->setCheckable(false);
    }

    // The Debug Controller can only choose the Pro Controller.
    if (debug) {
        ui->buttonScreenshot->setEnabled(false);
        ui->buttonHome->setEnabled(false);
        ui->comboControllerType->addItem(tr("Pro Controller"));
    } else {
        SetConnectableControllers();
    }

    UpdateControllerIcon();
    UpdateControllerAvailableButtons();
    UpdateControllerEnabledButtons();
    UpdateControllerButtonNames();
    UpdateMotionButtons();
    connect(ui->comboControllerType, qOverload<int>(&QComboBox::currentIndexChanged), [this](int) {
        UpdateControllerIcon();
        UpdateControllerAvailableButtons();
        UpdateControllerEnabledButtons();
        UpdateControllerButtonNames();
        UpdateMotionButtons();
    });

    connect(ui->comboDevices, qOverload<int>(&QComboBox::activated), this,
            &ConfigureInputPlayer::UpdateMappingWithDefaults);

    ui->comboDevices->setCurrentIndex(-1);

    ui->buttonRefreshDevices->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    connect(ui->buttonRefreshDevices, &QPushButton::clicked,
            [this] { emit RefreshInputDevices(); });

    timeout_timer->setSingleShot(true);
    connect(timeout_timer.get(), &QTimer::timeout, [this] { SetPollingResult({}, true); });

    connect(poll_timer.get(), &QTimer::timeout, [this] {
        Common::ParamPackage params;
        if (input_subsystem->GetGCButtons()->IsPolling()) {
            params = input_subsystem->GetGCButtons()->GetNextInput();
            if (params.Has("engine") && IsInputAcceptable(params)) {
                SetPollingResult(params, false);
                return;
            }
        }
        if (input_subsystem->GetGCAnalogs()->IsPolling()) {
            params = input_subsystem->GetGCAnalogs()->GetNextInput();
            if (params.Has("engine") && IsInputAcceptable(params)) {
                SetPollingResult(params, false);
                return;
            }
        }
        if (input_subsystem->GetUDPMotions()->IsPolling()) {
            params = input_subsystem->GetUDPMotions()->GetNextInput();
            if (params.Has("engine")) {
                SetPollingResult(params, false);
                return;
            }
        }
        if (input_subsystem->GetMouseButtons()->IsPolling()) {
            params = input_subsystem->GetMouseButtons()->GetNextInput();
            if (params.Has("engine") && IsInputAcceptable(params)) {
                SetPollingResult(params, false);
                return;
            }
        }
        if (input_subsystem->GetMouseAnalogs()->IsPolling()) {
            params = input_subsystem->GetMouseAnalogs()->GetNextInput();
            if (params.Has("engine") && IsInputAcceptable(params)) {
                SetPollingResult(params, false);
                return;
            }
        }
        if (input_subsystem->GetMouseMotions()->IsPolling()) {
            params = input_subsystem->GetMouseMotions()->GetNextInput();
            if (params.Has("engine") && IsInputAcceptable(params)) {
                SetPollingResult(params, false);
                return;
            }
        }
        if (input_subsystem->GetMouseTouch()->IsPolling()) {
            params = input_subsystem->GetMouseTouch()->GetNextInput();
            if (params.Has("engine") && IsInputAcceptable(params)) {
                SetPollingResult(params, false);
                return;
            }
        }
        for (auto& poller : device_pollers) {
            params = poller->GetNextInput();
            if (params.Has("engine") && IsInputAcceptable(params)) {
                SetPollingResult(params, false);
                return;
            }
        }
    });

    UpdateInputProfiles();

    connect(ui->buttonProfilesNew, &QPushButton::clicked, this,
            &ConfigureInputPlayer::CreateProfile);
    connect(ui->buttonProfilesDelete, &QPushButton::clicked, this,
            &ConfigureInputPlayer::DeleteProfile);
    connect(ui->comboProfiles, qOverload<int>(&QComboBox::activated), this,
            &ConfigureInputPlayer::LoadProfile);
    connect(ui->buttonProfilesSave, &QPushButton::clicked, this,
            &ConfigureInputPlayer::SaveProfile);

    LoadConfiguration();
    ui->controllerFrame->SetPlayerInput(player_index, buttons_param, analogs_param);
    ui->controllerFrame->SetConnectedStatus(ui->groupConnectedController->isChecked());
}

ConfigureInputPlayer::~ConfigureInputPlayer() = default;

void ConfigureInputPlayer::ApplyConfiguration() {
    auto& player = Settings::values.players.GetValue()[player_index];
    auto& buttons = debug ? Settings::values.debug_pad_buttons : player.buttons;
    auto& analogs = debug ? Settings::values.debug_pad_analogs : player.analogs;

    std::transform(buttons_param.begin(), buttons_param.end(), buttons.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });
    std::transform(analogs_param.begin(), analogs_param.end(), analogs.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });

    if (debug) {
        return;
    }

    auto& motions = player.motions;

    std::transform(motions_param.begin(), motions_param.end(), motions.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });

    // Apply configuration for handheld
    if (player_index == 0) {
        auto& handheld = Settings::values.players.GetValue()[HANDHELD_INDEX];
        const auto handheld_connected = handheld.connected;
        handheld = player;
        handheld.connected = handheld_connected;
    }
}

void ConfigureInputPlayer::TryConnectSelectedController() {
    auto& player = Settings::values.players.GetValue()[player_index];

    const auto controller_type =
        GetControllerTypeFromIndex(ui->comboControllerType->currentIndex());
    const auto player_connected = ui->groupConnectedController->isChecked() &&
                                  controller_type != Settings::ControllerType::Handheld;

    // Connect Handheld depending on Player 1's controller configuration.
    if (player_index == 0) {
        auto& handheld = Settings::values.players.GetValue()[HANDHELD_INDEX];
        const auto handheld_connected = ui->groupConnectedController->isChecked() &&
                                        controller_type == Settings::ControllerType::Handheld;
        // Connect only if handheld is going from disconnected to connected
        if (!handheld.connected && handheld_connected) {
            UpdateController(controller_type, HANDHELD_INDEX, true);
        }
        handheld.connected = handheld_connected;
    }

    if (player.controller_type == controller_type && player.connected == player_connected) {
        // Set vibration devices in the event that the input device has changed.
        ConfigureVibration::SetVibrationDevices(player_index);
        return;
    }

    player.controller_type = controller_type;
    player.connected = player_connected;

    ConfigureVibration::SetVibrationDevices(player_index);

    if (!player.connected) {
        return;
    }

    UpdateController(controller_type, player_index, true);
}

void ConfigureInputPlayer::TryDisconnectSelectedController() {
    const auto& player = Settings::values.players.GetValue()[player_index];

    const auto controller_type =
        GetControllerTypeFromIndex(ui->comboControllerType->currentIndex());
    const auto player_connected = ui->groupConnectedController->isChecked() &&
                                  controller_type != Settings::ControllerType::Handheld;

    // Disconnect Handheld depending on Player 1's controller configuration.
    if (player_index == 0 && player.controller_type == Settings::ControllerType::Handheld) {
        const auto& handheld = Settings::values.players.GetValue()[HANDHELD_INDEX];
        const auto handheld_connected = ui->groupConnectedController->isChecked() &&
                                        controller_type == Settings::ControllerType::Handheld;
        // Disconnect only if handheld is going from connected to disconnected
        if (handheld.connected && !handheld_connected) {
            UpdateController(controller_type, HANDHELD_INDEX, false);
        }
        return;
    }

    // Do not do anything if the controller configuration has not changed.
    if (player.controller_type == controller_type && player.connected == player_connected) {
        return;
    }

    // Do not disconnect if the controller is already disconnected
    if (!player.connected) {
        return;
    }

    // Disconnect the controller first.
    UpdateController(controller_type, player_index, false);
}

void ConfigureInputPlayer::showEvent(QShowEvent* event) {
    if (bottom_row == nullptr) {
        return;
    }
    QWidget::showEvent(event);
    ui->main->addWidget(bottom_row);
}

void ConfigureInputPlayer::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureInputPlayer::RetranslateUI() {
    ui->retranslateUi(this);
    UpdateUI();
}

void ConfigureInputPlayer::LoadConfiguration() {
    auto& player = Settings::values.players.GetValue()[player_index];
    if (debug) {
        std::transform(Settings::values.debug_pad_buttons.begin(),
                       Settings::values.debug_pad_buttons.end(), buttons_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
        std::transform(Settings::values.debug_pad_analogs.begin(),
                       Settings::values.debug_pad_analogs.end(), analogs_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
    } else {
        std::transform(player.buttons.begin(), player.buttons.end(), buttons_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
        std::transform(player.analogs.begin(), player.analogs.end(), analogs_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
        std::transform(player.motions.begin(), player.motions.end(), motions_param.begin(),
                       [](const std::string& str) { return Common::ParamPackage(str); });
    }

    UpdateUI();
    UpdateInputDeviceCombobox();

    if (debug) {
        return;
    }

    ui->comboControllerType->setCurrentIndex(GetIndexFromControllerType(player.controller_type));
    ui->groupConnectedController->setChecked(
        player.connected ||
        (player_index == 0 && Settings::values.players.GetValue()[HANDHELD_INDEX].connected));
}

void ConfigureInputPlayer::ConnectPlayer(bool connected) {
    ui->groupConnectedController->setChecked(connected);
}

void ConfigureInputPlayer::UpdateInputDeviceCombobox() {
    // Skip input device persistence if "Input Devices" is set to "Any".
    if (ui->comboDevices->currentIndex() == 0) {
        UpdateInputDevices();
        return;
    }

    // Find the first button that isn't empty.
    const auto button_param =
        std::find_if(buttons_param.begin(), buttons_param.end(),
                     [](const Common::ParamPackage param) { return param.Has("engine"); });
    const bool buttons_empty = button_param == buttons_param.end();

    const auto current_engine = button_param->Get("engine", "");
    const auto current_guid = button_param->Get("guid", "");
    const auto current_port = button_param->Get("port", "");

    const bool is_keyboard_mouse = current_engine == "keyboard" || current_engine == "mouse";

    UpdateInputDevices();

    if (buttons_empty) {
        return;
    }

    const bool all_one_device =
        std::all_of(buttons_param.begin(), buttons_param.end(),
                    [current_engine, current_guid, current_port,
                     is_keyboard_mouse](const Common::ParamPackage param) {
                        if (is_keyboard_mouse) {
                            return !param.Has("engine") || param.Get("engine", "") == "keyboard" ||
                                   param.Get("engine", "") == "mouse";
                        }
                        return !param.Has("engine") || (param.Get("engine", "") == current_engine &&
                                                        param.Get("guid", "") == current_guid &&
                                                        param.Get("port", "") == current_port);
                    });

    if (all_one_device) {
        if (is_keyboard_mouse) {
            ui->comboDevices->setCurrentIndex(1);
            return;
        }
        const auto devices_it = std::find_if(
            input_devices.begin(), input_devices.end(),
            [current_engine, current_guid, current_port](const Common::ParamPackage param) {
                return param.Get("class", "") == current_engine &&
                       param.Get("guid", "") == current_guid &&
                       param.Get("port", "") == current_port;
            });
        const int device_index =
            devices_it != input_devices.end()
                ? static_cast<int>(std::distance(input_devices.begin(), devices_it))
                : 0;
        ui->comboDevices->setCurrentIndex(device_index);
    } else {
        ui->comboDevices->setCurrentIndex(0);
    }
}

void ConfigureInputPlayer::RestoreDefaults() {
    UpdateMappingWithDefaults();
}

void ConfigureInputPlayer::ClearAll() {
    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; ++button_id) {
        const auto* const button = button_map[button_id];
        if (button == nullptr) {
            continue;
        }

        buttons_param[button_id].Clear();
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; ++analog_id) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; ++sub_button_id) {
            const auto* const analog_button = analog_map_buttons[analog_id][sub_button_id];
            if (analog_button == nullptr) {
                continue;
            }

            analogs_param[analog_id].Clear();
        }
    }

    for (int motion_id = 0; motion_id < Settings::NativeMotion::NumMotions; ++motion_id) {
        const auto* const motion_button = motion_map[motion_id];
        if (motion_button == nullptr) {
            continue;
        }

        motions_param[motion_id].Clear();
    }

    UpdateUI();
    UpdateInputDevices();
}

void ConfigureInputPlayer::UpdateUI() {
    for (int button = 0; button < Settings::NativeButton::NumButtons; ++button) {
        button_map[button]->setText(ButtonToText(buttons_param[button]));
    }

    for (int motion_id = 0; motion_id < Settings::NativeMotion::NumMotions; ++motion_id) {
        motion_map[motion_id]->setText(ButtonToText(motions_param[motion_id]));
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; ++analog_id) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; ++sub_button_id) {
            auto* const analog_button = analog_map_buttons[analog_id][sub_button_id];

            if (analog_button == nullptr) {
                continue;
            }

            analog_button->setText(
                AnalogToText(analogs_param[analog_id], analog_sub_buttons[sub_button_id]));
        }

        analog_map_modifier_button[analog_id]->setText(
            ButtonToText(Common::ParamPackage{analogs_param[analog_id].Get("modifier", "")}));

        const auto deadzone_label = analog_map_deadzone_label[analog_id];
        const auto deadzone_slider = analog_map_deadzone_slider[analog_id];
        const auto modifier_groupbox = analog_map_modifier_groupbox[analog_id];
        const auto modifier_label = analog_map_modifier_label[analog_id];
        const auto modifier_slider = analog_map_modifier_slider[analog_id];
        const auto range_groupbox = analog_map_range_groupbox[analog_id];
        const auto range_spinbox = analog_map_range_spinbox[analog_id];

        int slider_value;
        auto& param = analogs_param[analog_id];
        const bool is_controller = param.Get("engine", "") == "sdl" ||
                                   param.Get("engine", "") == "gcpad" ||
                                   param.Get("engine", "") == "mouse";

        if (is_controller) {
            if (!param.Has("deadzone")) {
                param.Set("deadzone", 0.1f);
            }
            slider_value = static_cast<int>(param.Get("deadzone", 0.1f) * 100);
            deadzone_label->setText(tr("Deadzone: %1%").arg(slider_value));
            deadzone_slider->setValue(slider_value);
            if (!param.Has("range")) {
                param.Set("range", 1.0f);
            }
            range_spinbox->setValue(static_cast<int>(param.Get("range", 1.0f) * 100));
        } else {
            if (!param.Has("modifier_scale")) {
                param.Set("modifier_scale", 0.5f);
            }
            slider_value = static_cast<int>(param.Get("modifier_scale", 0.5f) * 100);
            modifier_label->setText(tr("Modifier Range: %1%").arg(slider_value));
            modifier_slider->setValue(slider_value);
        }

        deadzone_label->setVisible(is_controller);
        deadzone_slider->setVisible(is_controller);
        modifier_groupbox->setVisible(!is_controller);
        modifier_label->setVisible(!is_controller);
        modifier_slider->setVisible(!is_controller);
        range_groupbox->setVisible(is_controller);
        ui->controllerFrame->SetPlayerInput(player_index, buttons_param, analogs_param);
    }
}

void ConfigureInputPlayer::SetConnectableControllers() {
    const auto add_controllers = [this](bool enable_all,
                                        Controller_NPad::NpadStyleSet npad_style_set = {}) {
        index_controller_type_pairs.clear();
        ui->comboControllerType->clear();

        if (enable_all || npad_style_set.fullkey == 1) {
            index_controller_type_pairs.emplace_back(ui->comboControllerType->count(),
                                                     Settings::ControllerType::ProController);
            ui->comboControllerType->addItem(tr("Pro Controller"));
        }

        if (enable_all || npad_style_set.joycon_dual == 1) {
            index_controller_type_pairs.emplace_back(ui->comboControllerType->count(),
                                                     Settings::ControllerType::DualJoyconDetached);
            ui->comboControllerType->addItem(tr("Dual Joycons"));
        }

        if (enable_all || npad_style_set.joycon_left == 1) {
            index_controller_type_pairs.emplace_back(ui->comboControllerType->count(),
                                                     Settings::ControllerType::LeftJoycon);
            ui->comboControllerType->addItem(tr("Left Joycon"));
        }

        if (enable_all || npad_style_set.joycon_right == 1) {
            index_controller_type_pairs.emplace_back(ui->comboControllerType->count(),
                                                     Settings::ControllerType::RightJoycon);
            ui->comboControllerType->addItem(tr("Right Joycon"));
        }

        if (player_index == 0 && (enable_all || npad_style_set.handheld == 1)) {
            index_controller_type_pairs.emplace_back(ui->comboControllerType->count(),
                                                     Settings::ControllerType::Handheld);
            ui->comboControllerType->addItem(tr("Handheld"));
        }

        if (enable_all || npad_style_set.gamecube == 1) {
            index_controller_type_pairs.emplace_back(ui->comboControllerType->count(),
                                                     Settings::ControllerType::GameCube);
            ui->comboControllerType->addItem(tr("GameCube Controller"));
        }
    };

    Core::System& system{Core::System::GetInstance()};

    if (!system.IsPoweredOn()) {
        add_controllers(true);
        return;
    }

    Service::SM::ServiceManager& sm = system.ServiceManager();

    auto& npad = sm.GetService<Hid>("hid")->GetAppletResource()->GetController<Controller_NPad>(
        HidController::NPad);

    add_controllers(false, npad.GetSupportedStyleSet());
}

Settings::ControllerType ConfigureInputPlayer::GetControllerTypeFromIndex(int index) const {
    const auto it =
        std::find_if(index_controller_type_pairs.begin(), index_controller_type_pairs.end(),
                     [index](const auto& pair) { return pair.first == index; });

    if (it == index_controller_type_pairs.end()) {
        return Settings::ControllerType::ProController;
    }

    return it->second;
}

int ConfigureInputPlayer::GetIndexFromControllerType(Settings::ControllerType type) const {
    const auto it =
        std::find_if(index_controller_type_pairs.begin(), index_controller_type_pairs.end(),
                     [type](const auto& pair) { return pair.second == type; });

    if (it == index_controller_type_pairs.end()) {
        return -1;
    }

    return it->first;
}

void ConfigureInputPlayer::UpdateInputDevices() {
    input_devices = input_subsystem->GetInputDevices();
    ui->comboDevices->clear();
    for (auto device : input_devices) {
        ui->comboDevices->addItem(QString::fromStdString(device.Get("display", "Unknown")), {});
    }
}

void ConfigureInputPlayer::UpdateControllerIcon() {
    // We aren't using Qt's built in theme support here since we aren't drawing an icon (and its
    // "nonstandard" to use an image through the icon support)
    const QString stylesheet = [this] {
        switch (GetControllerTypeFromIndex(ui->comboControllerType->currentIndex())) {
        case Settings::ControllerType::ProController:
            return QStringLiteral("image: url(:/controller/pro_controller%0)");
        case Settings::ControllerType::DualJoyconDetached:
            return QStringLiteral("image: url(:/controller/dual_joycon%0)");
        case Settings::ControllerType::LeftJoycon:
            return QStringLiteral("image: url(:/controller/single_joycon_left_vertical%0)");
        case Settings::ControllerType::RightJoycon:
            return QStringLiteral("image: url(:/controller/single_joycon_right_vertical%0)");
        case Settings::ControllerType::Handheld:
            return QStringLiteral("image: url(:/controller/handheld%0)");
        default:
            return QString{};
        }
    }();

    const QString theme = [] {
        if (QIcon::themeName().contains(QStringLiteral("dark"))) {
            return QStringLiteral("_dark");
        } else if (QIcon::themeName().contains(QStringLiteral("midnight"))) {
            return QStringLiteral("_midnight");
        } else {
            return QString{};
        }
    }();
    ui->controllerFrame->SetControllerType(
        GetControllerTypeFromIndex(ui->comboControllerType->currentIndex()));
}

void ConfigureInputPlayer::UpdateControllerAvailableButtons() {
    auto layout = GetControllerTypeFromIndex(ui->comboControllerType->currentIndex());
    if (debug) {
        layout = Settings::ControllerType::ProController;
    }

    // List of all the widgets that will be hidden by any of the following layouts that need
    // "unhidden" after the controller type changes
    const std::array<QWidget*, 11> layout_show = {
        ui->buttonShoulderButtonsSLSR,
        ui->horizontalSpacerShoulderButtonsWidget,
        ui->horizontalSpacerShoulderButtonsWidget2,
        ui->buttonShoulderButtonsLeft,
        ui->buttonMiscButtonsMinusScreenshot,
        ui->bottomLeft,
        ui->buttonShoulderButtonsRight,
        ui->buttonMiscButtonsPlusHome,
        ui->bottomRight,
        ui->buttonMiscButtonsMinusGroup,
        ui->buttonMiscButtonsScreenshotGroup,
    };

    for (auto* widget : layout_show) {
        widget->show();
    }

    std::vector<QWidget*> layout_hidden;
    switch (layout) {
    case Settings::ControllerType::ProController:
    case Settings::ControllerType::DualJoyconDetached:
    case Settings::ControllerType::Handheld:
        layout_hidden = {
            ui->buttonShoulderButtonsSLSR,
            ui->horizontalSpacerShoulderButtonsWidget2,
        };
        break;
    case Settings::ControllerType::LeftJoycon:
        layout_hidden = {
            ui->horizontalSpacerShoulderButtonsWidget2,
            ui->buttonShoulderButtonsRight,
            ui->buttonMiscButtonsPlusHome,
            ui->bottomRight,
        };
        break;
    case Settings::ControllerType::RightJoycon:
        layout_hidden = {
            ui->horizontalSpacerShoulderButtonsWidget,
            ui->buttonShoulderButtonsLeft,
            ui->buttonMiscButtonsMinusScreenshot,
            ui->bottomLeft,
        };
        break;
    case Settings::ControllerType::GameCube:
        layout_hidden = {
            ui->buttonShoulderButtonsSLSR,
            ui->horizontalSpacerShoulderButtonsWidget2,
            ui->buttonMiscButtonsMinusGroup,
            ui->buttonMiscButtonsScreenshotGroup,
        };
        break;
    }

    for (auto* widget : layout_hidden) {
        widget->hide();
    }
}

void ConfigureInputPlayer::UpdateControllerEnabledButtons() {
    auto layout = GetControllerTypeFromIndex(ui->comboControllerType->currentIndex());
    if (debug) {
        layout = Settings::ControllerType::ProController;
    }

    // List of all the widgets that will be disabled by any of the following layouts that need
    // "enabled" after the controller type changes
    const std::array<QWidget*, 4> layout_enable = {
        ui->buttonHome,
        ui->buttonLStickPressedGroup,
        ui->groupRStickPressed,
        ui->buttonShoulderButtonsButtonLGroup,
    };

    for (auto* widget : layout_enable) {
        widget->setEnabled(true);
    }

    std::vector<QWidget*> layout_disable;
    switch (layout) {
    case Settings::ControllerType::ProController:
    case Settings::ControllerType::DualJoyconDetached:
    case Settings::ControllerType::Handheld:
    case Settings::ControllerType::LeftJoycon:
    case Settings::ControllerType::RightJoycon:
        // TODO(wwylele): enable this when we actually emulate it
        layout_disable = {
            ui->buttonHome,
        };
        break;
    case Settings::ControllerType::GameCube:
        layout_disable = {
            ui->buttonHome,
            ui->buttonLStickPressedGroup,
            ui->groupRStickPressed,
            ui->buttonShoulderButtonsButtonLGroup,
        };
        break;
    }

    for (auto* widget : layout_disable) {
        widget->setEnabled(false);
    }
}

void ConfigureInputPlayer::UpdateMotionButtons() {
    if (debug) {
        // Motion isn't used with the debug controller, hide both groupboxes.
        ui->buttonMotionLeftGroup->hide();
        ui->buttonMotionRightGroup->hide();
        return;
    }

    // Show/hide the "Motion 1/2" groupboxes depending on the currently selected controller.
    switch (GetControllerTypeFromIndex(ui->comboControllerType->currentIndex())) {
    case Settings::ControllerType::ProController:
    case Settings::ControllerType::LeftJoycon:
    case Settings::ControllerType::Handheld:
        // Show "Motion 1" and hide "Motion 2".
        ui->buttonMotionLeftGroup->show();
        ui->buttonMotionRightGroup->hide();
        break;
    case Settings::ControllerType::RightJoycon:
        // Show "Motion 2" and hide "Motion 1".
        ui->buttonMotionLeftGroup->hide();
        ui->buttonMotionRightGroup->show();
        break;
    case Settings::ControllerType::GameCube:
        // Hide both "Motion 1/2".
        ui->buttonMotionLeftGroup->hide();
        ui->buttonMotionRightGroup->hide();
        break;
    case Settings::ControllerType::DualJoyconDetached:
    default:
        // Show both "Motion 1/2".
        ui->buttonMotionLeftGroup->show();
        ui->buttonMotionRightGroup->show();
        break;
    }
}

void ConfigureInputPlayer::UpdateControllerButtonNames() {
    auto layout = GetControllerTypeFromIndex(ui->comboControllerType->currentIndex());
    if (debug) {
        layout = Settings::ControllerType::ProController;
    }

    switch (layout) {
    case Settings::ControllerType::ProController:
    case Settings::ControllerType::DualJoyconDetached:
    case Settings::ControllerType::Handheld:
    case Settings::ControllerType::LeftJoycon:
    case Settings::ControllerType::RightJoycon:
        ui->buttonMiscButtonsPlusGroup->setTitle(tr("Plus"));
        ui->buttonShoulderButtonsButtonZLGroup->setTitle(tr("ZL"));
        ui->buttonShoulderButtonsZRGroup->setTitle(tr("ZR"));
        ui->buttonShoulderButtonsRGroup->setTitle(tr("R"));
        ui->LStick->setTitle(tr("Left Stick"));
        ui->RStick->setTitle(tr("Right Stick"));
        break;
    case Settings::ControllerType::GameCube:
        ui->buttonMiscButtonsPlusGroup->setTitle(tr("Start / Pause"));
        ui->buttonShoulderButtonsButtonZLGroup->setTitle(tr("L"));
        ui->buttonShoulderButtonsZRGroup->setTitle(tr("R"));
        ui->buttonShoulderButtonsRGroup->setTitle(tr("Z"));
        ui->LStick->setTitle(tr("Control Stick"));
        ui->RStick->setTitle(tr("C-Stick"));
        break;
    }
}

void ConfigureInputPlayer::UpdateMappingWithDefaults() {
    if (ui->comboDevices->currentIndex() == 0) {
        return;
    }

    if (ui->comboDevices->currentIndex() == 1) {
        // Reset keyboard bindings
        for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; ++button_id) {
            buttons_param[button_id] = Common::ParamPackage{
                InputCommon::GenerateKeyboardParam(Config::default_buttons[button_id])};
        }
        for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; ++analog_id) {
            for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; ++sub_button_id) {
                Common::ParamPackage params{InputCommon::GenerateKeyboardParam(
                    Config::default_analogs[analog_id][sub_button_id])};
                SetAnalogParam(params, analogs_param[analog_id], analog_sub_buttons[sub_button_id]);
            }

            analogs_param[analog_id].Set("modifier", InputCommon::GenerateKeyboardParam(
                                                         Config::default_stick_mod[analog_id]));
        }

        for (int motion_id = 0; motion_id < Settings::NativeMotion::NumMotions; ++motion_id) {
            motions_param[motion_id] = Common::ParamPackage{
                InputCommon::GenerateKeyboardParam(Config::default_motions[motion_id])};
        }

        UpdateUI();
        return;
    }

    // Reset controller bindings
    const auto& device = input_devices[ui->comboDevices->currentIndex()];
    auto button_mapping = input_subsystem->GetButtonMappingForDevice(device);
    auto analog_mapping = input_subsystem->GetAnalogMappingForDevice(device);
    auto motion_mapping = input_subsystem->GetMotionMappingForDevice(device);
    for (std::size_t i = 0; i < buttons_param.size(); ++i) {
        buttons_param[i] = button_mapping[static_cast<Settings::NativeButton::Values>(i)];
    }
    for (std::size_t i = 0; i < analogs_param.size(); ++i) {
        analogs_param[i] = analog_mapping[static_cast<Settings::NativeAnalog::Values>(i)];
    }
    for (std::size_t i = 0; i < motions_param.size(); ++i) {
        motions_param[i] = motion_mapping[static_cast<Settings::NativeMotion::Values>(i)];
    }

    UpdateUI();
}

void ConfigureInputPlayer::HandleClick(
    QPushButton* button, std::size_t button_id,
    std::function<void(const Common::ParamPackage&)> new_input_setter,
    InputCommon::Polling::DeviceType type) {
    if (button == ui->buttonMotionLeft || button == ui->buttonMotionRight) {
        button->setText(tr("Shake!"));
    } else {
        button->setText(tr("[waiting]"));
    }
    button->setFocus();

    // The first two input devices are always Any and Keyboard/Mouse. If the user filtered to a
    // controller, then they don't want keyboard/mouse input
    want_keyboard_mouse = ui->comboDevices->currentIndex() < 2;

    input_setter = new_input_setter;

    device_pollers = input_subsystem->GetPollers(type);

    for (auto& poller : device_pollers) {
        poller->Start();
    }

    QWidget::grabMouse();
    QWidget::grabKeyboard();

    if (type == InputCommon::Polling::DeviceType::Button) {
        input_subsystem->GetGCButtons()->BeginConfiguration();
    } else {
        input_subsystem->GetGCAnalogs()->BeginConfiguration();
    }

    if (type == InputCommon::Polling::DeviceType::Motion) {
        input_subsystem->GetUDPMotions()->BeginConfiguration();
    }

    if (type == InputCommon::Polling::DeviceType::Button) {
        input_subsystem->GetMouseButtons()->BeginConfiguration();
    } else if (type == InputCommon::Polling::DeviceType::AnalogPreferred) {
        input_subsystem->GetMouseAnalogs()->BeginConfiguration();
    } else if (type == InputCommon::Polling::DeviceType::Motion) {
        input_subsystem->GetMouseMotions()->BeginConfiguration();
    } else {
        input_subsystem->GetMouseTouch()->BeginConfiguration();
    }

    if (type == InputCommon::Polling::DeviceType::Button) {
        ui->controllerFrame->BeginMappingButton(button_id);
    } else if (type == InputCommon::Polling::DeviceType::AnalogPreferred) {
        ui->controllerFrame->BeginMappingAnalog(button_id);
    }

    timeout_timer->start(2500); // Cancel after 2.5 seconds
    poll_timer->start(50);      // Check for new inputs every 50ms
}

void ConfigureInputPlayer::SetPollingResult(const Common::ParamPackage& params, bool abort) {
    timeout_timer->stop();
    poll_timer->stop();
    for (auto& poller : device_pollers) {
        poller->Stop();
    }

    QWidget::releaseMouse();
    QWidget::releaseKeyboard();

    input_subsystem->GetGCButtons()->EndConfiguration();
    input_subsystem->GetGCAnalogs()->EndConfiguration();

    input_subsystem->GetUDPMotions()->EndConfiguration();

    input_subsystem->GetMouseButtons()->EndConfiguration();
    input_subsystem->GetMouseAnalogs()->EndConfiguration();
    input_subsystem->GetMouseMotions()->EndConfiguration();
    input_subsystem->GetMouseTouch()->EndConfiguration();

    if (!abort) {
        (*input_setter)(params);
    }

    UpdateUI();
    UpdateInputDeviceCombobox();
    ui->controllerFrame->EndMapping();

    input_setter = std::nullopt;
}

bool ConfigureInputPlayer::IsInputAcceptable(const Common::ParamPackage& params) const {
    if (ui->comboDevices->currentIndex() == 0) {
        return true;
    }

    // Keyboard/Mouse
    if (ui->comboDevices->currentIndex() == 1) {
        return params.Get("engine", "") == "keyboard" || params.Get("engine", "") == "mouse";
    }

    const auto current_input_device = input_devices[ui->comboDevices->currentIndex()];
    return params.Get("engine", "") == current_input_device.Get("class", "") &&
           params.Get("guid", "") == current_input_device.Get("guid", "") &&
           params.Get("port", "") == current_input_device.Get("port", "");
}

void ConfigureInputPlayer::mousePressEvent(QMouseEvent* event) {
    if (!input_setter || !event) {
        return;
    }

    const auto button = GRenderWindow::QtButtonToMouseButton(event->button());
    input_subsystem->GetMouse()->PressButton(0, 0, button);
}

void ConfigureInputPlayer::keyPressEvent(QKeyEvent* event) {
    if (!input_setter || !event) {
        return;
    }

    if (event->key() != Qt::Key_Escape) {
        if (want_keyboard_mouse) {
            SetPollingResult(Common::ParamPackage{InputCommon::GenerateKeyboardParam(event->key())},
                             false);
        } else {
            // Escape key wasn't pressed and we don't want any keyboard keys, so don't stop polling
            return;
        }
    }

    SetPollingResult({}, true);
}

void ConfigureInputPlayer::CreateProfile() {
    const auto profile_name =
        LimitableInputDialog::GetText(this, tr("New Profile"), tr("Enter a profile name:"), 1, 20,
                                      LimitableInputDialog::InputLimiter::Filesystem);

    if (profile_name.isEmpty()) {
        return;
    }

    if (!InputProfiles::IsProfileNameValid(profile_name.toStdString())) {
        QMessageBox::critical(this, tr("Create Input Profile"),
                              tr("The given profile name is not valid!"));
        return;
    }

    ApplyConfiguration();

    if (!profiles->CreateProfile(profile_name.toStdString(), player_index)) {
        QMessageBox::critical(this, tr("Create Input Profile"),
                              tr("Failed to create the input profile \"%1\"").arg(profile_name));
        UpdateInputProfiles();
        emit RefreshInputProfiles(player_index);
        return;
    }

    emit RefreshInputProfiles(player_index);

    ui->comboProfiles->addItem(profile_name);
    ui->comboProfiles->setCurrentIndex(ui->comboProfiles->count() - 1);
}

void ConfigureInputPlayer::DeleteProfile() {
    const QString profile_name = ui->comboProfiles->itemText(ui->comboProfiles->currentIndex());

    if (profile_name.isEmpty()) {
        return;
    }

    if (!profiles->DeleteProfile(profile_name.toStdString())) {
        QMessageBox::critical(this, tr("Delete Input Profile"),
                              tr("Failed to delete the input profile \"%1\"").arg(profile_name));
        UpdateInputProfiles();
        emit RefreshInputProfiles(player_index);
        return;
    }

    emit RefreshInputProfiles(player_index);

    ui->comboProfiles->removeItem(ui->comboProfiles->currentIndex());
    ui->comboProfiles->setCurrentIndex(-1);
}

void ConfigureInputPlayer::LoadProfile() {
    const QString profile_name = ui->comboProfiles->itemText(ui->comboProfiles->currentIndex());

    if (profile_name.isEmpty()) {
        return;
    }

    ApplyConfiguration();

    if (!profiles->LoadProfile(profile_name.toStdString(), player_index)) {
        QMessageBox::critical(this, tr("Load Input Profile"),
                              tr("Failed to load the input profile \"%1\"").arg(profile_name));
        UpdateInputProfiles();
        emit RefreshInputProfiles(player_index);
        return;
    }

    LoadConfiguration();
}

void ConfigureInputPlayer::SaveProfile() {
    const QString profile_name = ui->comboProfiles->itemText(ui->comboProfiles->currentIndex());

    if (profile_name.isEmpty()) {
        return;
    }

    ApplyConfiguration();

    if (!profiles->SaveProfile(profile_name.toStdString(), player_index)) {
        QMessageBox::critical(this, tr("Save Input Profile"),
                              tr("Failed to save the input profile \"%1\"").arg(profile_name));
        UpdateInputProfiles();
        emit RefreshInputProfiles(player_index);
        return;
    }
}

void ConfigureInputPlayer::UpdateInputProfiles() {
    ui->comboProfiles->clear();

    for (const auto& profile_name : profiles->GetInputProfileNames()) {
        ui->comboProfiles->addItem(QString::fromStdString(profile_name));
    }

    ui->comboProfiles->setCurrentIndex(-1);
}
