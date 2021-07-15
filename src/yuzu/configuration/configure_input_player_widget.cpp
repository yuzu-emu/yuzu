// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include "yuzu/configuration/configure_input_player_widget.h"

PlayerControlPreview::PlayerControlPreview(QWidget* parent) : QFrame(parent) {
    UpdateColors();
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, QOverload<>::of(&PlayerControlPreview::UpdateInput));

    // refresh at 60hz
    timer->start(16);
}

PlayerControlPreview::~PlayerControlPreview() = default;

void PlayerControlPreview::SetPlayerInput(std::size_t index, const ButtonParam& buttons_param,
                                          const AnalogParam& analogs_param) {
    player_index = index;
    Settings::ButtonsRaw buttonss;
    Settings::AnalogsRaw analogs;
    std::transform(buttons_param.begin(), buttons_param.end(), buttonss.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });
    std::transform(analogs_param.begin(), analogs_param.end(), analogs.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });

    std::transform(buttonss.begin() + Settings::NativeButton::BUTTON_HID_BEGIN,
                   buttonss.begin() + Settings::NativeButton::BUTTON_NS_END, buttons.begin(),
                   Input::CreateDevice<Input::ButtonDevice>);
    std::transform(analogs.begin() + Settings::NativeAnalog::STICK_HID_BEGIN,
                   analogs.begin() + Settings::NativeAnalog::STICK_HID_END, sticks.begin(),
                   Input::CreateDevice<Input::AnalogDevice>);
    UpdateColors();
}
void PlayerControlPreview::SetPlayerInputRaw(std::size_t index,
                                             const Settings::ButtonsRaw& buttons_,
                                             Settings::AnalogsRaw analogs_) {
    player_index = index;
    std::transform(buttons_.begin() + Settings::NativeButton::BUTTON_HID_BEGIN,
                   buttons_.begin() + Settings::NativeButton::BUTTON_NS_END, buttons.begin(),
                   Input::CreateDevice<Input::ButtonDevice>);
    std::transform(analogs_.begin() + Settings::NativeAnalog::STICK_HID_BEGIN,
                   analogs_.begin() + Settings::NativeAnalog::STICK_HID_END, sticks.begin(),
                   Input::CreateDevice<Input::AnalogDevice>);
    UpdateColors();
}

PlayerControlPreview::LedPattern PlayerControlPreview::GetColorPattern(std::size_t index,
                                                                       bool player_on) {
    if (!player_on) {
        return {0, 0, 0, 0};
    }

    switch (index) {
    case 0:
        return {1, 0, 0, 0};
    case 1:
        return {1, 1, 0, 0};
    case 2:
        return {1, 1, 1, 0};
    case 3:
        return {1, 1, 1, 1};
    case 4:
        return {1, 0, 0, 1};
    case 5:
        return {1, 0, 1, 0};
    case 6:
        return {1, 0, 1, 1};
    case 7:
        return {0, 1, 1, 0};
    default:
        return {0, 0, 0, 0};
    }
}

void PlayerControlPreview::SetConnectedStatus(bool checked) {
    LedPattern led_pattern = GetColorPattern(player_index, checked);

    led_color[0] = led_pattern.position1 ? colors.led_on : colors.led_off;
    led_color[1] = led_pattern.position2 ? colors.led_on : colors.led_off;
    led_color[2] = led_pattern.position3 ? colors.led_on : colors.led_off;
    led_color[3] = led_pattern.position4 ? colors.led_on : colors.led_off;
    is_enabled = checked;
    ResetInputs();
}

void PlayerControlPreview::SetControllerType(const Settings::ControllerType type) {
    controller_type = type;
    UpdateColors();
}

void PlayerControlPreview::BeginMappingButton(std::size_t index) {
    button_mapping_index = index;
    mapping_active = true;
}

void PlayerControlPreview::BeginMappingAnalog(std::size_t index) {
    button_mapping_index = Settings::NativeButton::LStick + index;
    analog_mapping_index = index;
    mapping_active = true;
}

void PlayerControlPreview::EndMapping() {
    button_mapping_index = Settings::NativeButton::BUTTON_NS_END;
    analog_mapping_index = Settings::NativeAnalog::NumAnalogs;
    mapping_active = false;
    blink_counter = 0;
    ResetInputs();
}

void PlayerControlPreview::UpdateColors() {
    if (QIcon::themeName().contains(QStringLiteral("dark")) ||
        QIcon::themeName().contains(QStringLiteral("midnight"))) {
        colors.primary = QColor(204, 204, 204);
        colors.button = QColor(35, 38, 41);
        colors.button2 = QColor(26, 27, 30);
        colors.slider_arrow = QColor(14, 15, 18);
        colors.font2 = QColor(255, 255, 255);
        colors.indicator = QColor(170, 238, 255);
        colors.deadzone = QColor(204, 136, 136);
        colors.slider_button = colors.button;
    }

    if (QIcon::themeName().contains(QStringLiteral("dark"))) {
        colors.outline = QColor(160, 160, 160);
    } else if (QIcon::themeName().contains(QStringLiteral("midnight"))) {
        colors.outline = QColor(145, 145, 145);
    } else {
        colors.outline = QColor(0, 0, 0);
        colors.primary = QColor(225, 225, 225);
        colors.button = QColor(109, 111, 114);
        colors.button2 = QColor(109, 111, 114);
        colors.button2 = QColor(77, 80, 84);
        colors.slider_arrow = QColor(65, 68, 73);
        colors.font2 = QColor(0, 0, 0);
        colors.indicator = QColor(0, 0, 200);
        colors.deadzone = QColor(170, 0, 0);
        colors.slider_button = QColor(153, 149, 149);
    }

    // Constant colors
    colors.highlight = QColor(170, 0, 0);
    colors.highlight2 = QColor(119, 0, 0);
    colors.slider = QColor(103, 106, 110);
    colors.transparent = QColor(0, 0, 0, 0);
    colors.font = QColor(255, 255, 255);
    colors.led_on = QColor(255, 255, 0);
    colors.led_off = QColor(170, 238, 255);

    colors.left = colors.primary;
    colors.right = colors.primary;
    // Possible alternative to set colors from settings
    // colors.left = QColor(Settings::values.players.GetValue()[player_index].body_color_left);
    // colors.right = QColor(Settings::values.players.GetValue()[player_index].body_color_right);
}

void PlayerControlPreview::ResetInputs() {
    for (std::size_t index = 0; index < button_values.size(); ++index) {
        button_values[index] = false;
    }

    for (std::size_t index = 0; index < axis_values.size(); ++index) {
        axis_values[index].properties = {0, 1, 0};
        axis_values[index].value = {0, 0};
        axis_values[index].raw_value = {0, 0};
    }
    update();
}

void PlayerControlPreview::UpdateInput() {
    if (!is_enabled && !mapping_active && !Settings::values.tas_enable) {
        return;
    }
    bool input_changed = false;
    const auto& button_state = buttons;
    for (std::size_t index = 0; index < button_values.size(); ++index) {
        bool value = false;
        if (index < Settings::NativeButton::BUTTON_NS_END) {
            value = button_state[index]->GetStatus();
        }
        bool blink = mapping_active && index == button_mapping_index;
        if (analog_mapping_index == Settings::NativeAnalog::NUM_STICKS_HID) {
            blink &= blink_counter > 25;
        }
        if (button_values[index] != value || blink) {
            input_changed = true;
        }
        button_values[index] = value || blink;
    }

    const auto& analog_state = sticks;
    for (std::size_t index = 0; index < axis_values.size(); ++index) {
        const auto [stick_x_f, stick_y_f] = analog_state[index]->GetStatus();
        const auto [stick_x_rf, stick_y_rf] = analog_state[index]->GetRawStatus();

        if (static_cast<int>(stick_x_rf * 45) !=
                static_cast<int>(axis_values[index].raw_value.x() * 45) ||
            static_cast<int>(-stick_y_rf * 45) !=
                static_cast<int>(axis_values[index].raw_value.y() * 45)) {
            input_changed = true;
        }

        axis_values[index].properties = analog_state[index]->GetAnalogProperties();
        axis_values[index].value = QPointF(stick_x_f, -stick_y_f);
        axis_values[index].raw_value = QPointF(stick_x_rf, -stick_y_rf);

        const bool blink_analog = mapping_active && index == analog_mapping_index;
        if (blink_analog) {
            input_changed = true;
            axis_values[index].value =
                QPointF(blink_counter < 25 ? -blink_counter / 25.0f : 0,
                        blink_counter > 25 ? -(blink_counter - 25) / 25.0f : 0);
        }
    }

    if (input_changed) {
        update();
        if (controller_callback.input != nullptr) {
            ControllerInput input{
                .axis_values = {std::pair<float, float>{
                                    axis_values[Settings::NativeAnalog::LStick].value.x(),
                                    axis_values[Settings::NativeAnalog::LStick].value.y()},
                                std::pair<float, float>{
                                    axis_values[Settings::NativeAnalog::RStick].value.x(),
                                    axis_values[Settings::NativeAnalog::RStick].value.y()}},
                .button_values = button_values,
                .changed = true,
            };
            controller_callback.input(std::move(input));
        }
    }

    if (mapping_active) {
        blink_counter = (blink_counter + 1) % 50;
    }
}

void PlayerControlPreview::SetCallBack(ControllerCallback callback_) {
    controller_callback = std::move(callback_);
}

void PlayerControlPreview::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF center = rect().center();

    switch (controller_type) {
    case Settings::ControllerType::Handheld:
        DrawHandheldController(p, center);
        break;
    case Settings::ControllerType::DualJoyconDetached:
        DrawDualController(p, center);
        break;
    case Settings::ControllerType::LeftJoycon:
        DrawLeftController(p, center);
        break;
    case Settings::ControllerType::RightJoycon:
        DrawRightController(p, center);
        break;
    case Settings::ControllerType::GameCube:
        DrawGCController(p, center);
        break;
    case Settings::ControllerType::ProController:
    default:
        DrawProController(p, center);
        break;
    }
}

void PlayerControlPreview::DrawLeftController(QPainter& p, const QPointF center) {
    {
        using namespace Settings::NativeButton;

        // Sideview left joystick
        DrawJoystickSideview(p, center + QPoint(142, -69),
                             -axis_values[Settings::NativeAnalog::LStick].value.y(), 1.15f,
                             button_values[LStick]);

        // Topview D-pad buttons
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRoundButton(p, center + QPoint(-163, -21), button_values[DLeft], 11, 5, Direction::Up);
        DrawRoundButton(p, center + QPoint(-117, -21), button_values[DRight], 11, 5, Direction::Up);

        // Topview left joystick
        DrawJoystickSideview(p, center + QPointF(-140.5f, -28),
                             -axis_values[Settings::NativeAnalog::LStick].value.x() + 15.0f, 1.15f,
                             button_values[LStick]);

        // Topview minus button
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRoundButton(p, center + QPoint(-111, -22), button_values[Minus], 8, 4, Direction::Up,
                        1);

        // Left trigger
        DrawLeftTriggers(p, center, button_values[L]);
        DrawRoundButton(p, center + QPoint(151, -146), button_values[L], 8, 4, Direction::Down);
        DrawLeftZTriggers(p, center, button_values[ZL]);

        // Sideview D-pad buttons
        DrawRoundButton(p, center + QPoint(135, 14), button_values[DLeft], 5, 11, Direction::Right);
        DrawRoundButton(p, center + QPoint(135, 36), button_values[DDown], 5, 11, Direction::Right);
        DrawRoundButton(p, center + QPoint(135, -10), button_values[DUp], 5, 11, Direction::Right);
        DrawRoundButton(p, center + QPoint(135, 14), button_values[DRight], 5, 11,
                        Direction::Right);
        DrawRoundButton(p, center + QPoint(135, 71), button_values[Screenshot], 3, 8,
                        Direction::Right, 1);

        // Sideview minus button
        DrawRoundButton(p, center + QPoint(135, -118), button_values[Minus], 4, 2.66f,
                        Direction::Right, 1);

        // Sideview SL and SR buttons
        button_color = colors.slider_button;
        DrawRoundButton(p, center + QPoint(59, 52), button_values[SR], 5, 12, Direction::Left);
        DrawRoundButton(p, center + QPoint(59, -69), button_values[SL], 5, 12, Direction::Left);

        DrawLeftBody(p, center);

        // Left trigger top view
        DrawLeftTriggersTopView(p, center, button_values[L]);
        DrawLeftZTriggersTopView(p, center, button_values[ZL]);
    }

    {
        // Draw joysticks
        using namespace Settings::NativeAnalog;
        DrawJoystick(p, center + QPointF(9, -69) + (axis_values[LStick].value * 8), 1.8f,
                     button_values[Settings::NativeButton::LStick]);
        DrawRawJoystick(p, center + QPointF(-140, 90), axis_values[LStick].raw_value,
                        axis_values[LStick].properties);
    }

    using namespace Settings::NativeButton;

    // D-pad constants
    const QPointF dpad_center = center + QPoint(9, 14);
    constexpr int dpad_distance = 23;
    constexpr int dpad_radius = 11;
    constexpr float dpad_arrow_size = 1.2f;

    // D-pad buttons
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawCircleButton(p, dpad_center + QPoint(dpad_distance, 0), button_values[DRight], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(0, dpad_distance), button_values[DDown], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(0, -dpad_distance), button_values[DUp], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(-dpad_distance, 0), button_values[DLeft], dpad_radius);

    // D-pad arrows
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawArrow(p, dpad_center + QPoint(dpad_distance, 0), Direction::Right, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(0, dpad_distance), Direction::Down, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(0, -dpad_distance), Direction::Up, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(-dpad_distance, 0), Direction::Left, dpad_arrow_size);

    // SR and SL buttons
    p.setPen(colors.outline);
    button_color = colors.slider_button;
    DrawRoundButton(p, center + QPoint(155, 52), button_values[SR], 5.2f, 12, Direction::None, 4);
    DrawRoundButton(p, center + QPoint(155, -69), button_values[SL], 5.2f, 12, Direction::None, 4);

    // SR and SL text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(155, 52), Symbol::SR, 1.0f);
    DrawSymbol(p, center + QPointF(155, -69), Symbol::SL, 1.0f);

    // Minus button
    button_color = colors.button;
    DrawMinusButton(p, center + QPoint(39, -118), button_values[Minus], 16);

    // Screenshot button
    DrawRoundButton(p, center + QPoint(26, 71), button_values[Screenshot], 8, 8);
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawCircle(p, center + QPoint(26, 71), 5);
}

void PlayerControlPreview::DrawRightController(QPainter& p, const QPointF center) {
    {
        using namespace Settings::NativeButton;

        // Sideview right joystick
        DrawJoystickSideview(p, center + QPoint(173 - 315, 11),
                             axis_values[Settings::NativeAnalog::RStick].value.y() + 10.0f, 1.15f,
                             button_values[Settings::NativeButton::RStick]);

        // Topview face buttons
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRoundButton(p, center + QPoint(163, -21), button_values[A], 11, 5, Direction::Up);
        DrawRoundButton(p, center + QPoint(117, -21), button_values[Y], 11, 5, Direction::Up);

        // Topview right joystick
        DrawJoystickSideview(p, center + QPointF(140, -28),
                             -axis_values[Settings::NativeAnalog::RStick].value.x() + 15.0f, 1.15f,
                             button_values[RStick]);

        // Topview plus button
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRoundButton(p, center + QPoint(111, -22), button_values[Plus], 8, 4, Direction::Up, 1);
        DrawRoundButton(p, center + QPoint(111, -22), button_values[Plus], 2.66f, 4, Direction::Up,
                        1);

        // Right trigger
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRightTriggers(p, center, button_values[R]);
        DrawRoundButton(p, center + QPoint(-151, -146), button_values[R], 8, 4, Direction::Down);
        DrawRightZTriggers(p, center, button_values[ZR]);

        // Sideview face buttons
        DrawRoundButton(p, center + QPoint(-135, -73), button_values[A], 5, 11, Direction::Left);
        DrawRoundButton(p, center + QPoint(-135, -50), button_values[B], 5, 11, Direction::Left);
        DrawRoundButton(p, center + QPoint(-135, -95), button_values[X], 5, 11, Direction::Left);
        DrawRoundButton(p, center + QPoint(-135, -73), button_values[Y], 5, 11, Direction::Left);

        // Sideview home and plus button
        DrawRoundButton(p, center + QPoint(-135, 66), button_values[Home], 3, 12, Direction::Left);
        DrawRoundButton(p, center + QPoint(-135, -118), button_values[Plus], 4, 8, Direction::Left,
                        1);
        DrawRoundButton(p, center + QPoint(-135, -118), button_values[Plus], 4, 2.66f,
                        Direction::Left, 1);

        // Sideview SL and SR buttons
        button_color = colors.slider_button;
        DrawRoundButton(p, center + QPoint(-59, 52), button_values[SL], 5, 11, Direction::Right);
        DrawRoundButton(p, center + QPoint(-59, -69), button_values[SR], 5, 11, Direction::Right);

        DrawRightBody(p, center);

        // Right trigger top view
        DrawRightTriggersTopView(p, center, button_values[R]);
        DrawRightZTriggersTopView(p, center, button_values[ZR]);
    }

    {
        // Draw joysticks
        using namespace Settings::NativeAnalog;
        DrawJoystick(p, center + QPointF(-9, 11) + (axis_values[RStick].value * 8), 1.8f,
                     button_values[Settings::NativeButton::RStick]);
        DrawRawJoystick(p, center + QPointF(140, 90), axis_values[RStick].raw_value,
                        axis_values[RStick].properties);
    }

    using namespace Settings::NativeButton;

    // Face buttons constants
    const QPointF face_center = center + QPoint(-9, -73);
    constexpr int face_distance = 23;
    constexpr int face_radius = 11;
    constexpr float text_size = 1.1f;

    // Face buttons
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawCircleButton(p, face_center + QPoint(face_distance, 0), button_values[A], face_radius);
    DrawCircleButton(p, face_center + QPoint(0, face_distance), button_values[B], face_radius);
    DrawCircleButton(p, face_center + QPoint(0, -face_distance), button_values[X], face_radius);
    DrawCircleButton(p, face_center + QPoint(-face_distance, 0), button_values[Y], face_radius);

    // Face buttons text
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, face_center + QPoint(face_distance, 0), Symbol::A, text_size);
    DrawSymbol(p, face_center + QPoint(0, face_distance), Symbol::B, text_size);
    DrawSymbol(p, face_center + QPoint(0, -face_distance), Symbol::X, text_size);
    DrawSymbol(p, face_center + QPoint(-face_distance, 1), Symbol::Y, text_size);

    // SR and SL buttons
    p.setPen(colors.outline);
    button_color = colors.slider_button;
    DrawRoundButton(p, center + QPoint(-155, 52), button_values[SL], 5, 12, Direction::None, 4.0f);
    DrawRoundButton(p, center + QPoint(-155, -69), button_values[SR], 5, 12, Direction::None, 4.0f);

    // SR and SL text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    p.rotate(-180);
    DrawSymbol(p, QPointF(-center.x(), -center.y()) + QPointF(155, 69), Symbol::SR, 1.0f);
    DrawSymbol(p, QPointF(-center.x(), -center.y()) + QPointF(155, -52), Symbol::SL, 1.0f);
    p.rotate(180);

    // Plus Button
    DrawPlusButton(p, center + QPoint(-40, -118), button_values[Plus], 16);

    // Home Button
    p.setPen(colors.outline);
    button_color = colors.slider_button;
    DrawCircleButton(p, center + QPoint(-26, 66), button_values[Home], 12);
    button_color = colors.button;
    DrawCircleButton(p, center + QPoint(-26, 66), button_values[Home], 9);
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPoint(-26, 66), Symbol::House, 5);
}

void PlayerControlPreview::DrawDualController(QPainter& p, const QPointF center) {
    {
        using namespace Settings::NativeButton;

        // Left/Right trigger
        DrawDualTriggers(p, center, button_values[L], button_values[R]);

        // Topview face buttons
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRoundButton(p, center + QPoint(200, -71), button_values[A], 10, 5, Direction::Up);
        DrawRoundButton(p, center + QPoint(160, -71), button_values[Y], 10, 5, Direction::Up);

        // Topview right joystick
        DrawJoystickSideview(p, center + QPointF(180, -78),
                             -axis_values[Settings::NativeAnalog::RStick].value.x() + 15.0f, 1,
                             button_values[RStick]);

        // Topview plus button
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRoundButton(p, center + QPoint(154, -72), button_values[Plus], 7, 4, Direction::Up, 1);
        DrawRoundButton(p, center + QPoint(154, -72), button_values[Plus], 2.33f, 4, Direction::Up,
                        1);

        // Topview D-pad buttons
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRoundButton(p, center + QPoint(-200, -71), button_values[DLeft], 10, 5, Direction::Up);
        DrawRoundButton(p, center + QPoint(-160, -71), button_values[DRight], 10, 5, Direction::Up);

        // Topview left joystick
        DrawJoystickSideview(p, center + QPointF(-180.5f, -78),
                             -axis_values[Settings::NativeAnalog::LStick].value.x() + 15.0f, 1,
                             button_values[LStick]);

        // Topview minus button
        p.setPen(colors.outline);
        button_color = colors.button;
        DrawRoundButton(p, center + QPoint(-154, -72), button_values[Minus], 7, 4, Direction::Up,
                        1);

        DrawDualBody(p, center);

        // Right trigger top view
        DrawDualTriggersTopView(p, center, button_values[L], button_values[R]);
        DrawDualZTriggersTopView(p, center, button_values[ZL], button_values[ZR]);
    }

    {
        // Draw joysticks
        using namespace Settings::NativeAnalog;
        const auto& l_stick = axis_values[LStick];
        const auto l_button = button_values[Settings::NativeButton::LStick];
        const auto& r_stick = axis_values[RStick];
        const auto r_button = button_values[Settings::NativeButton::RStick];

        DrawJoystick(p, center + QPointF(-65, -65) + (l_stick.value * 7), 1.62f, l_button);
        DrawJoystick(p, center + QPointF(65, 12) + (r_stick.value * 7), 1.62f, r_button);
        DrawRawJoystick(p, center + QPointF(-180, 90), l_stick.raw_value, l_stick.properties);
        DrawRawJoystick(p, center + QPointF(180, 90), r_stick.raw_value, r_stick.properties);
    }

    using namespace Settings::NativeButton;

    // Face buttons constants
    const QPointF face_center = center + QPoint(65, -65);
    constexpr int face_distance = 20;
    constexpr int face_radius = 10;
    constexpr float text_size = 1.0f;

    // Face buttons
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawCircleButton(p, face_center + QPoint(face_distance, 0), button_values[A], face_radius);
    DrawCircleButton(p, face_center + QPoint(0, face_distance), button_values[B], face_radius);
    DrawCircleButton(p, face_center + QPoint(0, -face_distance), button_values[X], face_radius);
    DrawCircleButton(p, face_center + QPoint(-face_distance, 0), button_values[Y], face_radius);

    // Face buttons text
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, face_center + QPoint(face_distance, 0), Symbol::A, text_size);
    DrawSymbol(p, face_center + QPoint(0, face_distance), Symbol::B, text_size);
    DrawSymbol(p, face_center + QPoint(0, -face_distance), Symbol::X, text_size);
    DrawSymbol(p, face_center + QPoint(-face_distance, 1), Symbol::Y, text_size);

    // D-pad constants
    const QPointF dpad_center = center + QPoint(-65, 12);
    constexpr int dpad_distance = 20;
    constexpr int dpad_radius = 10;
    constexpr float dpad_arrow_size = 1.1f;

    // D-pad buttons
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawCircleButton(p, dpad_center + QPoint(dpad_distance, 0), button_values[DRight], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(0, dpad_distance), button_values[DDown], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(0, -dpad_distance), button_values[DUp], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(-dpad_distance, 0), button_values[DLeft], dpad_radius);

    // D-pad arrows
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawArrow(p, dpad_center + QPoint(dpad_distance, 0), Direction::Right, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(0, dpad_distance), Direction::Down, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(0, -dpad_distance), Direction::Up, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(-dpad_distance, 0), Direction::Left, dpad_arrow_size);

    // Minus and Plus button
    button_color = colors.button;
    DrawMinusButton(p, center + QPoint(-39, -106), button_values[Minus], 14);
    DrawPlusButton(p, center + QPoint(39, -106), button_values[Plus], 14);

    // Screenshot button
    p.setPen(colors.outline);
    DrawRoundButton(p, center + QPoint(-52, 63), button_values[Screenshot], 8, 8);
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawCircle(p, center + QPoint(-52, 63), 5);

    // Home Button
    p.setPen(colors.outline);
    button_color = colors.slider_button;
    DrawCircleButton(p, center + QPoint(50, 60), button_values[Home], 11);
    button_color = colors.button;
    DrawCircleButton(p, center + QPoint(50, 60), button_values[Home], 8.5f);
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPoint(50, 60), Symbol::House, 4.2f);
}

void PlayerControlPreview::DrawHandheldController(QPainter& p, const QPointF center) {
    DrawHandheldTriggers(p, center, button_values[Settings::NativeButton::L],
                         button_values[Settings::NativeButton::R]);
    DrawHandheldBody(p, center);
    {
        // Draw joysticks
        using namespace Settings::NativeAnalog;
        const auto& l_stick = axis_values[LStick];
        const auto l_button = button_values[Settings::NativeButton::LStick];
        const auto& r_stick = axis_values[RStick];
        const auto r_button = button_values[Settings::NativeButton::RStick];

        DrawJoystick(p, center + QPointF(-171, -41) + (l_stick.value * 4), 1.0f, l_button);
        DrawJoystick(p, center + QPointF(171, 8) + (r_stick.value * 4), 1.0f, r_button);
        DrawRawJoystick(p, center + QPointF(-50, 0), l_stick.raw_value, l_stick.properties);
        DrawRawJoystick(p, center + QPointF(50, 0), r_stick.raw_value, r_stick.properties);
    }

    using namespace Settings::NativeButton;

    // Face buttons constants
    const QPointF face_center = center + QPoint(171, -41);
    constexpr float face_distance = 12.8f;
    constexpr float face_radius = 6.4f;
    constexpr float text_size = 0.6f;

    // Face buttons
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawCircleButton(p, face_center + QPoint(face_distance, 0), button_values[A], face_radius);
    DrawCircleButton(p, face_center + QPoint(0, face_distance), button_values[B], face_radius);
    DrawCircleButton(p, face_center + QPoint(0, -face_distance), button_values[X], face_radius);
    DrawCircleButton(p, face_center + QPoint(-face_distance, 0), button_values[Y], face_radius);

    // Face buttons text
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, face_center + QPoint(face_distance, 0), Symbol::A, text_size);
    DrawSymbol(p, face_center + QPoint(0, face_distance), Symbol::B, text_size);
    DrawSymbol(p, face_center + QPoint(0, -face_distance), Symbol::X, text_size);
    DrawSymbol(p, face_center + QPoint(-face_distance, 1), Symbol::Y, text_size);

    // D-pad constants
    const QPointF dpad_center = center + QPoint(-171, 8);
    constexpr float dpad_distance = 12.8f;
    constexpr float dpad_radius = 6.4f;
    constexpr float dpad_arrow_size = 0.68f;

    // D-pad buttons
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawCircleButton(p, dpad_center + QPoint(dpad_distance, 0), button_values[DRight], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(0, dpad_distance), button_values[DDown], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(0, -dpad_distance), button_values[DUp], dpad_radius);
    DrawCircleButton(p, dpad_center + QPoint(-dpad_distance, 0), button_values[DLeft], dpad_radius);

    // D-pad arrows
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawArrow(p, dpad_center + QPoint(dpad_distance, 0), Direction::Right, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(0, dpad_distance), Direction::Down, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(0, -dpad_distance), Direction::Up, dpad_arrow_size);
    DrawArrow(p, dpad_center + QPoint(-dpad_distance, 0), Direction::Left, dpad_arrow_size);

    // ZL and ZR buttons
    p.setPen(colors.outline);
    DrawTriggerButton(p, center + QPoint(-210, -130), Direction::Left, button_values[ZL]);
    DrawTriggerButton(p, center + QPoint(210, -130), Direction::Right, button_values[ZR]);
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, center + QPoint(-210, -130), Symbol::ZL, 1.5f);
    DrawSymbol(p, center + QPoint(210, -130), Symbol::ZR, 1.5f);

    // Minus and Plus button
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawMinusButton(p, center + QPoint(-155, -67), button_values[Minus], 8);
    DrawPlusButton(p, center + QPoint(155, -67), button_values[Plus], 8);

    // Screenshot button
    p.setPen(colors.outline);
    DrawRoundButton(p, center + QPoint(-162, 39), button_values[Screenshot], 5, 5);
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawCircle(p, center + QPoint(-162, 39), 3);

    // Home Button
    p.setPen(colors.outline);
    button_color = colors.slider_button;
    DrawCircleButton(p, center + QPoint(161, 37), button_values[Home], 7);
    button_color = colors.button;
    DrawCircleButton(p, center + QPoint(161, 37), button_values[Home], 5);
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPoint(161, 37), Symbol::House, 2.75f);
}

void PlayerControlPreview::DrawProController(QPainter& p, const QPointF center) {
    DrawProTriggers(p, center, button_values[Settings::NativeButton::L],
                    button_values[Settings::NativeButton::R]);
    DrawProBody(p, center);
    {
        // Draw joysticks
        using namespace Settings::NativeAnalog;
        DrawProJoystick(p, center + QPointF(-111, -55), axis_values[LStick].value, 11,
                        button_values[Settings::NativeButton::LStick]);
        DrawProJoystick(p, center + QPointF(51, 0), axis_values[RStick].value, 11,
                        button_values[Settings::NativeButton::RStick]);
        DrawRawJoystick(p, center + QPointF(-50, 105), axis_values[LStick].raw_value,
                        axis_values[LStick].properties);
        DrawRawJoystick(p, center + QPointF(50, 105), axis_values[RStick].raw_value,
                        axis_values[RStick].properties);
    }

    using namespace Settings::NativeButton;

    // Face buttons constants
    const QPointF face_center = center + QPoint(105, -56);
    constexpr int face_distance = 31;
    constexpr int face_radius = 15;
    constexpr float text_size = 1.5f;

    // Face buttons
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawCircleButton(p, face_center + QPoint(face_distance, 0), button_values[A], face_radius);
    DrawCircleButton(p, face_center + QPoint(0, face_distance), button_values[B], face_radius);
    DrawCircleButton(p, face_center + QPoint(0, -face_distance), button_values[X], face_radius);
    DrawCircleButton(p, face_center + QPoint(-face_distance, 0), button_values[Y], face_radius);

    // Face buttons text
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, face_center + QPoint(face_distance, 0), Symbol::A, text_size);
    DrawSymbol(p, face_center + QPoint(0, face_distance), Symbol::B, text_size);
    DrawSymbol(p, face_center + QPoint(0, -face_distance), Symbol::X, text_size);
    DrawSymbol(p, face_center + QPoint(-face_distance, 1), Symbol::Y, text_size);

    // D-pad buttons
    const QPointF dpad_postion = center + QPoint(-61, 0);
    DrawArrowButton(p, dpad_postion, Direction::Up, button_values[DUp]);
    DrawArrowButton(p, dpad_postion, Direction::Left, button_values[DLeft]);
    DrawArrowButton(p, dpad_postion, Direction::Right, button_values[DRight]);
    DrawArrowButton(p, dpad_postion, Direction::Down, button_values[DDown]);
    DrawArrowButtonOutline(p, dpad_postion);

    // ZL and ZR buttons
    p.setPen(colors.outline);
    DrawTriggerButton(p, center + QPoint(-210, -130), Direction::Left, button_values[ZL]);
    DrawTriggerButton(p, center + QPoint(210, -130), Direction::Right, button_values[ZR]);
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, center + QPoint(-210, -130), Symbol::ZL, 1.5f);
    DrawSymbol(p, center + QPoint(210, -130), Symbol::ZR, 1.5f);

    // Minus and Plus buttons
    p.setPen(colors.outline);
    DrawCircleButton(p, center + QPoint(-50, -86), button_values[Minus], 9);
    DrawCircleButton(p, center + QPoint(50, -86), button_values[Plus], 9);

    // Minus and Plus symbols
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawRectangle(p, center + QPoint(-50, -86), 9, 1.5f);
    DrawRectangle(p, center + QPoint(50, -86), 9, 1.5f);
    DrawRectangle(p, center + QPoint(50, -86), 1.5f, 9);

    // Screenshot button
    p.setPen(colors.outline);
    DrawRoundButton(p, center + QPoint(-29, -56), button_values[Screenshot], 7, 7);
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawCircle(p, center + QPoint(-29, -56), 4.5f);

    // Home Button
    p.setPen(colors.outline);
    button_color = colors.slider_button;
    DrawCircleButton(p, center + QPoint(29, -56), button_values[Home], 10.0f);
    button_color = colors.button;
    DrawCircleButton(p, center + QPoint(29, -56), button_values[Home], 7.1f);
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPoint(29, -56), Symbol::House, 3.9f);
}

void PlayerControlPreview::DrawGCController(QPainter& p, const QPointF center) {
    DrawGCTriggers(p, center, button_values[Settings::NativeButton::ZL],
                   button_values[Settings::NativeButton::ZR]);
    DrawGCButtonZ(p, center, button_values[Settings::NativeButton::R]);
    DrawGCBody(p, center);
    {
        // Draw joysticks
        using namespace Settings::NativeAnalog;
        DrawGCJoystick(p, center + QPointF(-111, -44) + (axis_values[LStick].value * 10), false);
        button_color = colors.button2;
        DrawCircleButton(p, center + QPointF(61, 37) + (axis_values[RStick].value * 9.5f), false,
                         15);
        p.setPen(colors.transparent);
        p.setBrush(colors.font);
        DrawSymbol(p, center + QPointF(61, 37) + (axis_values[RStick].value * 9.5f), Symbol::C,
                   1.0f);
        DrawRawJoystick(p, center + QPointF(-198, -125), axis_values[LStick].raw_value,
                        axis_values[LStick].properties);
        DrawRawJoystick(p, center + QPointF(198, -125), axis_values[RStick].raw_value,
                        axis_values[RStick].properties);
    }

    using namespace Settings::NativeButton;

    // Face buttons constants
    constexpr float text_size = 1.1f;

    // Face buttons
    p.setPen(colors.outline);
    button_color = colors.button;
    DrawCircleButton(p, center + QPoint(111, -44), button_values[A], 21);
    DrawCircleButton(p, center + QPoint(70, -23), button_values[B], 13);
    DrawGCButtonX(p, center, button_values[Settings::NativeButton::X]);
    DrawGCButtonY(p, center, button_values[Settings::NativeButton::Y]);

    // Face buttons text
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, center + QPoint(111, -44), Symbol::A, 1.5f);
    DrawSymbol(p, center + QPoint(70, -23), Symbol::B, text_size);
    DrawSymbol(p, center + QPoint(151, -53), Symbol::X, text_size);
    DrawSymbol(p, center + QPoint(100, -83), Symbol::Y, text_size);

    // D-pad buttons
    const QPointF dpad_postion = center + QPoint(-61, 37);
    const float dpad_size = 0.8f;
    DrawArrowButton(p, dpad_postion, Direction::Up, button_values[DUp], dpad_size);
    DrawArrowButton(p, dpad_postion, Direction::Left, button_values[DLeft], dpad_size);
    DrawArrowButton(p, dpad_postion, Direction::Right, button_values[DRight], dpad_size);
    DrawArrowButton(p, dpad_postion, Direction::Down, button_values[DDown], dpad_size);
    DrawArrowButtonOutline(p, dpad_postion, dpad_size);

    // Minus and Plus buttons
    p.setPen(colors.outline);
    DrawCircleButton(p, center + QPoint(0, -44), button_values[Plus], 8);
}

constexpr std::array<float, 13 * 2> symbol_a = {
    -1.085f, -5.2f,   1.085f, -5.2f,   5.085f, 5.0f,    2.785f,  5.0f,  1.785f,
    2.65f,   -1.785f, 2.65f,  -2.785f, 5.0f,   -5.085f, 5.0f,    -1.4f, 1.0f,
    0.0f,    -2.8f,   1.4f,   1.0f,    -1.4f,  1.0f,    -5.085f, 5.0f,
};
constexpr std::array<float, 134 * 2> symbol_b = {
    -4.0f, 0.0f,  -4.0f, 0.0f,  -4.0f, -0.1f, -3.8f, -5.1f, 1.8f,  -5.0f, 2.3f,  -4.9f, 2.6f,
    -4.8f, 2.8f,  -4.7f, 2.9f,  -4.6f, 3.1f,  -4.5f, 3.2f,  -4.4f, 3.4f,  -4.3f, 3.4f,  -4.2f,
    3.5f,  -4.1f, 3.7f,  -4.0f, 3.7f,  -3.9f, 3.8f,  -3.8f, 3.8f,  -3.7f, 3.9f,  -3.6f, 3.9f,
    -3.5f, 4.0f,  -3.4f, 4.0f,  -3.3f, 4.1f,  -3.1f, 4.1f,  -3.0f, 4.0f,  -2.0f, 4.0f,  -1.9f,
    3.9f,  -1.7f, 3.9f,  -1.6f, 3.8f,  -1.5f, 3.8f,  -1.4f, 3.7f,  -1.3f, 3.7f,  -1.2f, 3.6f,
    -1.1f, 3.6f,  -1.0f, 3.5f,  -0.9f, 3.3f,  -0.8f, 3.3f,  -0.7f, 3.2f,  -0.6f, 3.0f,  -0.5f,
    2.9f,  -0.4f, 2.7f,  -0.3f, 2.9f,  -0.2f, 3.2f,  -0.1f, 3.3f,  0.0f,  3.5f,  0.1f,  3.6f,
    0.2f,  3.8f,  0.3f,  3.9f,  0.4f,  4.0f,  0.6f,  4.1f,  0.7f,  4.3f,  0.8f,  4.3f,  0.9f,
    4.4f,  1.0f,  4.4f,  1.1f,  4.5f,  1.3f,  4.5f,  1.4f,  4.6f,  1.6f,  4.6f,  1.7f,  4.5f,
    2.8f,  4.5f,  2.9f,  4.4f,  3.1f,  4.4f,  3.2f,  4.3f,  3.4f,  4.3f,  3.5f,  4.2f,  3.6f,
    4.2f,  3.7f,  4.1f,  3.8f,  4.1f,  3.9f,  4.0f,  4.0f,  3.9f,  4.2f,  3.8f,  4.3f,  3.6f,
    4.4f,  3.6f,  4.5f,  3.4f,  4.6f,  3.3f,  4.7f,  3.1f,  4.8f,  2.8f,  4.9f,  2.6f,  5.0f,
    2.1f,  5.1f,  -4.0f, 5.0f,  -4.0f, 4.9f,

    -4.0f, 0.0f,  1.1f,  3.4f,  1.1f,  3.4f,  1.5f,  3.3f,  1.8f,  3.2f,  2.0f,  3.1f,  2.1f,
    3.0f,  2.3f,  2.9f,  2.3f,  2.8f,  2.4f,  2.7f,  2.4f,  2.6f,  2.5f,  2.3f,  2.5f,  2.2f,
    2.4f,  1.7f,  2.4f,  1.6f,  2.3f,  1.4f,  2.3f,  1.3f,  2.2f,  1.2f,  2.2f,  1.1f,  2.1f,
    1.0f,  1.9f,  0.9f,  1.6f,  0.8f,  1.4f,  0.7f,  -1.9f, 0.6f,  -1.9f, 0.7f,  -1.8f, 3.4f,
    1.1f,  3.4f,  -4.0f, 0.0f,

    0.3f,  -1.1f, 0.3f,  -1.1f, 1.3f,  -1.2f, 1.5f,  -1.3f, 1.8f,  -1.4f, 1.8f,  -1.5f, 1.9f,
    -1.6f, 2.0f,  -1.8f, 2.0f,  -1.9f, 2.1f,  -2.0f, 2.1f,  -2.1f, 2.0f,  -2.7f, 2.0f,  -2.8f,
    1.9f,  -2.9f, 1.9f,  -3.0f, 1.8f,  -3.1f, 1.6f,  -3.2f, 1.6f,  -3.3f, 1.3f,  -3.4f, -1.9f,
    -3.3f, -1.9f, -3.2f, -1.8f, -1.0f, 0.2f,  -1.1f, 0.3f,  -1.1f, -4.0f, 0.0f,
};

constexpr std::array<float, 9 * 2> symbol_y = {
    -4.79f, -4.9f, -2.44f, -4.9f, 0.0f,  -0.9f,  2.44f, -4.9f,  4.79f,
    -4.9f,  1.05f, 1.0f,   1.05f, 5.31f, -1.05f, 5.31f, -1.05f, 1.0f,

};

constexpr std::array<float, 12 * 2> symbol_x = {
    -4.4f, -5.0f, -2.0f, -5.0f, 0.0f, -1.7f, 2.0f,  -5.0f, 4.4f,  -5.0f, 1.2f,  0.0f,
    4.4f,  5.0f,  2.0f,  5.0f,  0.0f, 1.7f,  -2.0f, 5.0f,  -4.4f, 5.0f,  -1.2f, 0.0f,

};

constexpr std::array<float, 7 * 2> symbol_l = {
    2.4f, -3.23f, 2.4f, 2.1f, 5.43f, 2.1f, 5.43f, 3.22f, 0.98f, 3.22f, 0.98f, -3.23f, 2.4f, -3.23f,
};

constexpr std::array<float, 98 * 2> symbol_r = {
    1.0f, 0.0f,  1.0f, -0.1f, 1.1f, -3.3f, 4.3f, -3.2f, 5.1f, -3.1f, 5.4f, -3.0f, 5.6f, -2.9f,
    5.7f, -2.8f, 5.9f, -2.7f, 5.9f, -2.6f, 6.0f, -2.5f, 6.1f, -2.3f, 6.2f, -2.2f, 6.2f, -2.1f,
    6.3f, -2.0f, 6.3f, -1.9f, 6.2f, -0.8f, 6.2f, -0.7f, 6.1f, -0.6f, 6.1f, -0.5f, 6.0f, -0.4f,
    6.0f, -0.3f, 5.9f, -0.2f, 5.7f, -0.1f, 5.7f, 0.0f,  5.6f, 0.1f,  5.4f, 0.2f,  5.1f, 0.3f,
    4.7f, 0.4f,  4.7f, 0.5f,  4.9f, 0.6f,  5.0f, 0.7f,  5.2f, 0.8f,  5.2f, 0.9f,  5.3f, 1.0f,
    5.5f, 1.1f,  5.5f, 1.2f,  5.6f, 1.3f,  5.7f, 1.5f,  5.8f, 1.6f,  5.9f, 1.8f,  6.0f, 1.9f,
    6.1f, 2.1f,  6.2f, 2.2f,  6.2f, 2.3f,  6.3f, 2.4f,  6.4f, 2.6f,  6.5f, 2.7f,  6.6f, 2.9f,
    6.7f, 3.0f,  6.7f, 3.1f,  6.8f, 3.2f,  6.8f, 3.3f,  5.3f, 3.2f,  5.2f, 3.1f,  5.2f, 3.0f,
    5.1f, 2.9f,  5.0f, 2.7f,  4.9f, 2.6f,  4.8f, 2.4f,  4.7f, 2.3f,  4.6f, 2.1f,  4.5f, 2.0f,
    4.4f, 1.8f,  4.3f, 1.7f,  4.1f, 1.4f,  4.0f, 1.3f,  3.9f, 1.1f,  3.8f, 1.0f,  3.6f, 0.9f,
    3.6f, 0.8f,  3.5f, 0.7f,  3.3f, 0.6f,  2.9f, 0.5f,  2.3f, 0.6f,  2.3f, 0.7f,  2.2f, 3.3f,
    1.0f, 3.2f,  1.0f, 3.1f,  1.0f, 0.0f,

    4.2f, -0.5f, 4.4f, -0.6f, 4.7f, -0.7f, 4.8f, -0.8f, 4.9f, -1.0f, 5.0f, -1.1f, 5.0f, -1.2f,
    4.9f, -1.7f, 4.9f, -1.8f, 4.8f, -1.9f, 4.8f, -2.0f, 4.6f, -2.1f, 4.3f, -2.2f, 2.3f, -2.1f,
    2.3f, -2.0f, 2.4f, -0.5f, 4.2f, -0.5f, 1.0f, 0.0f,
};

constexpr std::array<float, 18 * 2> symbol_zl = {
    -2.6f, -2.13f, -5.6f, -2.13f, -5.6f, -3.23f, -0.8f, -3.23f, -0.8f, -2.13f, -4.4f, 2.12f,
    -0.7f, 2.12f,  -0.7f, 3.22f,  -6.0f, 3.22f,  -6.0f, 2.12f,  2.4f,  -3.23f, 2.4f,  2.1f,
    5.43f, 2.1f,   5.43f, 3.22f,  0.98f, 3.22f,  0.98f, -3.23f, 2.4f,  -3.23f, -6.0f, 2.12f,
};

constexpr std::array<float, 57 * 2> symbol_sl = {
    -3.0f,  -3.65f, -2.76f, -4.26f, -2.33f, -4.76f, -1.76f, -5.09f, -1.13f, -5.26f, -0.94f,
    -4.77f, -0.87f, -4.11f, -1.46f, -3.88f, -1.91f, -3.41f, -2.05f, -2.78f, -1.98f, -2.13f,
    -1.59f, -1.61f, -0.96f, -1.53f, -0.56f, -2.04f, -0.38f, -2.67f, -0.22f, -3.31f, 0.0f,
    -3.93f, 0.34f,  -4.49f, 0.86f,  -4.89f, 1.49f,  -5.05f, 2.14f,  -4.95f, 2.69f,  -4.6f,
    3.07f,  -4.07f, 3.25f,  -3.44f, 3.31f,  -2.78f, 3.25f,  -2.12f, 3.07f,  -1.49f, 2.7f,
    -0.95f, 2.16f,  -0.58f, 1.52f,  -0.43f, 1.41f,  -0.99f, 1.38f,  -1.65f, 1.97f,  -1.91f,
    2.25f,  -2.49f, 2.25f,  -3.15f, 1.99f,  -3.74f, 1.38f,  -3.78f, 1.06f,  -3.22f, 0.88f,
    -2.58f, 0.71f,  -1.94f, 0.49f,  -1.32f, 0.13f,  -0.77f, -0.4f,  -0.4f,  -1.04f, -0.25f,
    -1.69f, -0.32f, -2.28f, -0.61f, -2.73f, -1.09f, -2.98f, -1.69f, -3.09f, -2.34f,

    3.23f,  2.4f,   -2.1f,  2.4f,   -2.1f,  5.43f,  -3.22f, 5.43f,  -3.22f, 0.98f,  3.23f,
    0.98f,  3.23f,  2.4f,   -3.09f, -2.34f,
};
constexpr std::array<float, 109 * 2> symbol_zr = {
    -2.6f, -2.13f, -5.6f, -2.13f, -5.6f, -3.23f, -0.8f, -3.23f, -0.8f, -2.13f, -4.4f, 2.12f, -0.7f,
    2.12f, -0.7f,  3.22f, -6.0f,  3.22f, -6.0f,  2.12f,

    1.0f,  0.0f,   1.0f,  -0.1f,  1.1f,  -3.3f,  4.3f,  -3.2f,  5.1f,  -3.1f,  5.4f,  -3.0f, 5.6f,
    -2.9f, 5.7f,   -2.8f, 5.9f,   -2.7f, 5.9f,   -2.6f, 6.0f,   -2.5f, 6.1f,   -2.3f, 6.2f,  -2.2f,
    6.2f,  -2.1f,  6.3f,  -2.0f,  6.3f,  -1.9f,  6.2f,  -0.8f,  6.2f,  -0.7f,  6.1f,  -0.6f, 6.1f,
    -0.5f, 6.0f,   -0.4f, 6.0f,   -0.3f, 5.9f,   -0.2f, 5.7f,   -0.1f, 5.7f,   0.0f,  5.6f,  0.1f,
    5.4f,  0.2f,   5.1f,  0.3f,   4.7f,  0.4f,   4.7f,  0.5f,   4.9f,  0.6f,   5.0f,  0.7f,  5.2f,
    0.8f,  5.2f,   0.9f,  5.3f,   1.0f,  5.5f,   1.1f,  5.5f,   1.2f,  5.6f,   1.3f,  5.7f,  1.5f,
    5.8f,  1.6f,   5.9f,  1.8f,   6.0f,  1.9f,   6.1f,  2.1f,   6.2f,  2.2f,   6.2f,  2.3f,  6.3f,
    2.4f,  6.4f,   2.6f,  6.5f,   2.7f,  6.6f,   2.9f,  6.7f,   3.0f,  6.7f,   3.1f,  6.8f,  3.2f,
    6.8f,  3.3f,   5.3f,  3.2f,   5.2f,  3.1f,   5.2f,  3.0f,   5.1f,  2.9f,   5.0f,  2.7f,  4.9f,
    2.6f,  4.8f,   2.4f,  4.7f,   2.3f,  4.6f,   2.1f,  4.5f,   2.0f,  4.4f,   1.8f,  4.3f,  1.7f,
    4.1f,  1.4f,   4.0f,  1.3f,   3.9f,  1.1f,   3.8f,  1.0f,   3.6f,  0.9f,   3.6f,  0.8f,  3.5f,
    0.7f,  3.3f,   0.6f,  2.9f,   0.5f,  2.3f,   0.6f,  2.3f,   0.7f,  2.2f,   3.3f,  1.0f,  3.2f,
    1.0f,  3.1f,   1.0f,  0.0f,

    4.2f,  -0.5f,  4.4f,  -0.6f,  4.7f,  -0.7f,  4.8f,  -0.8f,  4.9f,  -1.0f,  5.0f,  -1.1f, 5.0f,
    -1.2f, 4.9f,   -1.7f, 4.9f,   -1.8f, 4.8f,   -1.9f, 4.8f,   -2.0f, 4.6f,   -2.1f, 4.3f,  -2.2f,
    2.3f,  -2.1f,  2.3f,  -2.0f,  2.4f,  -0.5f,  4.2f,  -0.5f,  1.0f,  0.0f,   -6.0f, 2.12f,
};

constexpr std::array<float, 148 * 2> symbol_sr = {
    -3.0f,  -3.65f, -2.76f, -4.26f, -2.33f, -4.76f, -1.76f, -5.09f, -1.13f, -5.26f, -0.94f, -4.77f,
    -0.87f, -4.11f, -1.46f, -3.88f, -1.91f, -3.41f, -2.05f, -2.78f, -1.98f, -2.13f, -1.59f, -1.61f,
    -0.96f, -1.53f, -0.56f, -2.04f, -0.38f, -2.67f, -0.22f, -3.31f, 0.0f,   -3.93f, 0.34f,  -4.49f,
    0.86f,  -4.89f, 1.49f,  -5.05f, 2.14f,  -4.95f, 2.69f,  -4.6f,  3.07f,  -4.07f, 3.25f,  -3.44f,
    3.31f,  -2.78f, 3.25f,  -2.12f, 3.07f,  -1.49f, 2.7f,   -0.95f, 2.16f,  -0.58f, 1.52f,  -0.43f,
    1.41f,  -0.99f, 1.38f,  -1.65f, 1.97f,  -1.91f, 2.25f,  -2.49f, 2.25f,  -3.15f, 1.99f,  -3.74f,
    1.38f,  -3.78f, 1.06f,  -3.22f, 0.88f,  -2.58f, 0.71f,  -1.94f, 0.49f,  -1.32f, 0.13f,  -0.77f,
    -0.4f,  -0.4f,  -1.04f, -0.25f, -1.69f, -0.32f, -2.28f, -0.61f, -2.73f, -1.09f, -2.98f, -1.69f,
    -3.09f, -2.34f,

    -1.0f,  0.0f,   0.1f,   1.0f,   3.3f,   1.1f,   3.2f,   4.3f,   3.1f,   5.1f,   3.0f,   5.4f,
    2.9f,   5.6f,   2.8f,   5.7f,   2.7f,   5.9f,   2.6f,   5.9f,   2.5f,   6.0f,   2.3f,   6.1f,
    2.2f,   6.2f,   2.1f,   6.2f,   2.0f,   6.3f,   1.9f,   6.3f,   0.8f,   6.2f,   0.7f,   6.2f,
    0.6f,   6.1f,   0.5f,   6.1f,   0.4f,   6.0f,   0.3f,   6.0f,   0.2f,   5.9f,   0.1f,   5.7f,
    0.0f,   5.7f,   -0.1f,  5.6f,   -0.2f,  5.4f,   -0.3f,  5.1f,   -0.4f,  4.7f,   -0.5f,  4.7f,
    -0.6f,  4.9f,   -0.7f,  5.0f,   -0.8f,  5.2f,   -0.9f,  5.2f,   -1.0f,  5.3f,   -1.1f,  5.5f,
    -1.2f,  5.5f,   -1.3f,  5.6f,   -1.5f,  5.7f,   -1.6f,  5.8f,   -1.8f,  5.9f,   -1.9f,  6.0f,
    -2.1f,  6.1f,   -2.2f,  6.2f,   -2.3f,  6.2f,   -2.4f,  6.3f,   -2.6f,  6.4f,   -2.7f,  6.5f,
    -2.9f,  6.6f,   -3.0f,  6.7f,   -3.1f,  6.7f,   -3.2f,  6.8f,   -3.3f,  6.8f,   -3.2f,  5.3f,
    -3.1f,  5.2f,   -3.0f,  5.2f,   -2.9f,  5.1f,   -2.7f,  5.0f,   -2.6f,  4.9f,   -2.4f,  4.8f,
    -2.3f,  4.7f,   -2.1f,  4.6f,   -2.0f,  4.5f,   -1.8f,  4.4f,   -1.7f,  4.3f,   -1.4f,  4.1f,
    -1.3f,  4.0f,   -1.1f,  3.9f,   -1.0f,  3.8f,   -0.9f,  3.6f,   -0.8f,  3.6f,   -0.7f,  3.5f,
    -0.6f,  3.3f,   -0.5f,  2.9f,   -0.6f,  2.3f,   -0.7f,  2.3f,   -3.3f,  2.2f,   -3.2f,  1.0f,
    -3.1f,  1.0f,   0.0f,   1.0f,

    0.5f,   4.2f,   0.6f,   4.4f,   0.7f,   4.7f,   0.8f,   4.8f,   1.0f,   4.9f,   1.1f,   5.0f,
    1.2f,   5.0f,   1.7f,   4.9f,   1.8f,   4.9f,   1.9f,   4.8f,   2.0f,   4.8f,   2.1f,   4.6f,
    2.2f,   4.3f,   2.1f,   2.3f,   2.0f,   2.3f,   0.5f,   2.4f,   0.5f,   4.2f,   -0.0f,  1.0f,
    -3.09f, -2.34f,

};

constexpr std::array<float, 30 * 2> symbol_c = {
    2.86f,  7.57f,  0.99f,  7.94f,  -0.91f, 7.87f,  -2.73f, 7.31f,  -4.23f, 6.14f,  -5.2f,  4.51f,
    -5.65f, 2.66f,  -5.68f, 0.75f,  -5.31f, -1.12f, -4.43f, -2.81f, -3.01f, -4.08f, -1.24f, -4.78f,
    0.66f,  -4.94f, 2.54f,  -4.67f, 4.33f,  -4.0f,  4.63f,  -2.27f, 3.37f,  -2.7f,  1.6f,   -3.4f,
    -0.3f,  -3.5f,  -2.09f, -2.87f, -3.34f, -1.45f, -3.91f, 0.37f,  -3.95f, 2.27f,  -3.49f, 4.12f,
    -2.37f, 5.64f,  -0.65f, 6.44f,  1.25f,  6.47f,  3.06f,  5.89f,  4.63f,  4.92f,  4.63f,  6.83f,
};

constexpr std::array<float, 12 * 2> house = {
    -1.3f, 0.0f,  -0.93f, 0.0f, -0.93f, 1.15f, 0.93f,  1.15f, 0.93f, 0.0f, 1.3f,  0.0f,
    0.0f,  -1.2f, -1.3f,  0.0f, -0.43f, 0.0f,  -0.43f, .73f,  0.43f, .73f, 0.43f, 0.0f,
};

constexpr std::array<float, 11 * 2> up_arrow_button = {
    9.1f,   -9.1f, 9.1f,   -30.0f, 8.1f,   -30.1f, 7.7f,   -30.1f, -8.6f, -30.0f, -9.0f,
    -29.8f, -9.3f, -29.5f, -9.5f,  -29.1f, -9.1f,  -28.7f, -9.1f,  -9.1f, 0.0f,   0.6f,
};

constexpr std::array<float, 3 * 2> up_arrow_symbol = {
    0.0f, -3.0f, -3.0f, 2.0f, 3.0f, 2.0f,
};

constexpr std::array<float, 64 * 2> trigger_button = {
    5.5f,   -12.6f, 5.8f,   -12.6f, 6.7f,   -12.5f, 8.1f,   -12.3f, 8.6f,   -12.2f, 9.2f,   -12.0f,
    9.5f,   -11.9f, 9.9f,   -11.8f, 10.6f,  -11.5f, 11.0f,  -11.3f, 11.2f,  -11.2f, 11.4f,  -11.1f,
    11.8f,  -10.9f, 12.0f,  -10.8f, 12.2f,  -10.7f, 12.4f,  -10.5f, 12.6f,  -10.4f, 12.8f,  -10.3f,
    13.6f,  -9.7f,  13.8f,  -9.6f,  13.9f,  -9.4f,  14.1f,  -9.3f,  14.8f,  -8.6f,  15.0f,  -8.5f,
    15.1f,  -8.3f,  15.6f,  -7.8f,  15.7f,  -7.6f,  16.1f,  -7.0f,  16.3f,  -6.8f,  16.4f,  -6.6f,
    16.5f,  -6.4f,  16.8f,  -6.0f,  16.9f,  -5.8f,  17.0f,  -5.6f,  17.1f,  -5.4f,  17.2f,  -5.2f,
    17.3f,  -5.0f,  17.4f,  -4.8f,  17.5f,  -4.6f,  17.6f,  -4.4f,  17.7f,  -4.1f,  17.8f,  -3.9f,
    17.9f,  -3.5f,  18.0f,  -3.3f,  18.1f,  -3.0f,  18.2f,  -2.6f,  18.2f,  -2.3f,  18.3f,  -2.1f,
    18.3f,  -1.9f,  18.4f,  -1.4f,  18.5f,  -1.2f,  18.6f,  -0.3f,  18.6f,  0.0f,   18.3f,  13.9f,
    -17.0f, 13.8f,  -17.0f, 13.6f,  -16.4f, -11.4f, -16.3f, -11.6f, -16.1f, -11.8f, -15.7f, -12.0f,
    -15.5f, -12.1f, -15.1f, -12.3f, -14.6f, -12.4f, -13.4f, -12.5f,
};

constexpr std::array<float, 36 * 2> pro_left_trigger = {
    -65.2f,  -132.6f, -68.2f,  -134.1f, -71.3f,  -135.5f, -74.4f,  -136.7f, -77.6f,
    -137.6f, -80.9f,  -138.1f, -84.3f,  -138.3f, -87.6f,  -138.3f, -91.0f,  -138.1f,
    -94.3f,  -137.8f, -97.6f,  -137.3f, -100.9f, -136.7f, -107.5f, -135.3f, -110.7f,
    -134.5f, -120.4f, -131.8f, -123.6f, -130.8f, -126.8f, -129.7f, -129.9f, -128.5f,
    -132.9f, -127.1f, -135.9f, -125.6f, -138.8f, -123.9f, -141.6f, -122.0f, -144.1f,
    -119.8f, -146.3f, -117.3f, -148.4f, -114.7f, -150.4f, -112.0f, -152.3f, -109.2f,
    -155.3f, -104.0f, -152.0f, -104.3f, -148.7f, -104.5f, -145.3f, -104.8f, -35.5f,
    -117.2f, -38.5f,  -118.7f, -41.4f,  -120.3f, -44.4f,  -121.8f, -50.4f,  -124.9f,
};

constexpr std::array<float, 14 * 2> pro_body_top = {
    0.0f,   -115.4f, -4.4f,  -116.1f, -69.7f, -131.3f, -66.4f, -131.9f, -63.1f, -132.3f,
    -56.4f, -133.0f, -53.1f, -133.3f, -49.8f, -133.5f, -43.1f, -133.8f, -39.8f, -134.0f,
    -36.5f, -134.1f, -16.4f, -134.4f, -13.1f, -134.4f, 0.0f,   -134.1f,
};

constexpr std::array<float, 145 * 2> pro_left_handle = {
    -178.7f, -47.5f, -179.0f, -46.1f, -179.3f, -44.6f, -182.0f, -29.8f, -182.3f, -28.4f,
    -182.6f, -26.9f, -182.8f, -25.4f, -183.1f, -23.9f, -183.3f, -22.4f, -183.6f, -21.0f,
    -183.8f, -19.5f, -184.1f, -18.0f, -184.3f, -16.5f, -184.6f, -15.1f, -184.8f, -13.6f,
    -185.1f, -12.1f, -185.3f, -10.6f, -185.6f, -9.1f,  -185.8f, -7.7f,  -186.1f, -6.2f,
    -186.3f, -4.7f,  -186.6f, -3.2f,  -186.8f, -1.7f,  -187.1f, -0.3f,  -187.3f, 1.2f,
    -187.6f, 2.7f,   -187.8f, 4.2f,   -188.3f, 7.1f,   -188.5f, 8.6f,   -188.8f, 10.1f,
    -189.0f, 11.6f,  -189.3f, 13.1f,  -189.5f, 14.5f,  -190.0f, 17.5f,  -190.2f, 19.0f,
    -190.5f, 20.5f,  -190.7f, 21.9f,  -191.2f, 24.9f,  -191.4f, 26.4f,  -191.7f, 27.9f,
    -191.9f, 29.3f,  -192.4f, 32.3f,  -192.6f, 33.8f,  -193.1f, 36.8f,  -193.3f, 38.2f,
    -193.8f, 41.2f,  -194.0f, 42.7f,  -194.7f, 47.1f,  -194.9f, 48.6f,  -199.0f, 82.9f,
    -199.1f, 84.4f,  -199.1f, 85.9f,  -199.2f, 87.4f,  -199.2f, 88.9f,  -199.1f, 94.9f,
    -198.9f, 96.4f,  -198.8f, 97.8f,  -198.5f, 99.3f,  -198.3f, 100.8f, -198.0f, 102.3f,
    -197.7f, 103.7f, -197.4f, 105.2f, -197.0f, 106.7f, -196.6f, 108.1f, -195.7f, 111.0f,
    -195.2f, 112.4f, -194.1f, 115.2f, -193.5f, 116.5f, -192.8f, 117.9f, -192.1f, 119.2f,
    -190.6f, 121.8f, -189.8f, 123.1f, -188.9f, 124.3f, -187.0f, 126.6f, -186.0f, 127.7f,
    -183.9f, 129.8f, -182.7f, 130.8f, -180.3f, 132.6f, -179.1f, 133.4f, -177.8f, 134.1f,
    -176.4f, 134.8f, -175.1f, 135.5f, -173.7f, 136.0f, -169.4f, 137.3f, -167.9f, 137.7f,
    -166.5f, 138.0f, -165.0f, 138.3f, -163.5f, 138.4f, -162.0f, 138.4f, -160.5f, 138.3f,
    -159.0f, 138.0f, -157.6f, 137.7f, -156.1f, 137.3f, -154.7f, 136.9f, -153.2f, 136.5f,
    -151.8f, 136.0f, -150.4f, 135.4f, -149.1f, 134.8f, -147.7f, 134.1f, -146.5f, 133.3f,
    -145.2f, 132.5f, -144.0f, 131.6f, -142.8f, 130.6f, -141.7f, 129.6f, -139.6f, 127.5f,
    -138.6f, 126.4f, -137.7f, 125.2f, -135.1f, 121.5f, -134.3f, 120.3f, -133.5f, 119.0f,
    -131.9f, 116.5f, -131.1f, 115.2f, -128.8f, 111.3f, -128.0f, 110.1f, -127.2f, 108.8f,
    -126.5f, 107.5f, -125.7f, 106.2f, -125.0f, 104.9f, -124.2f, 103.6f, -123.5f, 102.3f,
    -122.0f, 99.6f,  -121.3f, 98.3f,  -115.8f, 87.7f,  -115.1f, 86.4f,  -114.4f, 85.0f,
    -113.7f, 83.7f,  -112.3f, 81.0f,  -111.6f, 79.7f,  -110.1f, 77.1f,  -109.4f, 75.8f,
    -108.0f, 73.1f,  -107.2f, 71.8f,  -106.4f, 70.6f,  -105.7f, 69.3f,  -104.8f, 68.0f,
    -104.0f, 66.8f,  -103.1f, 65.6f,  -101.1f, 63.3f,  -100.0f, 62.3f,  -98.8f,  61.4f,
    -97.6f,  60.6f,  -97.9f,  59.5f,  -98.8f,  58.3f,  -101.5f, 54.6f,  -102.4f, 53.4f,
};

constexpr std::array<float, 245 * 2> pro_body = {
    -0.7f,   -129.1f, -54.3f,  -129.1f, -55.0f,  -129.1f, -57.8f,  -129.0f, -58.5f,  -129.0f,
    -60.7f,  -128.9f, -61.4f,  -128.9f, -62.8f,  -128.8f, -63.5f,  -128.8f, -65.7f,  -128.7f,
    -66.4f,  -128.7f, -67.8f,  -128.6f, -68.5f,  -128.6f, -69.2f,  -128.5f, -70.0f,  -128.5f,
    -70.7f,  -128.4f, -71.4f,  -128.4f, -72.1f,  -128.3f, -72.8f,  -128.3f, -73.5f,  -128.2f,
    -74.2f,  -128.2f, -74.9f,  -128.1f, -75.7f,  -128.1f, -76.4f,  -128.0f, -77.1f,  -128.0f,
    -77.8f,  -127.9f, -78.5f,  -127.9f, -79.2f,  -127.8f, -80.6f,  -127.7f, -81.4f,  -127.6f,
    -82.1f,  -127.5f, -82.8f,  -127.5f, -83.5f,  -127.4f, -84.9f,  -127.3f, -85.6f,  -127.2f,
    -87.0f,  -127.1f, -87.7f,  -127.0f, -88.5f,  -126.9f, -89.2f,  -126.8f, -89.9f,  -126.8f,
    -90.6f,  -126.7f, -94.1f,  -126.3f, -94.8f,  -126.2f, -113.2f, -123.3f, -113.9f, -123.2f,
    -114.6f, -123.0f, -115.3f, -122.9f, -116.7f, -122.6f, -117.4f, -122.5f, -118.1f, -122.3f,
    -118.8f, -122.2f, -119.5f, -122.0f, -120.9f, -121.7f, -121.6f, -121.5f, -122.3f, -121.4f,
    -122.9f, -121.2f, -123.6f, -121.0f, -126.4f, -120.3f, -127.1f, -120.1f, -127.8f, -119.8f,
    -128.4f, -119.6f, -129.1f, -119.4f, -131.2f, -118.7f, -132.5f, -118.3f, -133.2f, -118.0f,
    -133.8f, -117.7f, -134.5f, -117.4f, -135.1f, -117.2f, -135.8f, -116.9f, -136.4f, -116.5f,
    -137.0f, -116.2f, -137.7f, -115.8f, -138.3f, -115.4f, -138.9f, -115.1f, -139.5f, -114.7f,
    -160.0f, -100.5f, -160.5f, -100.0f, -162.5f, -97.9f,  -162.9f, -97.4f,  -163.4f, -96.8f,
    -163.8f, -96.2f,  -165.3f, -93.8f,  -165.7f, -93.2f,  -166.0f, -92.6f,  -166.4f, -91.9f,
    -166.7f, -91.3f,  -167.3f, -90.0f,  -167.6f, -89.4f,  -167.8f, -88.7f,  -168.1f, -88.0f,
    -168.4f, -87.4f,  -168.6f, -86.7f,  -168.9f, -86.0f,  -169.1f, -85.4f,  -169.3f, -84.7f,
    -169.6f, -84.0f,  -169.8f, -83.3f,  -170.2f, -82.0f,  -170.4f, -81.3f,  -172.8f, -72.3f,
    -173.0f, -71.6f,  -173.5f, -69.5f,  -173.7f, -68.8f,  -173.9f, -68.2f,  -174.0f, -67.5f,
    -174.2f, -66.8f,  -174.5f, -65.4f,  -174.7f, -64.7f,  -174.8f, -64.0f,  -175.0f, -63.3f,
    -175.3f, -61.9f,  -175.5f, -61.2f,  -175.8f, -59.8f,  -176.0f, -59.1f,  -176.1f, -58.4f,
    -176.3f, -57.7f,  -176.6f, -56.3f,  -176.8f, -55.6f,  -176.9f, -54.9f,  -177.1f, -54.2f,
    -177.3f, -53.6f,  -177.4f, -52.9f,  -177.6f, -52.2f,  -177.9f, -50.8f,  -178.1f, -50.1f,
    -178.2f, -49.4f,  -178.2f, -48.7f,  -177.8f, -48.1f,  -177.1f, -46.9f,  -176.7f, -46.3f,
    -176.4f, -45.6f,  -176.0f, -45.0f,  -175.3f, -43.8f,  -174.9f, -43.2f,  -174.2f, -42.0f,
    -173.4f, -40.7f,  -173.1f, -40.1f,  -172.7f, -39.5f,  -172.0f, -38.3f,  -171.6f, -37.7f,
    -170.5f, -35.9f,  -170.1f, -35.3f,  -169.7f, -34.6f,  -169.3f, -34.0f,  -168.6f, -32.8f,
    -168.2f, -32.2f,  -166.3f, -29.2f,  -165.9f, -28.6f,  -163.2f, -24.4f,  -162.8f, -23.8f,
    -141.8f, 6.8f,    -141.4f, 7.4f,    -139.4f, 10.3f,   -139.0f, 10.9f,   -138.5f, 11.5f,
    -138.1f, 12.1f,   -137.3f, 13.2f,   -136.9f, 13.8f,   -136.0f, 15.0f,   -135.6f, 15.6f,
    -135.2f, 16.1f,   -134.8f, 16.7f,   -133.9f, 17.9f,   -133.5f, 18.4f,   -133.1f, 19.0f,
    -131.8f, 20.7f,   -131.4f, 21.3f,   -130.1f, 23.0f,   -129.7f, 23.6f,   -128.4f, 25.3f,
    -128.0f, 25.9f,   -126.7f, 27.6f,   -126.3f, 28.2f,   -125.4f, 29.3f,   -125.0f, 29.9f,
    -124.1f, 31.0f,   -123.7f, 31.6f,   -122.8f, 32.7f,   -122.4f, 33.3f,   -121.5f, 34.4f,
    -121.1f, 35.0f,   -120.6f, 35.6f,   -120.2f, 36.1f,   -119.7f, 36.7f,   -119.3f, 37.2f,
    -118.9f, 37.8f,   -118.4f, 38.4f,   -118.0f, 38.9f,   -117.5f, 39.5f,   -117.1f, 40.0f,
    -116.6f, 40.6f,   -116.2f, 41.1f,   -115.7f, 41.7f,   -115.2f, 42.2f,   -114.8f, 42.8f,
    -114.3f, 43.3f,   -113.9f, 43.9f,   -113.4f, 44.4f,   -112.4f, 45.5f,   -112.0f, 46.0f,
    -111.5f, 46.5f,   -110.5f, 47.6f,   -110.0f, 48.1f,   -109.6f, 48.6f,   -109.1f, 49.2f,
    -108.6f, 49.7f,   -107.7f, 50.8f,   -107.2f, 51.3f,   -105.7f, 52.9f,   -105.3f, 53.4f,
    -104.8f, 53.9f,   -104.3f, 54.5f,   -103.8f, 55.0f,   -100.7f, 58.0f,   -100.2f, 58.4f,
    -99.7f,  58.9f,   -99.1f,  59.3f,   -97.2f,  60.3f,   -96.5f,  60.1f,   -95.9f,  59.7f,
    -95.3f,  59.4f,   -94.6f,  59.1f,   -93.9f,  58.9f,   -92.6f,  58.5f,   -91.9f,  58.4f,
    -91.2f,  58.2f,   -90.5f,  58.1f,   -89.7f,  58.0f,   -89.0f,  57.9f,   -86.2f,  57.6f,
    -85.5f,  57.5f,   -84.1f,  57.4f,   -83.4f,  57.3f,   -82.6f,  57.3f,   -81.9f,  57.2f,
    -81.2f,  57.2f,   -80.5f,  57.1f,   -79.8f,  57.1f,   -78.4f,  57.0f,   -77.7f,  57.0f,
    -75.5f,  56.9f,   -74.8f,  56.9f,   -71.9f,  56.8f,   -71.2f,  56.8f,   0.0f,    56.8f,
};

constexpr std::array<float, 199 * 2> gc_body = {
    0.0f,     -138.03f, -4.91f,   -138.01f, -8.02f,   -137.94f, -11.14f,  -137.82f, -14.25f,
    -137.67f, -17.37f,  -137.48f, -20.48f,  -137.25f, -23.59f,  -137.0f,  -26.69f,  -136.72f,
    -29.8f,   -136.41f, -32.9f,   -136.07f, -35.99f,  -135.71f, -39.09f,  -135.32f, -42.18f,
    -134.91f, -45.27f,  -134.48f, -48.35f,  -134.03f, -51.43f,  -133.55f, -54.51f,  -133.05f,
    -57.59f,  -132.52f, -60.66f,  -131.98f, -63.72f,  -131.41f, -66.78f,  -130.81f, -69.84f,
    -130.2f,  -72.89f,  -129.56f, -75.94f,  -128.89f, -78.98f,  -128.21f, -82.02f,  -127.49f,
    -85.05f,  -126.75f, -88.07f,  -125.99f, -91.09f,  -125.19f, -94.1f,   -124.37f, -97.1f,
    -123.52f, -100.09f, -122.64f, -103.07f, -121.72f, -106.04f, -120.77f, -109.0f,  -119.79f,
    -111.95f, -118.77f, -114.88f, -117.71f, -117.8f,  -116.61f, -120.7f,  -115.46f, -123.58f,
    -114.27f, -126.44f, -113.03f, -129.27f, -111.73f, -132.08f, -110.38f, -134.86f, -108.96f,
    -137.6f,  -107.47f, -140.3f,  -105.91f, -142.95f, -104.27f, -145.55f, -102.54f, -148.07f,
    -100.71f, -150.51f, -98.77f,  -152.86f, -96.71f,  -155.09f, -94.54f,  -157.23f, -92.27f,
    -159.26f, -89.9f,   -161.2f,  -87.46f,  -163.04f, -84.94f,  -164.78f, -82.35f,  -166.42f,
    -79.7f,   -167.97f, -77.0f,   -169.43f, -74.24f,  -170.8f,  -71.44f,  -172.09f, -68.6f,
    -173.29f, -65.72f,  -174.41f, -62.81f,  -175.45f, -59.87f,  -176.42f, -56.91f,  -177.31f,
    -53.92f,  -178.14f, -50.91f,  -178.9f,  -47.89f,  -179.6f,  -44.85f,  -180.24f, -41.8f,
    -180.82f, -38.73f,  -181.34f, -35.66f,  -181.8f,  -32.57f,  -182.21f, -29.48f,  -182.57f,
    -26.38f,  -182.88f, -23.28f,  -183.15f, -20.17f,  -183.36f, -17.06f,  -183.54f, -13.95f,
    -183.71f, -10.84f,  -184.0f,  -7.73f,   -184.23f, -4.62f,   -184.44f, -1.51f,   -184.62f,
    1.6f,     -184.79f, 4.72f,    -184.95f, 7.83f,    -185.11f, 10.95f,   -185.25f, 14.06f,
    -185.38f, 17.18f,   -185.51f, 20.29f,   -185.63f, 23.41f,   -185.74f, 26.53f,   -185.85f,
    29.64f,   -185.95f, 32.76f,   -186.04f, 35.88f,   -186.12f, 39.0f,    -186.19f, 42.11f,
    -186.26f, 45.23f,   -186.32f, 48.35f,   -186.37f, 51.47f,   -186.41f, 54.59f,   -186.44f,
    57.7f,    -186.46f, 60.82f,   -186.46f, 63.94f,   -186.44f, 70.18f,   -186.41f, 73.3f,
    -186.36f, 76.42f,   -186.3f,  79.53f,   -186.22f, 82.65f,   -186.12f, 85.77f,   -185.99f,
    88.88f,   -185.84f, 92.0f,    -185.66f, 95.11f,   -185.44f, 98.22f,   -185.17f, 101.33f,
    -184.85f, 104.43f,  -184.46f, 107.53f,  -183.97f, 110.61f,  -183.37f, 113.67f,  -182.65f,
    116.7f,   -181.77f, 119.69f,  -180.71f, 122.62f,  -179.43f, 125.47f,  -177.89f, 128.18f,
    -176.05f, 130.69f,  -173.88f, 132.92f,  -171.36f, 134.75f,  -168.55f, 136.1f,   -165.55f,
    136.93f,  -162.45f, 137.29f,  -156.23f, 137.03f,  -153.18f, 136.41f,  -150.46f, 134.9f,
    -148.14f, 132.83f,  -146.14f, 130.43f,  -144.39f, 127.85f,  -142.83f, 125.16f,  -141.41f,
    122.38f,  -140.11f, 119.54f,  -138.9f,  116.67f,  -137.77f, 113.76f,  -136.7f,  110.84f,
    -135.68f, 107.89f,  -134.71f, 104.93f,  -133.77f, 101.95f,  -132.86f, 98.97f,   -131.97f,
    95.98f,   -131.09f, 92.99f,   -130.23f, 89.99f,   -129.36f, 86.99f,   -128.49f, 84.0f,
    -127.63f, 81.0f,    -126.76f, 78.01f,   -125.9f,  75.01f,   -124.17f, 69.02f,   -123.31f,
    66.02f,   -121.59f, 60.03f,   -120.72f, 57.03f,   -119.86f, 54.03f,   -118.13f, 48.04f,
    -117.27f, 45.04f,   -115.55f, 39.05f,   -114.68f, 36.05f,   -113.82f, 33.05f,   -112.96f,
    30.06f,   -110.4f,  28.29f,   -107.81f, 26.55f,   -105.23f, 24.8f,    -97.48f,  19.55f,
    -94.9f,   17.81f,   -92.32f,  16.06f,   -87.15f,  12.56f,   -84.57f,  10.81f,   -81.99f,
    9.07f,    -79.4f,   7.32f,    -76.82f,  5.57f,    -69.07f,  0.33f,    -66.49f,  -1.42f,
    -58.74f,  -6.66f,   -56.16f,  -8.41f,   -48.4f,   -13.64f,  -45.72f,  -15.22f,  -42.93f,
    -16.62f,  -40.07f,  -17.86f,  -37.15f,  -18.96f,  -34.19f,  -19.94f,  -31.19f,  -20.79f,
    -28.16f,  -21.55f,  -25.12f,  -22.21f,  -22.05f,  -22.79f,  -18.97f,  -23.28f,  -15.88f,
    -23.7f,   -12.78f,  -24.05f,  -9.68f,   -24.33f,  -6.57f,   -24.55f,  -3.45f,   -24.69f,
    0.0f,     -24.69f,
};

constexpr std::array<float, 99 * 2> gc_left_body = {
    -74.59f,  -97.22f,  -70.17f,  -94.19f,  -65.95f,  -90.89f,  -62.06f,  -87.21f,  -58.58f,
    -83.14f,  -55.58f,  -78.7f,   -53.08f,  -73.97f,  -51.05f,  -69.01f,  -49.46f,  -63.89f,
    -48.24f,  -58.67f,  -47.36f,  -53.39f,  -46.59f,  -48.09f,  -45.7f,   -42.8f,   -44.69f,
    -37.54f,  -43.54f,  -32.31f,  -42.25f,  -27.11f,  -40.8f,   -21.95f,  -39.19f,  -16.84f,
    -37.38f,  -11.8f,   -35.34f,  -6.84f,   -33.04f,  -2.0f,    -30.39f,  2.65f,    -27.26f,
    7.0f,     -23.84f,  11.11f,   -21.19f,  15.76f,   -19.18f,  20.73f,   -17.73f,  25.88f,
    -16.82f,  31.16f,   -16.46f,  36.5f,    -16.7f,   41.85f,   -17.63f,  47.13f,   -19.31f,
    52.21f,   -21.8f,   56.95f,   -24.91f,  61.3f,    -28.41f,  65.36f,   -32.28f,  69.06f,
    -36.51f,  72.35f,   -41.09f,  75.13f,   -45.97f,  77.32f,   -51.1f,   78.86f,   -56.39f,
    79.7f,    -61.74f,  79.84f,   -67.07f,  79.3f,    -72.3f,   78.15f,   -77.39f,  76.48f,
    -82.29f,  74.31f,   -86.76f,  71.37f,   -90.7f,   67.75f,   -94.16f,  63.66f,   -97.27f,
    59.3f,    -100.21f, 54.81f,   -103.09f, 50.3f,    -106.03f, 45.82f,   -109.11f, 41.44f,
    -112.37f, 37.19f,   -115.85f, 33.11f,   -119.54f, 29.22f,   -123.45f, 25.56f,   -127.55f,
    22.11f,   -131.77f, 18.81f,   -136.04f, 15.57f,   -140.34f, 12.37f,   -144.62f, 9.15f,
    -148.86f, 5.88f,    -153.03f, 2.51f,    -157.05f, -1.03f,   -160.83f, -4.83f,   -164.12f,
    -9.05f,   -166.71f, -13.73f,  -168.91f, -18.62f,  -170.77f, -23.64f,  -172.3f,  -28.78f,
    -173.49f, -34.0f,   -174.3f,  -39.3f,   -174.72f, -44.64f,  -174.72f, -49.99f,  -174.28f,
    -55.33f,  -173.37f, -60.61f,  -172.0f,  -65.79f,  -170.17f, -70.82f,  -167.79f, -75.62f,
    -164.84f, -80.09f,  -161.43f, -84.22f,  -157.67f, -88.03f,  -153.63f, -91.55f,  -149.37f,
    -94.81f,  -144.94f, -97.82f,  -140.37f, -100.61f, -135.65f, -103.16f, -130.73f, -105.26f,
    -125.62f, -106.86f, -120.37f, -107.95f, -115.05f, -108.56f, -109.7f,  -108.69f, -104.35f,
    -108.36f, -99.05f,  -107.6f,  -93.82f,  -106.41f, -88.72f,  -104.79f, -83.78f,  -102.7f,
};

constexpr std::array<float, 47 * 2> left_gc_trigger = {
    -99.69f,  -125.04f, -101.81f, -126.51f, -104.02f, -127.85f, -106.3f,  -129.06f, -108.65f,
    -130.12f, -111.08f, -130.99f, -113.58f, -131.62f, -116.14f, -131.97f, -121.26f, -131.55f,
    -123.74f, -130.84f, -126.17f, -129.95f, -128.53f, -128.9f,  -130.82f, -127.71f, -133.03f,
    -126.38f, -135.15f, -124.92f, -137.18f, -123.32f, -139.11f, -121.6f,  -140.91f, -119.75f,
    -142.55f, -117.77f, -144.0f,  -115.63f, -145.18f, -113.34f, -146.17f, -110.95f, -147.05f,
    -108.53f, -147.87f, -106.08f, -148.64f, -103.61f, -149.37f, -101.14f, -149.16f, -100.12f,
    -147.12f, -101.71f, -144.99f, -103.16f, -142.8f,  -104.53f, -140.57f, -105.83f, -138.31f,
    -107.08f, -136.02f, -108.27f, -133.71f, -109.42f, -131.38f, -110.53f, -129.04f, -111.61f,
    -126.68f, -112.66f, -124.31f, -113.68f, -121.92f, -114.67f, -119.53f, -115.64f, -117.13f,
    -116.58f, -114.72f, -117.51f, -112.3f,  -118.41f, -109.87f, -119.29f, -107.44f, -120.16f,
    -105.0f,  -121.0f,  -100.11f, -122.65f,
};

constexpr std::array<float, 50 * 2> gc_button_x = {
    142.1f,  -50.67f, 142.44f, -48.65f, 142.69f, -46.62f, 142.8f,  -44.57f, 143.0f,  -42.54f,
    143.56f, -40.57f, 144.42f, -38.71f, 145.59f, -37.04f, 147.08f, -35.64f, 148.86f, -34.65f,
    150.84f, -34.11f, 152.88f, -34.03f, 154.89f, -34.38f, 156.79f, -35.14f, 158.49f, -36.28f,
    159.92f, -37.74f, 161.04f, -39.45f, 161.85f, -41.33f, 162.4f,  -43.3f,  162.72f, -45.32f,
    162.85f, -47.37f, 162.82f, -49.41f, 162.67f, -51.46f, 162.39f, -53.48f, 162.0f,  -55.5f,
    161.51f, -57.48f, 160.9f,  -59.44f, 160.17f, -61.35f, 159.25f, -63.18f, 158.19f, -64.93f,
    157.01f, -66.61f, 155.72f, -68.2f,  154.31f, -69.68f, 152.78f, -71.04f, 151.09f, -72.2f,
    149.23f, -73.04f, 147.22f, -73.36f, 145.19f, -73.11f, 143.26f, -72.42f, 141.51f, -71.37f,
    140.0f,  -69.99f, 138.82f, -68.32f, 138.13f, -66.4f,  138.09f, -64.36f, 138.39f, -62.34f,
    139.05f, -60.41f, 139.91f, -58.55f, 140.62f, -56.63f, 141.21f, -54.67f, 141.67f, -52.67f,
};

constexpr std::array<float, 50 * 2> gc_button_y = {
    104.02f, -75.23f, 106.01f, -75.74f, 108.01f, -76.15f, 110.04f, -76.42f, 112.05f, -76.78f,
    113.97f, -77.49f, 115.76f, -78.49f, 117.33f, -79.79f, 118.6f,  -81.39f, 119.46f, -83.25f,
    119.84f, -85.26f, 119.76f, -87.3f,  119.24f, -89.28f, 118.33f, -91.11f, 117.06f, -92.71f,
    115.49f, -94.02f, 113.7f,  -95.01f, 111.77f, -95.67f, 109.76f, -96.05f, 107.71f, -96.21f,
    105.67f, -96.18f, 103.63f, -95.99f, 101.61f, -95.67f, 99.61f,  -95.24f, 97.63f,  -94.69f,
    95.69f,  -94.04f, 93.79f,  -93.28f, 91.94f,  -92.4f,  90.19f,  -91.34f, 88.53f,  -90.14f,
    86.95f,  -88.84f, 85.47f,  -87.42f, 84.1f,   -85.9f,  82.87f,  -84.26f, 81.85f,  -82.49f,
    81.15f,  -80.57f, 81.0f,   -78.54f, 81.41f,  -76.54f, 82.24f,  -74.67f, 83.43f,  -73.01f,
    84.92f,  -71.61f, 86.68f,  -70.57f, 88.65f,  -70.03f, 90.69f,  -70.15f, 92.68f,  -70.61f,
    94.56f,  -71.42f, 96.34f,  -72.43f, 98.2f,   -73.29f, 100.11f, -74.03f, 102.06f, -74.65f,
};

constexpr std::array<float, 47 * 2> gc_button_z = {
    95.74f,  -126.41f, 98.34f,  -126.38f, 100.94f, -126.24f, 103.53f, -126.01f, 106.11f, -125.7f,
    108.69f, -125.32f, 111.25f, -124.87f, 113.8f,  -124.34f, 116.33f, -123.73f, 118.84f, -123.05f,
    121.33f, -122.3f,  123.79f, -121.47f, 126.23f, -120.56f, 128.64f, -119.58f, 131.02f, -118.51f,
    133.35f, -117.37f, 135.65f, -116.14f, 137.9f,  -114.84f, 140.1f,  -113.46f, 142.25f, -111.99f,
    144.35f, -110.45f, 146.38f, -108.82f, 148.35f, -107.13f, 150.25f, -105.35f, 151.89f, -103.38f,
    151.43f, -100.86f, 149.15f, -100.15f, 146.73f, -101.06f, 144.36f, -102.12f, 141.98f, -103.18f,
    139.6f,  -104.23f, 137.22f, -105.29f, 134.85f, -106.35f, 132.47f, -107.41f, 127.72f, -109.53f,
    125.34f, -110.58f, 122.96f, -111.64f, 120.59f, -112.7f,  118.21f, -113.76f, 113.46f, -115.88f,
    111.08f, -116.93f, 108.7f,  -117.99f, 106.33f, -119.05f, 103.95f, -120.11f, 99.2f,   -122.23f,
    96.82f,  -123.29f, 94.44f,  -124.34f,
};

constexpr std::array<float, 84 * 2> left_joycon_body = {
    -145.0f, -78.9f, -145.0f, -77.9f, -145.0f, 85.6f,  -145.0f, 85.6f,  -168.3f, 85.5f,
    -169.3f, 85.4f,  -171.3f, 85.1f,  -172.3f, 84.9f,  -173.4f, 84.7f,  -174.3f, 84.5f,
    -175.3f, 84.2f,  -176.3f, 83.8f,  -177.3f, 83.5f,  -178.2f, 83.1f,  -179.2f, 82.7f,
    -180.1f, 82.2f,  -181.0f, 81.8f,  -181.9f, 81.3f,  -182.8f, 80.7f,  -183.7f, 80.2f,
    -184.5f, 79.6f,  -186.2f, 78.3f,  -186.9f, 77.7f,  -187.7f, 77.0f,  -189.2f, 75.6f,
    -189.9f, 74.8f,  -190.6f, 74.1f,  -191.3f, 73.3f,  -191.9f, 72.5f,  -192.5f, 71.6f,
    -193.1f, 70.8f,  -193.7f, 69.9f,  -194.3f, 69.1f,  -194.8f, 68.2f,  -196.2f, 65.5f,
    -196.6f, 64.5f,  -197.0f, 63.6f,  -197.4f, 62.6f,  -198.1f, 60.7f,  -198.4f, 59.7f,
    -198.6f, 58.7f,  -199.2f, 55.6f,  -199.3f, 54.6f,  -199.5f, 51.5f,  -199.5f, 50.5f,
    -199.5f, -49.4f, -199.4f, -50.5f, -199.3f, -51.5f, -199.1f, -52.5f, -198.2f, -56.5f,
    -197.9f, -57.5f, -197.2f, -59.4f, -196.8f, -60.4f, -196.4f, -61.3f, -195.9f, -62.2f,
    -194.3f, -64.9f, -193.7f, -65.7f, -193.1f, -66.6f, -192.5f, -67.4f, -191.8f, -68.2f,
    -191.2f, -68.9f, -190.4f, -69.7f, -188.2f, -71.8f, -187.4f, -72.5f, -186.6f, -73.1f,
    -185.8f, -73.8f, -185.0f, -74.4f, -184.1f, -74.9f, -183.2f, -75.5f, -182.4f, -76.0f,
    -181.5f, -76.5f, -179.6f, -77.5f, -178.7f, -77.9f, -177.8f, -78.4f, -176.8f, -78.8f,
    -175.9f, -79.1f, -174.9f, -79.5f, -173.9f, -79.8f, -170.9f, -80.6f, -169.9f, -80.8f,
    -167.9f, -81.1f, -166.9f, -81.2f, -165.8f, -81.2f, -145.0f, -80.9f,
};

constexpr std::array<float, 84 * 2> left_joycon_trigger = {
    -166.8f, -83.3f, -167.9f, -83.2f, -168.9f, -83.1f, -170.0f, -83.0f, -171.0f, -82.8f,
    -172.1f, -82.6f, -173.1f, -82.4f, -174.2f, -82.1f, -175.2f, -81.9f, -176.2f, -81.5f,
    -177.2f, -81.2f, -178.2f, -80.8f, -180.1f, -80.0f, -181.1f, -79.5f, -182.0f, -79.0f,
    -183.0f, -78.5f, -183.9f, -78.0f, -184.8f, -77.4f, -185.7f, -76.9f, -186.6f, -76.3f,
    -187.4f, -75.6f, -188.3f, -75.0f, -189.1f, -74.3f, -192.2f, -71.5f, -192.9f, -70.7f,
    -193.7f, -69.9f, -194.3f, -69.1f, -195.0f, -68.3f, -195.6f, -67.4f, -196.8f, -65.7f,
    -197.3f, -64.7f, -197.8f, -63.8f, -198.2f, -62.8f, -198.9f, -60.8f, -198.6f, -59.8f,
    -197.6f, -59.7f, -196.6f, -60.0f, -195.6f, -60.5f, -194.7f, -60.9f, -193.7f, -61.4f,
    -192.8f, -61.9f, -191.8f, -62.4f, -190.9f, -62.8f, -189.9f, -63.3f, -189.0f, -63.8f,
    -187.1f, -64.8f, -186.2f, -65.2f, -185.2f, -65.7f, -184.3f, -66.2f, -183.3f, -66.7f,
    -182.4f, -67.1f, -181.4f, -67.6f, -180.5f, -68.1f, -179.5f, -68.6f, -178.6f, -69.0f,
    -177.6f, -69.5f, -176.7f, -70.0f, -175.7f, -70.5f, -174.8f, -70.9f, -173.8f, -71.4f,
    -172.9f, -71.9f, -171.9f, -72.4f, -171.0f, -72.8f, -170.0f, -73.3f, -169.1f, -73.8f,
    -168.1f, -74.3f, -167.2f, -74.7f, -166.2f, -75.2f, -165.3f, -75.7f, -164.3f, -76.2f,
    -163.4f, -76.6f, -162.4f, -77.1f, -161.5f, -77.6f, -160.5f, -78.1f, -159.6f, -78.5f,
    -158.7f, -79.0f, -157.7f, -79.5f, -156.8f, -80.0f, -155.8f, -80.4f, -154.9f, -80.9f,
    -154.2f, -81.6f, -154.3f, -82.6f, -155.2f, -83.3f, -156.2f, -83.3f,
};

constexpr std::array<float, 70 * 2> handheld_body = {
    -137.3f, -81.9f, -137.6f, -81.8f, -137.8f, -81.6f, -138.0f, -81.3f, -138.1f, -81.1f,
    -138.1f, -80.8f, -138.2f, -78.7f, -138.2f, -78.4f, -138.3f, -78.1f, -138.7f, -77.3f,
    -138.9f, -77.0f, -139.0f, -76.8f, -139.2f, -76.5f, -139.5f, -76.3f, -139.7f, -76.1f,
    -139.9f, -76.0f, -140.2f, -75.8f, -140.5f, -75.7f, -140.7f, -75.6f, -141.0f, -75.5f,
    -141.9f, -75.3f, -142.2f, -75.3f, -142.5f, -75.2f, -143.0f, -74.9f, -143.2f, -74.7f,
    -143.3f, -74.4f, -143.0f, -74.1f, -143.0f, 85.3f,  -143.0f, 85.6f,  -142.7f, 85.8f,
    -142.4f, 85.9f,  -142.2f, 85.9f,  143.0f,  85.6f,  143.1f,  85.4f,  143.3f,  85.1f,
    143.0f,  84.8f,  143.0f,  -74.9f, 142.8f,  -75.1f, 142.5f,  -75.2f, 141.9f,  -75.3f,
    141.6f,  -75.3f, 141.3f,  -75.4f, 141.1f,  -75.4f, 140.8f,  -75.5f, 140.5f,  -75.7f,
    140.2f,  -75.8f, 140.0f,  -76.0f, 139.7f,  -76.1f, 139.5f,  -76.3f, 139.1f,  -76.8f,
    138.9f,  -77.0f, 138.6f,  -77.5f, 138.4f,  -77.8f, 138.3f,  -78.1f, 138.3f,  -78.3f,
    138.2f,  -78.6f, 138.2f,  -78.9f, 138.1f,  -79.2f, 138.1f,  -79.5f, 138.0f,  -81.3f,
    137.8f,  -81.6f, 137.6f,  -81.8f, 137.3f,  -81.9f, 137.1f,  -81.9f, 120.0f,  -70.0f,
    -120.0f, -70.0f, -120.0f, 70.0f,  120.0f,  70.0f,  120.0f,  -70.0f, 137.1f,  -81.9f,
};

constexpr std::array<float, 40 * 2> handheld_bezel = {
    -131.4f, -75.9f, -132.2f, -75.7f, -132.9f, -75.3f, -134.2f, -74.3f, -134.7f, -73.6f,
    -135.1f, -72.8f, -135.4f, -72.0f, -135.5f, -71.2f, -135.5f, -70.4f, -135.2f, 76.7f,
    -134.8f, 77.5f,  -134.3f, 78.1f,  -133.7f, 78.8f,  -133.1f, 79.2f,  -132.3f, 79.6f,
    -131.5f, 79.9f,  -130.7f, 80.0f,  -129.8f, 80.0f,  132.2f,  79.7f,  133.0f,  79.3f,
    133.7f,  78.8f,  134.3f,  78.3f,  134.8f,  77.6f,  135.1f,  76.8f,  135.5f,  75.2f,
    135.5f,  74.3f,  135.2f,  -72.7f, 134.8f,  -73.5f, 134.4f,  -74.2f, 133.8f,  -74.8f,
    133.1f,  -75.3f, 132.3f,  -75.6f, 130.7f,  -76.0f, 129.8f,  -76.0f, -112.9f, -62.2f,
    112.9f,  -62.2f, 112.9f,  62.2f,  -112.9f, 62.2f,  -112.9f, -62.2f, 129.8f,  -76.0f,
};

constexpr std::array<float, 58 * 2> handheld_buttons = {
    -82.48f,  -82.95f, -82.53f,  -82.95f, -106.69f, -82.96f, -106.73f, -82.98f, -106.78f, -83.01f,
    -106.81f, -83.05f, -106.83f, -83.1f,  -106.83f, -83.15f, -106.82f, -83.93f, -106.81f, -83.99f,
    -106.8f,  -84.04f, -106.78f, -84.08f, -106.76f, -84.13f, -106.73f, -84.18f, -106.7f,  -84.22f,
    -106.6f,  -84.34f, -106.56f, -84.37f, -106.51f, -84.4f,  -106.47f, -84.42f, -106.42f, -84.45f,
    -106.37f, -84.47f, -106.32f, -84.48f, -106.17f, -84.5f,  -98.9f,   -84.48f, -98.86f,  -84.45f,
    -98.83f,  -84.41f, -98.81f,  -84.36f, -98.8f,   -84.31f, -98.8f,   -84.26f, -98.79f,  -84.05f,
    -90.26f,  -84.1f,  -90.26f,  -84.15f, -90.25f,  -84.36f, -90.23f,  -84.41f, -90.2f,   -84.45f,
    -90.16f,  -84.48f, -90.11f,  -84.5f,  -82.79f,  -84.49f, -82.74f,  -84.48f, -82.69f,  -84.46f,
    -82.64f,  -84.45f, -82.59f,  -84.42f, -82.55f,  -84.4f,  -82.5f,   -84.37f, -82.46f,  -84.33f,
    -82.42f,  -84.3f,  -82.39f,  -84.26f, -82.3f,   -84.13f, -82.28f,  -84.08f, -82.25f,  -83.98f,
    -82.24f,  -83.93f, -82.23f,  -83.83f, -82.23f,  -83.78f, -82.24f,  -83.1f,  -82.26f,  -83.05f,
    -82.29f,  -83.01f, -82.33f,  -82.97f, -82.38f,  -82.95f,
};

constexpr std::array<float, 47 * 2> left_joycon_slider = {
    -23.7f, -118.2f, -23.7f, -117.3f, -23.7f, 96.6f,   -22.8f, 96.6f,  -21.5f, 97.2f,  -21.5f,
    98.1f,  -21.2f,  106.7f, -20.8f,  107.5f, -20.1f,  108.2f, -19.2f, 108.2f, -16.4f, 108.1f,
    -15.8f, 107.5f,  -15.8f, 106.5f,  -15.8f, 62.8f,   -16.3f, 61.9f,  -15.8f, 61.0f,  -17.3f,
    60.3f,  -19.1f,  58.9f,  -19.1f,  58.1f,  -19.1f,  57.2f,  -19.1f, 34.5f,  -17.9f, 33.9f,
    -17.2f, 33.2f,   -16.6f, 32.4f,   -16.2f, 31.6f,   -15.8f, 30.7f,  -15.8f, 29.7f,  -15.8f,
    28.8f,  -15.8f,  -46.4f, -16.3f,  -47.3f, -15.8f,  -48.1f, -17.4f, -48.8f, -19.1f, -49.4f,
    -19.1f, -50.1f,  -19.1f, -51.0f,  -19.1f, -51.9f,  -19.1f, -73.7f, -19.1f, -74.5f, -17.5f,
    -75.2f, -16.4f,  -76.7f, -16.0f,  -77.6f, -15.8f,  -78.5f, -15.8f, -79.4f, -15.8f, -80.4f,
    -15.8f, -118.2f, -15.8f, -118.2f, -18.3f, -118.2f,
};

constexpr std::array<float, 66 * 2> left_joycon_sideview = {
    -158.8f, -133.5f, -159.8f, -133.5f, -173.5f, -133.3f, -174.5f, -133.0f, -175.4f, -132.6f,
    -176.2f, -132.1f, -177.0f, -131.5f, -177.7f, -130.9f, -178.3f, -130.1f, -179.4f, -128.5f,
    -179.8f, -127.6f, -180.4f, -125.7f, -180.6f, -124.7f, -180.7f, -123.8f, -180.7f, -122.8f,
    -180.0f, 128.8f,  -179.6f, 129.7f,  -179.1f, 130.5f,  -177.9f, 132.1f,  -177.2f, 132.7f,
    -176.4f, 133.3f,  -175.6f, 133.8f,  -174.7f, 134.3f,  -173.8f, 134.6f,  -172.8f, 134.8f,
    -170.9f, 135.0f,  -169.9f, 135.0f,  -156.1f, 134.8f,  -155.2f, 134.6f,  -154.2f, 134.3f,
    -153.3f, 134.0f,  -152.4f, 133.6f,  -151.6f, 133.1f,  -150.7f, 132.6f,  -149.9f, 132.0f,
    -149.2f, 131.4f,  -148.5f, 130.7f,  -147.1f, 129.2f,  -146.5f, 128.5f,  -146.0f, 127.7f,
    -145.5f, 126.8f,  -145.0f, 126.0f,  -144.6f, 125.1f,  -144.2f, 124.1f,  -143.9f, 123.2f,
    -143.7f, 122.2f,  -143.6f, 121.3f,  -143.5f, 120.3f,  -143.5f, 119.3f,  -144.4f, -123.4f,
    -144.8f, -124.3f, -145.3f, -125.1f, -145.8f, -126.0f, -146.3f, -126.8f, -147.0f, -127.5f,
    -147.6f, -128.3f, -148.3f, -129.0f, -149.0f, -129.6f, -149.8f, -130.3f, -150.6f, -130.8f,
    -151.4f, -131.4f, -152.2f, -131.9f, -153.1f, -132.3f, -155.9f, -133.3f, -156.8f, -133.5f,
    -157.8f, -133.5f,
};

constexpr std::array<float, 40 * 2> left_joycon_body_trigger = {
    -146.1f, -124.3f, -146.0f, -122.0f, -145.8f, -119.7f, -145.7f, -117.4f, -145.4f, -112.8f,
    -145.3f, -110.5f, -145.0f, -105.9f, -144.9f, -103.6f, -144.6f, -99.1f,  -144.5f, -96.8f,
    -144.5f, -89.9f,  -144.5f, -87.6f,  -144.5f, -83.0f,  -144.5f, -80.7f,  -144.5f, -80.3f,
    -142.4f, -82.4f,  -141.4f, -84.5f,  -140.2f, -86.4f,  -138.8f, -88.3f,  -137.4f, -90.1f,
    -134.5f, -93.6f,  -133.0f, -95.3f,  -130.0f, -98.8f,  -128.5f, -100.6f, -127.1f, -102.4f,
    -125.8f, -104.3f, -124.7f, -106.3f, -123.9f, -108.4f, -125.1f, -110.2f, -127.4f, -110.3f,
    -129.7f, -110.3f, -134.2f, -110.5f, -136.4f, -111.4f, -138.1f, -112.8f, -139.4f, -114.7f,
    -140.5f, -116.8f, -141.4f, -118.9f, -143.3f, -123.1f, -144.6f, -124.9f, -146.2f, -126.0f,
};

constexpr std::array<float, 49 * 2> left_joycon_topview = {
    -184.8f, -20.8f, -185.6f, -21.1f, -186.4f, -21.5f, -187.1f, -22.1f, -187.8f, -22.6f,
    -188.4f, -23.2f, -189.6f, -24.5f, -190.2f, -25.2f, -190.7f, -25.9f, -191.1f, -26.7f,
    -191.4f, -27.5f, -191.6f, -28.4f, -191.7f, -29.2f, -191.7f, -30.1f, -191.5f, -47.7f,
    -191.2f, -48.5f, -191.0f, -49.4f, -190.7f, -50.2f, -190.3f, -51.0f, -190.0f, -51.8f,
    -189.6f, -52.6f, -189.1f, -53.4f, -188.6f, -54.1f, -187.5f, -55.4f, -186.9f, -56.1f,
    -186.2f, -56.7f, -185.5f, -57.2f, -184.0f, -58.1f, -183.3f, -58.5f, -182.5f, -58.9f,
    -181.6f, -59.2f, -180.8f, -59.5f, -179.9f, -59.7f, -179.1f, -59.9f, -178.2f, -60.0f,
    -174.7f, -60.1f, -168.5f, -60.2f, -162.4f, -60.3f, -156.2f, -60.4f, -149.2f, -60.5f,
    -143.0f, -60.6f, -136.9f, -60.7f, -130.7f, -60.8f, -123.7f, -60.9f, -117.5f, -61.0f,
    -110.5f, -61.1f, -94.4f,  -60.4f, -94.4f,  -59.5f, -94.4f,  -20.6f,
};

constexpr std::array<float, 41 * 2> left_joycon_slider_topview = {
    -95.1f, -51.5f, -95.0f, -51.5f, -91.2f, -51.6f, -91.2f, -51.7f, -91.1f, -52.4f, -91.1f, -52.6f,
    -91.0f, -54.1f, -86.3f, -54.0f, -86.0f, -53.9f, -85.9f, -53.8f, -85.6f, -53.4f, -85.5f, -53.2f,
    -85.5f, -53.1f, -85.4f, -52.9f, -85.4f, -52.8f, -85.3f, -52.4f, -85.3f, -52.3f, -85.4f, -27.2f,
    -85.4f, -27.1f, -85.5f, -27.0f, -85.5f, -26.9f, -85.6f, -26.7f, -85.6f, -26.6f, -85.7f, -26.5f,
    -85.9f, -26.4f, -86.0f, -26.3f, -86.4f, -26.0f, -86.5f, -25.9f, -86.7f, -25.8f, -87.1f, -25.7f,
    -90.4f, -25.8f, -90.7f, -25.9f, -90.8f, -26.0f, -90.9f, -26.3f, -91.0f, -26.4f, -91.0f, -26.5f,
    -91.1f, -26.7f, -91.1f, -26.9f, -91.2f, -28.9f, -95.2f, -29.1f, -95.2f, -29.2f,
};

constexpr std::array<float, 42 * 2> left_joycon_sideview_zl = {
    -148.9f, -128.2f, -148.7f, -126.6f, -148.4f, -124.9f, -148.2f, -123.3f, -147.9f, -121.7f,
    -147.7f, -120.1f, -147.4f, -118.5f, -147.2f, -116.9f, -146.9f, -115.3f, -146.4f, -112.1f,
    -146.1f, -110.5f, -145.9f, -108.9f, -145.6f, -107.3f, -144.2f, -107.3f, -142.6f, -107.5f,
    -141.0f, -107.8f, -137.8f, -108.3f, -136.2f, -108.6f, -131.4f, -109.4f, -129.8f, -109.7f,
    -125.6f, -111.4f, -124.5f, -112.7f, -123.9f, -114.1f, -123.8f, -115.8f, -123.8f, -117.4f,
    -123.9f, -120.6f, -124.5f, -122.1f, -125.8f, -123.1f, -127.4f, -123.4f, -129.0f, -123.6f,
    -130.6f, -124.0f, -132.1f, -124.4f, -133.7f, -124.8f, -135.3f, -125.3f, -136.8f, -125.9f,
    -138.3f, -126.4f, -139.9f, -126.9f, -141.4f, -127.5f, -142.9f, -128.0f, -144.5f, -128.5f,
    -146.0f, -129.0f, -147.6f, -129.4f,
};

constexpr std::array<float, 72 * 2> left_joystick_sideview = {
    -14.7f, -3.8f,  -15.2f, -5.6f,  -15.2f, -7.6f,  -15.5f, -17.6f, -17.4f, -18.3f, -19.4f, -18.2f,
    -21.3f, -17.6f, -22.8f, -16.4f, -23.4f, -14.5f, -23.4f, -12.5f, -24.1f, -8.6f,  -24.8f, -6.7f,
    -25.3f, -4.8f,  -25.7f, -2.8f,  -25.9f, -0.8f,  -26.0f, 1.2f,   -26.0f, 3.2f,   -25.8f, 5.2f,
    -25.5f, 7.2f,   -25.0f, 9.2f,   -24.4f, 11.1f,  -23.7f, 13.0f,  -23.4f, 14.9f,  -23.4f, 16.9f,
    -23.3f, 18.9f,  -22.0f, 20.5f,  -20.2f, 21.3f,  -18.3f, 21.6f,  -16.3f, 21.4f,  -15.3f, 19.9f,
    -15.3f, 17.8f,  -15.2f, 7.8f,   -13.5f, 6.4f,   -12.4f, 7.2f,   -11.4f, 8.9f,   -10.2f, 10.5f,
    -8.7f,  11.8f,  -7.1f,  13.0f,  -5.3f,  14.0f,  -3.5f,  14.7f,  -1.5f,  15.0f,  0.5f,   15.0f,
    2.5f,   14.7f,  4.4f,   14.2f,  6.3f,   13.4f,  8.0f,   12.4f,  9.6f,   11.1f,  10.9f,  9.6f,
    12.0f,  7.9f,   12.7f,  6.0f,   13.2f,  4.1f,   13.3f,  2.1f,   13.2f,  0.1f,   12.9f,  -1.9f,
    12.2f,  -3.8f,  11.3f,  -5.6f,  10.2f,  -7.2f,  8.8f,   -8.6f,  7.1f,   -9.8f,  5.4f,   -10.8f,
    3.5f,   -11.5f, 1.5f,   -11.9f, -0.5f,  -12.0f, -2.5f,  -11.8f, -4.4f,  -11.3f, -6.2f,  -10.4f,
    -8.0f,  -9.4f,  -9.6f,  -8.2f,  -10.9f, -6.7f,  -11.9f, -4.9f,  -12.8f, -3.2f,  -13.5f, -3.8f,
};

constexpr std::array<float, 63 * 2> left_joystick_L_topview = {
    -186.7f, -43.7f, -186.4f, -43.7f, -110.6f, -43.4f, -110.6f, -43.1f, -110.7f, -34.3f,
    -110.7f, -34.0f, -110.8f, -33.7f, -111.1f, -32.9f, -111.2f, -32.6f, -111.4f, -32.3f,
    -111.5f, -32.1f, -111.7f, -31.8f, -111.8f, -31.5f, -112.0f, -31.3f, -112.2f, -31.0f,
    -112.4f, -30.8f, -112.8f, -30.3f, -113.0f, -30.1f, -114.1f, -29.1f, -114.3f, -28.9f,
    -114.6f, -28.7f, -114.8f, -28.6f, -115.1f, -28.4f, -115.3f, -28.3f, -115.6f, -28.1f,
    -115.9f, -28.0f, -116.4f, -27.8f, -116.7f, -27.7f, -117.3f, -27.6f, -117.6f, -27.5f,
    -182.9f, -27.6f, -183.5f, -27.7f, -183.8f, -27.8f, -184.4f, -27.9f, -184.6f, -28.1f,
    -184.9f, -28.2f, -185.4f, -28.5f, -185.7f, -28.7f, -185.9f, -28.8f, -186.2f, -29.0f,
    -186.4f, -29.2f, -187.0f, -29.9f, -187.2f, -30.1f, -187.6f, -30.6f, -187.8f, -30.8f,
    -187.9f, -31.1f, -188.1f, -31.3f, -188.2f, -31.6f, -188.4f, -31.9f, -188.5f, -32.1f,
    -188.6f, -32.4f, -188.8f, -33.3f, -188.9f, -33.6f, -188.9f, -33.9f, -188.8f, -39.9f,
    -188.8f, -40.2f, -188.7f, -41.1f, -188.7f, -41.4f, -188.6f, -41.7f, -188.0f, -43.1f,
    -187.9f, -43.4f, -187.6f, -43.6f, -187.3f, -43.7f,
};

constexpr std::array<float, 44 * 2> left_joystick_ZL_topview = {
    -179.4f, -53.3f, -177.4f, -53.3f, -111.2f, -53.3f, -111.3f, -53.3f, -111.5f, -58.6f,
    -111.8f, -60.5f, -112.2f, -62.4f, -113.1f, -66.1f, -113.8f, -68.0f, -114.5f, -69.8f,
    -115.3f, -71.5f, -116.3f, -73.2f, -117.3f, -74.8f, -118.5f, -76.4f, -119.8f, -77.8f,
    -121.2f, -79.1f, -122.8f, -80.2f, -124.4f, -81.2f, -126.2f, -82.0f, -128.1f, -82.6f,
    -130.0f, -82.9f, -131.9f, -83.0f, -141.5f, -82.9f, -149.3f, -82.8f, -153.1f, -82.6f,
    -155.0f, -82.1f, -156.8f, -81.6f, -158.7f, -80.9f, -160.4f, -80.2f, -162.2f, -79.3f,
    -163.8f, -78.3f, -165.4f, -77.2f, -166.9f, -76.0f, -168.4f, -74.7f, -169.7f, -73.3f,
    -172.1f, -70.3f, -173.2f, -68.7f, -174.2f, -67.1f, -175.2f, -65.4f, -176.1f, -63.7f,
    -178.7f, -58.5f, -179.6f, -56.8f, -180.4f, -55.1f, -181.3f, -53.3f,
};

void PlayerControlPreview::DrawProBody(QPainter& p, const QPointF center) {
    std::array<QPointF, pro_left_handle.size() / 2> qleft_handle;
    std::array<QPointF, pro_left_handle.size() / 2> qright_handle;
    std::array<QPointF, pro_body.size()> qbody;
    constexpr int radius1 = 32;

    for (std::size_t point = 0; point < pro_left_handle.size() / 2; ++point) {
        const float left_x = pro_left_handle[point * 2 + 0];
        const float left_y = pro_left_handle[point * 2 + 1];

        qleft_handle[point] = center + QPointF(left_x, left_y);
        qright_handle[point] = center + QPointF(-left_x, left_y);
    }
    for (std::size_t point = 0; point < pro_body.size() / 2; ++point) {
        const float body_x = pro_body[point * 2 + 0];
        const float body_y = pro_body[point * 2 + 1];

        qbody[point] = center + QPointF(body_x, body_y);
        qbody[pro_body.size() - 1 - point] = center + QPointF(-body_x, body_y);
    }

    // Draw left handle body
    p.setPen(colors.outline);
    p.setBrush(colors.left);
    DrawPolygon(p, qleft_handle);

    // Draw right handle body
    p.setBrush(colors.right);
    DrawPolygon(p, qright_handle);

    // Draw body
    p.setBrush(colors.primary);
    DrawPolygon(p, qbody);

    // Draw joycon circles
    p.setBrush(colors.transparent);
    p.drawEllipse(center + QPoint(-111, -55), radius1, radius1);
    p.drawEllipse(center + QPoint(51, 0), radius1, radius1);
}

void PlayerControlPreview::DrawGCBody(QPainter& p, const QPointF center) {
    std::array<QPointF, gc_left_body.size() / 2> qleft_handle;
    std::array<QPointF, gc_left_body.size() / 2> qright_handle;
    std::array<QPointF, gc_body.size()> qbody;
    std::array<QPointF, 8> left_hex;
    std::array<QPointF, 8> right_hex;
    constexpr float angle = 2 * 3.1415f / 8;

    for (std::size_t point = 0; point < gc_left_body.size() / 2; ++point) {
        const float body_x = gc_left_body[point * 2 + 0];
        const float body_y = gc_left_body[point * 2 + 1];

        qleft_handle[point] = center + QPointF(body_x, body_y);
        qright_handle[point] = center + QPointF(-body_x, body_y);
    }
    for (std::size_t point = 0; point < gc_body.size() / 2; ++point) {
        const float body_x = gc_body[point * 2 + 0];
        const float body_y = gc_body[point * 2 + 1];

        qbody[point] = center + QPointF(body_x, body_y);
        qbody[gc_body.size() - 1 - point] = center + QPointF(-body_x, body_y);
    }
    for (std::size_t point = 0; point < 8; ++point) {
        const float point_cos = std::cos(point * angle);
        const float point_sin = std::sin(point * angle);

        left_hex[point] = center + QPointF(34 * point_cos - 111, 34 * point_sin - 44);
        right_hex[point] = center + QPointF(26 * point_cos + 61, 26 * point_sin + 37);
    }

    // Draw body
    p.setPen(colors.outline);
    p.setBrush(colors.primary);
    DrawPolygon(p, qbody);

    // Draw left handle body
    p.setBrush(colors.left);
    DrawPolygon(p, qleft_handle);

    // Draw right handle body
    p.setBrush(colors.right);
    DrawPolygon(p, qright_handle);

    DrawText(p, center + QPoint(0, -58), 4.7f, tr("START/PAUSE"));

    // Draw right joystick body
    p.setBrush(colors.button);
    DrawCircle(p, center + QPointF(61, 37), 23.5f);

    // Draw joystick details
    p.setBrush(colors.transparent);
    DrawPolygon(p, left_hex);
    DrawPolygon(p, right_hex);
}

void PlayerControlPreview::DrawHandheldBody(QPainter& p, const QPointF center) {
    const std::size_t body_outline_end = handheld_body.size() / 2 - 6;
    const std::size_t bezel_outline_end = handheld_bezel.size() / 2 - 6;
    const std::size_t bezel_inline_size = 4;
    const std::size_t bezel_inline_start = 35;
    std::array<QPointF, left_joycon_body.size() / 2> left_joycon;
    std::array<QPointF, left_joycon_body.size() / 2> right_joycon;
    std::array<QPointF, handheld_body.size() / 2> qhandheld_body;
    std::array<QPointF, body_outline_end> qhandheld_body_outline;
    std::array<QPointF, handheld_bezel.size() / 2> qhandheld_bezel;
    std::array<QPointF, bezel_inline_size> qhandheld_bezel_inline;
    std::array<QPointF, bezel_outline_end> qhandheld_bezel_outline;
    std::array<QPointF, handheld_buttons.size() / 2> qhandheld_buttons;

    for (std::size_t point = 0; point < left_joycon_body.size() / 2; ++point) {
        left_joycon[point] =
            center + QPointF(left_joycon_body[point * 2], left_joycon_body[point * 2 + 1]);
        right_joycon[point] =
            center + QPointF(-left_joycon_body[point * 2], left_joycon_body[point * 2 + 1]);
    }
    for (std::size_t point = 0; point < body_outline_end; ++point) {
        qhandheld_body_outline[point] =
            center + QPointF(handheld_body[point * 2], handheld_body[point * 2 + 1]);
    }
    for (std::size_t point = 0; point < handheld_body.size() / 2; ++point) {
        qhandheld_body[point] =
            center + QPointF(handheld_body[point * 2], handheld_body[point * 2 + 1]);
    }
    for (std::size_t point = 0; point < handheld_bezel.size() / 2; ++point) {
        qhandheld_bezel[point] =
            center + QPointF(handheld_bezel[point * 2], handheld_bezel[point * 2 + 1]);
    }
    for (std::size_t point = 0; point < bezel_outline_end; ++point) {
        qhandheld_bezel_outline[point] =
            center + QPointF(handheld_bezel[point * 2], handheld_bezel[point * 2 + 1]);
    }
    for (std::size_t point = 0; point < bezel_inline_size; ++point) {
        qhandheld_bezel_inline[point] =
            center + QPointF(handheld_bezel[(point + bezel_inline_start) * 2],
                             handheld_bezel[(point + bezel_inline_start) * 2 + 1]);
    }
    for (std::size_t point = 0; point < handheld_buttons.size() / 2; ++point) {
        qhandheld_buttons[point] =
            center + QPointF(handheld_buttons[point * 2], handheld_buttons[point * 2 + 1]);
    }

    // Draw left joycon
    p.setPen(colors.outline);
    p.setBrush(colors.left);
    DrawPolygon(p, left_joycon);

    // Draw right joycon
    p.setPen(colors.outline);
    p.setBrush(colors.right);
    DrawPolygon(p, right_joycon);

    // Draw Handheld buttons
    p.setPen(colors.outline);
    p.setBrush(colors.button);
    DrawPolygon(p, qhandheld_buttons);

    // Draw handheld body
    p.setPen(colors.transparent);
    p.setBrush(colors.primary);
    DrawPolygon(p, qhandheld_body);
    p.setPen(colors.outline);
    p.setBrush(colors.transparent);
    DrawPolygon(p, qhandheld_body_outline);

    // Draw Handheld bezel
    p.setPen(colors.transparent);
    p.setBrush(colors.button);
    DrawPolygon(p, qhandheld_bezel);
    p.setPen(colors.outline);
    p.setBrush(colors.transparent);
    DrawPolygon(p, qhandheld_bezel_outline);
    DrawPolygon(p, qhandheld_bezel_inline);
}

void PlayerControlPreview::DrawDualBody(QPainter& p, const QPointF center) {
    std::array<QPointF, left_joycon_body.size() / 2> left_joycon;
    std::array<QPointF, left_joycon_body.size() / 2> right_joycon;
    std::array<QPointF, left_joycon_slider.size() / 2> qleft_joycon_slider;
    std::array<QPointF, left_joycon_slider.size() / 2> qright_joycon_slider;
    std::array<QPointF, left_joycon_slider_topview.size() / 2> qleft_joycon_slider_topview;
    std::array<QPointF, left_joycon_slider_topview.size() / 2> qright_joycon_slider_topview;
    std::array<QPointF, left_joycon_topview.size() / 2> qleft_joycon_topview;
    std::array<QPointF, left_joycon_topview.size() / 2> qright_joycon_topview;
    constexpr float size = 1.61f;
    constexpr float size2 = 0.9f;
    constexpr float offset = 209.3f;

    for (std::size_t point = 0; point < left_joycon_body.size() / 2; ++point) {
        const float body_x = left_joycon_body[point * 2 + 0];
        const float body_y = left_joycon_body[point * 2 + 1];

        left_joycon[point] = center + QPointF(body_x * size + offset, body_y * size - 1);
        right_joycon[point] = center + QPointF(-body_x * size - offset, body_y * size - 1);
    }
    for (std::size_t point = 0; point < left_joycon_slider.size() / 2; ++point) {
        const float slider_x = left_joycon_slider[point * 2 + 0];
        const float slider_y = left_joycon_slider[point * 2 + 1];

        qleft_joycon_slider[point] = center + QPointF(slider_x, slider_y);
        qright_joycon_slider[point] = center + QPointF(-slider_x, slider_y);
    }
    for (std::size_t point = 0; point < left_joycon_topview.size() / 2; ++point) {
        const float top_view_x = left_joycon_topview[point * 2 + 0];
        const float top_view_y = left_joycon_topview[point * 2 + 1];

        qleft_joycon_topview[point] =
            center + QPointF(top_view_x * size2 - 52, top_view_y * size2 - 52);
        qright_joycon_topview[point] =
            center + QPointF(-top_view_x * size2 + 52, top_view_y * size2 - 52);
    }
    for (std::size_t point = 0; point < left_joycon_slider_topview.size() / 2; ++point) {
        const float top_view_x = left_joycon_slider_topview[point * 2 + 0];
        const float top_view_y = left_joycon_slider_topview[point * 2 + 1];

        qleft_joycon_slider_topview[point] =
            center + QPointF(top_view_x * size2 - 52, top_view_y * size2 - 52);
        qright_joycon_slider_topview[point] =
            center + QPointF(-top_view_x * size2 + 52, top_view_y * size2 - 52);
    }

    // right joycon body
    p.setPen(colors.outline);
    p.setBrush(colors.right);
    DrawPolygon(p, right_joycon);

    // Left joycon body
    p.setPen(colors.outline);
    p.setBrush(colors.left);
    DrawPolygon(p, left_joycon);

    // Slider release button top view
    p.setBrush(colors.button);
    DrawRoundRectangle(p, center + QPoint(-149, -108), 12, 11, 2);
    DrawRoundRectangle(p, center + QPoint(149, -108), 12, 11, 2);

    // Joycon slider top view
    p.setBrush(colors.slider);
    DrawPolygon(p, qleft_joycon_slider_topview);
    p.drawLine(center + QPointF(-133.8f, -99.0f), center + QPointF(-133.8f, -78.5f));
    DrawPolygon(p, qright_joycon_slider_topview);
    p.drawLine(center + QPointF(133.8f, -99.0f), center + QPointF(133.8f, -78.5f));

    // Joycon body top view
    p.setBrush(colors.left);
    DrawPolygon(p, qleft_joycon_topview);
    p.setBrush(colors.right);
    DrawPolygon(p, qright_joycon_topview);

    // Right SR and SL sideview buttons
    p.setPen(colors.outline);
    p.setBrush(colors.slider_button);
    DrawRoundRectangle(p, center + QPoint(19, 47), 7, 22, 1);
    DrawRoundRectangle(p, center + QPoint(19, -62), 7, 22, 1);

    // Left SR and SL sideview buttons
    DrawRoundRectangle(p, center + QPoint(-19, 47), 7, 22, 1);
    DrawRoundRectangle(p, center + QPoint(-19, -62), 7, 22, 1);

    // Right Sideview body
    p.setBrush(colors.slider);
    DrawPolygon(p, qright_joycon_slider);

    // Left Sideview body
    p.setBrush(colors.slider);
    DrawPolygon(p, qleft_joycon_slider);
}

void PlayerControlPreview::DrawLeftBody(QPainter& p, const QPointF center) {
    std::array<QPointF, left_joycon_body.size() / 2> left_joycon;
    std::array<QPointF, left_joycon_sideview.size() / 2> qleft_joycon_sideview;
    std::array<QPointF, left_joycon_body_trigger.size() / 2> qleft_joycon_trigger;
    std::array<QPointF, left_joycon_slider.size() / 2> qleft_joycon_slider;
    std::array<QPointF, left_joycon_slider_topview.size() / 2> qleft_joycon_slider_topview;
    std::array<QPointF, left_joycon_topview.size() / 2> qleft_joycon_topview;
    constexpr float size = 1.78f;
    constexpr float size2 = 1.1115f;
    constexpr float offset = 312.39f;
    constexpr float offset2 = 335;

    for (std::size_t point = 0; point < left_joycon_body.size() / 2; ++point) {
        left_joycon[point] = center + QPointF(left_joycon_body[point * 2] * size + offset,
                                              left_joycon_body[point * 2 + 1] * size - 1);
    }

    for (std::size_t point = 0; point < left_joycon_sideview.size() / 2; ++point) {
        qleft_joycon_sideview[point] =
            center + QPointF(left_joycon_sideview[point * 2] * size2 + offset2,
                             left_joycon_sideview[point * 2 + 1] * size2 + 2);
    }
    for (std::size_t point = 0; point < left_joycon_slider.size() / 2; ++point) {
        qleft_joycon_slider[point] = center + QPointF(left_joycon_slider[point * 2] * size2 + 81,
                                                      left_joycon_slider[point * 2 + 1] * size2);
    }
    for (std::size_t point = 0; point < left_joycon_body_trigger.size() / 2; ++point) {
        qleft_joycon_trigger[point] =
            center + QPointF(left_joycon_body_trigger[point * 2] * size2 + offset2,
                             left_joycon_body_trigger[point * 2 + 1] * size2 + 2);
    }
    for (std::size_t point = 0; point < left_joycon_topview.size() / 2; ++point) {
        qleft_joycon_topview[point] =
            center + QPointF(left_joycon_topview[point * 2], left_joycon_topview[point * 2 + 1]);
    }
    for (std::size_t point = 0; point < left_joycon_slider_topview.size() / 2; ++point) {
        qleft_joycon_slider_topview[point] =
            center + QPointF(left_joycon_slider_topview[point * 2],
                             left_joycon_slider_topview[point * 2 + 1]);
    }

    // Joycon body
    p.setPen(colors.outline);
    p.setBrush(colors.left);
    DrawPolygon(p, left_joycon);
    DrawPolygon(p, qleft_joycon_trigger);

    // Slider release button top view
    p.setBrush(colors.button);
    DrawRoundRectangle(p, center + QPoint(-107, -62), 14, 12, 2);

    // Joycon slider top view
    p.setBrush(colors.slider);
    DrawPolygon(p, qleft_joycon_slider_topview);
    p.drawLine(center + QPointF(-91.1f, -51.7f), center + QPointF(-91.1f, -26.5f));

    // Joycon body top view
    p.setBrush(colors.left);
    DrawPolygon(p, qleft_joycon_topview);

    // Slider release button
    p.setBrush(colors.button);
    DrawRoundRectangle(p, center + QPoint(175, -110), 12, 14, 2);

    // Sideview body
    p.setBrush(colors.left);
    DrawPolygon(p, qleft_joycon_sideview);
    p.setBrush(colors.slider);
    DrawPolygon(p, qleft_joycon_slider);

    const QPointF sideview_center = QPointF(155, 0) + center;

    // Sideview slider body
    p.setBrush(colors.slider);
    DrawRoundRectangle(p, sideview_center + QPointF(0, -5), 28, 253, 3);
    p.setBrush(colors.button2);
    DrawRoundRectangle(p, sideview_center + QPointF(0, 97), 22.44f, 44.66f, 3);

    // Slider decorations
    p.setPen(colors.outline);
    p.setBrush(colors.slider_arrow);
    DrawArrow(p, sideview_center + QPoint(0, 83), Direction::Down, 2.2f);
    DrawArrow(p, sideview_center + QPoint(0, 96), Direction::Down, 2.2f);
    DrawArrow(p, sideview_center + QPoint(0, 109), Direction::Down, 2.2f);
    DrawCircle(p, sideview_center + QPointF(0, 19), 4.44f);

    // LED indicators
    const float led_size = 5.0f;
    const QPointF led_position = sideview_center + QPointF(0, -36);
    int led_count = 0;
    for (const auto color : led_color) {
        p.setBrush(color);
        DrawRectangle(p, led_position + QPointF(0, 12 * led_count++), led_size, led_size);
    }
}

void PlayerControlPreview::DrawRightBody(QPainter& p, const QPointF center) {
    std::array<QPointF, left_joycon_body.size() / 2> right_joycon;
    std::array<QPointF, left_joycon_sideview.size() / 2> qright_joycon_sideview;
    std::array<QPointF, left_joycon_body_trigger.size() / 2> qright_joycon_trigger;
    std::array<QPointF, left_joycon_slider.size() / 2> qright_joycon_slider;
    std::array<QPointF, left_joycon_slider_topview.size() / 2> qright_joycon_slider_topview;
    std::array<QPointF, left_joycon_topview.size() / 2> qright_joycon_topview;
    constexpr float size = 1.78f;
    constexpr float size2 = 1.1115f;
    constexpr float offset = 312.39f;
    constexpr float offset2 = 335;

    for (std::size_t point = 0; point < left_joycon_body.size() / 2; ++point) {
        right_joycon[point] = center + QPointF(-left_joycon_body[point * 2] * size - offset,
                                               left_joycon_body[point * 2 + 1] * size - 1);
    }

    for (std::size_t point = 0; point < left_joycon_sideview.size() / 2; ++point) {
        qright_joycon_sideview[point] =
            center + QPointF(-left_joycon_sideview[point * 2] * size2 - offset2,
                             left_joycon_sideview[point * 2 + 1] * size2 + 2);
    }
    for (std::size_t point = 0; point < left_joycon_body_trigger.size() / 2; ++point) {
        qright_joycon_trigger[point] =
            center + QPointF(-left_joycon_body_trigger[point * 2] * size2 - offset2,
                             left_joycon_body_trigger[point * 2 + 1] * size2 + 2);
    }
    for (std::size_t point = 0; point < left_joycon_slider.size() / 2; ++point) {
        qright_joycon_slider[point] = center + QPointF(-left_joycon_slider[point * 2] * size2 - 81,
                                                       left_joycon_slider[point * 2 + 1] * size2);
    }
    for (std::size_t point = 0; point < left_joycon_topview.size() / 2; ++point) {
        qright_joycon_topview[point] =
            center + QPointF(-left_joycon_topview[point * 2], left_joycon_topview[point * 2 + 1]);
    }
    for (std::size_t point = 0; point < left_joycon_slider_topview.size() / 2; ++point) {
        qright_joycon_slider_topview[point] =
            center + QPointF(-left_joycon_slider_topview[point * 2],
                             left_joycon_slider_topview[point * 2 + 1]);
    }

    // Joycon body
    p.setPen(colors.outline);
    p.setBrush(colors.left);
    DrawPolygon(p, right_joycon);
    DrawPolygon(p, qright_joycon_trigger);

    // Slider release button top view
    p.setBrush(colors.button);
    DrawRoundRectangle(p, center + QPoint(107, -62), 14, 12, 2);

    // Joycon slider top view
    p.setBrush(colors.slider);
    DrawPolygon(p, qright_joycon_slider_topview);
    p.drawLine(center + QPointF(91.1f, -51.7f), center + QPointF(91.1f, -26.5f));

    // Joycon body top view
    p.setBrush(colors.left);
    DrawPolygon(p, qright_joycon_topview);

    // Slider release button
    p.setBrush(colors.button);
    DrawRoundRectangle(p, center + QPoint(-175, -110), 12, 14, 2);

    // Sideview body
    p.setBrush(colors.left);
    DrawPolygon(p, qright_joycon_sideview);
    p.setBrush(colors.slider);
    DrawPolygon(p, qright_joycon_slider);

    const QPointF sideview_center = QPointF(-155, 0) + center;

    // Sideview slider body
    p.setBrush(colors.slider);
    DrawRoundRectangle(p, sideview_center + QPointF(0, -5), 28, 253, 3);
    p.setBrush(colors.button2);
    DrawRoundRectangle(p, sideview_center + QPointF(0, 97), 22.44f, 44.66f, 3);

    // Slider decorations
    p.setPen(colors.outline);
    p.setBrush(colors.slider_arrow);
    DrawArrow(p, sideview_center + QPoint(0, 83), Direction::Down, 2.2f);
    DrawArrow(p, sideview_center + QPoint(0, 96), Direction::Down, 2.2f);
    DrawArrow(p, sideview_center + QPoint(0, 109), Direction::Down, 2.2f);
    DrawCircle(p, sideview_center + QPointF(0, 19), 4.44f);

    // LED indicators
    const float led_size = 5.0f;
    const QPointF led_position = sideview_center + QPointF(0, -36);
    int led_count = 0;
    for (const auto color : led_color) {
        p.setBrush(color);
        DrawRectangle(p, led_position + QPointF(0, 12 * led_count++), led_size, led_size);
    }
}

void PlayerControlPreview::DrawProTriggers(QPainter& p, const QPointF center, bool left_pressed,
                                           bool right_pressed) {
    std::array<QPointF, pro_left_trigger.size() / 2> qleft_trigger;
    std::array<QPointF, pro_left_trigger.size() / 2> qright_trigger;
    std::array<QPointF, pro_body_top.size()> qbody_top;

    for (std::size_t point = 0; point < pro_left_trigger.size() / 2; ++point) {
        const float trigger_x = pro_left_trigger[point * 2 + 0];
        const float trigger_y = pro_left_trigger[point * 2 + 1];

        qleft_trigger[point] = center + QPointF(trigger_x, trigger_y + (left_pressed ? 2 : 0));
        qright_trigger[point] = center + QPointF(-trigger_x, trigger_y + (right_pressed ? 2 : 0));
    }

    for (std::size_t point = 0; point < pro_body_top.size() / 2; ++point) {
        const float top_x = pro_body_top[point * 2 + 0];
        const float top_y = pro_body_top[point * 2 + 1];

        qbody_top[pro_body_top.size() - 1 - point] = center + QPointF(-top_x, top_y);
        qbody_top[point] = center + QPointF(top_x, top_y);
    }

    // Pro body detail
    p.setPen(colors.outline);
    p.setBrush(colors.primary);
    DrawPolygon(p, qbody_top);

    // Left trigger
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);

    // Right trigger
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);
}

void PlayerControlPreview::DrawGCTriggers(QPainter& p, const QPointF center, bool left_pressed,
                                          bool right_pressed) {
    std::array<QPointF, left_gc_trigger.size() / 2> qleft_trigger;
    std::array<QPointF, left_gc_trigger.size() / 2> qright_trigger;

    for (std::size_t point = 0; point < left_gc_trigger.size() / 2; ++point) {
        const float trigger_x = left_gc_trigger[point * 2 + 0];
        const float trigger_y = left_gc_trigger[point * 2 + 1];

        qleft_trigger[point] = center + QPointF(trigger_x, trigger_y + (left_pressed ? 10 : 0));
        qright_trigger[point] = center + QPointF(-trigger_x, trigger_y + (right_pressed ? 10 : 0));
    }

    // Left trigger
    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);

    // Right trigger
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);

    // Draw L text
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, center + QPointF(-132, -119 + (left_pressed ? 10 : 0)), Symbol::L, 1.7f);

    // Draw R text
    p.setPen(colors.transparent);
    p.setBrush(colors.font);
    DrawSymbol(p, center + QPointF(121.5f, -119 + (right_pressed ? 10 : 0)), Symbol::R, 1.7f);
}

void PlayerControlPreview::DrawHandheldTriggers(QPainter& p, const QPointF center,
                                                bool left_pressed, bool right_pressed) {
    std::array<QPointF, left_joycon_trigger.size() / 2> qleft_trigger;
    std::array<QPointF, left_joycon_trigger.size() / 2> qright_trigger;

    for (std::size_t point = 0; point < left_joycon_trigger.size() / 2; ++point) {
        const float left_trigger_x = left_joycon_trigger[point * 2 + 0];
        const float left_trigger_y = left_joycon_trigger[point * 2 + 1];

        qleft_trigger[point] =
            center + QPointF(left_trigger_x, left_trigger_y + (left_pressed ? 0.5f : 0));
        qright_trigger[point] =
            center + QPointF(-left_trigger_x, left_trigger_y + (right_pressed ? 0.5f : 0));
    }

    // Left trigger
    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);

    // Right trigger
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);
}

void PlayerControlPreview::DrawDualTriggers(QPainter& p, const QPointF center, bool left_pressed,
                                            bool right_pressed) {
    std::array<QPointF, left_joycon_trigger.size() / 2> qleft_trigger;
    std::array<QPointF, left_joycon_trigger.size() / 2> qright_trigger;
    constexpr float size = 1.62f;
    constexpr float offset = 210.6f;
    for (std::size_t point = 0; point < left_joycon_trigger.size() / 2; ++point) {
        const float left_trigger_x = left_joycon_trigger[point * 2 + 0];
        const float left_trigger_y = left_joycon_trigger[point * 2 + 1];

        qleft_trigger[point] = center + QPointF(left_trigger_x * size + offset,
                                                left_trigger_y * size + (left_pressed ? 0.5f : 0));
        qright_trigger[point] =
            center + QPointF(-left_trigger_x * size - offset,
                             left_trigger_y * size + (right_pressed ? 0.5f : 0));
    }

    // Left trigger
    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);

    // Right trigger
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);
}

void PlayerControlPreview::DrawDualTriggersTopView(QPainter& p, const QPointF center,
                                                   bool left_pressed, bool right_pressed) {
    std::array<QPointF, left_joystick_L_topview.size() / 2> qleft_trigger;
    std::array<QPointF, left_joystick_L_topview.size() / 2> qright_trigger;
    constexpr float size = 0.9f;

    for (std::size_t point = 0; point < left_joystick_L_topview.size() / 2; ++point) {
        const float top_view_x = left_joystick_L_topview[point * 2 + 0];
        const float top_view_y = left_joystick_L_topview[point * 2 + 1];

        qleft_trigger[point] = center + QPointF(top_view_x * size - 50, top_view_y * size - 52);
    }
    for (std::size_t point = 0; point < left_joystick_L_topview.size() / 2; ++point) {
        const float top_view_x = left_joystick_L_topview[point * 2 + 0];
        const float top_view_y = left_joystick_L_topview[point * 2 + 1];

        qright_trigger[point] = center + QPointF(-top_view_x * size + 50, top_view_y * size - 52);
    }

    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);

    // Draw L text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(-183, -84), Symbol::L, 1.0f);

    // Draw R text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(177, -84), Symbol::R, 1.0f);
}

void PlayerControlPreview::DrawDualZTriggersTopView(QPainter& p, const QPointF center,
                                                    bool left_pressed, bool right_pressed) {
    std::array<QPointF, left_joystick_ZL_topview.size() / 2> qleft_trigger;
    std::array<QPointF, left_joystick_ZL_topview.size() / 2> qright_trigger;
    constexpr float size = 0.9f;

    for (std::size_t point = 0; point < left_joystick_ZL_topview.size() / 2; ++point) {
        qleft_trigger[point] =
            center + QPointF(left_joystick_ZL_topview[point * 2] * size - 52,
                             left_joystick_ZL_topview[point * 2 + 1] * size - 52);
    }
    for (std::size_t point = 0; point < left_joystick_ZL_topview.size() / 2; ++point) {
        qright_trigger[point] =
            center + QPointF(-left_joystick_ZL_topview[point * 2] * size + 52,
                             left_joystick_ZL_topview[point * 2 + 1] * size - 52);
    }

    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);

    // Draw ZL text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(-180, -113), Symbol::ZL, 1.0f);

    // Draw ZR text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(180, -113), Symbol::ZR, 1.0f);
}

void PlayerControlPreview::DrawLeftTriggers(QPainter& p, const QPointF center, bool left_pressed) {
    std::array<QPointF, left_joycon_trigger.size() / 2> qleft_trigger;
    constexpr float size = 1.78f;
    constexpr float offset = 311.5f;

    for (std::size_t point = 0; point < left_joycon_trigger.size() / 2; ++point) {
        qleft_trigger[point] = center + QPointF(left_joycon_trigger[point * 2] * size + offset,
                                                left_joycon_trigger[point * 2 + 1] * size -
                                                    (left_pressed ? 0.5f : 1.0f));
    }

    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);
}

void PlayerControlPreview::DrawLeftZTriggers(QPainter& p, const QPointF center, bool left_pressed) {
    std::array<QPointF, left_joycon_sideview_zl.size() / 2> qleft_trigger;
    constexpr float size = 1.1115f;
    constexpr float offset2 = 335;

    for (std::size_t point = 0; point < left_joycon_sideview_zl.size() / 2; ++point) {
        qleft_trigger[point] = center + QPointF(left_joycon_sideview_zl[point * 2] * size + offset2,
                                                left_joycon_sideview_zl[point * 2 + 1] * size +
                                                    (left_pressed ? 1.5f : 1.0f));
    }

    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);
    p.drawArc(center.x() + 158, center.y() + (left_pressed ? -203.5f : -204.0f), 77, 77, 225 * 16,
              44 * 16);
}

void PlayerControlPreview::DrawLeftTriggersTopView(QPainter& p, const QPointF center,
                                                   bool left_pressed) {
    std::array<QPointF, left_joystick_L_topview.size() / 2> qleft_trigger;

    for (std::size_t point = 0; point < left_joystick_L_topview.size() / 2; ++point) {
        qleft_trigger[point] = center + QPointF(left_joystick_L_topview[point * 2],
                                                left_joystick_L_topview[point * 2 + 1]);
    }

    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);

    // Draw L text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(-143, -36), Symbol::L, 1.0f);
}

void PlayerControlPreview::DrawLeftZTriggersTopView(QPainter& p, const QPointF center,
                                                    bool left_pressed) {
    std::array<QPointF, left_joystick_ZL_topview.size() / 2> qleft_trigger;

    for (std::size_t point = 0; point < left_joystick_ZL_topview.size() / 2; ++point) {
        qleft_trigger[point] = center + QPointF(left_joystick_ZL_topview[point * 2],
                                                left_joystick_ZL_topview[point * 2 + 1]);
    }

    p.setPen(colors.outline);
    p.setBrush(left_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qleft_trigger);

    // Draw ZL text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(-140, -68), Symbol::ZL, 1.0f);
}

void PlayerControlPreview::DrawRightTriggers(QPainter& p, const QPointF center,
                                             bool right_pressed) {
    std::array<QPointF, left_joycon_trigger.size() / 2> qright_trigger;
    constexpr float size = 1.78f;
    constexpr float offset = 311.5f;

    for (std::size_t point = 0; point < left_joycon_trigger.size() / 2; ++point) {
        qright_trigger[point] = center + QPointF(-left_joycon_trigger[point * 2] * size - offset,
                                                 left_joycon_trigger[point * 2 + 1] * size -
                                                     (right_pressed ? 0.5f : 1.0f));
    }

    p.setPen(colors.outline);
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);
}

void PlayerControlPreview::DrawRightZTriggers(QPainter& p, const QPointF center,
                                              bool right_pressed) {
    std::array<QPointF, left_joycon_sideview_zl.size() / 2> qright_trigger;
    constexpr float size = 1.1115f;
    constexpr float offset2 = 335;

    for (std::size_t point = 0; point < left_joycon_sideview_zl.size() / 2; ++point) {
        qright_trigger[point] =
            center +
            QPointF(-left_joycon_sideview_zl[point * 2] * size - offset2,
                    left_joycon_sideview_zl[point * 2 + 1] * size + (right_pressed ? 0.5f : 0) + 1);
    }

    p.setPen(colors.outline);
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);
    p.drawArc(center.x() - 236, center.y() + (right_pressed ? -203.5f : -204.0f), 77, 77, 271 * 16,
              44 * 16);
}

void PlayerControlPreview::DrawRightTriggersTopView(QPainter& p, const QPointF center,
                                                    bool right_pressed) {
    std::array<QPointF, left_joystick_L_topview.size() / 2> qright_trigger;

    for (std::size_t point = 0; point < left_joystick_L_topview.size() / 2; ++point) {
        qright_trigger[point] = center + QPointF(-left_joystick_L_topview[point * 2],
                                                 left_joystick_L_topview[point * 2 + 1]);
    }

    p.setPen(colors.outline);
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);

    // Draw R text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(137, -36), Symbol::R, 1.0f);
}

void PlayerControlPreview::DrawRightZTriggersTopView(QPainter& p, const QPointF center,
                                                     bool right_pressed) {
    std::array<QPointF, left_joystick_ZL_topview.size() / 2> qright_trigger;

    for (std::size_t point = 0; point < left_joystick_ZL_topview.size() / 2; ++point) {
        qright_trigger[point] = center + QPointF(-left_joystick_ZL_topview[point * 2],
                                                 left_joystick_ZL_topview[point * 2 + 1]);
    }

    p.setPen(colors.outline);
    p.setBrush(right_pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qright_trigger);

    // Draw ZR text
    p.setPen(colors.transparent);
    p.setBrush(colors.font2);
    DrawSymbol(p, center + QPointF(140, -68), Symbol::ZR, 1.0f);
}

void PlayerControlPreview::DrawJoystick(QPainter& p, const QPointF center, float size,
                                        bool pressed) {
    const float radius1 = 13.0f * size;
    const float radius2 = 9.0f * size;

    // Outer circle
    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    DrawCircle(p, center, radius1);

    // Cross
    p.drawLine(center - QPoint(radius1, 0), center + QPoint(radius1, 0));
    p.drawLine(center - QPoint(0, radius1), center + QPoint(0, radius1));

    // Inner circle
    p.setBrush(pressed ? colors.highlight2 : colors.button2);
    DrawCircle(p, center, radius2);
}

void PlayerControlPreview::DrawJoystickSideview(QPainter& p, const QPointF center, float angle,
                                                float size, bool pressed) {
    QVector<QPointF> joystick;
    joystick.reserve(static_cast<int>(left_joystick_sideview.size() / 2));

    for (std::size_t point = 0; point < left_joystick_sideview.size() / 2; ++point) {
        joystick.append(QPointF(left_joystick_sideview[point * 2] * size + (pressed ? 1 : 0),
                                left_joystick_sideview[point * 2 + 1] * size - 1));
    }

    // Rotate joystick
    QTransform t;
    t.translate(center.x(), center.y());
    t.rotate(18 * angle);
    QPolygonF p2 = t.map(QPolygonF(joystick));

    // Draw joystick
    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    p.drawPolygon(p2);
    p.drawLine(p2.at(1), p2.at(30));
    p.drawLine(p2.at(32), p2.at(71));
}

void PlayerControlPreview::DrawProJoystick(QPainter& p, const QPointF center, const QPointF offset,
                                           float offset_scalar, bool pressed) {
    const float radius1 = 24.0f;
    const float radius2 = 17.0f;

    const QPointF offset_center = center + offset * offset_scalar;

    const auto amplitude = static_cast<float>(
        1.0 - std::sqrt((offset.x() * offset.x()) + (offset.y() * offset.y())) * 0.1f);

    const float rotation =
        ((offset.x() == 0) ? atan(1) * 2 : atan(offset.y() / offset.x())) * (180 / (atan(1) * 4));

    p.save();
    p.translate(offset_center);
    p.rotate(rotation);

    // Outer circle
    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    p.drawEllipse(QPointF(0, 0), radius1 * amplitude, radius1);

    // Inner circle
    p.setBrush(pressed ? colors.highlight2 : colors.button2);

    const float inner_offset =
        (radius1 - radius2) * 0.4f * ((offset.x() == 0 && offset.y() < 0) ? -1.0f : 1.0f);
    const float offset_factor = (1.0f - amplitude) / 0.1f;

    p.drawEllipse(QPointF((offset.x() < 0) ? -inner_offset : inner_offset, 0) * offset_factor,
                  radius2 * amplitude, radius2);

    p.restore();
}

void PlayerControlPreview::DrawGCJoystick(QPainter& p, const QPointF center, bool pressed) {
    // Outer circle
    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    DrawCircle(p, center, 26.0f);

    // Inner circle
    p.setBrush(pressed ? colors.highlight2 : colors.button2);
    DrawCircle(p, center, 19.0f);
    p.setBrush(colors.transparent);
    DrawCircle(p, center, 13.5f);
    DrawCircle(p, center, 7.5f);
}

void PlayerControlPreview::DrawRawJoystick(QPainter& p, const QPointF center, const QPointF value,
                                           const Input::AnalogProperties& properties) {
    constexpr float size = 45.0f;
    const float range = size * properties.range;
    const float deadzone = size * properties.deadzone;

    // Max range zone circle
    p.setPen(colors.outline);
    p.setBrush(colors.transparent);
    QPen pen = p.pen();
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    DrawCircle(p, center, range);

    // Deadzone circle
    pen.setColor(colors.deadzone);
    p.setPen(pen);
    DrawCircle(p, center, deadzone);

    // Dot pointer
    p.setPen(colors.indicator);
    p.setBrush(colors.indicator);
    DrawCircle(p, center + (value * range), 2);
}

void PlayerControlPreview::DrawRoundButton(QPainter& p, QPointF center, bool pressed, float width,
                                           float height, Direction direction, float radius) {
    p.setBrush(button_color);
    if (pressed) {
        switch (direction) {
        case Direction::Left:
            center.setX(center.x() - 1);
            break;
        case Direction::Right:
            center.setX(center.x() + 1);
            break;
        case Direction::Down:
            center.setY(center.y() + 1);
            break;
        case Direction::Up:
            center.setY(center.y() - 1);
            break;
        case Direction::None:
            break;
        }
        p.setBrush(colors.highlight);
    }
    QRectF rect = {center.x() - width, center.y() - height, width * 2.0f, height * 2.0f};
    p.drawRoundedRect(rect, radius, radius);
}
void PlayerControlPreview::DrawMinusButton(QPainter& p, const QPointF center, bool pressed,
                                           int button_size) {
    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    DrawRectangle(p, center, button_size, button_size / 3.0f);
}
void PlayerControlPreview::DrawPlusButton(QPainter& p, const QPointF center, bool pressed,
                                          int button_size) {
    // Draw outer line
    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    DrawRectangle(p, center, button_size, button_size / 3.0f);
    DrawRectangle(p, center, button_size / 3.0f, button_size);

    // Scale down size
    button_size *= 0.88f;

    // Draw inner color
    p.setPen(colors.transparent);
    DrawRectangle(p, center, button_size, button_size / 3.0f);
    DrawRectangle(p, center, button_size / 3.0f, button_size);
}

void PlayerControlPreview::DrawGCButtonX(QPainter& p, const QPointF center, bool pressed) {
    std::array<QPointF, gc_button_x.size() / 2> button_x;

    for (std::size_t point = 0; point < gc_button_x.size() / 2; ++point) {
        button_x[point] = center + QPointF(gc_button_x[point * 2], gc_button_x[point * 2 + 1]);
    }

    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    DrawPolygon(p, button_x);
}

void PlayerControlPreview::DrawGCButtonY(QPainter& p, const QPointF center, bool pressed) {
    std::array<QPointF, gc_button_y.size() / 2> button_x;

    for (std::size_t point = 0; point < gc_button_y.size() / 2; ++point) {
        button_x[point] = center + QPointF(gc_button_y[point * 2], gc_button_y[point * 2 + 1]);
    }

    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    DrawPolygon(p, button_x);
}

void PlayerControlPreview::DrawGCButtonZ(QPainter& p, const QPointF center, bool pressed) {
    std::array<QPointF, gc_button_z.size() / 2> button_x;

    for (std::size_t point = 0; point < gc_button_z.size() / 2; ++point) {
        button_x[point] = center + QPointF(gc_button_z[point * 2],
                                           gc_button_z[point * 2 + 1] + (pressed ? 1 : 0));
    }

    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button2);
    DrawPolygon(p, button_x);
}

void PlayerControlPreview::DrawCircleButton(QPainter& p, const QPointF center, bool pressed,
                                            float button_size) {
    p.setBrush(button_color);
    if (pressed) {
        p.setBrush(colors.highlight);
    }
    p.drawEllipse(center, button_size, button_size);
}

void PlayerControlPreview::DrawArrowButtonOutline(QPainter& p, const QPointF center, float size) {
    const std::size_t arrow_points = up_arrow_button.size() / 2;
    std::array<QPointF, (arrow_points - 1) * 4> arrow_button_outline;

    for (std::size_t point = 0; point < arrow_points - 1; ++point) {
        const float up_arrow_x = up_arrow_button[point * 2 + 0];
        const float up_arrow_y = up_arrow_button[point * 2 + 1];

        arrow_button_outline[point] = center + QPointF(up_arrow_x * size, up_arrow_y * size);
        arrow_button_outline[(arrow_points - 1) * 2 - point - 1] =
            center + QPointF(up_arrow_y * size, up_arrow_x * size);
        arrow_button_outline[(arrow_points - 1) * 2 + point] =
            center + QPointF(-up_arrow_x * size, -up_arrow_y * size);
        arrow_button_outline[(arrow_points - 1) * 4 - point - 1] =
            center + QPointF(-up_arrow_y * size, -up_arrow_x * size);
    }
    // Draw arrow button outline
    p.setPen(colors.outline);
    p.setBrush(colors.transparent);
    DrawPolygon(p, arrow_button_outline);
}

void PlayerControlPreview::DrawArrowButton(QPainter& p, const QPointF center,
                                           const Direction direction, bool pressed, float size) {
    std::array<QPointF, up_arrow_button.size() / 2> arrow_button;
    QPoint offset;

    for (std::size_t point = 0; point < up_arrow_button.size() / 2; ++point) {
        const float up_arrow_x = up_arrow_button[point * 2 + 0];
        const float up_arrow_y = up_arrow_button[point * 2 + 1];

        switch (direction) {
        case Direction::Up:
            arrow_button[point] = center + QPointF(up_arrow_x * size, up_arrow_y * size);
            break;
        case Direction::Left:
            arrow_button[point] = center + QPointF(up_arrow_y * size, up_arrow_x * size);
            break;
        case Direction::Right:
            arrow_button[point] = center + QPointF(-up_arrow_y * size, up_arrow_x * size);
            break;
        case Direction::Down:
            arrow_button[point] = center + QPointF(up_arrow_x * size, -up_arrow_y * size);
            break;
        case Direction::None:
            break;
        }
    }

    // Draw arrow button
    p.setPen(pressed ? colors.highlight : colors.button);
    p.setBrush(pressed ? colors.highlight : colors.button);
    DrawPolygon(p, arrow_button);

    switch (direction) {
    case Direction::Up:
        offset = QPoint(0, -20 * size);
        break;
    case Direction::Left:
        offset = QPoint(-20 * size, 0);
        break;
    case Direction::Right:
        offset = QPoint(20 * size, 0);
        break;
    case Direction::Down:
        offset = QPoint(0, 20 * size);
        break;
    case Direction::None:
        offset = QPoint(0, 0);
        break;
    }

    // Draw arrow icon
    p.setPen(colors.font2);
    p.setBrush(colors.font2);
    DrawArrow(p, center + offset, direction, size);
}

void PlayerControlPreview::DrawTriggerButton(QPainter& p, const QPointF center,
                                             const Direction direction, bool pressed) {
    std::array<QPointF, trigger_button.size() / 2> qtrigger_button;

    for (std::size_t point = 0; point < trigger_button.size() / 2; ++point) {
        const float trigger_button_x = trigger_button[point * 2 + 0];
        const float trigger_button_y = trigger_button[point * 2 + 1];

        switch (direction) {
        case Direction::Left:
            qtrigger_button[point] = center + QPointF(-trigger_button_x, trigger_button_y);
            break;
        case Direction::Right:
            qtrigger_button[point] = center + QPointF(trigger_button_x, trigger_button_y);
            break;
        case Direction::Up:
        case Direction::Down:
        case Direction::None:
            break;
        }
    }

    // Draw arrow button
    p.setPen(colors.outline);
    p.setBrush(pressed ? colors.highlight : colors.button);
    DrawPolygon(p, qtrigger_button);
}

void PlayerControlPreview::DrawSymbol(QPainter& p, const QPointF center, Symbol symbol,
                                      float icon_size) {
    std::array<QPointF, house.size() / 2> house_icon;
    std::array<QPointF, symbol_a.size() / 2> a_icon;
    std::array<QPointF, symbol_b.size() / 2> b_icon;
    std::array<QPointF, symbol_x.size() / 2> x_icon;
    std::array<QPointF, symbol_y.size() / 2> y_icon;
    std::array<QPointF, symbol_l.size() / 2> l_icon;
    std::array<QPointF, symbol_r.size() / 2> r_icon;
    std::array<QPointF, symbol_c.size() / 2> c_icon;
    std::array<QPointF, symbol_zl.size() / 2> zl_icon;
    std::array<QPointF, symbol_sl.size() / 2> sl_icon;
    std::array<QPointF, symbol_zr.size() / 2> zr_icon;
    std::array<QPointF, symbol_sr.size() / 2> sr_icon;
    switch (symbol) {
    case Symbol::House:
        for (std::size_t point = 0; point < house.size() / 2; ++point) {
            house_icon[point] = center + QPointF(house[point * 2] * icon_size,
                                                 (house[point * 2 + 1] - 0.025f) * icon_size);
        }
        p.drawPolygon(house_icon.data(), static_cast<int>(house_icon.size()));
        break;
    case Symbol::A:
        for (std::size_t point = 0; point < symbol_a.size() / 2; ++point) {
            a_icon[point] = center + QPointF(symbol_a[point * 2] * icon_size,
                                             symbol_a[point * 2 + 1] * icon_size);
        }
        p.drawPolygon(a_icon.data(), static_cast<int>(a_icon.size()));
        break;
    case Symbol::B:
        for (std::size_t point = 0; point < symbol_b.size() / 2; ++point) {
            b_icon[point] = center + QPointF(symbol_b[point * 2] * icon_size,
                                             symbol_b[point * 2 + 1] * icon_size);
        }
        p.drawPolygon(b_icon.data(), static_cast<int>(b_icon.size()));
        break;
    case Symbol::X:
        for (std::size_t point = 0; point < symbol_x.size() / 2; ++point) {
            x_icon[point] = center + QPointF(symbol_x[point * 2] * icon_size,
                                             symbol_x[point * 2 + 1] * icon_size);
        }
        p.drawPolygon(x_icon.data(), static_cast<int>(x_icon.size()));
        break;
    case Symbol::Y:
        for (std::size_t point = 0; point < symbol_y.size() / 2; ++point) {
            y_icon[point] = center + QPointF(symbol_y[point * 2] * icon_size,
                                             (symbol_y[point * 2 + 1] - 1.0f) * icon_size);
        }
        p.drawPolygon(y_icon.data(), static_cast<int>(y_icon.size()));
        break;
    case Symbol::L:
        for (std::size_t point = 0; point < symbol_l.size() / 2; ++point) {
            l_icon[point] = center + QPointF(symbol_l[point * 2] * icon_size,
                                             (symbol_l[point * 2 + 1] - 1.0f) * icon_size);
        }
        p.drawPolygon(l_icon.data(), static_cast<int>(l_icon.size()));
        break;
    case Symbol::R:
        for (std::size_t point = 0; point < symbol_r.size() / 2; ++point) {
            r_icon[point] = center + QPointF(symbol_r[point * 2] * icon_size,
                                             (symbol_r[point * 2 + 1] - 1.0f) * icon_size);
        }
        p.drawPolygon(r_icon.data(), static_cast<int>(r_icon.size()));
        break;
    case Symbol::C:
        for (std::size_t point = 0; point < symbol_c.size() / 2; ++point) {
            c_icon[point] = center + QPointF(symbol_c[point * 2] * icon_size,
                                             (symbol_c[point * 2 + 1] - 1.0f) * icon_size);
        }
        p.drawPolygon(c_icon.data(), static_cast<int>(c_icon.size()));
        break;
    case Symbol::ZL:
        for (std::size_t point = 0; point < symbol_zl.size() / 2; ++point) {
            zl_icon[point] = center + QPointF(symbol_zl[point * 2] * icon_size,
                                              symbol_zl[point * 2 + 1] * icon_size);
        }
        p.drawPolygon(zl_icon.data(), static_cast<int>(zl_icon.size()));
        break;
    case Symbol::SL:
        for (std::size_t point = 0; point < symbol_sl.size() / 2; ++point) {
            sl_icon[point] = center + QPointF(symbol_sl[point * 2] * icon_size,
                                              symbol_sl[point * 2 + 1] * icon_size);
        }
        p.drawPolygon(sl_icon.data(), static_cast<int>(sl_icon.size()));
        break;
    case Symbol::ZR:
        for (std::size_t point = 0; point < symbol_zr.size() / 2; ++point) {
            zr_icon[point] = center + QPointF(symbol_zr[point * 2] * icon_size,
                                              symbol_zr[point * 2 + 1] * icon_size);
        }
        p.drawPolygon(zr_icon.data(), static_cast<int>(zr_icon.size()));
        break;
    case Symbol::SR:
        for (std::size_t point = 0; point < symbol_sr.size() / 2; ++point) {
            sr_icon[point] = center + QPointF(symbol_sr[point * 2] * icon_size,
                                              symbol_sr[point * 2 + 1] * icon_size);
        }
        p.drawPolygon(sr_icon.data(), static_cast<int>(sr_icon.size()));
        break;
    }
}

void PlayerControlPreview::DrawArrow(QPainter& p, const QPointF center, const Direction direction,
                                     float size) {

    std::array<QPointF, up_arrow_symbol.size() / 2> arrow_symbol;

    for (std::size_t point = 0; point < up_arrow_symbol.size() / 2; ++point) {
        const float up_arrow_x = up_arrow_symbol[point * 2 + 0];
        const float up_arrow_y = up_arrow_symbol[point * 2 + 1];

        switch (direction) {
        case Direction::Up:
            arrow_symbol[point] = center + QPointF(up_arrow_x * size, up_arrow_y * size);
            break;
        case Direction::Left:
            arrow_symbol[point] = center + QPointF(up_arrow_y * size, up_arrow_x * size);
            break;
        case Direction::Right:
            arrow_symbol[point] = center + QPointF(-up_arrow_y * size, up_arrow_x * size);
            break;
        case Direction::Down:
            arrow_symbol[point] = center + QPointF(up_arrow_x * size, -up_arrow_y * size);
            break;
        case Direction::None:
            break;
        }
    }

    DrawPolygon(p, arrow_symbol);
}

template <size_t N>
void PlayerControlPreview::DrawPolygon(QPainter& p, const std::array<QPointF, N>& polygon) {
    p.drawPolygon(polygon.data(), static_cast<int>(polygon.size()));
}

void PlayerControlPreview::DrawCircle(QPainter& p, const QPointF center, float size) {
    p.drawEllipse(center, size, size);
}

void PlayerControlPreview::DrawRectangle(QPainter& p, const QPointF center, float width,
                                         float height) {
    const QRectF rect = QRectF(center.x() - (width / 2), center.y() - (height / 2), width, height);
    p.drawRect(rect);
}
void PlayerControlPreview::DrawRoundRectangle(QPainter& p, const QPointF center, float width,
                                              float height, float round) {
    const QRectF rect = QRectF(center.x() - (width / 2), center.y() - (height / 2), width, height);
    p.drawRoundedRect(rect, round, round);
}

void PlayerControlPreview::DrawText(QPainter& p, const QPointF center, float text_size,
                                    const QString& text) {
    SetTextFont(p, text_size);
    const QFontMetrics fm(p.font());
    const QPointF offset = {fm.horizontalAdvance(text) / 2.0f, -text_size / 2.0f};
    p.drawText(center - offset, text);
}

void PlayerControlPreview::SetTextFont(QPainter& p, float text_size, const QString& font_family) {
    QFont font = p.font();
    font.setPointSizeF(text_size);
    font.setFamily(font_family);
    p.setFont(font);
}
