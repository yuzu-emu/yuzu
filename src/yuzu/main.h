// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include <QMainWindow>
#include <QTimer>
#include <QTranslator>

#include "common/common_types.h"
#include "core/core.h"
#include "core/hle/service/acc/profile_manager.h"
#include "ui_main.h"
#include "yuzu/compatibility_list.h"
#include "yuzu/hotkeys.h"

class Config;
class EmuThread;
class GameList;
class GImageInfo;
class GRenderWindow;
class LoadingScreen;
class MicroProfileDialog;
class ProfilerWidget;
class ControllerDialog;
class QLabel;
class QPushButton;
class QProgressDialog;
class WaitTreeWidget;
enum class GameListOpenTarget;
enum class GameListRemoveTarget;
enum class DumpRomFSTarget;
enum class InstalledEntryType;
class GameListPlaceholder;

class QtSoftwareKeyboardDialog;

enum class StartGameType {
    Normal, // Can use custom configuration
    Global, // Only uses global configuration
};

namespace Core::Frontend {
struct ControllerParameters;
struct InlineAppearParameters;
struct InlineTextParameters;
struct KeyboardInitializeParameters;
} // namespace Core::Frontend

namespace DiscordRPC {
class DiscordInterface;
}

namespace FileSys {
class ContentProvider;
class ManualContentProvider;
class VfsFilesystem;
} // namespace FileSys

namespace InputCommon {
class InputSubsystem;
}

namespace Service::AM::Applets {
enum class SwkbdResult : u32;
enum class SwkbdTextCheckResult : u32;
enum class SwkbdReplyType : u32;
enum class WebExitReason : u32;
} // namespace Service::AM::Applets

enum class EmulatedDirectoryTarget {
    NAND,
    SDMC,
};

enum class InstallResult {
    Success,
    Overwrite,
    Failure,
    BaseInstallAttempted,
};

enum class ReinitializeKeyBehavior {
    NoWarning,
    Warning,
};

class GMainWindow : public QMainWindow {
    Q_OBJECT

    /// Max number of recently loaded items to keep track of
    static const int max_recent_files_item = 10;

    // TODO: Make use of this!
    enum {
        UI_IDLE,
        UI_EMU_BOOTING,
        UI_EMU_RUNNING,
        UI_EMU_STOPPING,
    };

public:
    void filterBarSetChecked(bool state);
    void UpdateUITheme();
    GMainWindow();
    ~GMainWindow() override;

    bool DropAction(QDropEvent* event);
    void AcceptDropEvent(QDropEvent* event);

signals:

    /**
     * Signal that is emitted when a new EmuThread has been created and an emulation session is
     * about to start. At this time, the core system emulation has been initialized, and all
     * emulation handles and memory should be valid.
     *
     * @param emu_thread Pointer to the newly created EmuThread (to be used by widgets that need to
     *      access/change emulation state).
     */
    void EmulationStarting(EmuThread* emu_thread);

    /**
     * Signal that is emitted when emulation is about to stop. At this time, the EmuThread and core
     * system emulation handles and memory are still valid, but are about become invalid.
     */
    void EmulationStopping();

    // Signal that tells widgets to update icons to use the current theme
    void UpdateThemedIcons();

    void UpdateInstallProgress();

    void ControllerSelectorReconfigureFinished();

    void ErrorDisplayFinished();

    void ProfileSelectorFinishedSelection(std::optional<Common::UUID> uuid);

    void SoftwareKeyboardSubmitNormalText(Service::AM::Applets::SwkbdResult result,
                                          std::u16string submitted_text);
    void SoftwareKeyboardSubmitInlineText(Service::AM::Applets::SwkbdReplyType reply_type,
                                          std::u16string submitted_text, s32 cursor_position);

    void WebBrowserExtractOfflineRomFS();
    void WebBrowserClosed(Service::AM::Applets::WebExitReason exit_reason, std::string last_url);

public slots:
    void OnLoadComplete();
    void OnExecuteProgram(std::size_t program_index);
    void ControllerSelectorReconfigureControllers(
        const Core::Frontend::ControllerParameters& parameters);
    void SoftwareKeyboardInitialize(
        bool is_inline, Core::Frontend::KeyboardInitializeParameters initialize_parameters);
    void SoftwareKeyboardShowNormal();
    void SoftwareKeyboardShowTextCheck(Service::AM::Applets::SwkbdTextCheckResult text_check_result,
                                       std::u16string text_check_message);
    void SoftwareKeyboardShowInline(Core::Frontend::InlineAppearParameters appear_parameters);
    void SoftwareKeyboardHideInline();
    void SoftwareKeyboardInlineTextChanged(Core::Frontend::InlineTextParameters text_parameters);
    void SoftwareKeyboardExit();
    void ErrorDisplayDisplayError(QString error_code, QString error_text);
    void ProfileSelectorSelectProfile();
    void WebBrowserOpenWebPage(const std::string& main_url, const std::string& additional_args,
                               bool is_local);
    void OnAppFocusStateChanged(Qt::ApplicationState state);

private:
    void RegisterMetaTypes();

    void InitializeWidgets();
    void InitializeDebugWidgets();
    void InitializeRecentFileMenuActions();

    void SetDefaultUIGeometry();
    void RestoreUIState();

    void ConnectWidgetEvents();
    void ConnectMenuEvents();

    void PreventOSSleep();
    void AllowOSSleep();

    bool LoadROM(const QString& filename, std::size_t program_index);
    void BootGame(const QString& filename, std::size_t program_index = 0,
                  StartGameType with_config = StartGameType::Normal);
    void ShutdownGame();

    void ShowTelemetryCallout();
    void SetDiscordEnabled(bool state);
    void LoadAmiibo(const QString& filename);

    void SelectAndSetCurrentUser();

    /**
     * Stores the filename in the recently loaded files list.
     * The new filename is stored at the beginning of the recently loaded files list.
     * After inserting the new entry, duplicates are removed meaning that if
     * this was inserted from \a OnMenuRecentFile(), the entry will be put on top
     * and remove from its previous position.
     *
     * Finally, this function calls \a UpdateRecentFiles() to update the UI.
     *
     * @param filename the filename to store
     */
    void StoreRecentFile(const QString& filename);

    /**
     * Updates the recent files menu.
     * Menu entries are rebuilt from the configuration file.
     * If there is no entry in the menu, the menu is greyed out.
     */
    void UpdateRecentFiles();

    /**
     * If the emulation is running,
     * asks the user if he really want to close the emulator
     *
     * @return true if the user confirmed
     */
    bool ConfirmClose();
    bool ConfirmChangeGame();
    bool ConfirmForceLockedExit();
    void RequestGameExit();
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnStartGame();
    void OnPauseGame();
    void OnStopGame();
    void OnMenuReportCompatibility();
    void OnOpenModsPage();
    void OnOpenQuickstartGuide();
    void OnOpenFAQ();
    /// Called whenever a user selects a game in the game list widget.
    void OnGameListLoadFile(QString game_path);
    void OnGameListOpenFolder(u64 program_id, GameListOpenTarget target,
                              const std::string& game_path);
    void OnTransferableShaderCacheOpenFile(u64 program_id);
    void OnGameListRemoveInstalledEntry(u64 program_id, InstalledEntryType type);
    void OnGameListRemoveFile(u64 program_id, GameListRemoveTarget target,
                              const std::string& game_path);
    void OnGameListDumpRomFS(u64 program_id, const std::string& game_path, DumpRomFSTarget target);
    void OnGameListCopyTID(u64 program_id);
    void OnGameListNavigateToGamedbEntry(u64 program_id,
                                         const CompatibilityList& compatibility_list);
    void OnGameListOpenDirectory(const QString& directory);
    void OnGameListAddDirectory();
    void OnGameListShowList(bool show);
    void OnGameListOpenPerGameProperties(const std::string& file);
    void OnMenuLoadFile();
    void OnMenuLoadFolder();
    void IncrementInstallProgress();
    void OnMenuInstallToNAND();
    void OnMenuRecentFile();
    void OnConfigure();
    void OnConfigurePerGame();
    void OnLoadAmiibo();
    void OnOpenYuzuFolder();
    void OnAbout();
    void OnToggleFilterBar();
    void OnDisplayTitleBars(bool);
    void InitializeHotkeys();
    void ToggleFullscreen();
    void ShowFullscreen();
    void HideFullscreen();
    void ToggleWindowMode();
    void ResetWindowSize720();
    void ResetWindowSize1080();
    void OnCaptureScreenshot();
    void OnCoreError(Core::System::ResultStatus, std::string);
    void OnReinitializeKeys(ReinitializeKeyBehavior behavior);
    void OnLanguageChanged(const QString& locale);
    void OnMouseActivity();

private:
    void RemoveBaseContent(u64 program_id, const QString& entry_type);
    void RemoveUpdateContent(u64 program_id, const QString& entry_type);
    void RemoveAddOnContent(u64 program_id, const QString& entry_type);
    void RemoveTransferableShaderCache(u64 program_id);
    void RemoveCustomConfiguration(u64 program_id, const std::string& game_path);
    std::optional<u64> SelectRomFSDumpTarget(const FileSys::ContentProvider&, u64 program_id);
    InstallResult InstallNSPXCI(const QString& filename);
    InstallResult InstallNCA(const QString& filename);
    void MigrateConfigFiles();
    void UpdateWindowTitle(std::string_view title_name = {}, std::string_view title_version = {},
                           std::string_view gpu_vendor = {});
    void UpdateStatusBar();
    void UpdateGPUAccuracyButton();
    void UpdateStatusButtons();
    void UpdateUISettings();
    void HideMouseCursor();
    void ShowMouseCursor();
    void OpenURL(const QUrl& url);
    void LoadTranslation();
    void OpenPerGameConfiguration(u64 title_id, const std::string& file_name);

    Ui::MainWindow ui;

    std::unique_ptr<DiscordRPC::DiscordInterface> discord_rpc;
    std::shared_ptr<InputCommon::InputSubsystem> input_subsystem;

    GRenderWindow* render_window;
    GameList* game_list;
    LoadingScreen* loading_screen;

    GameListPlaceholder* game_list_placeholder;

    // Status bar elements
    QLabel* message_label = nullptr;
    QLabel* shader_building_label = nullptr;
    QLabel* emu_speed_label = nullptr;
    QLabel* game_fps_label = nullptr;
    QLabel* emu_frametime_label = nullptr;
    QPushButton* gpu_accuracy_button = nullptr;
    QPushButton* renderer_status_button = nullptr;
    QPushButton* dock_status_button = nullptr;
    QTimer status_bar_update_timer;

    std::unique_ptr<Config> config;

    // Whether emulation is currently running in yuzu.
    bool emulation_running = false;
    std::unique_ptr<EmuThread> emu_thread;
    // The path to the game currently running
    QString game_path;

    bool auto_paused = false;
    QTimer mouse_hide_timer;

    // FS
    std::shared_ptr<FileSys::VfsFilesystem> vfs;
    std::unique_ptr<FileSys::ManualContentProvider> provider;

    // Debugger panes
    ProfilerWidget* profilerWidget;
    MicroProfileDialog* microProfileDialog;
    WaitTreeWidget* waitTreeWidget;
    ControllerDialog* controller_dialog;

    QAction* actions_recent_files[max_recent_files_item];

    // stores default icon theme search paths for the platform
    QStringList default_theme_paths;

    HotkeyRegistry hotkey_registry;

    QTranslator translator;

    // Install progress dialog
    QProgressDialog* install_progress;

    // Last game booted, used for multi-process apps
    QString last_filename_booted;

    // Disables the web applet for the rest of the emulated session
    bool disable_web_applet{};

    // Applets
    QtSoftwareKeyboardDialog* software_keyboard = nullptr;

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
};
