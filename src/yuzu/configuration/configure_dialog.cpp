// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QHash>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure.h"
#include "yuzu/configuration/config.h"
#include "yuzu/configuration/configure_dialog.h"
#include "yuzu/configuration/configure_input_player.h"
#include "yuzu/hotkeys.h"

ConfigureDialog::ConfigureDialog(QWidget* parent, HotkeyRegistry& registry,
                                 InputCommon::InputSubsystem* input_subsystem)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigureDialog>()), registry(registry) {
    Settings::SetConfiguringGlobal(true);

    ui->setupUi(this);
    ui->hotkeysTab->Populate(registry);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->inputTab->Initialize(input_subsystem);

    SetConfiguration();
    PopulateSelectionList();

    connect(ui->uiTab, &ConfigureUi::LanguageChanged, this, &ConfigureDialog::OnLanguageChanged);
    connect(ui->selectorList, &QListWidget::itemSelectionChanged, this,
            &ConfigureDialog::UpdateVisibleTabs);

    adjustSize();
    ui->selectorList->setCurrentRow(0);
}

ConfigureDialog::~ConfigureDialog() = default;

void ConfigureDialog::SetConfiguration() {}

void ConfigureDialog::ApplyConfiguration() {
    ui->generalTab->ApplyConfiguration();
    ui->uiTab->ApplyConfiguration();
    ui->systemTab->ApplyConfiguration();
    ui->profileManagerTab->ApplyConfiguration();
    ui->filesystemTab->applyConfiguration();
    ui->inputTab->ApplyConfiguration();
    ui->hotkeysTab->ApplyConfiguration(registry);
    ui->cpuTab->ApplyConfiguration();
    ui->cpuDebugTab->ApplyConfiguration();
    ui->graphicsTab->ApplyConfiguration();
    ui->graphicsAdvancedTab->ApplyConfiguration();
    ui->audioTab->ApplyConfiguration();
    ui->debugTab->ApplyConfiguration();
    ui->webTab->ApplyConfiguration();
    ui->serviceTab->ApplyConfiguration();
    Settings::Apply(Core::System::GetInstance());
    Settings::LogSettings();
}

void ConfigureDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigureDialog::RetranslateUI() {
    const int old_row = ui->selectorList->currentRow();
    const int old_index = ui->tabWidget->currentIndex();

    ui->retranslateUi(this);

    PopulateSelectionList();
    ui->selectorList->setCurrentRow(old_row);

    UpdateVisibleTabs();
    ui->tabWidget->setCurrentIndex(old_index);
}

Q_DECLARE_METATYPE(QList<QWidget*>);

void ConfigureDialog::PopulateSelectionList() {
    const std::array<std::pair<QString, QList<QWidget*>>, 6> items{
        {{tr("General"), {ui->generalTab, ui->hotkeysTab, ui->uiTab, ui->webTab, ui->debugTab}},
         {tr("System"), {ui->systemTab, ui->profileManagerTab, ui->serviceTab, ui->filesystemTab}},
         {tr("CPU"), {ui->cpuTab, ui->cpuDebugTab}},
         {tr("Graphics"), {ui->graphicsTab, ui->graphicsAdvancedTab}},
         {tr("Audio"), {ui->audioTab}},
         {tr("Controls"), ui->inputTab->GetSubTabs()}},
    };

    [[maybe_unused]] const QSignalBlocker blocker(ui->selectorList);

    ui->selectorList->clear();
    for (const auto& entry : items) {
        auto* const item = new QListWidgetItem(entry.first);
        item->setData(Qt::UserRole, QVariant::fromValue(entry.second));

        ui->selectorList->addItem(item);
    }
}

void ConfigureDialog::OnLanguageChanged(const QString& locale) {
    emit LanguageChanged(locale);
    // first apply the configuration, and then restore the display
    ApplyConfiguration();
    RetranslateUI();
    SetConfiguration();
}

void ConfigureDialog::UpdateVisibleTabs() {
    const auto items = ui->selectorList->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    const std::map<QWidget*, QString> widgets = {
        {ui->generalTab, tr("General")},
        {ui->systemTab, tr("System")},
        {ui->profileManagerTab, tr("Profiles")},
        {ui->inputTab, tr("Controls")},
        {ui->hotkeysTab, tr("Hotkeys")},
        {ui->cpuTab, tr("CPU")},
        {ui->cpuDebugTab, tr("Debug")},
        {ui->graphicsTab, tr("Graphics")},
        {ui->graphicsAdvancedTab, tr("Advanced")},
        {ui->audioTab, tr("Audio")},
        {ui->debugTab, tr("Debug")},
        {ui->webTab, tr("Web")},
        {ui->uiTab, tr("UI")},
        {ui->filesystemTab, tr("Filesystem")},
        {ui->serviceTab, tr("Services")},
    };

    [[maybe_unused]] const QSignalBlocker blocker(ui->tabWidget);

    ui->tabWidget->clear();

    const QList<QWidget*> tabs = qvariant_cast<QList<QWidget*>>(items[0]->data(Qt::UserRole));

    for (const auto tab : tabs) {
        ui->tabWidget->addTab(tab, tab->accessibleName());
    }
}
