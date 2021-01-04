// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QComboBox>
#include <QMessageBox>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_cpu.h"
#include "yuzu/configuration/configure_cpu.h"

ConfigureCpu::ConfigureCpu(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureCpu>()) {
    ui->setupUi(this);

    SetConfiguration();

    connect(ui->accuracy, qOverload<int>(&QComboBox::activated), this,
            &ConfigureCpu::AccuracyUpdated);
    connect(ui->accuracy, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureCpu::UpdateGroup);
}

ConfigureCpu::~ConfigureCpu() = default;

void ConfigureCpu::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->accuracy->setEnabled(runtime_lock);
    ui->accuracy->setCurrentIndex(static_cast<int>(Settings::values.cpu_accuracy));
    UpdateGroup(static_cast<int>(Settings::values.cpu_accuracy));

    ui->cpuopt_unsafe_unfuse_fma->setEnabled(runtime_lock);
    ui->cpuopt_unsafe_unfuse_fma->setChecked(Settings::values.cpuopt_unsafe_unfuse_fma);
    ui->cpuopt_unsafe_reduce_fp_error->setEnabled(runtime_lock);
    ui->cpuopt_unsafe_reduce_fp_error->setChecked(Settings::values.cpuopt_unsafe_reduce_fp_error);
    ui->cpuopt_unsafe_inaccurate_nan->setEnabled(runtime_lock);
    ui->cpuopt_unsafe_inaccurate_nan->setChecked(Settings::values.cpuopt_unsafe_inaccurate_nan);
}

void ConfigureCpu::AccuracyUpdated(int index) {
    if (static_cast<Settings::CPUAccuracy>(index) == Settings::CPUAccuracy::DebugMode) {
        const auto result = QMessageBox::warning(this, tr("Setting CPU to Debug Mode"),
                                                 tr("CPU Debug Mode is only intended for developer "
                                                    "use. Are you sure you want to enable this?"),
                                                 QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::No) {
            ui->accuracy->setCurrentIndex(static_cast<int>(Settings::CPUAccuracy::Accurate));
            UpdateGroup(static_cast<int>(Settings::CPUAccuracy::Accurate));
        }
    }
}

void ConfigureCpu::UpdateGroup(int index) {
    ui->unsafe_group->setVisible(static_cast<Settings::CPUAccuracy>(index) ==
                                 Settings::CPUAccuracy::Unsafe);
}

void ConfigureCpu::ApplyConfiguration() {
    Settings::values.cpu_accuracy =
        static_cast<Settings::CPUAccuracy>(ui->accuracy->currentIndex());
    Settings::values.cpuopt_unsafe_unfuse_fma = ui->cpuopt_unsafe_unfuse_fma->isChecked();
    Settings::values.cpuopt_unsafe_reduce_fp_error = ui->cpuopt_unsafe_reduce_fp_error->isChecked();
    Settings::values.cpuopt_unsafe_inaccurate_nan = ui->cpuopt_unsafe_inaccurate_nan->isChecked();
}

void ConfigureCpu::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureCpu::RetranslateUI() {
    ui->retranslateUi(this);
}
