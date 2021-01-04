// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>
#include "common/assert.h"
#include "common/file_util.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/settings.h"
#include "ui_configure_profile_manager.h"
#include "yuzu/configuration/configure_profile_manager.h"
#include "yuzu/util/limitable_input_dialog.h"

namespace {
// Same backup JPEG used by acc IProfile::GetImage if no jpeg found
constexpr std::array<u8, 107> backup_jpeg{
    0xff, 0xd8, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x06, 0x06, 0x05,
    0x06, 0x09, 0x08, 0x0a, 0x0a, 0x09, 0x08, 0x09, 0x09, 0x0a, 0x0c, 0x0f, 0x0c, 0x0a, 0x0b, 0x0e,
    0x0b, 0x09, 0x09, 0x0d, 0x11, 0x0d, 0x0e, 0x0f, 0x10, 0x10, 0x11, 0x10, 0x0a, 0x0c, 0x12, 0x13,
    0x12, 0x10, 0x13, 0x0f, 0x10, 0x10, 0x10, 0xff, 0xc9, 0x00, 0x0b, 0x08, 0x00, 0x01, 0x00, 0x01,
    0x01, 0x01, 0x11, 0x00, 0xff, 0xcc, 0x00, 0x06, 0x00, 0x10, 0x10, 0x05, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x00, 0x3f, 0x00, 0xd2, 0xcf, 0x20, 0xff, 0xd9,
};

QString GetImagePath(Common::UUID uuid) {
    const auto path = Common::FS::GetUserPath(Common::FS::UserPath::NANDDir) +
                      "/system/save/8000000000000010/su/avators/" + uuid.FormatSwitch() + ".jpg";
    return QString::fromStdString(path);
}

QString GetAccountUsername(const Service::Account::ProfileManager& manager, Common::UUID uuid) {
    Service::Account::ProfileBase profile;
    if (!manager.GetProfileBase(uuid, profile)) {
        return {};
    }

    const auto text = Common::StringFromFixedZeroTerminatedBuffer(
        reinterpret_cast<const char*>(profile.username.data()), profile.username.size());
    return QString::fromStdString(text);
}

QString FormatUserEntryText(const QString& username, Common::UUID uuid) {
    return ConfigureProfileManager::tr("%1\n%2",
                                       "%1 is the profile username, %2 is the formatted UUID (e.g. "
                                       "00112233-4455-6677-8899-AABBCCDDEEFF))")
        .arg(username, QString::fromStdString(uuid.FormatSwitch()));
}

QPixmap GetIcon(Common::UUID uuid) {
    QPixmap icon{GetImagePath(uuid)};

    if (!icon) {
        icon.fill(Qt::black);
        icon.loadFromData(backup_jpeg.data(), static_cast<u32>(backup_jpeg.size()));
    }

    return icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QString GetProfileUsernameFromUser(QWidget* parent, const QString& description_text) {
    return LimitableInputDialog::GetText(parent, ConfigureProfileManager::tr("Enter Username"),
                                         description_text, 1,
                                         static_cast<int>(Service::Account::profile_username_size));
}
} // Anonymous namespace

ConfigureProfileManager ::ConfigureProfileManager(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureProfileManager>()),
      profile_manager(std::make_unique<Service::Account::ProfileManager>()) {
    ui->setupUi(this);

    tree_view = new QTreeView;
    item_model = new QStandardItemModel(tree_view);
    item_model->insertColumns(0, 1);
    tree_view->setModel(item_model);
    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setUniformRowHeights(true);
    tree_view->setIconSize({64, 64});
    tree_view->setContextMenuPolicy(Qt::NoContextMenu);

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);

    ui->scrollArea->setLayout(layout);

    connect(tree_view, &QTreeView::clicked, this, &ConfigureProfileManager::SelectUser);

    connect(ui->pm_add, &QPushButton::clicked, this, &ConfigureProfileManager::AddUser);
    connect(ui->pm_rename, &QPushButton::clicked, this, &ConfigureProfileManager::RenameUser);
    connect(ui->pm_remove, &QPushButton::clicked, this, &ConfigureProfileManager::DeleteUser);
    connect(ui->pm_set_image, &QPushButton::clicked, this, &ConfigureProfileManager::SetUserImage);

    scene = new QGraphicsScene;
    ui->current_user_icon->setScene(scene);

    SetConfiguration();
    RetranslateUI();
}

ConfigureProfileManager::~ConfigureProfileManager() = default;

void ConfigureProfileManager::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureProfileManager::RetranslateUI() {
    ui->retranslateUi(this);
    item_model->setHeaderData(0, Qt::Horizontal, tr("Users"));
}

void ConfigureProfileManager::SetConfiguration() {
    enabled = !Core::System::GetInstance().IsPoweredOn();
    item_model->removeRows(0, item_model->rowCount());
    list_items.clear();

    PopulateUserList();
    UpdateCurrentUser();
}

void ConfigureProfileManager::PopulateUserList() {
    const auto& profiles = profile_manager->GetAllUsers();
    for (const auto& user : profiles) {
        Service::Account::ProfileBase profile;
        if (!profile_manager->GetProfileBase(user, profile))
            continue;

        const auto username = Common::StringFromFixedZeroTerminatedBuffer(
            reinterpret_cast<const char*>(profile.username.data()), profile.username.size());

        list_items.push_back(QList<QStandardItem*>{new QStandardItem{
            GetIcon(user), FormatUserEntryText(QString::fromStdString(username), user)}});
    }

    for (const auto& item : list_items)
        item_model->appendRow(item);
}

void ConfigureProfileManager::UpdateCurrentUser() {
    ui->pm_add->setEnabled(profile_manager->GetUserCount() < Service::Account::MAX_USERS);

    const auto& current_user = profile_manager->GetUser(Settings::values.current_user);
    ASSERT(current_user);
    const auto username = GetAccountUsername(*profile_manager, *current_user);

    scene->clear();
    scene->addPixmap(
        GetIcon(*current_user).scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    ui->current_user_username->setText(username);
}

void ConfigureProfileManager::ApplyConfiguration() {
    if (!enabled) {
        return;
    }

    Settings::Apply(Core::System::GetInstance());
}

void ConfigureProfileManager::SelectUser(const QModelIndex& index) {
    Settings::values.current_user =
        std::clamp<s32>(index.row(), 0, static_cast<s32>(profile_manager->GetUserCount() - 1));

    UpdateCurrentUser();

    ui->pm_remove->setEnabled(profile_manager->GetUserCount() >= 2);
    ui->pm_rename->setEnabled(true);
    ui->pm_set_image->setEnabled(true);
}

void ConfigureProfileManager::AddUser() {
    const auto username =
        GetProfileUsernameFromUser(this, tr("Enter a username for the new user:"));
    if (username.isEmpty()) {
        return;
    }

    const auto uuid = Common::UUID::Generate();
    profile_manager->CreateNewUser(uuid, username.toStdString());

    item_model->appendRow(new QStandardItem{GetIcon(uuid), FormatUserEntryText(username, uuid)});
}

void ConfigureProfileManager::RenameUser() {
    const auto user = tree_view->currentIndex().row();
    const auto uuid = profile_manager->GetUser(user);
    ASSERT(uuid);

    Service::Account::ProfileBase profile;
    if (!profile_manager->GetProfileBase(*uuid, profile))
        return;

    const auto new_username = GetProfileUsernameFromUser(this, tr("Enter a new username:"));
    if (new_username.isEmpty()) {
        return;
    }

    const auto username_std = new_username.toStdString();
    std::fill(profile.username.begin(), profile.username.end(), '\0');
    std::copy(username_std.begin(), username_std.end(), profile.username.begin());

    profile_manager->SetProfileBase(*uuid, profile);

    item_model->setItem(
        user, 0,
        new QStandardItem{GetIcon(*uuid),
                          FormatUserEntryText(QString::fromStdString(username_std), *uuid)});
    UpdateCurrentUser();
}

void ConfigureProfileManager::DeleteUser() {
    const auto index = tree_view->currentIndex().row();
    const auto uuid = profile_manager->GetUser(index);
    ASSERT(uuid);
    const auto username = GetAccountUsername(*profile_manager, *uuid);

    const auto confirm = QMessageBox::question(
        this, tr("Confirm Delete"),
        tr("You are about to delete user with name \"%1\". Are you sure?").arg(username));

    if (confirm == QMessageBox::No)
        return;

    if (Settings::values.current_user == tree_view->currentIndex().row())
        Settings::values.current_user = 0;
    UpdateCurrentUser();

    if (!profile_manager->RemoveUser(*uuid))
        return;

    item_model->removeRows(tree_view->currentIndex().row(), 1);
    tree_view->clearSelection();

    ui->pm_remove->setEnabled(false);
    ui->pm_rename->setEnabled(false);
}

void ConfigureProfileManager::SetUserImage() {
    const auto index = tree_view->currentIndex().row();
    const auto uuid = profile_manager->GetUser(index);
    ASSERT(uuid);

    const auto file = QFileDialog::getOpenFileName(this, tr("Select User Image"), QString(),
                                                   tr("JPEG Images (*.jpg *.jpeg)"));

    if (file.isEmpty()) {
        return;
    }

    const auto image_path = GetImagePath(*uuid);
    if (QFile::exists(image_path) && !QFile::remove(image_path)) {
        QMessageBox::warning(
            this, tr("Error deleting image"),
            tr("Error occurred attempting to overwrite previous image at: %1.").arg(image_path));
        return;
    }

    const auto raw_path = QString::fromStdString(
        Common::FS::GetUserPath(Common::FS::UserPath::NANDDir) + "/system/save/8000000000000010");
    const QFileInfo raw_info{raw_path};
    if (raw_info.exists() && !raw_info.isDir() && !QFile::remove(raw_path)) {
        QMessageBox::warning(this, tr("Error deleting file"),
                             tr("Unable to delete existing file: %1.").arg(raw_path));
        return;
    }

    const QString absolute_dst_path = QFileInfo{image_path}.absolutePath();
    if (!QDir{raw_path}.mkpath(absolute_dst_path)) {
        QMessageBox::warning(
            this, tr("Error creating user image directory"),
            tr("Unable to create directory %1 for storing user images.").arg(absolute_dst_path));
        return;
    }

    if (!QFile::copy(file, image_path)) {
        QMessageBox::warning(this, tr("Error copying user image"),
                             tr("Unable to copy image from %1 to %2").arg(file, image_path));
        return;
    }

    const auto username = GetAccountUsername(*profile_manager, *uuid);
    item_model->setItem(index, 0,
                        new QStandardItem{GetIcon(*uuid), FormatUserEntryText(username, *uuid)});
    UpdateCurrentUser();
}
