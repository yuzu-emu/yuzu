// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QAction>
#include <QLayout>
#include <QString>
#include "common/settings.h"
#include "input_common/main.h"
#include "input_common/tas/tas_input.h"
#include "yuzu/configuration/configure_input_player_widget.h"
#include "yuzu/debugger/controller.h"

ControllerDialog::ControllerDialog(QWidget* parent, InputCommon::InputSubsystem* input_subsystem_)
    : QWidget(parent, Qt::Dialog), input_subsystem{input_subsystem_} {
    setObjectName(QStringLiteral("Controller"));
    setWindowTitle(tr("Controller P1"));
    resize(500, 350);
    setMinimumSize(500, 350);
    // Remove the "?" button from the titlebar and enable the maximize button
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowMaximizeButtonHint);

    widget = new PlayerControlPreview(this);
    refreshConfiguration();
    QLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(widget);
    setLayout(layout);

    // Configure focus so that widget is focusable and the dialog automatically forwards focus to
    // it.
    setFocusProxy(widget);
    widget->SetConnectedStatus(false);
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->setFocus();
}

void ControllerDialog::refreshConfiguration() {
    const auto& players = Settings::values.players.GetValue();
    constexpr std::size_t player = 0;
    widget->SetPlayerInputRaw(player, players[player].buttons, players[player].analogs);
    widget->SetControllerType(players[player].controller_type);
    ControllerCallback callback{[this](ControllerInput input) { InputController(input); }};
    widget->SetCallBack(callback);
    widget->repaint();
    widget->SetConnectedStatus(players[player].connected);
}

QAction* ControllerDialog::toggleViewAction() {
    if (toggle_view_action == nullptr) {
        toggle_view_action = new QAction(tr("&Controller P1"), this);
        toggle_view_action->setCheckable(true);
        toggle_view_action->setChecked(isVisible());
        connect(toggle_view_action, &QAction::toggled, this, &ControllerDialog::setVisible);
    }

    return toggle_view_action;
}

void ControllerDialog::showEvent(QShowEvent* ev) {
    if (toggle_view_action) {
        toggle_view_action->setChecked(isVisible());
    }
    refreshConfiguration();
    QWidget::showEvent(ev);
}

void ControllerDialog::hideEvent(QHideEvent* ev) {
    if (toggle_view_action) {
        toggle_view_action->setChecked(isVisible());
    }
    widget->SetConnectedStatus(false);
    QWidget::hideEvent(ev);
}

void ControllerDialog::InputController(ControllerInput input) {
    u32 buttons = 0;
    int index = 0;
    for (bool btn : input.button_values) {
        buttons |= (btn ? 1U : 0U) << index;
        index++;
    }
    input_subsystem->GetTas()->RecordInput(buttons, input.axis_values);
}
