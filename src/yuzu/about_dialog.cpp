// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QIcon>
#include <fmt/format.h>
#include "common/scm_rev.h"
#include "ui_aboutdialog.h"
#include "yuzu/about_dialog.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent), ui(std::make_unique<Ui::AboutDialog>()) {
    const auto build_id = std::string(Common::g_build_id);
    const auto fmt = std::string(Common::g_title_bar_format_idle);
    const auto yuzu_build_version =
        fmt::format(fmt.empty() ? "yuzu Development Build" : fmt, std::string{}, std::string{},
                    std::string{}, std::string{}, std::string{}, build_id);

    ui->setupUi(this);
    ui->labelLogo->setPixmap(QIcon::fromTheme(QStringLiteral("yuzu")).pixmap(200));
    ui->labelBuildInfo->setText(ui->labelBuildInfo->text().arg(
        QString::fromStdString(yuzu_build_version), QString::fromUtf8(Common::g_scm_branch),
        QString::fromUtf8(Common::g_scm_desc), QString::fromUtf8(Common::g_build_date).left(10)));
}

AboutDialog::~AboutDialog() = default;
