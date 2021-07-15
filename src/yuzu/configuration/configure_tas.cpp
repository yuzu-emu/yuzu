// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QFileDialog>
#include <QMessageBox>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "ui_configure_tas.h"
#include "yuzu/configuration/configure_tas.h"
#include "yuzu/uisettings.h"

ConfigureTasDialog::ConfigureTasDialog(QWidget* parent)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigureTas>()) {

    ui->setupUi(this);

    setFocusPolicy(Qt::ClickFocus);
    setWindowTitle(tr("TAS Configuration"));

    connect(ui->tas_path_button, &QToolButton::pressed, this,
            [this] { SetDirectory(DirectoryTarget::TAS, ui->tas_path_edit); });

    LoadConfiguration();
}

ConfigureTasDialog::~ConfigureTasDialog() = default;

void ConfigureTasDialog::LoadConfiguration() {
    ui->tas_path_edit->setText(
        QString::fromStdString(Common::FS::GetYuzuPathString(Common::FS::YuzuPath::TASDir)));
    ui->tas_enable->setChecked(Settings::values.tas_enable.GetValue());
    ui->tas_control_swap->setChecked(Settings::values.tas_swap_controllers.GetValue());
    ui->tas_loop_script->setChecked(Settings::values.tas_loop.GetValue());
    ui->tas_pause_on_load->setChecked(Settings::values.pause_tas_on_load.GetValue());
}

void ConfigureTasDialog::ApplyConfiguration() {
    Common::FS::SetYuzuPath(Common::FS::YuzuPath::TASDir, ui->tas_path_edit->text().toStdString());
    Settings::values.tas_enable.SetValue(ui->tas_enable->isChecked());
    Settings::values.tas_swap_controllers.SetValue(ui->tas_control_swap->isChecked());
    Settings::values.tas_loop.SetValue(ui->tas_loop_script->isChecked());
    Settings::values.pause_tas_on_load.SetValue(ui->tas_pause_on_load->isChecked());
}

void ConfigureTasDialog::SetDirectory(DirectoryTarget target, QLineEdit* edit) {
    QString caption;

    switch (target) {
    case DirectoryTarget::TAS:
        caption = tr("Select TAS Load Directory...");
        break;
    }

    QString str = QFileDialog::getExistingDirectory(this, caption, edit->text());

    if (str.isNull() || str.isEmpty()) {
        return;
    }

    if (str.back() != QChar::fromLatin1('/')) {
        str.append(QChar::fromLatin1('/'));
    }

    edit->setText(str);
}

void ConfigureTasDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigureTasDialog::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureTasDialog::HandleApplyButtonClicked() {
    UISettings::values.configuration_applied = true;
    ApplyConfiguration();
}
