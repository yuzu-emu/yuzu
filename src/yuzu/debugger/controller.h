// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QFileSystemWatcher>
#include <QWidget>
#include "common/settings.h"

class QAction;
class QHideEvent;
class QShowEvent;
class PlayerControlPreview;

namespace InputCommon {
class InputSubsystem;
}

struct ControllerInput {
    std::array<std::pair<float, float>, Settings::NativeAnalog::NUM_STICKS_HID> axis_values{};
    std::array<bool, Settings::NativeButton::NumButtons> button_values{};
    bool changed{};
};

struct ControllerCallback {
    std::function<void(ControllerInput)> input;
};

class ControllerDialog : public QWidget {
    Q_OBJECT

public:
    explicit ControllerDialog(QWidget* parent = nullptr,
                              InputCommon::InputSubsystem* input_subsystem_ = nullptr);

    /// Returns a QAction that can be used to toggle visibility of this dialog.
    QAction* toggleViewAction();
    void refreshConfiguration();

protected:
    void showEvent(QShowEvent* ev) override;
    void hideEvent(QHideEvent* ev) override;

private:
    void InputController(ControllerInput input);
    QAction* toggle_view_action = nullptr;
    QFileSystemWatcher* watcher = nullptr;
    PlayerControlPreview* widget;
    InputCommon::InputSubsystem* input_subsystem;
};
