// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <QFrame>
#include <QPointer>
#include "common/settings.h"
#include "core/frontend/input.h"
#include "yuzu/debugger/controller.h"

class QLabel;

using AnalogParam = std::array<Common::ParamPackage, Settings::NativeAnalog::NumAnalogs>;
using ButtonParam = std::array<Common::ParamPackage, Settings::NativeButton::NumButtons>;

// Widget for representing controller animations
class PlayerControlPreview : public QFrame {
    Q_OBJECT

public:
    explicit PlayerControlPreview(QWidget* parent);
    ~PlayerControlPreview() override;

    void SetPlayerInput(std::size_t index, const ButtonParam& buttons_param,
                        const AnalogParam& analogs_param);
    void SetPlayerInputRaw(std::size_t index, const Settings::ButtonsRaw& buttons_,
                           Settings::AnalogsRaw analogs_);
    void SetConnectedStatus(bool checked);
    void SetControllerType(Settings::ControllerType type);
    void BeginMappingButton(std::size_t button_id);
    void BeginMappingAnalog(std::size_t button_id);
    void EndMapping();
    void UpdateInput();
    void SetCallBack(ControllerCallback callback_);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    enum class Direction : std::size_t {
        None,
        Up,
        Right,
        Down,
        Left,
    };

    enum class Symbol {
        House,
        A,
        B,
        X,
        Y,
        L,
        R,
        C,
        SL,
        ZL,
        ZR,
        SR,
    };

    struct AxisValue {
        QPointF value{};
        QPointF raw_value{};
        Input::AnalogProperties properties{};
        int size{};
        QPoint offset{};
        bool active{};
    };

    struct LedPattern {
        bool position1;
        bool position2;
        bool position3;
        bool position4;
    };

    struct ColorMapping {
        QColor outline{};
        QColor primary{};
        QColor left{};
        QColor right{};
        QColor button{};
        QColor button2{};
        QColor font{};
        QColor font2{};
        QColor highlight{};
        QColor highlight2{};
        QColor transparent{};
        QColor indicator{};
        QColor led_on{};
        QColor led_off{};
        QColor slider{};
        QColor slider_button{};
        QColor slider_arrow{};
        QColor deadzone{};
    };

    static LedPattern GetColorPattern(std::size_t index, bool player_on);
    void UpdateColors();
    void ResetInputs();

    // Draw controller functions
    void DrawHandheldController(QPainter& p, QPointF center);
    void DrawDualController(QPainter& p, QPointF center);
    void DrawLeftController(QPainter& p, QPointF center);
    void DrawRightController(QPainter& p, QPointF center);
    void DrawProController(QPainter& p, QPointF center);
    void DrawGCController(QPainter& p, QPointF center);

    // Draw body functions
    void DrawHandheldBody(QPainter& p, QPointF center);
    void DrawDualBody(QPainter& p, QPointF center);
    void DrawLeftBody(QPainter& p, QPointF center);
    void DrawRightBody(QPainter& p, QPointF center);
    void DrawProBody(QPainter& p, QPointF center);
    void DrawGCBody(QPainter& p, QPointF center);

    // Draw triggers functions
    void DrawProTriggers(QPainter& p, QPointF center, bool left_pressed, bool right_pressed);
    void DrawGCTriggers(QPainter& p, QPointF center, bool left_pressed, bool right_pressed);
    void DrawHandheldTriggers(QPainter& p, QPointF center, bool left_pressed, bool right_pressed);
    void DrawDualTriggers(QPainter& p, QPointF center, bool left_pressed, bool right_pressed);
    void DrawDualTriggersTopView(QPainter& p, QPointF center, bool left_pressed,
                                 bool right_pressed);
    void DrawDualZTriggersTopView(QPainter& p, QPointF center, bool left_pressed,
                                  bool right_pressed);
    void DrawLeftTriggers(QPainter& p, QPointF center, bool left_pressed);
    void DrawLeftZTriggers(QPainter& p, QPointF center, bool left_pressed);
    void DrawLeftTriggersTopView(QPainter& p, QPointF center, bool left_pressed);
    void DrawLeftZTriggersTopView(QPainter& p, QPointF center, bool left_pressed);
    void DrawRightTriggers(QPainter& p, QPointF center, bool right_pressed);
    void DrawRightZTriggers(QPainter& p, QPointF center, bool right_pressed);
    void DrawRightTriggersTopView(QPainter& p, QPointF center, bool right_pressed);
    void DrawRightZTriggersTopView(QPainter& p, QPointF center, bool right_pressed);

    // Draw joystick functions
    void DrawJoystick(QPainter& p, QPointF center, float size, bool pressed);
    void DrawJoystickSideview(QPainter& p, QPointF center, float angle, float size, bool pressed);
    void DrawRawJoystick(QPainter& p, QPointF center, QPointF value,
                         const Input::AnalogProperties& properties);
    void DrawProJoystick(QPainter& p, QPointF center, QPointF offset, float scalar, bool pressed);
    void DrawGCJoystick(QPainter& p, QPointF center, bool pressed);

    // Draw button functions
    void DrawCircleButton(QPainter& p, QPointF center, bool pressed, float button_size);
    void DrawRoundButton(QPainter& p, QPointF center, bool pressed, float width, float height,
                         Direction direction = Direction::None, float radius = 2);
    void DrawMinusButton(QPainter& p, QPointF center, bool pressed, int button_size);
    void DrawPlusButton(QPainter& p, QPointF center, bool pressed, int button_size);
    void DrawGCButtonX(QPainter& p, QPointF center, bool pressed);
    void DrawGCButtonY(QPainter& p, QPointF center, bool pressed);
    void DrawGCButtonZ(QPainter& p, QPointF center, bool pressed);
    void DrawArrowButtonOutline(QPainter& p, const QPointF center, float size = 1.0f);
    void DrawArrowButton(QPainter& p, QPointF center, Direction direction, bool pressed,
                         float size = 1.0f);
    void DrawTriggerButton(QPainter& p, QPointF center, Direction direction, bool pressed);

    // Draw icon functions
    void DrawSymbol(QPainter& p, QPointF center, Symbol symbol, float icon_size);
    void DrawArrow(QPainter& p, QPointF center, Direction direction, float size);

    // Draw primitive types
    template <size_t N>
    void DrawPolygon(QPainter& p, const std::array<QPointF, N>& polygon);
    void DrawCircle(QPainter& p, QPointF center, float size);
    void DrawRectangle(QPainter& p, QPointF center, float width, float height);
    void DrawRoundRectangle(QPainter& p, QPointF center, float width, float height, float round);
    void DrawText(QPainter& p, QPointF center, float text_size, const QString& text);
    void SetTextFont(QPainter& p, float text_size,
                     const QString& font_family = QStringLiteral("sans-serif"));

    using ButtonArray =
        std::array<std::unique_ptr<Input::ButtonDevice>, Settings::NativeButton::BUTTON_NS_END>;
    using StickArray =
        std::array<std::unique_ptr<Input::AnalogDevice>, Settings::NativeAnalog::NUM_STICKS_HID>;

    ControllerCallback controller_callback;
    bool is_enabled{};
    bool mapping_active{};
    int blink_counter{};
    QColor button_color{};
    ColorMapping colors{};
    std::array<QColor, 4> led_color{};
    ButtonArray buttons{};
    StickArray sticks{};
    std::size_t player_index{};
    std::size_t button_mapping_index{Settings::NativeButton::BUTTON_NS_END};
    std::size_t analog_mapping_index{Settings::NativeAnalog::NUM_STICKS_HID};
    std::array<AxisValue, Settings::NativeAnalog::NUM_STICKS_HID> axis_values{};
    std::array<bool, Settings::NativeButton::NumButtons> button_values{};
    Settings::ControllerType controller_type{Settings::ControllerType::ProController};
};
