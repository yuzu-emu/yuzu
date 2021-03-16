// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QFutureWatcher>
#include <QWizard>

namespace Ui {
class CompatDB;
}

enum class CompatibilityStatus {
    Perfect = 0,
    Great = 1,
    Okay = 2,
    Bad = 3,
    IntroMenu = 4,
    WontBoot = 5,
};

class CompatDB : public QWizard {
    Q_OBJECT

public:
    explicit CompatDB(QWidget* parent = nullptr);
    ~CompatDB();
    int nextId() const override;

private:
    QFutureWatcher<bool> testcase_watcher;

    std::unique_ptr<Ui::CompatDB> ui;

    void Submit();
    CompatibilityStatus CalculateCompatibility() const;
    void OnTestcaseSubmitted();
    void EnableNext();
};
