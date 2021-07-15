// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

namespace Ui {
class ConfigureTas;
}

class ConfigureTasDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureTasDialog(QWidget* parent);
    ~ConfigureTasDialog() override;

    /// Save all button configurations to settings file
    void ApplyConfiguration();

private:
    enum class DirectoryTarget {
        TAS,
    };

    void LoadConfiguration();

    void SetDirectory(DirectoryTarget target, QLineEdit* edit);

    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void HandleApplyButtonClicked();

    std::unique_ptr<Ui::ConfigureTas> ui;
};
