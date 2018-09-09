// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QModelIndex>
#include <QSettings>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include "common/common_types.h"
#include "yuzu/compatibility_list.h"

class GameListWorker;
class GMainWindow;

namespace FileSys {
class VfsFilesystem;
}

enum class GameListOpenTarget { SaveData };

class GameList : public QWidget {
    Q_OBJECT

public:
    enum {
        COLUMN_NAME,
        COLUMN_COMPATIBILITY,
        COLUMN_ADD_ONS,
        COLUMN_FILE_TYPE,
        COLUMN_SIZE,
        COLUMN_COUNT, // Number of columns
    };

    class SearchField : public QWidget {
    public:
        void setFilterResult(int visible, int total);
        void clear();
        void setFocus();
        explicit SearchField(GameList* parent = nullptr);

    private:
        class KeyReleaseEater : public QObject {
        public:
            explicit KeyReleaseEater(GameList* gamelist);

        private:
            GameList* gamelist = nullptr;
            QString edit_filter_text_old;

        protected:
            bool eventFilter(QObject* obj, QEvent* event) override;
        };
        QHBoxLayout* layout_filter = nullptr;
        QTreeView* tree_view = nullptr;
        QLabel* label_filter = nullptr;
        QLineEdit* edit_filter = nullptr;
        QLabel* label_filter_result = nullptr;
        QToolButton* button_filter_close = nullptr;
    };

    explicit GameList(std::shared_ptr<FileSys::VfsFilesystem> vfs, GMainWindow* parent = nullptr);
    ~GameList() override;

    void clearFilter();
    void setFilterFocus();
    void setFilterVisible(bool visibility);

    void LoadCompatibilityList();
    void PopulateAsync(const QString& dir_path, bool deep_scan);

    void SaveInterfaceLayout();
    void LoadInterfaceLayout();

    static const QStringList supported_file_extensions;

signals:
    void GameChosen(QString game_path);
    void ShouldCancelWorker();
    void OpenFolderRequested(u64 program_id, GameListOpenTarget target);
    void NavigateToGamedbEntryRequested(u64 program_id, CompatibilityList& compatibility_list);

private slots:
    void onTextChanged(const QString& newText);
    void onFilterCloseClicked();

private:
    void AddEntry(const QList<QStandardItem*>& entry_items);
    void ValidateEntry(const QModelIndex& item);
    void DonePopulating(QStringList watch_list);

    void PopupContextMenu(const QPoint& menu_location);
    void RefreshGameDirectory();

    std::shared_ptr<FileSys::VfsFilesystem> vfs;
    SearchField* search_field;
    GMainWindow* main_window = nullptr;
    QVBoxLayout* layout = nullptr;
    QTreeView* tree_view = nullptr;
    QStandardItemModel* item_model = nullptr;
    GameListWorker* current_worker = nullptr;
    QFileSystemWatcher* watcher = nullptr;
    CompatibilityList compatibility_list;
};

Q_DECLARE_METATYPE(GameListOpenTarget);
