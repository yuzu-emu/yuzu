// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <chrono>
#include <optional>

#include <QFileDialog>
#include <QGraphicsItem>
#include <QMessageBox>
#include "common/assert.h"
#include "common/file_util.h"
#include "core/core.h"
#include "core/hle/service/time/time.h"
#include "core/settings.h"
#include "ui_configure_system.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_system.h"

ConfigureSystem::ConfigureSystem(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureSystem>()) {
    ui->setupUi(this);
    connect(ui->button_regenerate_console_id, &QPushButton::clicked, this,
            &ConfigureSystem::RefreshConsoleID);

    connect(ui->rng_seed_checkbox, &QCheckBox::stateChanged, this, [this](int state) {
        ui->rng_seed_edit->setEnabled(state == Qt::Checked);
        if (state != Qt::Checked) {
            ui->rng_seed_edit->setText(QStringLiteral("00000000"));
        }
    });

    connect(ui->custom_rtc_checkbox, &QCheckBox::stateChanged, this, [this](int state) {
        ui->custom_rtc_edit->setEnabled(state == Qt::Checked);
        if (state != Qt::Checked) {
            ui->custom_rtc_edit->setDateTime(QDateTime::currentDateTime());
        }
    });

    ui->label_console_id->setVisible(Settings::IsConfiguringGlobal());
    ui->button_regenerate_console_id->setVisible(Settings::IsConfiguringGlobal());

    SetupPerGameUI();

    SetConfiguration();
}

ConfigureSystem::~ConfigureSystem() = default;

void ConfigureSystem::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureSystem::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureSystem::SetConfiguration() {
    enabled = !Core::System::GetInstance().IsPoweredOn();
    const auto rng_seed =
        QStringLiteral("%1")
            .arg(Settings::values.rng_seed.GetValue().value_or(0), 8, 16, QLatin1Char{'0'})
            .toUpper();
    const auto rtc_time = Settings::values.custom_rtc.GetValue().value_or(
        std::chrono::seconds(QDateTime::currentSecsSinceEpoch()));

    ui->rng_seed_checkbox->setChecked(Settings::values.rng_seed.GetValue().has_value());
    ui->rng_seed_edit->setEnabled(Settings::values.rng_seed.GetValue().has_value() &&
                                  Settings::values.rng_seed.UsingGlobal());
    ui->rng_seed_edit->setText(rng_seed);

    ui->custom_rtc_checkbox->setChecked(Settings::values.custom_rtc.GetValue().has_value());
    ui->custom_rtc_edit->setEnabled(Settings::values.custom_rtc.GetValue().has_value() &&
                                    Settings::values.rng_seed.UsingGlobal());
    ui->custom_rtc_edit->setDateTime(QDateTime::fromSecsSinceEpoch(rtc_time.count()));

    if (Settings::IsConfiguringGlobal()) {
        ui->combo_language->setCurrentIndex(Settings::values.language_index.GetValue());
        ui->combo_region->setCurrentIndex(Settings::values.region_index.GetValue());
        ui->combo_time_zone->setCurrentIndex(Settings::values.time_zone_index.GetValue());
        ui->combo_sound->setCurrentIndex(Settings::values.sound_index.GetValue());
    } else {
        ConfigurationShared::SetPerGameSetting(ui->combo_language,
                                               &Settings::values.language_index);
        ConfigurationShared::SetPerGameSetting(ui->combo_region, &Settings::values.region_index);
        ConfigurationShared::SetPerGameSetting(ui->combo_time_zone,
                                               &Settings::values.time_zone_index);
        ConfigurationShared::SetPerGameSetting(ui->combo_sound, &Settings::values.sound_index);

        ConfigurationShared::SetHighlight(ui->label_language,
                                          !Settings::values.language_index.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->label_region,
                                          !Settings::values.region_index.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->label_timezone,
                                          !Settings::values.time_zone_index.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->label_sound,
                                          !Settings::values.sound_index.UsingGlobal());
    }
}

void ConfigureSystem::ReadSystemSettings() {}

void ConfigureSystem::ApplyConfiguration() {
    auto& system = Core::System::GetInstance();

    // Allow setting custom RTC even if system is powered on,
    // to allow in-game time to be fast forwarded
    if (Settings::values.custom_rtc.UsingGlobal()) {
        if (ui->custom_rtc_checkbox->isChecked()) {
            Settings::values.custom_rtc.SetValue(
                std::chrono::seconds(ui->custom_rtc_edit->dateTime().toSecsSinceEpoch()));
            if (system.IsPoweredOn()) {
                const s64 posix_time{Settings::values.custom_rtc.GetValue()->count() +
                                     Service::Time::TimeManager::GetExternalTimeZoneOffset()};
                system.GetTimeManager().UpdateLocalSystemClockTime(posix_time);
            }
        } else {
            Settings::values.custom_rtc.SetValue(std::nullopt);
        }
    }

    if (!enabled) {
        return;
    }

    if (Settings::IsConfiguringGlobal()) {
        // Guard if during game and set to game-specific value
        if (Settings::values.language_index.UsingGlobal()) {
            Settings::values.language_index.SetValue(ui->combo_language->currentIndex());
        }
        if (Settings::values.region_index.UsingGlobal()) {
            Settings::values.region_index.SetValue(ui->combo_region->currentIndex());
        }
        if (Settings::values.time_zone_index.UsingGlobal()) {
            Settings::values.time_zone_index.SetValue(ui->combo_time_zone->currentIndex());
        }
        if (Settings::values.sound_index.UsingGlobal()) {
            Settings::values.sound_index.SetValue(ui->combo_sound->currentIndex());
        }

        if (Settings::values.rng_seed.UsingGlobal()) {
            if (ui->rng_seed_checkbox->isChecked()) {
                Settings::values.rng_seed.SetValue(
                    ui->rng_seed_edit->text().toULongLong(nullptr, 16));
            } else {
                Settings::values.rng_seed.SetValue(std::nullopt);
            }
        }
    } else {
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.language_index,
                                                 ui->combo_language);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.region_index, ui->combo_region);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.time_zone_index,
                                                 ui->combo_time_zone);
        ConfigurationShared::ApplyPerGameSetting(&Settings::values.sound_index, ui->combo_sound);

        switch (use_rng_seed) {
        case ConfigurationShared::CheckState::On:
        case ConfigurationShared::CheckState::Off:
            Settings::values.rng_seed.SetGlobal(false);
            if (ui->rng_seed_checkbox->isChecked()) {
                Settings::values.rng_seed.SetValue(
                    ui->rng_seed_edit->text().toULongLong(nullptr, 16));
            } else {
                Settings::values.rng_seed.SetValue(std::nullopt);
            }
            break;
        case ConfigurationShared::CheckState::Global:
            Settings::values.rng_seed.SetGlobal(false);
            Settings::values.rng_seed.SetValue(std::nullopt);
            Settings::values.rng_seed.SetGlobal(true);
            break;
        case ConfigurationShared::CheckState::Count:
            break;
        }

        switch (use_custom_rtc) {
        case ConfigurationShared::CheckState::On:
        case ConfigurationShared::CheckState::Off:
            Settings::values.custom_rtc.SetGlobal(false);
            if (ui->custom_rtc_checkbox->isChecked()) {
                Settings::values.custom_rtc.SetValue(
                    std::chrono::seconds(ui->custom_rtc_edit->dateTime().toSecsSinceEpoch()));
            } else {
                Settings::values.custom_rtc.SetValue(std::nullopt);
            }
            break;
        case ConfigurationShared::CheckState::Global:
            Settings::values.custom_rtc.SetGlobal(false);
            Settings::values.custom_rtc.SetValue(std::nullopt);
            Settings::values.custom_rtc.SetGlobal(true);
            break;
        case ConfigurationShared::CheckState::Count:
            break;
        }
    }

    Settings::Apply(system);
}

void ConfigureSystem::RefreshConsoleID() {
    QMessageBox::StandardButton reply;
    QString warning_text = tr("This will replace your current virtual Switch with a new one. "
                              "Your current virtual Switch will not be recoverable. "
                              "This might have unexpected effects in games. This might fail, "
                              "if you use an outdated config savegame. Continue?");
    reply = QMessageBox::critical(this, tr("Warning"), warning_text,
                                  QMessageBox::No | QMessageBox::Yes);
    if (reply == QMessageBox::No) {
        return;
    }

    u64 console_id{};
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));
}

void ConfigureSystem::SetupPerGameUI() {
    if (Settings::IsConfiguringGlobal()) {
        ui->combo_language->setEnabled(Settings::values.language_index.UsingGlobal());
        ui->combo_region->setEnabled(Settings::values.region_index.UsingGlobal());
        ui->combo_time_zone->setEnabled(Settings::values.time_zone_index.UsingGlobal());
        ui->combo_sound->setEnabled(Settings::values.sound_index.UsingGlobal());
        ui->rng_seed_checkbox->setEnabled(Settings::values.rng_seed.UsingGlobal());
        ui->rng_seed_edit->setEnabled(Settings::values.rng_seed.UsingGlobal());
        ui->custom_rtc_checkbox->setEnabled(Settings::values.custom_rtc.UsingGlobal());
        ui->custom_rtc_edit->setEnabled(Settings::values.custom_rtc.UsingGlobal());

        return;
    }

    ConfigurationShared::SetColoredComboBox(ui->combo_language, ui->label_language,
                                            Settings::values.language_index.GetValue(true));
    ConfigurationShared::SetColoredComboBox(ui->combo_region, ui->label_region,
                                            Settings::values.region_index.GetValue(true));
    ConfigurationShared::SetColoredComboBox(ui->combo_time_zone, ui->label_timezone,
                                            Settings::values.time_zone_index.GetValue(true));
    ConfigurationShared::SetColoredComboBox(ui->combo_sound, ui->label_sound,
                                            Settings::values.sound_index.GetValue(true));

    ConfigurationShared::SetColoredTristate(
        ui->rng_seed_checkbox, Settings::values.rng_seed.UsingGlobal(),
        Settings::values.rng_seed.GetValue().has_value(),
        Settings::values.rng_seed.GetValue(true).has_value(), use_rng_seed);
    ConfigurationShared::SetColoredTristate(
        ui->custom_rtc_checkbox, Settings::values.custom_rtc.UsingGlobal(),
        Settings::values.custom_rtc.GetValue().has_value(),
        Settings::values.custom_rtc.GetValue(true).has_value(), use_custom_rtc);
}
