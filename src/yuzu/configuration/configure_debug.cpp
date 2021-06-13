// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QUrl>
#include "common/fs/path_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_debug.h"
#include "yuzu/configuration/configure_debug.h"
#include "yuzu/debugger/console.h"
#include "yuzu/uisettings.h"

ConfigureDebug::ConfigureDebug(QWidget* parent) : QWidget(parent), ui(new Ui::ConfigureDebug) {
    ui->setupUi(this);
    SetConfiguration();

    connect(ui->open_log_button, &QPushButton::clicked, []() {
        const auto path =
            QString::fromStdString(Common::FS::GetYuzuPathString(Common::FS::YuzuPath::LogDir));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
}

ConfigureDebug::~ConfigureDebug() = default;

void ConfigureDebug::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->toggle_console->setEnabled(runtime_lock);
    ui->toggle_console->setChecked(UISettings::values.show_console);
    ui->log_filter_edit->setText(QString::fromStdString(Settings::values.log_filter));
    ui->homebrew_args_edit->setText(QString::fromStdString(Settings::values.program_args));
    ui->fs_access_log->setEnabled(runtime_lock);
    ui->fs_access_log->setChecked(Settings::values.enable_fs_access_log);
    ui->reporting_services->setChecked(Settings::values.reporting_services);
    ui->quest_flag->setChecked(Settings::values.quest_flag);
    ui->use_debug_asserts->setChecked(Settings::values.use_debug_asserts);
    ui->use_auto_stub->setChecked(Settings::values.use_auto_stub);
    ui->enable_graphics_debugging->setEnabled(runtime_lock);
    ui->enable_graphics_debugging->setChecked(Settings::values.renderer_debug);
    ui->enable_nsight_aftermath->setEnabled(runtime_lock);
    ui->enable_nsight_aftermath->setChecked(Settings::values.enable_nsight_aftermath);
    ui->disable_macro_jit->setEnabled(runtime_lock);
    ui->disable_macro_jit->setChecked(Settings::values.disable_macro_jit);
    ui->disable_loop_safety_checks->setEnabled(runtime_lock);
    ui->disable_loop_safety_checks->setChecked(Settings::values.disable_shader_loop_safety_checks);
    ui->extended_logging->setChecked(Settings::values.extended_logging);
}

void ConfigureDebug::ApplyConfiguration() {
    UISettings::values.show_console = ui->toggle_console->isChecked();
    Settings::values.log_filter = ui->log_filter_edit->text().toStdString();
    Settings::values.program_args = ui->homebrew_args_edit->text().toStdString();
    Settings::values.enable_fs_access_log = ui->fs_access_log->isChecked();
    Settings::values.reporting_services = ui->reporting_services->isChecked();
    Settings::values.quest_flag = ui->quest_flag->isChecked();
    Settings::values.use_debug_asserts = ui->use_debug_asserts->isChecked();
    Settings::values.use_auto_stub = ui->use_auto_stub->isChecked();
    Settings::values.renderer_debug = ui->enable_graphics_debugging->isChecked();
    Settings::values.enable_nsight_aftermath = ui->enable_nsight_aftermath->isChecked();
    Settings::values.disable_shader_loop_safety_checks =
        ui->disable_loop_safety_checks->isChecked();
    Settings::values.disable_macro_jit = ui->disable_macro_jit->isChecked();
    Settings::values.extended_logging = ui->extended_logging->isChecked();
    Debugger::ToggleConsole();
    Common::Log::Filter filter;
    filter.ParseFilterString(Settings::values.log_filter);
    Common::Log::SetGlobalFilter(filter);
}

void ConfigureDebug::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureDebug::RetranslateUI() {
    ui->retranslateUi(this);
}
