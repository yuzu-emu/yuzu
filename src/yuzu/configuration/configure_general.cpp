// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QSpinBox>
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_general.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_general.h"
#include "yuzu/uisettings.h"

ConfigureGeneral::ConfigureGeneral(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureGeneral>()) {
    ui->setupUi(this);

    SetupPerGameUI();

    SetConfiguration();

    if (Settings::IsConfiguringGlobal()) {
        connect(ui->toggle_frame_limit, &QCheckBox::clicked, ui->frame_limit,
                [this]() { ui->frame_limit->setEnabled(ui->toggle_frame_limit->isChecked()); });
    }
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->use_multi_core->setEnabled(runtime_lock);
    ui->use_multi_core->setChecked(Settings::values.use_multi_core.GetValue());

    ui->toggle_check_exit->setChecked(UISettings::values.confirm_before_closing);
    ui->toggle_user_on_boot->setChecked(UISettings::values.select_user_on_boot);
    ui->toggle_background_pause->setChecked(UISettings::values.pause_when_in_background);
    ui->toggle_hide_mouse->setChecked(UISettings::values.hide_mouse);

    ui->toggle_frame_limit->setChecked(Settings::values.use_frame_limit.GetValue());
    ui->frame_limit->setValue(Settings::values.frame_limit.GetValue());

    if (Settings::IsConfiguringGlobal()) {
        ui->frame_limit->setEnabled(Settings::values.use_frame_limit.GetValue());
    } else {
        ui->frame_limit->setEnabled(Settings::values.use_frame_limit.GetValue() &&
                                    use_frame_limit != ConfigurationShared::CheckState::Global);
    }
}

void ConfigureGeneral::ApplyConfiguration() {
    if (Settings::IsConfiguringGlobal()) {
        UISettings::values.confirm_before_closing = ui->toggle_check_exit->isChecked();
        UISettings::values.select_user_on_boot = ui->toggle_user_on_boot->isChecked();
        UISettings::values.pause_when_in_background = ui->toggle_background_pause->isChecked();
        UISettings::values.hide_mouse = ui->toggle_hide_mouse->isChecked();

        // Guard if during game and set to game-specific value
        if (Settings::values.use_frame_limit.UsingGlobal()) {
            Settings::values.use_frame_limit.SetValue(ui->toggle_frame_limit->checkState() ==
                                                      Qt::Checked);
            Settings::values.frame_limit.SetValue(ui->frame_limit->value());
        }
        if (Settings::values.use_multi_core.UsingGlobal()) {
            Settings::values.use_multi_core.SetValue(ui->use_multi_core->isChecked());
        }
    } else {
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_multi_core,
                                                 ui->use_multi_core, use_multi_core);

        bool global_frame_limit = use_frame_limit == ConfigurationShared::CheckState::Global;
        Settings::values.use_frame_limit.SetGlobal(global_frame_limit);
        Settings::values.frame_limit.SetGlobal(global_frame_limit);
        if (!global_frame_limit) {
            Settings::values.use_frame_limit.SetValue(ui->toggle_frame_limit->checkState() ==
                                                      Qt::Checked);
            Settings::values.frame_limit.SetValue(ui->frame_limit->value());
        }
    }
}

void ConfigureGeneral::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGeneral::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGeneral::SetupPerGameUI() {
    if (Settings::IsConfiguringGlobal()) {
        ui->toggle_frame_limit->setEnabled(Settings::values.use_frame_limit.UsingGlobal());
        ui->frame_limit->setEnabled(Settings::values.frame_limit.UsingGlobal());

        return;
    }

    ui->toggle_check_exit->setVisible(false);
    ui->toggle_user_on_boot->setVisible(false);
    ui->toggle_background_pause->setVisible(false);
    ui->toggle_hide_mouse->setVisible(false);

    ConfigurationShared::SetColoredTristate(ui->toggle_frame_limit,
                                            Settings::values.use_frame_limit, use_frame_limit);
    ConfigurationShared::SetColoredTristate(ui->use_multi_core, Settings::values.use_multi_core,
                                            use_multi_core);

    connect(ui->toggle_frame_limit, &QCheckBox::clicked, ui->frame_limit, [this]() {
        ui->frame_limit->setEnabled(ui->toggle_frame_limit->isChecked() &&
                                    (use_frame_limit != ConfigurationShared::CheckState::Global));
    });
}
