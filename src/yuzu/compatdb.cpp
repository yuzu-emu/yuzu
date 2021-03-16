// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QButtonGroup>
#include <QMessageBox>
#include <QPushButton>
#include <QtConcurrent/qtconcurrentrun.h>
#include "common/logging/log.h"
#include "common/telemetry.h"
#include "core/core.h"
#include "core/telemetry_session.h"
#include "ui_compatdb.h"
#include "yuzu/compatdb.h"

CompatDB::CompatDB(QWidget* parent)
    : QWizard(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::CompatDB>()} {
    ui->setupUi(this);
    connect(ui->radioButton_GameBoot_Yes, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_GameBoot_No, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Gameplay_Yes, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Gameplay_No, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Freeze_Yes, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Freeze_No, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Graphical_Yes, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Graphical_No, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Audio_Yes, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Audio_No, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Complete_Yes, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Complete_No, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(button(NextButton), &QPushButton::clicked, this, &CompatDB::Submit);
    connect(&testcase_watcher, &QFutureWatcher<bool>::finished, this,
            &CompatDB::OnTestcaseSubmitted);
}

CompatDB::~CompatDB() = default;

enum class CompatDBPage {
    Intro = 0,
    GameBoot = 1,
    GamePlay = 2,
    Freeze = 3,
    FreezeSelection = 4,
    Graphical = 5,
    GraphicalSelection = 6,
    Audio = 7,
    AudioSelection = 8,
    Complete = 9,
    Final = 10,
};

void CompatDB::Submit() {
    QButtonGroup* compatibility_GameBoot = new QButtonGroup(this);
    compatibility_GameBoot->addButton(ui->radioButton_GameBoot_Yes, 0);
    compatibility_GameBoot->addButton(ui->radioButton_GameBoot_No, 1);

    QButtonGroup* compatibility_Gameplay = new QButtonGroup(this);
    compatibility_Gameplay->addButton(ui->radioButton_Gameplay_Yes, 0);
    compatibility_Gameplay->addButton(ui->radioButton_Gameplay_No, 1);

    QButtonGroup* compatibility_Freeze = new QButtonGroup(this);
    compatibility_Freeze->addButton(ui->radioButton_Freeze_Yes, 0);
    compatibility_Freeze->addButton(ui->radioButton_Freeze_No, 1);

    QButtonGroup* compatibility_Graphical = new QButtonGroup(this);
    compatibility_Graphical->addButton(ui->radioButton_Graphical_Yes, 0);
    compatibility_Graphical->addButton(ui->radioButton_Graphical_No, 1);

    QButtonGroup* compatibility_Audio = new QButtonGroup(this);
    compatibility_Audio->addButton(ui->radioButton_Audio_Yes, 0);
    compatibility_Audio->addButton(ui->radioButton_Audio_No, 1);

    QButtonGroup* compatibility_Complete = new QButtonGroup(this);
    compatibility_Complete->addButton(ui->radioButton_Complete_Yes, 0);
    compatibility_Complete->addButton(ui->radioButton_Complete_No, 1);

    const int compatiblity = static_cast<int>(CalculateCompatibility());

    switch ((static_cast<CompatDBPage>(currentId()))) {
    case CompatDBPage::Intro:
        break;
    case CompatDBPage::GameBoot:
        if (compatibility_GameBoot->checkedId() == -1) {
            button(NextButton)->setEnabled(false);
        }
        break;
    case CompatDBPage::GamePlay:
        if (compatibility_Gameplay->checkedId() == -1) {
            button(NextButton)->setEnabled(false);
        }
        break;
    case CompatDBPage::Freeze:
        if (compatibility_Freeze->checkedId() == -1) {
            button(NextButton)->setEnabled(false);
        }
        break;
    case CompatDBPage::FreezeSelection:
        break;
    case CompatDBPage::Graphical:
        if (compatibility_Graphical->checkedId() == -1) {
            button(NextButton)->setEnabled(false);
        }
        break;
    case CompatDBPage::GraphicalSelection:
        break;
    case CompatDBPage::Audio:
        if (compatibility_Audio->checkedId() == -1) {
            button(NextButton)->setEnabled(false);
        }
        break;
    case CompatDBPage::AudioSelection:
        break;
    case CompatDBPage::Complete:
        if (compatibility_Complete->checkedId() == -1) {
            button(NextButton)->setEnabled(false);
        }
        break;
    case CompatDBPage::Final:
        back();
        // TODO: Use checkboxes to submit aditional data
        LOG_DEBUG(Frontend, "Compatibility Rating: {}", compatiblity);
        Core::System::GetInstance().TelemetrySession().AddField(
            Common::Telemetry::FieldType::UserFeedback, "Compatibility", compatiblity);

        button(NextButton)->setEnabled(false);
        button(NextButton)->setText(tr("Submitting"));
        button(CancelButton)->setVisible(false);

        testcase_watcher.setFuture(QtConcurrent::run(
            [] { return Core::System::GetInstance().TelemetrySession().SubmitTestcase(); }));
        break;
    default:
        LOG_ERROR(Frontend, "Unexpected page: {}", currentId());
    }
}

int CompatDB::nextId() const {
    switch ((static_cast<CompatDBPage>(currentId()))) {
    case CompatDBPage::Intro:
        return static_cast<int>(CompatDBPage::GameBoot);
    case CompatDBPage::GameBoot:
        if (ui->radioButton_GameBoot_No->isChecked()) {
            return static_cast<int>(CompatDBPage::Final);
        }
        return static_cast<int>(CompatDBPage::GamePlay);
    case CompatDBPage::GamePlay:
        if (ui->radioButton_Gameplay_No->isChecked()) {
            return static_cast<int>(CompatDBPage::Final);
        }
        return static_cast<int>(CompatDBPage::Freeze);
    case CompatDBPage::Freeze:
        if (ui->radioButton_Freeze_Yes->isChecked()) {
            return static_cast<int>(CompatDBPage::FreezeSelection);
        }
        return static_cast<int>(CompatDBPage::Graphical);
    case CompatDBPage::FreezeSelection:
        return static_cast<int>(CompatDBPage::Final);
    case CompatDBPage::Graphical:
        if (ui->radioButton_Graphical_Yes->isChecked()) {
            return static_cast<int>(CompatDBPage::GraphicalSelection);
        }
        return static_cast<int>(CompatDBPage::Audio);
    case CompatDBPage::GraphicalSelection:
        return static_cast<int>(CompatDBPage::Audio);
    case CompatDBPage::Audio:
        if (ui->radioButton_Audio_Yes->isChecked()) {
            return static_cast<int>(CompatDBPage::AudioSelection);
        }
        return static_cast<int>(CompatDBPage::Complete);
    case CompatDBPage::AudioSelection:
        return static_cast<int>(CompatDBPage::Complete);
    case CompatDBPage::Complete:
        return static_cast<int>(CompatDBPage::Final);
    case CompatDBPage::Final:
        return static_cast<int>(CompatDBPage::Final);
    default:
        LOG_ERROR(Frontend, "Unexpected page: {}", currentId());
        return static_cast<int>(CompatDBPage::Intro);
    }
}

CompatibilityStatus CompatDB::CalculateCompatibility() const {
    int score = 0;

    if (ui->radioButton_GameBoot_No->isChecked()) {
        return CompatibilityStatus::WontBoot;
    }

    if (ui->radioButton_Gameplay_No->isChecked()) {
        return CompatibilityStatus::IntroMenu;
    }

    if (ui->radioButton_Freeze_Yes->isChecked()) {
        return CompatibilityStatus::Bad;
    }

    if (ui->radioButton_Graphical_Yes->isChecked()) {
        score++;
        if (ui->checkbox_Graphical_missing->isChecked()) {
            score += 3;
        }
        if (ui->checkbox_Graphical_corrupted->isChecked()) {
            score += 3;
        }
        if (ui->checkbox_Graphical_fliquer->isChecked()) {
            score += 2;
        }
        if (ui->checkbox_Graphical_wrong_orientation->isChecked()) {
            score++;
        }
        if (ui->checkbox_Graphical_wrong_colors->isChecked()) {
            score++;
        }
        if (ui->checkbox_Graphical_wrong_animation->isChecked()) {
            score++;
        }
        if (ui->checkbox_Graphical_overlay->isChecked()) {
            score++;
        }
    }

    if (ui->radioButton_Audio_Yes->isChecked()) {
        score++;
        if (ui->checkbox_Audio_faster->isChecked()) {
            score++;
        }
        if (ui->checkbox_Audio_slower->isChecked()) {
            score++;
        }
        if (ui->checkbox_Audio_loud->isChecked()) {
            score++;
        }
        if (ui->checkbox_Audio_quiet->isChecked()) {
            score++;
        }
        if (ui->checkbox_Freeze_Audio_corrupted->isChecked()) {
            score += 2;
        }
        if (ui->checkbox_Freeze_Audio_missing->isChecked()) {
            score += 2;
        }
    }

    if (ui->radioButton_Complete_No->isChecked()) {
        score++;
    }

    // The lower the score the better
    if (score == 0) {
        return CompatibilityStatus::Perfect;
    }
    if (score < 5) {
        return CompatibilityStatus::Great;
    }
    if (score < 10) {
        return CompatibilityStatus::Okay;
    }
    return CompatibilityStatus::Bad;
}

void CompatDB::OnTestcaseSubmitted() {
    if (!testcase_watcher.result()) {
        QMessageBox::critical(this, tr("Communication error"),
                              tr("An error occurred while sending the Testcase"));
        button(NextButton)->setEnabled(true);
        button(NextButton)->setText(tr("Next"));
        button(CancelButton)->setVisible(true);
    } else {
        next();
        // older versions of QT don't support the "NoCancelButtonOnLastPage" option, this is a
        // workaround
        button(CancelButton)->setVisible(false);
    }
}

void CompatDB::EnableNext() {
    button(NextButton)->setEnabled(true);
}
