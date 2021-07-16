// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <clocale>
#include <memory>
#include <thread>
#ifdef __APPLE__
#include <unistd.h> // for chdir
#endif

// VFS includes must be before glad as they will conflict with Windows file api, which uses defines.
#include "applets/qt_controller.h"
#include "applets/qt_error.h"
#include "applets/qt_profile_select.h"
#include "applets/qt_software_keyboard.h"
#include "applets/qt_web_browser.h"
#include "common/nvidia_flags.h"
#include "configuration/configure_input.h"
#include "configuration/configure_per_game.h"
#include "configuration/configure_vibration.h"
#include "core/file_sys/vfs.h"
#include "core/file_sys/vfs_real.h"
#include "core/frontend/applets/controller.h"
#include "core/frontend/applets/general_frontend.h"
#include "core/frontend/applets/software_keyboard.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/applet_ae.h"
#include "core/hle/service/am/applet_oe.h"
#include "core/hle/service/am/applets/applets.h"

// These are wrappers to avoid the calls to CreateDirectory and CreateFile because of the Windows
// defines.
static FileSys::VirtualDir VfsFilesystemCreateDirectoryWrapper(
    const FileSys::VirtualFilesystem& vfs, const std::string& path, FileSys::Mode mode) {
    return vfs->CreateDirectory(path, mode);
}

static FileSys::VirtualFile VfsDirectoryCreateFileWrapper(const FileSys::VirtualDir& dir,
                                                          const std::string& path) {
    return dir->CreateFile(path);
}

#include <fmt/ostream.h>
#include <glad/glad.h>

#define QT_NO_OPENGL
#include <QClipboard>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QString>
#include <QSysInfo>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

#include <fmt/format.h>
#include "common/detached_tasks.h"
#include "common/fs/fs.h"
#include "common/fs/fs_paths.h"
#include "common/fs/path_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/memory_detect.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#ifdef ARCHITECTURE_x86_64
#include "common/x64/cpu_detect.h"
#endif
#include "common/settings.h"
#include "common/telemetry.h"
#include "core/core.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/common_funcs.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/savedata_factory.h"
#include "core/file_sys/submission_package.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/nfp/nfp.h"
#include "core/hle/service/sm/sm.h"
#include "core/loader/loader.h"
#include "core/perf_stats.h"
#include "core/telemetry_session.h"
#include "input_common/main.h"
#include "util/overlay_dialog.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"
#include "video_core/shader_notify.h"
#include "yuzu/about_dialog.h"
#include "yuzu/bootmanager.h"
#include "yuzu/compatdb.h"
#include "yuzu/compatibility_list.h"
#include "yuzu/configuration/config.h"
#include "yuzu/configuration/configure_dialog.h"
#include "yuzu/debugger/console.h"
#include "yuzu/debugger/controller.h"
#include "yuzu/debugger/profiler.h"
#include "yuzu/debugger/wait_tree.h"
#include "yuzu/discord.h"
#include "yuzu/game_list.h"
#include "yuzu/game_list_p.h"
#include "yuzu/hotkeys.h"
#include "yuzu/install_dialog.h"
#include "yuzu/loading_screen.h"
#include "yuzu/main.h"
#include "yuzu/uisettings.h"

#ifdef USE_DISCORD_PRESENCE
#include "yuzu/discord_impl.h"
#endif

#ifdef QT_STATICPLUGIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

#ifdef _WIN32
#include <windows.h>
extern "C" {
// tells Nvidia and AMD drivers to use the dedicated GPU by default on laptops with switchable
// graphics
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

constexpr int default_mouse_timeout = 2500;

/**
 * "Callouts" are one-time instructional messages shown to the user. In the config settings, there
 * is a bitfield "callout_flags" options, used to track if a message has already been shown to the
 * user. This is 32-bits - if we have more than 32 callouts, we should retire and recycle old ones.
 */
enum class CalloutFlag : uint32_t {
    Telemetry = 0x1,
    DRDDeprecation = 0x2,
};

void GMainWindow::ShowTelemetryCallout() {
    if (UISettings::values.callout_flags.GetValue() &
        static_cast<uint32_t>(CalloutFlag::Telemetry)) {
        return;
    }

    UISettings::values.callout_flags =
        UISettings::values.callout_flags.GetValue() | static_cast<uint32_t>(CalloutFlag::Telemetry);
    const QString telemetry_message =
        tr("<a href='https://yuzu-emu.org/help/feature/telemetry/'>Anonymous "
           "data is collected</a> to help improve yuzu. "
           "<br/><br/>Would you like to share your usage data with us?");
    if (QMessageBox::question(this, tr("Telemetry"), telemetry_message) != QMessageBox::Yes) {
        Settings::values.enable_telemetry = false;
        Core::System::GetInstance().ApplySettings();
    }
}

const int GMainWindow::max_recent_files_item;

static void InitializeLogging() {
    using namespace Common;

    Log::Filter log_filter;
    log_filter.ParseFilterString(Settings::values.log_filter.GetValue());
    Log::SetGlobalFilter(log_filter);

    const auto log_dir = FS::GetYuzuPath(FS::YuzuPath::LogDir);
    void(FS::CreateDir(log_dir));
    Log::AddBackend(std::make_unique<Log::FileBackend>(log_dir / LOG_FILE));
#ifdef _WIN32
    Log::AddBackend(std::make_unique<Log::DebuggerBackend>());
#endif
}

static void RemoveCachedContents() {
    const auto cache_dir = Common::FS::GetYuzuPath(Common::FS::YuzuPath::CacheDir);
    const auto offline_fonts = cache_dir / "fonts";
    const auto offline_manual = cache_dir / "offline_web_applet_manual";
    const auto offline_legal_information = cache_dir / "offline_web_applet_legal_information";
    const auto offline_system_data = cache_dir / "offline_web_applet_system_data";

    Common::FS::RemoveDirRecursively(offline_fonts);
    Common::FS::RemoveDirRecursively(offline_manual);
    Common::FS::RemoveDirRecursively(offline_legal_information);
    Common::FS::RemoveDirRecursively(offline_system_data);
}

GMainWindow::GMainWindow()
    : input_subsystem{std::make_shared<InputCommon::InputSubsystem>()},
      config{std::make_unique<Config>()}, vfs{std::make_shared<FileSys::RealVfsFilesystem>()},
      provider{std::make_unique<FileSys::ManualContentProvider>()} {
    InitializeLogging();

    LoadTranslation();

    setAcceptDrops(true);
    ui.setupUi(this);
    statusBar()->hide();

    default_theme_paths = QIcon::themeSearchPaths();
    UpdateUITheme();

    SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    discord_rpc->Update();

    RegisterMetaTypes();

    InitializeWidgets();
    InitializeDebugWidgets();
    InitializeRecentFileMenuActions();
    InitializeHotkeys();

    SetDefaultUIGeometry();
    RestoreUIState();

    ConnectMenuEvents();
    ConnectWidgetEvents();

    const auto branch_name = std::string(Common::g_scm_branch);
    const auto description = std::string(Common::g_scm_desc);
    const auto build_id = std::string(Common::g_build_id);

    const auto yuzu_build = fmt::format("yuzu Development Build | {}-{}", branch_name, description);
    const auto override_build =
        fmt::format(fmt::runtime(std::string(Common::g_title_bar_format_idle)), build_id);
    const auto yuzu_build_version = override_build.empty() ? yuzu_build : override_build;

    LOG_INFO(Frontend, "yuzu Version: {}", yuzu_build_version);
#ifdef ARCHITECTURE_x86_64
    const auto& caps = Common::GetCPUCaps();
    std::string cpu_string = caps.cpu_string;
    if (caps.avx || caps.avx2 || caps.avx512) {
        cpu_string += " | AVX";
        if (caps.avx512) {
            cpu_string += "512";
        } else if (caps.avx2) {
            cpu_string += '2';
        }
        if (caps.fma || caps.fma4) {
            cpu_string += " | FMA";
        }
    }
    LOG_INFO(Frontend, "Host CPU: {}", cpu_string);
#endif
    LOG_INFO(Frontend, "Host OS: {}", QSysInfo::prettyProductName().toStdString());
    LOG_INFO(Frontend, "Host RAM: {:.2f} GB",
             Common::GetMemInfo().TotalPhysicalMemory / 1024.0f / 1024 / 1024);
    LOG_INFO(Frontend, "Host Swap: {:.2f} GB",
             Common::GetMemInfo().TotalSwapMemory / 1024.0f / 1024 / 1024);
    UpdateWindowTitle();

    show();

    Core::System::GetInstance().SetContentProvider(
        std::make_unique<FileSys::ContentProviderUnion>());
    Core::System::GetInstance().RegisterContentProvider(
        FileSys::ContentProviderUnionSlot::FrontendManual, provider.get());
    Core::System::GetInstance().GetFileSystemController().CreateFactories(*vfs);

    // Remove cached contents generated during the previous session
    RemoveCachedContents();

    // Gen keys if necessary
    OnReinitializeKeys(ReinitializeKeyBehavior::NoWarning);

    game_list->LoadCompatibilityList();
    game_list->PopulateAsync(UISettings::values.game_dirs);

    // Show one-time "callout" messages to the user
    ShowTelemetryCallout();

    // make sure menubar has the arrow cursor instead of inheriting from this
    ui.menubar->setCursor(QCursor());
    statusBar()->setCursor(QCursor());

    mouse_hide_timer.setInterval(default_mouse_timeout);
    connect(&mouse_hide_timer, &QTimer::timeout, this, &GMainWindow::HideMouseCursor);
    connect(ui.menubar, &QMenuBar::hovered, this, &GMainWindow::ShowMouseCursor);

    MigrateConfigFiles();

    ui.action_Fullscreen->setChecked(false);

    QStringList args = QApplication::arguments();

    if (args.size() < 2) {
        return;
    }

    QString game_path;

    for (int i = 1; i < args.size(); ++i) {
        // Preserves drag/drop functionality
        if (args.size() == 2 && !args[1].startsWith(QChar::fromLatin1('-'))) {
            game_path = args[1];
            break;
        }

        // Launch game in fullscreen mode
        if (args[i] == QStringLiteral("-f")) {
            ui.action_Fullscreen->setChecked(true);
            continue;
        }

        // Launch game with a specific user
        if (args[i] == QStringLiteral("-u")) {
            if (i >= args.size() - 1) {
                continue;
            }

            if (args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }

            bool argument_ok;
            const std::size_t selected_user = args[++i].toUInt(&argument_ok);

            if (!argument_ok) {
                LOG_ERROR(Frontend, "Invalid user argument");
                continue;
            }

            const Service::Account::ProfileManager manager;
            if (!manager.UserExistsIndex(selected_user)) {
                LOG_ERROR(Frontend, "Selected user doesn't exist");
                continue;
            }

            Settings::values.current_user = static_cast<s32>(selected_user);
            continue;
        }

        // Launch game at path
        if (args[i] == QStringLiteral("-g")) {
            if (i >= args.size() - 1) {
                continue;
            }

            if (args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }

            game_path = args[++i];
        }
    }

    if (!game_path.isEmpty()) {
        BootGame(game_path);
    }
}

GMainWindow::~GMainWindow() {
    // will get automatically deleted otherwise
    if (render_window->parent() == nullptr)
        delete render_window;
}

void GMainWindow::RegisterMetaTypes() {
    // Register integral and floating point types
    qRegisterMetaType<u8>("u8");
    qRegisterMetaType<u16>("u16");
    qRegisterMetaType<u32>("u32");
    qRegisterMetaType<u64>("u64");
    qRegisterMetaType<u128>("u128");
    qRegisterMetaType<s8>("s8");
    qRegisterMetaType<s16>("s16");
    qRegisterMetaType<s32>("s32");
    qRegisterMetaType<s64>("s64");
    qRegisterMetaType<f32>("f32");
    qRegisterMetaType<f64>("f64");

    // Register string types
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<std::wstring>("std::wstring");
    qRegisterMetaType<std::u8string>("std::u8string");
    qRegisterMetaType<std::u16string>("std::u16string");
    qRegisterMetaType<std::u32string>("std::u32string");
    qRegisterMetaType<std::string_view>("std::string_view");
    qRegisterMetaType<std::wstring_view>("std::wstring_view");
    qRegisterMetaType<std::u8string_view>("std::u8string_view");
    qRegisterMetaType<std::u16string_view>("std::u16string_view");
    qRegisterMetaType<std::u32string_view>("std::u32string_view");

    // Register applet types

    // Controller Applet
    qRegisterMetaType<Core::Frontend::ControllerParameters>("Core::Frontend::ControllerParameters");

    // Software Keyboard Applet
    qRegisterMetaType<Core::Frontend::KeyboardInitializeParameters>(
        "Core::Frontend::KeyboardInitializeParameters");
    qRegisterMetaType<Core::Frontend::InlineAppearParameters>(
        "Core::Frontend::InlineAppearParameters");
    qRegisterMetaType<Core::Frontend::InlineTextParameters>("Core::Frontend::InlineTextParameters");
    qRegisterMetaType<Service::AM::Applets::SwkbdResult>("Service::AM::Applets::SwkbdResult");
    qRegisterMetaType<Service::AM::Applets::SwkbdTextCheckResult>(
        "Service::AM::Applets::SwkbdTextCheckResult");
    qRegisterMetaType<Service::AM::Applets::SwkbdReplyType>("Service::AM::Applets::SwkbdReplyType");

    // Web Browser Applet
    qRegisterMetaType<Service::AM::Applets::WebExitReason>("Service::AM::Applets::WebExitReason");

    // Register loader types
    qRegisterMetaType<Core::System::ResultStatus>("Core::System::ResultStatus");
}

void GMainWindow::ControllerSelectorReconfigureControllers(
    const Core::Frontend::ControllerParameters& parameters) {
    QtControllerSelectorDialog dialog(this, parameters, input_subsystem.get());

    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint |
                          Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();

    emit ControllerSelectorReconfigureFinished();

    // Don't forget to apply settings.
    Core::System::GetInstance().ApplySettings();
    config->Save();

    UpdateStatusButtons();
}

void GMainWindow::ProfileSelectorSelectProfile() {
    QtProfileSelectionDialog dialog(this);
    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint |
                          Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                          Qt::WindowCloseButtonHint);
    dialog.setWindowModality(Qt::WindowModal);
    if (dialog.exec() == QDialog::Rejected) {
        emit ProfileSelectorFinishedSelection(std::nullopt);
        return;
    }

    const Service::Account::ProfileManager manager;
    const auto uuid = manager.GetUser(static_cast<std::size_t>(dialog.GetIndex()));
    if (!uuid.has_value()) {
        emit ProfileSelectorFinishedSelection(std::nullopt);
        return;
    }

    emit ProfileSelectorFinishedSelection(uuid);
}

void GMainWindow::SoftwareKeyboardInitialize(
    bool is_inline, Core::Frontend::KeyboardInitializeParameters initialize_parameters) {
    if (software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is already initialized!");
        return;
    }

    software_keyboard = new QtSoftwareKeyboardDialog(render_window, Core::System::GetInstance(),
                                                     is_inline, std::move(initialize_parameters));

    if (is_inline) {
        connect(
            software_keyboard, &QtSoftwareKeyboardDialog::SubmitInlineText, this,
            [this](Service::AM::Applets::SwkbdReplyType reply_type, std::u16string submitted_text,
                   s32 cursor_position) {
                emit SoftwareKeyboardSubmitInlineText(reply_type, submitted_text, cursor_position);
            },
            Qt::QueuedConnection);
    } else {
        connect(
            software_keyboard, &QtSoftwareKeyboardDialog::SubmitNormalText, this,
            [this](Service::AM::Applets::SwkbdResult result, std::u16string submitted_text) {
                emit SoftwareKeyboardSubmitNormalText(result, submitted_text);
            },
            Qt::QueuedConnection);
    }
}

void GMainWindow::SoftwareKeyboardShowNormal() {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    const auto& layout = render_window->GetFramebufferLayout();

    const auto x = layout.screen.left;
    const auto y = layout.screen.top;
    const auto w = layout.screen.GetWidth();
    const auto h = layout.screen.GetHeight();

    software_keyboard->ShowNormalKeyboard(render_window->mapToGlobal(QPoint(x, y)), QSize(w, h));
}

void GMainWindow::SoftwareKeyboardShowTextCheck(
    Service::AM::Applets::SwkbdTextCheckResult text_check_result,
    std::u16string text_check_message) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->ShowTextCheckDialog(text_check_result, text_check_message);
}

void GMainWindow::SoftwareKeyboardShowInline(
    Core::Frontend::InlineAppearParameters appear_parameters) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    const auto& layout = render_window->GetFramebufferLayout();

    const auto x =
        static_cast<int>(layout.screen.left + (0.5f * layout.screen.GetWidth() *
                                               ((2.0f * appear_parameters.key_top_translate_x) +
                                                (1.0f - appear_parameters.key_top_scale_x))));
    const auto y =
        static_cast<int>(layout.screen.top + (layout.screen.GetHeight() *
                                              ((2.0f * appear_parameters.key_top_translate_y) +
                                               (1.0f - appear_parameters.key_top_scale_y))));
    const auto w = static_cast<int>(layout.screen.GetWidth() * appear_parameters.key_top_scale_x);
    const auto h = static_cast<int>(layout.screen.GetHeight() * appear_parameters.key_top_scale_y);

    software_keyboard->ShowInlineKeyboard(std::move(appear_parameters),
                                          render_window->mapToGlobal(QPoint(x, y)), QSize(w, h));
}

void GMainWindow::SoftwareKeyboardHideInline() {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->HideInlineKeyboard();
}

void GMainWindow::SoftwareKeyboardInlineTextChanged(
    Core::Frontend::InlineTextParameters text_parameters) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->InlineTextChanged(std::move(text_parameters));
}

void GMainWindow::SoftwareKeyboardExit() {
    if (!software_keyboard) {
        return;
    }

    software_keyboard->ExitKeyboard();

    software_keyboard = nullptr;
}

void GMainWindow::WebBrowserOpenWebPage(const std::string& main_url,
                                        const std::string& additional_args, bool is_local) {
#ifdef YUZU_USE_QT_WEB_ENGINE

    if (disable_web_applet) {
        emit WebBrowserClosed(Service::AM::Applets::WebExitReason::WindowClosed,
                              "http://localhost/");
        return;
    }

    QtNXWebEngineView web_browser_view(this, Core::System::GetInstance(), input_subsystem.get());

    ui.action_Pause->setEnabled(false);
    ui.action_Restart->setEnabled(false);
    ui.action_Stop->setEnabled(false);

    {
        QProgressDialog loading_progress(this);
        loading_progress.setLabelText(tr("Loading Web Applet..."));
        loading_progress.setRange(0, 3);
        loading_progress.setValue(0);

        if (is_local && !Common::FS::Exists(main_url)) {
            loading_progress.show();

            auto future = QtConcurrent::run([this] { emit WebBrowserExtractOfflineRomFS(); });

            while (!future.isFinished()) {
                QCoreApplication::processEvents();

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        loading_progress.setValue(1);

        if (is_local) {
            web_browser_view.LoadLocalWebPage(main_url, additional_args);
        } else {
            web_browser_view.LoadExternalWebPage(main_url, additional_args);
        }

        if (render_window->IsLoadingComplete()) {
            render_window->hide();
        }

        const auto& layout = render_window->GetFramebufferLayout();
        web_browser_view.resize(layout.screen.GetWidth(), layout.screen.GetHeight());
        web_browser_view.move(layout.screen.left, layout.screen.top + menuBar()->height());
        web_browser_view.setZoomFactor(static_cast<qreal>(layout.screen.GetWidth()) /
                                       static_cast<qreal>(Layout::ScreenUndocked::Width));

        web_browser_view.setFocus();
        web_browser_view.show();

        loading_progress.setValue(2);

        QCoreApplication::processEvents();

        loading_progress.setValue(3);
    }

    bool exit_check = false;

    // TODO (Morph): Remove this
    QAction* exit_action = new QAction(tr("Disable Web Applet"), this);
    connect(exit_action, &QAction::triggered, this, [this, &web_browser_view] {
        const auto result = QMessageBox::warning(
            this, tr("Disable Web Applet"),
            tr("Disabling the web applet will cause it to not be shown again for the rest of the "
               "emulated session. This can lead to undefined behavior and should only be used with "
               "Super Mario 3D All-Stars. Are you sure you want to disable the web applet?"),
            QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) {
            disable_web_applet = true;
            web_browser_view.SetFinished(true);
        }
    });
    ui.menubar->addAction(exit_action);

    while (!web_browser_view.IsFinished()) {
        QCoreApplication::processEvents();

        if (!exit_check) {
            web_browser_view.page()->runJavaScript(
                QStringLiteral("end_applet;"), [&](const QVariant& variant) {
                    exit_check = false;
                    if (variant.toBool()) {
                        web_browser_view.SetFinished(true);
                        web_browser_view.SetExitReason(
                            Service::AM::Applets::WebExitReason::EndButtonPressed);
                    }
                });

            exit_check = true;
        }

        if (web_browser_view.GetCurrentURL().contains(QStringLiteral("localhost"))) {
            if (!web_browser_view.IsFinished()) {
                web_browser_view.SetFinished(true);
                web_browser_view.SetExitReason(Service::AM::Applets::WebExitReason::CallbackURL);
            }

            web_browser_view.SetLastURL(web_browser_view.GetCurrentURL().toStdString());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto exit_reason = web_browser_view.GetExitReason();
    const auto last_url = web_browser_view.GetLastURL();

    web_browser_view.hide();

    render_window->setFocus();

    if (render_window->IsLoadingComplete()) {
        render_window->show();
    }

    ui.action_Pause->setEnabled(true);
    ui.action_Restart->setEnabled(true);
    ui.action_Stop->setEnabled(true);

    ui.menubar->removeAction(exit_action);

    QCoreApplication::processEvents();

    emit WebBrowserClosed(exit_reason, last_url);

#else

    // Utilize the same fallback as the default web browser applet.
    emit WebBrowserClosed(Service::AM::Applets::WebExitReason::WindowClosed, "http://localhost/");

#endif
}

void GMainWindow::InitializeWidgets() {
#ifdef YUZU_ENABLE_COMPATIBILITY_REPORTING
    ui.action_Report_Compatibility->setVisible(true);
#endif
    render_window = new GRenderWindow(this, emu_thread.get(), input_subsystem);
    render_window->hide();

    game_list = new GameList(vfs, provider.get(), this);
    ui.horizontalLayout->addWidget(game_list);

    game_list_placeholder = new GameListPlaceholder(this);
    ui.horizontalLayout->addWidget(game_list_placeholder);
    game_list_placeholder->setVisible(false);

    loading_screen = new LoadingScreen(this);
    loading_screen->hide();
    ui.horizontalLayout->addWidget(loading_screen);
    connect(loading_screen, &LoadingScreen::Hidden, [&] {
        loading_screen->Clear();
        if (emulation_running) {
            render_window->show();
            render_window->setFocus();
        }
    });

    // Create status bar
    message_label = new QLabel();
    // Configured separately for left alignment
    message_label->setFrameStyle(QFrame::NoFrame);
    message_label->setContentsMargins(4, 0, 4, 0);
    message_label->setAlignment(Qt::AlignLeft);
    statusBar()->addPermanentWidget(message_label, 1);

    shader_building_label = new QLabel();
    shader_building_label->setToolTip(tr("The amount of shaders currently being built"));
    emu_speed_label = new QLabel();
    emu_speed_label->setToolTip(
        tr("Current emulation speed. Values higher or lower than 100% "
           "indicate emulation is running faster or slower than a Switch."));
    game_fps_label = new QLabel();
    game_fps_label->setToolTip(tr("How many frames per second the game is currently displaying. "
                                  "This will vary from game to game and scene to scene."));
    emu_frametime_label = new QLabel();
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a Switch frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    for (auto& label :
         {shader_building_label, emu_speed_label, game_fps_label, emu_frametime_label}) {
        label->setVisible(false);
        label->setFrameStyle(QFrame::NoFrame);
        label->setContentsMargins(4, 0, 4, 0);
        statusBar()->addPermanentWidget(label);
    }

    // Setup Dock button
    dock_status_button = new QPushButton();
    dock_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    dock_status_button->setFocusPolicy(Qt::NoFocus);
    connect(dock_status_button, &QPushButton::clicked, [&] {
        const bool is_docked = Settings::values.use_docked_mode.GetValue();
        auto& controller_type = Settings::values.players.GetValue()[0].controller_type;

        if (!is_docked && controller_type == Settings::ControllerType::Handheld) {
            QMessageBox::warning(this, tr("Invalid config detected"),
                                 tr("Handheld controller can't be used on docked mode. Pro "
                                    "controller will be selected."));
            controller_type = Settings::ControllerType::ProController;
            ConfigureDialog configure_dialog(this, hotkey_registry, input_subsystem.get());
            configure_dialog.ApplyConfiguration();
            controller_dialog->refreshConfiguration();
        }

        Settings::values.use_docked_mode.SetValue(!is_docked);
        dock_status_button->setChecked(!is_docked);
        OnDockedModeChanged(is_docked, !is_docked);
    });
    dock_status_button->setText(tr("DOCK"));
    dock_status_button->setCheckable(true);
    dock_status_button->setChecked(Settings::values.use_docked_mode.GetValue());
    statusBar()->insertPermanentWidget(0, dock_status_button);

    // Setup ASync button
    async_status_button = new QPushButton();
    async_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    async_status_button->setFocusPolicy(Qt::NoFocus);
    connect(async_status_button, &QPushButton::clicked, [&] {
        if (emulation_running) {
            return;
        }
        Settings::values.use_asynchronous_gpu_emulation.SetValue(
            !Settings::values.use_asynchronous_gpu_emulation.GetValue());
        async_status_button->setChecked(Settings::values.use_asynchronous_gpu_emulation.GetValue());
        Core::System::GetInstance().ApplySettings();
    });
    async_status_button->setText(tr("ASYNC"));
    async_status_button->setCheckable(true);
    async_status_button->setChecked(Settings::values.use_asynchronous_gpu_emulation.GetValue());

    // Setup Multicore button
    multicore_status_button = new QPushButton();
    multicore_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    multicore_status_button->setFocusPolicy(Qt::NoFocus);
    connect(multicore_status_button, &QPushButton::clicked, [&] {
        if (emulation_running) {
            return;
        }
        Settings::values.use_multi_core.SetValue(!Settings::values.use_multi_core.GetValue());
        multicore_status_button->setChecked(Settings::values.use_multi_core.GetValue());
        Core::System::GetInstance().ApplySettings();
    });
    multicore_status_button->setText(tr("MULTICORE"));
    multicore_status_button->setCheckable(true);
    multicore_status_button->setChecked(Settings::values.use_multi_core.GetValue());

    statusBar()->insertPermanentWidget(0, multicore_status_button);
    statusBar()->insertPermanentWidget(0, async_status_button);

    // Setup Renderer API button
    renderer_status_button = new QPushButton();
    renderer_status_button->setObjectName(QStringLiteral("RendererStatusBarButton"));
    renderer_status_button->setCheckable(true);
    renderer_status_button->setFocusPolicy(Qt::NoFocus);
    connect(renderer_status_button, &QPushButton::toggled, [this](bool checked) {
        renderer_status_button->setText(checked ? tr("VULKAN") : tr("OPENGL"));
    });
    renderer_status_button->toggle();

    renderer_status_button->setChecked(Settings::values.renderer_backend.GetValue() ==
                                       Settings::RendererBackend::Vulkan);
    connect(renderer_status_button, &QPushButton::clicked, [this] {
        if (emulation_running) {
            return;
        }
        if (renderer_status_button->isChecked()) {
            Settings::values.renderer_backend.SetValue(Settings::RendererBackend::Vulkan);
        } else {
            Settings::values.renderer_backend.SetValue(Settings::RendererBackend::OpenGL);
        }

        Core::System::GetInstance().ApplySettings();
    });
    statusBar()->insertPermanentWidget(0, renderer_status_button);

    statusBar()->setVisible(true);
    setStyleSheet(QStringLiteral("QStatusBar::item{border: none;}"));
}

void GMainWindow::InitializeDebugWidgets() {
    QMenu* debug_menu = ui.menu_View_Debugging;

#if MICROPROFILE_ENABLED
    microProfileDialog = new MicroProfileDialog(this);
    microProfileDialog->hide();
    debug_menu->addAction(microProfileDialog->toggleViewAction());
#endif

    waitTreeWidget = new WaitTreeWidget(this);
    addDockWidget(Qt::LeftDockWidgetArea, waitTreeWidget);
    waitTreeWidget->hide();
    debug_menu->addAction(waitTreeWidget->toggleViewAction());

    controller_dialog = new ControllerDialog(this);
    controller_dialog->hide();
    debug_menu->addAction(controller_dialog->toggleViewAction());

    connect(this, &GMainWindow::EmulationStarting, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStopping);
}

void GMainWindow::InitializeRecentFileMenuActions() {
    for (int i = 0; i < max_recent_files_item; ++i) {
        actions_recent_files[i] = new QAction(this);
        actions_recent_files[i]->setVisible(false);
        connect(actions_recent_files[i], &QAction::triggered, this, &GMainWindow::OnMenuRecentFile);

        ui.menu_recent_files->addAction(actions_recent_files[i]);
    }
    ui.menu_recent_files->addSeparator();
    QAction* action_clear_recent_files = new QAction(this);
    action_clear_recent_files->setText(tr("&Clear Recent Files"));
    connect(action_clear_recent_files, &QAction::triggered, this, [this] {
        UISettings::values.recent_files.clear();
        UpdateRecentFiles();
    });
    ui.menu_recent_files->addAction(action_clear_recent_files);

    UpdateRecentFiles();
}

void GMainWindow::InitializeHotkeys() {
    hotkey_registry.LoadHotkeys();

    const QString main_window = QStringLiteral("Main Window");
    const QString load_file = QStringLiteral("Load File");
    const QString load_amiibo = QStringLiteral("Load Amiibo");
    const QString exit_yuzu = QStringLiteral("Exit yuzu");
    const QString restart_emulation = QStringLiteral("Restart Emulation");
    const QString stop_emulation = QStringLiteral("Stop Emulation");
    const QString toggle_filter_bar = QStringLiteral("Toggle Filter Bar");
    const QString toggle_status_bar = QStringLiteral("Toggle Status Bar");
    const QString fullscreen = QStringLiteral("Fullscreen");
    const QString capture_screenshot = QStringLiteral("Capture Screenshot");

    ui.action_Load_File->setShortcut(hotkey_registry.GetKeySequence(main_window, load_file));
    ui.action_Load_File->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, load_file));

    ui.action_Load_Amiibo->setShortcut(hotkey_registry.GetKeySequence(main_window, load_amiibo));
    ui.action_Load_Amiibo->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, load_amiibo));

    ui.action_Exit->setShortcut(hotkey_registry.GetKeySequence(main_window, exit_yuzu));
    ui.action_Exit->setShortcutContext(hotkey_registry.GetShortcutContext(main_window, exit_yuzu));

    ui.action_Restart->setShortcut(hotkey_registry.GetKeySequence(main_window, restart_emulation));
    ui.action_Restart->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, restart_emulation));

    ui.action_Stop->setShortcut(hotkey_registry.GetKeySequence(main_window, stop_emulation));
    ui.action_Stop->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, stop_emulation));

    ui.action_Show_Filter_Bar->setShortcut(
        hotkey_registry.GetKeySequence(main_window, toggle_filter_bar));
    ui.action_Show_Filter_Bar->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, toggle_filter_bar));

    ui.action_Show_Status_Bar->setShortcut(
        hotkey_registry.GetKeySequence(main_window, toggle_status_bar));
    ui.action_Show_Status_Bar->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, toggle_status_bar));

    ui.action_Capture_Screenshot->setShortcut(
        hotkey_registry.GetKeySequence(main_window, capture_screenshot));
    ui.action_Capture_Screenshot->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, capture_screenshot));

    ui.action_Fullscreen->setShortcut(
        hotkey_registry.GetHotkey(main_window, fullscreen, this)->key());
    ui.action_Fullscreen->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, fullscreen));

    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Load File"), this),
            &QShortcut::activated, this, &GMainWindow::OnMenuLoadFile);
    connect(
        hotkey_registry.GetHotkey(main_window, QStringLiteral("Continue/Pause Emulation"), this),
        &QShortcut::activated, this, [&] {
            if (emulation_running) {
                if (emu_thread->IsRunning()) {
                    OnPauseGame();
                } else {
                    OnStartGame();
                }
            }
        });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Restart Emulation"), this),
            &QShortcut::activated, this, [this] {
                if (!Core::System::GetInstance().IsPoweredOn()) {
                    return;
                }
                BootGame(game_path);
            });
    connect(hotkey_registry.GetHotkey(main_window, fullscreen, render_window),
            &QShortcut::activated, ui.action_Fullscreen, &QAction::trigger);
    connect(hotkey_registry.GetHotkey(main_window, fullscreen, render_window),
            &QShortcut::activatedAmbiguously, ui.action_Fullscreen, &QAction::trigger);
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Exit Fullscreen"), this),
            &QShortcut::activated, this, [&] {
                if (emulation_running) {
                    ui.action_Fullscreen->setChecked(false);
                    ToggleFullscreen();
                }
            });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Toggle Speed Limit"), this),
            &QShortcut::activated, this, [&] {
                Settings::values.use_frame_limit.SetValue(
                    !Settings::values.use_frame_limit.GetValue());
                UpdateStatusBar();
            });
    constexpr u16 SPEED_LIMIT_STEP = 5;
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Increase Speed Limit"), this),
            &QShortcut::activated, this, [&] {
                if (Settings::values.frame_limit.GetValue() < 9999 - SPEED_LIMIT_STEP) {
                    Settings::values.frame_limit.SetValue(SPEED_LIMIT_STEP +
                                                          Settings::values.frame_limit.GetValue());
                    UpdateStatusBar();
                }
            });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Decrease Speed Limit"), this),
            &QShortcut::activated, this, [&] {
                if (Settings::values.frame_limit.GetValue() > SPEED_LIMIT_STEP) {
                    Settings::values.frame_limit.SetValue(Settings::values.frame_limit.GetValue() -
                                                          SPEED_LIMIT_STEP);
                    UpdateStatusBar();
                }
            });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Load Amiibo"), this),
            &QShortcut::activated, this, [&] {
                if (ui.action_Load_Amiibo->isEnabled()) {
                    OnLoadAmiibo();
                }
            });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Capture Screenshot"), this),
            &QShortcut::activated, this, [&] {
                if (emu_thread != nullptr && emu_thread->IsRunning()) {
                    OnCaptureScreenshot();
                }
            });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Change Docked Mode"), this),
            &QShortcut::activated, this, [&] {
                Settings::values.use_docked_mode.SetValue(
                    !Settings::values.use_docked_mode.GetValue());
                OnDockedModeChanged(!Settings::values.use_docked_mode.GetValue(),
                                    Settings::values.use_docked_mode.GetValue());
                dock_status_button->setChecked(Settings::values.use_docked_mode.GetValue());
            });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Mute Audio"), this),
            &QShortcut::activated, this,
            [] { Settings::values.audio_muted = !Settings::values.audio_muted; });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Toggle Framerate Limit"), this),
            &QShortcut::activated, this, [] {
                Settings::values.disable_fps_limit.SetValue(
                    !Settings::values.disable_fps_limit.GetValue());
            });
    connect(hotkey_registry.GetHotkey(main_window, QStringLiteral("Toggle Mouse Panning"), this),
            &QShortcut::activated, this, [&] {
                Settings::values.mouse_panning = !Settings::values.mouse_panning;
                if (Settings::values.mouse_panning) {
                    render_window->installEventFilter(render_window);
                    render_window->setAttribute(Qt::WA_Hover, true);
                }
            });
}

void GMainWindow::SetDefaultUIGeometry() {
    // geometry: 53% of the window contents are in the upper screen half, 47% in the lower half
    const QRect screenRect = QApplication::desktop()->screenGeometry(this);

    const int w = screenRect.width() * 2 / 3;
    const int h = screenRect.height() * 2 / 3;
    const int x = (screenRect.x() + screenRect.width()) / 2 - w / 2;
    const int y = (screenRect.y() + screenRect.height()) / 2 - h * 53 / 100;

    setGeometry(x, y, w, h);
}

void GMainWindow::RestoreUIState() {
    restoreGeometry(UISettings::values.geometry);
    restoreState(UISettings::values.state);
    render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
#if MICROPROFILE_ENABLED
    microProfileDialog->restoreGeometry(UISettings::values.microprofile_geometry);
    microProfileDialog->setVisible(UISettings::values.microprofile_visible.GetValue());
#endif

    game_list->LoadInterfaceLayout();

    ui.action_Single_Window_Mode->setChecked(UISettings::values.single_window_mode.GetValue());
    ToggleWindowMode();

    ui.action_Fullscreen->setChecked(UISettings::values.fullscreen.GetValue());

    ui.action_Display_Dock_Widget_Headers->setChecked(
        UISettings::values.display_titlebar.GetValue());
    OnDisplayTitleBars(ui.action_Display_Dock_Widget_Headers->isChecked());

    ui.action_Show_Filter_Bar->setChecked(UISettings::values.show_filter_bar.GetValue());
    game_list->SetFilterVisible(ui.action_Show_Filter_Bar->isChecked());

    ui.action_Show_Status_Bar->setChecked(UISettings::values.show_status_bar.GetValue());
    statusBar()->setVisible(ui.action_Show_Status_Bar->isChecked());
    Debugger::ToggleConsole();
}

void GMainWindow::OnAppFocusStateChanged(Qt::ApplicationState state) {
    if (!UISettings::values.pause_when_in_background) {
        return;
    }
    if (state != Qt::ApplicationHidden && state != Qt::ApplicationInactive &&
        state != Qt::ApplicationActive) {
        LOG_DEBUG(Frontend, "ApplicationState unusual flag: {} ", state);
    }
    if (ui.action_Pause->isEnabled() &&
        (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
        auto_paused = true;
        OnPauseGame();
    } else if (ui.action_Start->isEnabled() && auto_paused && state == Qt::ApplicationActive) {
        auto_paused = false;
        OnStartGame();
    }
}

void GMainWindow::ConnectWidgetEvents() {
    connect(game_list, &GameList::BootGame, this, &GMainWindow::BootGame);
    connect(game_list, &GameList::GameChosen, this, &GMainWindow::OnGameListLoadFile);
    connect(game_list, &GameList::OpenDirectory, this, &GMainWindow::OnGameListOpenDirectory);
    connect(game_list, &GameList::OpenFolderRequested, this, &GMainWindow::OnGameListOpenFolder);
    connect(game_list, &GameList::OpenTransferableShaderCacheRequested, this,
            &GMainWindow::OnTransferableShaderCacheOpenFile);
    connect(game_list, &GameList::RemoveInstalledEntryRequested, this,
            &GMainWindow::OnGameListRemoveInstalledEntry);
    connect(game_list, &GameList::RemoveFileRequested, this, &GMainWindow::OnGameListRemoveFile);
    connect(game_list, &GameList::DumpRomFSRequested, this, &GMainWindow::OnGameListDumpRomFS);
    connect(game_list, &GameList::CopyTIDRequested, this, &GMainWindow::OnGameListCopyTID);
    connect(game_list, &GameList::NavigateToGamedbEntryRequested, this,
            &GMainWindow::OnGameListNavigateToGamedbEntry);
    connect(game_list, &GameList::AddDirectory, this, &GMainWindow::OnGameListAddDirectory);
    connect(game_list_placeholder, &GameListPlaceholder::AddDirectory, this,
            &GMainWindow::OnGameListAddDirectory);
    connect(game_list, &GameList::ShowList, this, &GMainWindow::OnGameListShowList);

    connect(game_list, &GameList::OpenPerGameGeneralRequested, this,
            &GMainWindow::OnGameListOpenPerGameProperties);

    connect(this, &GMainWindow::UpdateInstallProgress, this,
            &GMainWindow::IncrementInstallProgress);

    connect(this, &GMainWindow::EmulationStarting, render_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, render_window,
            &GRenderWindow::OnEmulationStopping);

    // Software Keyboard Applet
    connect(this, &GMainWindow::EmulationStarting, this, &GMainWindow::SoftwareKeyboardExit);
    connect(this, &GMainWindow::EmulationStopping, this, &GMainWindow::SoftwareKeyboardExit);

    connect(&status_bar_update_timer, &QTimer::timeout, this, &GMainWindow::UpdateStatusBar);
}

void GMainWindow::ConnectMenuEvents() {
    // File
    connect(ui.action_Load_File, &QAction::triggered, this, &GMainWindow::OnMenuLoadFile);
    connect(ui.action_Load_Folder, &QAction::triggered, this, &GMainWindow::OnMenuLoadFolder);
    connect(ui.action_Install_File_NAND, &QAction::triggered, this,
            &GMainWindow::OnMenuInstallToNAND);
    connect(ui.action_Exit, &QAction::triggered, this, &QMainWindow::close);
    connect(ui.action_Load_Amiibo, &QAction::triggered, this, &GMainWindow::OnLoadAmiibo);

    // Emulation
    connect(ui.action_Start, &QAction::triggered, this, &GMainWindow::OnStartGame);
    connect(ui.action_Pause, &QAction::triggered, this, &GMainWindow::OnPauseGame);
    connect(ui.action_Stop, &QAction::triggered, this, &GMainWindow::OnStopGame);
    connect(ui.action_Report_Compatibility, &QAction::triggered, this,
            &GMainWindow::OnMenuReportCompatibility);
    connect(ui.action_Open_Mods_Page, &QAction::triggered, this, &GMainWindow::OnOpenModsPage);
    connect(ui.action_Open_Quickstart_Guide, &QAction::triggered, this,
            &GMainWindow::OnOpenQuickstartGuide);
    connect(ui.action_Open_FAQ, &QAction::triggered, this, &GMainWindow::OnOpenFAQ);
    connect(ui.action_Restart, &QAction::triggered, this, [this] { BootGame(QString(game_path)); });
    connect(ui.action_Configure, &QAction::triggered, this, &GMainWindow::OnConfigure);
    connect(ui.action_Configure_Current_Game, &QAction::triggered, this,
            &GMainWindow::OnConfigurePerGame);

    // View
    connect(ui.action_Single_Window_Mode, &QAction::triggered, this,
            &GMainWindow::ToggleWindowMode);
    connect(ui.action_Display_Dock_Widget_Headers, &QAction::triggered, this,
            &GMainWindow::OnDisplayTitleBars);
    connect(ui.action_Show_Filter_Bar, &QAction::triggered, this, &GMainWindow::OnToggleFilterBar);
    connect(ui.action_Show_Status_Bar, &QAction::triggered, statusBar(), &QStatusBar::setVisible);
    connect(ui.action_Reset_Window_Size_720, &QAction::triggered, this,
            &GMainWindow::ResetWindowSize720);
    connect(ui.action_Reset_Window_Size_1080, &QAction::triggered, this,
            &GMainWindow::ResetWindowSize1080);

    // Fullscreen
    connect(ui.action_Fullscreen, &QAction::triggered, this, &GMainWindow::ToggleFullscreen);

    // Movie
    connect(ui.action_Capture_Screenshot, &QAction::triggered, this,
            &GMainWindow::OnCaptureScreenshot);

    // Help
    connect(ui.action_Open_yuzu_Folder, &QAction::triggered, this, &GMainWindow::OnOpenYuzuFolder);
    connect(ui.action_Rederive, &QAction::triggered, this,
            std::bind(&GMainWindow::OnReinitializeKeys, this, ReinitializeKeyBehavior::Warning));
    connect(ui.action_About, &QAction::triggered, this, &GMainWindow::OnAbout);
}

void GMainWindow::OnDisplayTitleBars(bool show) {
    QList<QDockWidget*> widgets = findChildren<QDockWidget*>();

    if (show) {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(nullptr);
            if (old != nullptr)
                delete old;
        }
    } else {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(new QWidget());
            if (old != nullptr)
                delete old;
        }
    }
}

void GMainWindow::PreventOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#endif
}

void GMainWindow::AllowOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

bool GMainWindow::LoadROM(const QString& filename, u64 program_id, std::size_t program_index) {
    // Shutdown previous session if the emu thread is still active...
    if (emu_thread != nullptr)
        ShutdownGame();

    if (!render_window->InitRenderTarget()) {
        return false;
    }

    Core::System& system{Core::System::GetInstance()};
    system.SetFilesystem(vfs);

    system.SetAppletFrontendSet({
        std::make_unique<QtControllerSelector>(*this), // Controller Selector
        std::make_unique<QtErrorDisplay>(*this),       // Error Display
        nullptr,                                       // Parental Controls
        nullptr,                                       // Photo Viewer
        std::make_unique<QtProfileSelector>(*this),    // Profile Selector
        std::make_unique<QtSoftwareKeyboard>(*this),   // Software Keyboard
        std::make_unique<QtWebBrowser>(*this),         // Web Browser
    });

    const Core::System::ResultStatus result{
        system.Load(*render_window, filename.toStdString(), program_id, program_index)};

    const auto drd_callout = (UISettings::values.callout_flags.GetValue() &
                              static_cast<u32>(CalloutFlag::DRDDeprecation)) == 0;

    if (result == Core::System::ResultStatus::Success &&
        system.GetAppLoader().GetFileType() == Loader::FileType::DeconstructedRomDirectory &&
        drd_callout) {
        UISettings::values.callout_flags = UISettings::values.callout_flags.GetValue() |
                                           static_cast<u32>(CalloutFlag::DRDDeprecation);
        QMessageBox::warning(
            this, tr("Warning Outdated Game Format"),
            tr("You are using the deconstructed ROM directory format for this game, which is an "
               "outdated format that has been superseded by others such as NCA, NAX, XCI, or "
               "NSP. Deconstructed ROM directories lack icons, metadata, and update "
               "support.<br><br>For an explanation of the various Switch formats yuzu supports, <a "
               "href='https://yuzu-emu.org/wiki/overview-of-switch-game-formats'>check out our "
               "wiki</a>. This message will not be shown again."));
    }

    if (result != Core::System::ResultStatus::Success) {
        switch (result) {
        case Core::System::ResultStatus::ErrorGetLoader:
            LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", filename.toStdString());
            QMessageBox::critical(this, tr("Error while loading ROM!"),
                                  tr("The ROM format is not supported."));
            break;
        case Core::System::ResultStatus::ErrorVideoCore:
            QMessageBox::critical(
                this, tr("An error occurred initializing the video core."),
                tr("yuzu has encountered an error while running the video core, please see the "
                   "log for more details."
                   "For more information on accessing the log, please see the following page: "
                   "<a href='https://community.citra-emu.org/t/how-to-upload-the-log-file/296'>How "
                   "to "
                   "Upload the Log File</a>."
                   "Ensure that you have the latest graphics drivers for your GPU."));

            break;

        default:
            if (result > Core::System::ResultStatus::ErrorLoader) {
                const u16 loader_id = static_cast<u16>(Core::System::ResultStatus::ErrorLoader);
                const u16 error_id = static_cast<u16>(result) - loader_id;
                const std::string error_code = fmt::format("({:04X}-{:04X})", loader_id, error_id);
                LOG_CRITICAL(Frontend, "Failed to load ROM! {}", error_code);

                const auto title =
                    tr("Error while loading ROM! %1", "%1 signifies a numeric error code.")
                        .arg(QString::fromStdString(error_code));
                const auto description =
                    tr("%1<br>Please follow <a href='https://yuzu-emu.org/help/quickstart/'>the "
                       "yuzu quickstart guide</a> to redump your files.<br>You can refer "
                       "to the yuzu wiki</a> or the yuzu Discord</a> for help.",
                       "%1 signifies an error string.")
                        .arg(QString::fromStdString(
                            GetResultStatusString(static_cast<Loader::ResultStatus>(error_id))));

                QMessageBox::critical(this, title, description);
            } else {
                QMessageBox::critical(
                    this, tr("Error while loading ROM!"),
                    tr("An unknown error occurred. Please see the log for more details."));
            }
            break;
        }
        return false;
    }
    game_path = filename;

    system.TelemetrySession().AddField(Common::Telemetry::FieldType::App, "Frontend", "Qt");
    return true;
}

void GMainWindow::SelectAndSetCurrentUser() {
    QtProfileSelectionDialog dialog(this);
    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    dialog.setWindowModality(Qt::WindowModal);

    if (dialog.exec() == QDialog::Rejected) {
        return;
    }

    Settings::values.current_user = dialog.GetIndex();
}

void GMainWindow::BootGame(const QString& filename, u64 program_id, std::size_t program_index,
                           StartGameType type) {
    LOG_INFO(Frontend, "yuzu starting...");
    StoreRecentFile(filename); // Put the filename on top of the list

    u64 title_id{0};

    last_filename_booted = filename;

    auto& system = Core::System::GetInstance();
    const auto v_file = Core::GetGameFileFromPath(vfs, filename.toUtf8().constData());
    const auto loader = Loader::GetLoader(system, v_file, program_id, program_index);

    if (loader != nullptr && loader->ReadProgramId(title_id) == Loader::ResultStatus::Success &&
        type == StartGameType::Normal) {
        // Load per game settings
        const auto file_path = std::filesystem::path{filename.toStdU16String()};
        const auto config_file_name = title_id == 0
                                          ? Common::FS::PathToUTF8String(file_path.filename())
                                          : fmt::format("{:016X}", title_id);
        Config per_game_config(config_file_name, Config::ConfigType::PerGameConfig);
    }

    ConfigureVibration::SetAllVibrationDevices();

    // Disable fps limit toggle when booting a new title
    Settings::values.disable_fps_limit.SetValue(false);

    // Save configurations
    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    config->Save();

    Settings::LogSettings();

    if (UISettings::values.select_user_on_boot) {
        SelectAndSetCurrentUser();
    }

    if (!LoadROM(filename, program_id, program_index))
        return;

    // Create and start the emulation thread
    emu_thread = std::make_unique<EmuThread>();
    emit EmulationStarting(emu_thread.get());
    emu_thread->start();

    // Register an ExecuteProgram callback such that Core can execute a sub-program
    system.RegisterExecuteProgramCallback(
        [this](std::size_t program_index) { render_window->ExecuteProgram(program_index); });

    connect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    connect(render_window, &GRenderWindow::MouseActivity, this, &GMainWindow::OnMouseActivity);
    // BlockingQueuedConnection is important here, it makes sure we've finished refreshing our views
    // before the CPU continues
    connect(emu_thread.get(), &EmuThread::DebugModeEntered, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeEntered, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeLeft, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeLeft, Qt::BlockingQueuedConnection);

    connect(emu_thread.get(), &EmuThread::LoadProgress, loading_screen,
            &LoadingScreen::OnLoadProgress, Qt::QueuedConnection);

    // Update the GUI
    UpdateStatusButtons();
    if (ui.action_Single_Window_Mode->isChecked()) {
        game_list->hide();
        game_list_placeholder->hide();
    }
    status_bar_update_timer.start(500);
    async_status_button->setDisabled(true);
    multicore_status_button->setDisabled(true);
    renderer_status_button->setDisabled(true);

    if (UISettings::values.hide_mouse || Settings::values.mouse_panning) {
        render_window->installEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, true);
    }

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }

    std::string title_name;
    std::string title_version;
    const auto res = system.GetGameName(title_name);

    const auto metadata = [&system, title_id] {
        const FileSys::PatchManager pm(title_id, system.GetFileSystemController(),
                                       system.GetContentProvider());
        return pm.GetControlMetadata();
    }();
    if (metadata.first != nullptr) {
        title_version = metadata.first->GetVersionString();
        title_name = metadata.first->GetApplicationName();
    }
    if (res != Loader::ResultStatus::Success || title_name.empty()) {
        title_name = Common::FS::PathToUTF8String(
            std::filesystem::path{filename.toStdU16String()}.filename());
    }
    const bool is_64bit = system.Kernel().CurrentProcess()->Is64BitProcess();
    const auto instruction_set_suffix = is_64bit ? tr("(64-bit)") : tr("(32-bit)");
    title_name = tr("%1 %2", "%1 is the title name. %2 indicates if the title is 64-bit or 32-bit")
                     .arg(QString::fromStdString(title_name), instruction_set_suffix)
                     .toStdString();
    LOG_INFO(Frontend, "Booting game: {:016X} | {} | {}", title_id, title_name, title_version);
    const auto gpu_vendor = system.GPU().Renderer().GetDeviceVendor();
    UpdateWindowTitle(title_name, title_version, gpu_vendor);

    loading_screen->Prepare(system.GetAppLoader());
    loading_screen->show();

    emulation_running = true;
    if (ui.action_Fullscreen->isChecked()) {
        ShowFullscreen();
    }
    OnStartGame();
}

void GMainWindow::ShutdownGame() {
    if (!emulation_running) {
        return;
    }

    if (ui.action_Fullscreen->isChecked()) {
        HideFullscreen();
    }

    AllowOSSleep();

    discord_rpc->Pause();
    emu_thread->RequestStop();

    emit EmulationStopping();

    // Wait for emulation thread to complete and delete it
    emu_thread->wait();
    emu_thread = nullptr;

    discord_rpc->Update();

    // The emulation is stopped, so closing the window or not does not matter anymore
    disconnect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);

    // Update the GUI
    ui.action_Start->setEnabled(false);
    ui.action_Start->setText(tr("Start"));
    ui.action_Pause->setEnabled(false);
    ui.action_Stop->setEnabled(false);
    ui.action_Restart->setEnabled(false);
    ui.action_Configure_Current_Game->setEnabled(false);
    ui.action_Report_Compatibility->setEnabled(false);
    ui.action_Load_Amiibo->setEnabled(false);
    ui.action_Capture_Screenshot->setEnabled(false);
    render_window->hide();
    loading_screen->hide();
    loading_screen->Clear();
    if (game_list->IsEmpty()) {
        game_list_placeholder->show();
    } else {
        game_list->show();
    }
    game_list->SetFilterFocus();

    render_window->removeEventFilter(render_window);
    render_window->setAttribute(Qt::WA_Hover, false);

    UpdateWindowTitle();

    // Disable status bar updates
    status_bar_update_timer.stop();
    shader_building_label->setVisible(false);
    emu_speed_label->setVisible(false);
    game_fps_label->setVisible(false);
    emu_frametime_label->setVisible(false);
    async_status_button->setEnabled(true);
    multicore_status_button->setEnabled(true);
    renderer_status_button->setEnabled(true);

    emulation_running = false;

    game_path.clear();

    // When closing the game, destroy the GLWindow to clear the context after the game is closed
    render_window->ReleaseRenderTarget();
}

void GMainWindow::StoreRecentFile(const QString& filename) {
    UISettings::values.recent_files.prepend(filename);
    UISettings::values.recent_files.removeDuplicates();
    while (UISettings::values.recent_files.size() > max_recent_files_item) {
        UISettings::values.recent_files.removeLast();
    }

    UpdateRecentFiles();
}

void GMainWindow::UpdateRecentFiles() {
    const int num_recent_files =
        std::min(UISettings::values.recent_files.size(), max_recent_files_item);

    for (int i = 0; i < num_recent_files; i++) {
        const QString text = QStringLiteral("&%1. %2").arg(i + 1).arg(
            QFileInfo(UISettings::values.recent_files[i]).fileName());
        actions_recent_files[i]->setText(text);
        actions_recent_files[i]->setData(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setToolTip(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setVisible(true);
    }

    for (int j = num_recent_files; j < max_recent_files_item; ++j) {
        actions_recent_files[j]->setVisible(false);
    }

    // Enable the recent files menu if the list isn't empty
    ui.menu_recent_files->setEnabled(num_recent_files != 0);
}

void GMainWindow::OnGameListLoadFile(QString game_path, u64 program_id) {
    BootGame(game_path, program_id);
}

void GMainWindow::OnGameListOpenFolder(u64 program_id, GameListOpenTarget target,
                                       const std::string& game_path) {
    std::filesystem::path path;
    QString open_target;
    auto& system = Core::System::GetInstance();

    const auto [user_save_size, device_save_size] = [this, &game_path, &program_id, &system] {
        const FileSys::PatchManager pm{program_id, system.GetFileSystemController(),
                                       system.GetContentProvider()};
        const auto control = pm.GetControlMetadata().first;
        if (control != nullptr) {
            return std::make_pair(control->GetDefaultNormalSaveSize(),
                                  control->GetDeviceSaveDataSize());
        } else {
            const auto file = Core::GetGameFileFromPath(vfs, game_path);
            const auto loader = Loader::GetLoader(system, file);

            FileSys::NACP nacp{};
            loader->ReadControlData(nacp);
            return std::make_pair(nacp.GetDefaultNormalSaveSize(), nacp.GetDeviceSaveDataSize());
        }
    }();

    const bool has_user_save{user_save_size > 0};
    const bool has_device_save{device_save_size > 0};

    ASSERT_MSG(has_user_save != has_device_save, "Game uses both user and device savedata?");

    switch (target) {
    case GameListOpenTarget::SaveData: {
        open_target = tr("Save Data");
        const auto nand_dir = Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir);

        if (has_user_save) {
            // User save data
            const auto select_profile = [this] {
                QtProfileSelectionDialog dialog(this);
                dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                                      Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
                dialog.setWindowModality(Qt::WindowModal);

                if (dialog.exec() == QDialog::Rejected) {
                    return -1;
                }

                return dialog.GetIndex();
            };

            const auto index = select_profile();
            if (index == -1) {
                return;
            }

            Service::Account::ProfileManager manager;
            const auto user_id = manager.GetUser(static_cast<std::size_t>(index));
            ASSERT(user_id);

            const auto user_save_data_path = FileSys::SaveDataFactory::GetFullPath(
                system, FileSys::SaveDataSpaceId::NandUser, FileSys::SaveDataType::SaveData,
                program_id, user_id->uuid, 0);

            path = Common::FS::ConcatPathSafe(nand_dir, user_save_data_path);
        } else {
            // Device save data
            const auto device_save_data_path = FileSys::SaveDataFactory::GetFullPath(
                system, FileSys::SaveDataSpaceId::NandUser, FileSys::SaveDataType::SaveData,
                program_id, {}, 0);

            path = Common::FS::ConcatPathSafe(nand_dir, device_save_data_path);
        }

        if (!Common::FS::CreateDirs(path)) {
            LOG_ERROR(Frontend, "Unable to create the directories for save data");
        }

        break;
    }
    case GameListOpenTarget::ModData: {
        open_target = tr("Mod Data");
        path = Common::FS::GetYuzuPath(Common::FS::YuzuPath::LoadDir) /
               fmt::format("{:016X}", program_id);
        break;
    }
    default:
        UNIMPLEMENTED();
    }

    const QString qpath = QString::fromStdString(Common::FS::PathToUTF8String(path));
    const QDir dir(qpath);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("Error Opening %1 Folder").arg(open_target),
                             tr("Folder does not exist!"));
        return;
    }
    LOG_INFO(Frontend, "Opening {} path for program_id={:016x}", open_target.toStdString(),
             program_id);
    QDesktopServices::openUrl(QUrl::fromLocalFile(qpath));
}

void GMainWindow::OnTransferableShaderCacheOpenFile(u64 program_id) {
    const auto shader_cache_dir = Common::FS::GetYuzuPath(Common::FS::YuzuPath::ShaderDir);
    const auto transferable_shader_cache_folder_path = shader_cache_dir / "opengl" / "transferable";
    const auto transferable_shader_cache_file_path =
        transferable_shader_cache_folder_path / fmt::format("{:016X}.bin", program_id);

    if (!Common::FS::Exists(transferable_shader_cache_file_path)) {
        QMessageBox::warning(this, tr("Error Opening Transferable Shader Cache"),
                             tr("A shader cache for this title does not exist."));
        return;
    }

    const auto qt_shader_cache_folder_path =
        QString::fromStdString(Common::FS::PathToUTF8String(transferable_shader_cache_folder_path));
    const auto qt_shader_cache_file_path =
        QString::fromStdString(Common::FS::PathToUTF8String(transferable_shader_cache_file_path));

    // Windows supports opening a folder with selecting a specified file in explorer. On every other
    // OS we just open the transferable shader cache folder without preselecting the transferable
    // shader cache file for the selected game.
#if defined(Q_OS_WIN)
    const QString explorer = QStringLiteral("explorer");
    QStringList param;
    if (!QFileInfo(qt_shader_cache_file_path).isDir()) {
        param << QStringLiteral("/select,");
    }
    param << QDir::toNativeSeparators(qt_shader_cache_file_path);
    QProcess::startDetached(explorer, param);
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(qt_shader_cache_folder_path));
#endif
}

static std::size_t CalculateRomFSEntrySize(const FileSys::VirtualDir& dir, bool full) {
    std::size_t out = 0;

    for (const auto& subdir : dir->GetSubdirectories()) {
        out += 1 + CalculateRomFSEntrySize(subdir, full);
    }

    return out + (full ? dir->GetFiles().size() : 0);
}

static bool RomFSRawCopy(QProgressDialog& dialog, const FileSys::VirtualDir& src,
                         const FileSys::VirtualDir& dest, std::size_t block_size, bool full) {
    if (src == nullptr || dest == nullptr || !src->IsReadable() || !dest->IsWritable())
        return false;
    if (dialog.wasCanceled())
        return false;

    if (full) {
        for (const auto& file : src->GetFiles()) {
            const auto out = VfsDirectoryCreateFileWrapper(dest, file->GetName());
            if (!FileSys::VfsRawCopy(file, out, block_size))
                return false;
            dialog.setValue(dialog.value() + 1);
            if (dialog.wasCanceled())
                return false;
        }
    }

    for (const auto& dir : src->GetSubdirectories()) {
        const auto out = dest->CreateSubdirectory(dir->GetName());
        if (!RomFSRawCopy(dialog, dir, out, block_size, full))
            return false;
        dialog.setValue(dialog.value() + 1);
        if (dialog.wasCanceled())
            return false;
    }

    return true;
}

void GMainWindow::OnGameListRemoveInstalledEntry(u64 program_id, InstalledEntryType type) {
    const QString entry_type = [this, type] {
        switch (type) {
        case InstalledEntryType::Game:
            return tr("Contents");
        case InstalledEntryType::Update:
            return tr("Update");
        case InstalledEntryType::AddOnContent:
            return tr("DLC");
        default:
            return QString{};
        }
    }();

    if (QMessageBox::question(
            this, tr("Remove Entry"), tr("Remove Installed Game %1?").arg(entry_type),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    switch (type) {
    case InstalledEntryType::Game:
        RemoveBaseContent(program_id, entry_type);
        [[fallthrough]];
    case InstalledEntryType::Update:
        RemoveUpdateContent(program_id, entry_type);
        if (type != InstalledEntryType::Game) {
            break;
        }
        [[fallthrough]];
    case InstalledEntryType::AddOnContent:
        RemoveAddOnContent(program_id, entry_type);
        break;
    }
    Common::FS::RemoveDirRecursively(Common::FS::GetYuzuPath(Common::FS::YuzuPath::CacheDir) /
                                     "game_list");
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void GMainWindow::RemoveBaseContent(u64 program_id, const QString& entry_type) {
    const auto& fs_controller = Core::System::GetInstance().GetFileSystemController();
    const auto res = fs_controller.GetUserNANDContents()->RemoveExistingEntry(program_id) ||
                     fs_controller.GetSDMCContents()->RemoveExistingEntry(program_id);

    if (res) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the installed base game."));
    } else {
        QMessageBox::warning(
            this, tr("Error Removing %1").arg(entry_type),
            tr("The base game is not installed in the NAND and cannot be removed."));
    }
}

void GMainWindow::RemoveUpdateContent(u64 program_id, const QString& entry_type) {
    const auto update_id = program_id | 0x800;
    const auto& fs_controller = Core::System::GetInstance().GetFileSystemController();
    const auto res = fs_controller.GetUserNANDContents()->RemoveExistingEntry(update_id) ||
                     fs_controller.GetSDMCContents()->RemoveExistingEntry(update_id);

    if (res) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the installed update."));
    } else {
        QMessageBox::warning(this, tr("Error Removing %1").arg(entry_type),
                             tr("There is no update installed for this title."));
    }
}

void GMainWindow::RemoveAddOnContent(u64 program_id, const QString& entry_type) {
    u32 count{};
    const auto& fs_controller = Core::System::GetInstance().GetFileSystemController();
    const auto dlc_entries = Core::System::GetInstance().GetContentProvider().ListEntriesFilter(
        FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);

    for (const auto& entry : dlc_entries) {
        if (FileSys::GetBaseTitleID(entry.title_id) == program_id) {
            const auto res =
                fs_controller.GetUserNANDContents()->RemoveExistingEntry(entry.title_id) ||
                fs_controller.GetSDMCContents()->RemoveExistingEntry(entry.title_id);
            if (res) {
                ++count;
            }
        }
    }

    if (count == 0) {
        QMessageBox::warning(this, tr("Error Removing %1").arg(entry_type),
                             tr("There are no DLC installed for this title."));
        return;
    }

    QMessageBox::information(this, tr("Successfully Removed"),
                             tr("Successfully removed %1 installed DLC.").arg(count));
}

void GMainWindow::OnGameListRemoveFile(u64 program_id, GameListRemoveTarget target,
                                       const std::string& game_path) {
    const QString question = [this, target] {
        switch (target) {
        case GameListRemoveTarget::ShaderCache:
            return tr("Delete Transferable Shader Cache?");
        case GameListRemoveTarget::CustomConfiguration:
            return tr("Remove Custom Game Configuration?");
        default:
            return QString{};
        }
    }();

    if (QMessageBox::question(this, tr("Remove File"), question, QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    switch (target) {
    case GameListRemoveTarget::ShaderCache:
        RemoveTransferableShaderCache(program_id);
        break;
    case GameListRemoveTarget::CustomConfiguration:
        RemoveCustomConfiguration(program_id, game_path);
        break;
    }
}

void GMainWindow::RemoveTransferableShaderCache(u64 program_id) {
    const auto shader_cache_dir = Common::FS::GetYuzuPath(Common::FS::YuzuPath::ShaderDir);
    const auto transferable_shader_cache_file_path =
        shader_cache_dir / "opengl" / "transferable" / fmt::format("{:016X}.bin", program_id);

    if (!Common::FS::Exists(transferable_shader_cache_file_path)) {
        QMessageBox::warning(this, tr("Error Removing Transferable Shader Cache"),
                             tr("A shader cache for this title does not exist."));
        return;
    }

    if (Common::FS::RemoveFile(transferable_shader_cache_file_path)) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the transferable shader cache."));
    } else {
        QMessageBox::warning(this, tr("Error Removing Transferable Shader Cache"),
                             tr("Failed to remove the transferable shader cache."));
    }
}

void GMainWindow::RemoveCustomConfiguration(u64 program_id, const std::string& game_path) {
    const auto file_path = std::filesystem::path(Common::FS::ToU8String(game_path));
    const auto config_file_name =
        program_id == 0 ? Common::FS::PathToUTF8String(file_path.filename()).append(".ini")
                        : fmt::format("{:016X}.ini", program_id);
    const auto custom_config_file_path =
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir) / "custom" / config_file_name;

    if (!Common::FS::Exists(custom_config_file_path)) {
        QMessageBox::warning(this, tr("Error Removing Custom Configuration"),
                             tr("A custom configuration for this title does not exist."));
        return;
    }

    if (Common::FS::RemoveFile(custom_config_file_path)) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the custom game configuration."));
    } else {
        QMessageBox::warning(this, tr("Error Removing Custom Configuration"),
                             tr("Failed to remove the custom game configuration."));
    }
}

void GMainWindow::OnGameListDumpRomFS(u64 program_id, const std::string& game_path,
                                      DumpRomFSTarget target) {
    const auto failed = [this] {
        QMessageBox::warning(this, tr("RomFS Extraction Failed!"),
                             tr("There was an error copying the RomFS files or the user "
                                "cancelled the operation."));
    };

    auto& system = Core::System::GetInstance();
    const auto loader = Loader::GetLoader(system, vfs->OpenFile(game_path, FileSys::Mode::Read));
    if (loader == nullptr) {
        failed();
        return;
    }

    FileSys::VirtualFile file;
    if (loader->ReadRomFS(file) != Loader::ResultStatus::Success) {
        failed();
        return;
    }

    const auto& installed = system.GetContentProvider();
    const auto romfs_title_id = SelectRomFSDumpTarget(installed, program_id);

    if (!romfs_title_id) {
        failed();
        return;
    }

    const auto dump_dir =
        target == DumpRomFSTarget::Normal
            ? Common::FS::GetYuzuPath(Common::FS::YuzuPath::DumpDir)
            : Common::FS::GetYuzuPath(Common::FS::YuzuPath::SDMCDir) / "atmosphere" / "contents";
    const auto romfs_dir = fmt::format("{:016X}/romfs", *romfs_title_id);

    const auto path = Common::FS::PathToUTF8String(dump_dir / romfs_dir);

    FileSys::VirtualFile romfs;

    if (*romfs_title_id == program_id) {
        const u64 ivfc_offset = loader->ReadRomFSIVFCOffset();
        const FileSys::PatchManager pm{program_id, system.GetFileSystemController(), installed};
        romfs =
            pm.PatchRomFS(file, ivfc_offset, FileSys::ContentRecordType::Program, nullptr, false);
    } else {
        romfs = installed.GetEntry(*romfs_title_id, FileSys::ContentRecordType::Data)->GetRomFS();
    }

    const auto extracted = FileSys::ExtractRomFS(romfs, FileSys::RomFSExtractionType::Full);
    if (extracted == nullptr) {
        failed();
        return;
    }

    const auto out = VfsFilesystemCreateDirectoryWrapper(vfs, path, FileSys::Mode::ReadWrite);

    if (out == nullptr) {
        failed();
        vfs->DeleteDirectory(path);
        return;
    }

    bool ok = false;
    const QStringList selections{tr("Full"), tr("Skeleton")};
    const auto res = QInputDialog::getItem(
        this, tr("Select RomFS Dump Mode"),
        tr("Please select the how you would like the RomFS dumped.<br>Full will copy all of the "
           "files into the new directory while <br>skeleton will only create the directory "
           "structure."),
        selections, 0, false, &ok);
    if (!ok) {
        failed();
        vfs->DeleteDirectory(path);
        return;
    }

    const auto full = res == selections.constFirst();
    const auto entry_size = CalculateRomFSEntrySize(extracted, full);

    // The minimum required space is the size of the extracted RomFS + 1 GiB
    const auto minimum_free_space = extracted->GetSize() + 0x40000000;

    if (full && Common::FS::GetFreeSpaceSize(path) < minimum_free_space) {
        QMessageBox::warning(this, tr("RomFS Extraction Failed!"),
                             tr("There is not enough free space at %1 to extract the RomFS. Please "
                                "free up space or select a different dump directory at "
                                "Emulation > Configure > System > Filesystem > Dump Root")
                                 .arg(QString::fromStdString(path)));
        return;
    }

    QProgressDialog progress(tr("Extracting RomFS..."), tr("Cancel"), 0,
                             static_cast<s32>(entry_size), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);

    if (RomFSRawCopy(progress, extracted, out, 0x400000, full)) {
        progress.close();
        QMessageBox::information(this, tr("RomFS Extraction Succeeded!"),
                                 tr("The operation completed successfully."));
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
    } else {
        progress.close();
        failed();
        vfs->DeleteDirectory(path);
    }
}

void GMainWindow::OnGameListCopyTID(u64 program_id) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(QString::fromStdString(fmt::format("{:016X}", program_id)));
}

void GMainWindow::OnGameListNavigateToGamedbEntry(u64 program_id,
                                                  const CompatibilityList& compatibility_list) {
    const auto it = FindMatchingCompatibilityEntry(compatibility_list, program_id);

    QString directory;
    if (it != compatibility_list.end()) {
        directory = it->second.second;
    }

    QDesktopServices::openUrl(QUrl(QStringLiteral("https://yuzu-emu.org/game/") + directory));
}

void GMainWindow::OnGameListOpenDirectory(const QString& directory) {
    std::filesystem::path fs_path;
    if (directory == QStringLiteral("SDMC")) {
        fs_path =
            Common::FS::GetYuzuPath(Common::FS::YuzuPath::SDMCDir) / "Nintendo/Contents/registered";
    } else if (directory == QStringLiteral("UserNAND")) {
        fs_path =
            Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "user/Contents/registered";
    } else if (directory == QStringLiteral("SysNAND")) {
        fs_path =
            Common::FS::GetYuzuPath(Common::FS::YuzuPath::NANDDir) / "system/Contents/registered";
    } else {
        fs_path = directory.toStdString();
    }

    const auto qt_path = QString::fromStdString(Common::FS::PathToUTF8String(fs_path));

    if (!Common::FS::IsDir(fs_path)) {
        QMessageBox::critical(this, tr("Error Opening %1").arg(qt_path),
                              tr("Folder does not exist!"));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(qt_path));
}

void GMainWindow::OnGameListAddDirectory() {
    const QString dir_path = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
    if (dir_path.isEmpty()) {
        return;
    }

    UISettings::GameDir game_dir{dir_path, false, true};
    if (!UISettings::values.game_dirs.contains(game_dir)) {
        UISettings::values.game_dirs.append(game_dir);
        game_list->PopulateAsync(UISettings::values.game_dirs);
    } else {
        LOG_WARNING(Frontend, "Selected directory is already in the game list");
    }
}

void GMainWindow::OnGameListShowList(bool show) {
    if (emulation_running && ui.action_Single_Window_Mode->isChecked())
        return;
    game_list->setVisible(show);
    game_list_placeholder->setVisible(!show);
};

void GMainWindow::OnGameListOpenPerGameProperties(const std::string& file) {
    u64 title_id{};
    const auto v_file = Core::GetGameFileFromPath(vfs, file);
    const auto loader = Loader::GetLoader(Core::System::GetInstance(), v_file);

    if (loader == nullptr || loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        QMessageBox::information(this, tr("Properties"),
                                 tr("The game properties could not be loaded."));
        return;
    }

    OpenPerGameConfiguration(title_id, file);
}

void GMainWindow::OnMenuLoadFile() {
    const QString extensions =
        QStringLiteral("*.")
            .append(GameList::supported_file_extensions.join(QStringLiteral(" *.")))
            .append(QStringLiteral(" main"));
    const QString file_filter = tr("Switch Executable (%1);;All Files (*.*)",
                                   "%1 is an identifier for the Switch executable file extensions.")
                                    .arg(extensions);
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load File"), UISettings::values.roms_path, file_filter);

    if (filename.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filename).path();
    BootGame(filename);
}

void GMainWindow::OnMenuLoadFolder() {
    const QString dir_path =
        QFileDialog::getExistingDirectory(this, tr("Open Extracted ROM Directory"));

    if (dir_path.isNull()) {
        return;
    }

    const QDir dir{dir_path};
    const QStringList matching_main = dir.entryList({QStringLiteral("main")}, QDir::Files);
    if (matching_main.size() == 1) {
        BootGame(dir.path() + QDir::separator() + matching_main[0]);
    } else {
        QMessageBox::warning(this, tr("Invalid Directory Selected"),
                             tr("The directory you have selected does not contain a 'main' file."));
    }
}

void GMainWindow::IncrementInstallProgress() {
    install_progress->setValue(install_progress->value() + 1);
}

void GMainWindow::OnMenuInstallToNAND() {
    const QString file_filter =
        tr("Installable Switch File (*.nca *.nsp *.xci);;Nintendo Content Archive "
           "(*.nca);;Nintendo Submission Package (*.nsp);;NX Cartridge "
           "Image (*.xci)");

    QStringList filenames = QFileDialog::getOpenFileNames(
        this, tr("Install Files"), UISettings::values.roms_path, file_filter);

    if (filenames.isEmpty()) {
        return;
    }

    InstallDialog installDialog(this, filenames);
    if (installDialog.exec() == QDialog::Rejected) {
        return;
    }

    const QStringList files = installDialog.GetFiles();

    if (files.isEmpty()) {
        return;
    }

    int remaining = filenames.size();

    // This would only overflow above 2^43 bytes (8.796 TB)
    int total_size = 0;
    for (const QString& file : files) {
        total_size += static_cast<int>(QFile(file).size() / 0x1000);
    }
    if (total_size < 0) {
        LOG_CRITICAL(Frontend, "Attempting to install too many files, aborting.");
        return;
    }

    QStringList new_files{};         // Newly installed files that do not yet exist in the NAND
    QStringList overwritten_files{}; // Files that overwrote those existing in the NAND
    QStringList failed_files{};      // Files that failed to install due to errors
    bool detected_base_install{};    // Whether a base game was attempted to be installed

    ui.action_Install_File_NAND->setEnabled(false);

    install_progress = new QProgressDialog(QString{}, tr("Cancel"), 0, total_size, this);
    install_progress->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint &
                                     ~Qt::WindowMaximizeButtonHint);
    install_progress->setAttribute(Qt::WA_DeleteOnClose, true);
    install_progress->setFixedWidth(installDialog.GetMinimumWidth() + 40);
    install_progress->show();

    for (const QString& file : files) {
        install_progress->setWindowTitle(tr("%n file(s) remaining", "", remaining));
        install_progress->setLabelText(
            tr("Installing file \"%1\"...").arg(QFileInfo(file).fileName()));

        QFuture<InstallResult> future;
        InstallResult result;

        if (file.endsWith(QStringLiteral("xci"), Qt::CaseInsensitive) ||
            file.endsWith(QStringLiteral("nsp"), Qt::CaseInsensitive)) {

            future = QtConcurrent::run([this, &file] { return InstallNSPXCI(file); });

            while (!future.isFinished()) {
                QCoreApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            result = future.result();

        } else {
            result = InstallNCA(file);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        switch (result) {
        case InstallResult::Success:
            new_files.append(QFileInfo(file).fileName());
            break;
        case InstallResult::Overwrite:
            overwritten_files.append(QFileInfo(file).fileName());
            break;
        case InstallResult::Failure:
            failed_files.append(QFileInfo(file).fileName());
            break;
        case InstallResult::BaseInstallAttempted:
            failed_files.append(QFileInfo(file).fileName());
            detected_base_install = true;
            break;
        }

        --remaining;
    }

    install_progress->close();

    if (detected_base_install) {
        QMessageBox::warning(
            this, tr("Install Results"),
            tr("To avoid possible conflicts, we discourage users from installing base games to the "
               "NAND.\nPlease, only use this feature to install updates and DLC."));
    }

    const QString install_results =
        (new_files.isEmpty() ? QString{}
                             : tr("%n file(s) were newly installed\n", "", new_files.size())) +
        (overwritten_files.isEmpty()
             ? QString{}
             : tr("%n file(s) were overwritten\n", "", overwritten_files.size())) +
        (failed_files.isEmpty() ? QString{}
                                : tr("%n file(s) failed to install\n", "", failed_files.size()));

    QMessageBox::information(this, tr("Install Results"), install_results);
    Common::FS::RemoveDirRecursively(Common::FS::GetYuzuPath(Common::FS::YuzuPath::CacheDir) /
                                     "game_list");
    game_list->PopulateAsync(UISettings::values.game_dirs);
    ui.action_Install_File_NAND->setEnabled(true);
}

InstallResult GMainWindow::InstallNSPXCI(const QString& filename) {
    const auto qt_raw_copy = [this](const FileSys::VirtualFile& src,
                                    const FileSys::VirtualFile& dest, std::size_t block_size) {
        if (src == nullptr || dest == nullptr) {
            return false;
        }
        if (!dest->Resize(src->GetSize())) {
            return false;
        }

        std::array<u8, 0x1000> buffer{};

        for (std::size_t i = 0; i < src->GetSize(); i += buffer.size()) {
            if (install_progress->wasCanceled()) {
                dest->Resize(0);
                return false;
            }

            emit UpdateInstallProgress();

            const auto read = src->Read(buffer.data(), buffer.size(), i);
            dest->Write(buffer.data(), read, i);
        }
        return true;
    };

    std::shared_ptr<FileSys::NSP> nsp;
    if (filename.endsWith(QStringLiteral("nsp"), Qt::CaseInsensitive)) {
        nsp = std::make_shared<FileSys::NSP>(
            vfs->OpenFile(filename.toStdString(), FileSys::Mode::Read));
        if (nsp->IsExtractedType()) {
            return InstallResult::Failure;
        }
    } else {
        const auto xci = std::make_shared<FileSys::XCI>(
            vfs->OpenFile(filename.toStdString(), FileSys::Mode::Read));
        nsp = xci->GetSecurePartitionNSP();
    }

    if (nsp->GetStatus() != Loader::ResultStatus::Success) {
        return InstallResult::Failure;
    }
    const auto res =
        Core::System::GetInstance().GetFileSystemController().GetUserNANDContents()->InstallEntry(
            *nsp, true, qt_raw_copy);
    switch (res) {
    case FileSys::InstallResult::Success:
        return InstallResult::Success;
    case FileSys::InstallResult::OverwriteExisting:
        return InstallResult::Overwrite;
    case FileSys::InstallResult::ErrorBaseInstall:
        return InstallResult::BaseInstallAttempted;
    default:
        return InstallResult::Failure;
    }
}

InstallResult GMainWindow::InstallNCA(const QString& filename) {
    const auto qt_raw_copy = [this](const FileSys::VirtualFile& src,
                                    const FileSys::VirtualFile& dest, std::size_t block_size) {
        if (src == nullptr || dest == nullptr) {
            return false;
        }
        if (!dest->Resize(src->GetSize())) {
            return false;
        }

        std::array<u8, 0x1000> buffer{};

        for (std::size_t i = 0; i < src->GetSize(); i += buffer.size()) {
            if (install_progress->wasCanceled()) {
                dest->Resize(0);
                return false;
            }

            emit UpdateInstallProgress();

            const auto read = src->Read(buffer.data(), buffer.size(), i);
            dest->Write(buffer.data(), read, i);
        }
        return true;
    };

    const auto nca =
        std::make_shared<FileSys::NCA>(vfs->OpenFile(filename.toStdString(), FileSys::Mode::Read));
    const auto id = nca->GetStatus();

    // Game updates necessary are missing base RomFS
    if (id != Loader::ResultStatus::Success &&
        id != Loader::ResultStatus::ErrorMissingBKTRBaseRomFS) {
        return InstallResult::Failure;
    }

    const QStringList tt_options{tr("System Application"),
                                 tr("System Archive"),
                                 tr("System Application Update"),
                                 tr("Firmware Package (Type A)"),
                                 tr("Firmware Package (Type B)"),
                                 tr("Game"),
                                 tr("Game Update"),
                                 tr("Game DLC"),
                                 tr("Delta Title")};
    bool ok;
    const auto item = QInputDialog::getItem(
        this, tr("Select NCA Install Type..."),
        tr("Please select the type of title you would like to install this NCA as:\n(In "
           "most instances, the default 'Game' is fine.)"),
        tt_options, 5, false, &ok);

    auto index = tt_options.indexOf(item);
    if (!ok || index == -1) {
        QMessageBox::warning(this, tr("Failed to Install"),
                             tr("The title type you selected for the NCA is invalid."));
        return InstallResult::Failure;
    }

    // If index is equal to or past Game, add the jump in TitleType.
    if (index >= 5) {
        index += static_cast<size_t>(FileSys::TitleType::Application) -
                 static_cast<size_t>(FileSys::TitleType::FirmwarePackageB);
    }

    FileSys::InstallResult res;
    if (index >= static_cast<s32>(FileSys::TitleType::Application)) {
        res = Core::System::GetInstance()
                  .GetFileSystemController()
                  .GetUserNANDContents()
                  ->InstallEntry(*nca, static_cast<FileSys::TitleType>(index), true, qt_raw_copy);
    } else {
        res = Core::System::GetInstance()
                  .GetFileSystemController()
                  .GetSystemNANDContents()
                  ->InstallEntry(*nca, static_cast<FileSys::TitleType>(index), true, qt_raw_copy);
    }

    if (res == FileSys::InstallResult::Success) {
        return InstallResult::Success;
    } else if (res == FileSys::InstallResult::OverwriteExisting) {
        return InstallResult::Overwrite;
    } else {
        return InstallResult::Failure;
    }
}

void GMainWindow::OnMenuRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    assert(action);

    const QString filename = action->data().toString();
    if (QFileInfo::exists(filename)) {
        BootGame(filename);
    } else {
        // Display an error message and remove the file from the list.
        QMessageBox::information(this, tr("File not found"),
                                 tr("File \"%1\" not found").arg(filename));

        UISettings::values.recent_files.removeOne(filename);
        UpdateRecentFiles();
    }
}

void GMainWindow::OnStartGame() {
    PreventOSSleep();

    emu_thread->SetRunning(true);

    connect(emu_thread.get(), &EmuThread::ErrorThrown, this, &GMainWindow::OnCoreError);

    ui.action_Start->setEnabled(false);
    ui.action_Start->setText(tr("&Continue"));

    ui.action_Pause->setEnabled(true);
    ui.action_Stop->setEnabled(true);
    ui.action_Restart->setEnabled(true);
    ui.action_Configure_Current_Game->setEnabled(true);
    ui.action_Report_Compatibility->setEnabled(true);

    discord_rpc->Update();
    ui.action_Load_Amiibo->setEnabled(true);
    ui.action_Capture_Screenshot->setEnabled(true);
}

void GMainWindow::OnPauseGame() {
    emu_thread->SetRunning(false);

    ui.action_Start->setEnabled(true);
    ui.action_Pause->setEnabled(false);
    ui.action_Stop->setEnabled(true);
    ui.action_Capture_Screenshot->setEnabled(false);

    AllowOSSleep();
}

void GMainWindow::OnStopGame() {
    auto& system{Core::System::GetInstance()};
    if (system.GetExitLock() && !ConfirmForceLockedExit()) {
        return;
    }

    ShutdownGame();

    Settings::RestoreGlobalState(system.IsPoweredOn());
    UpdateStatusButtons();
}

void GMainWindow::OnLoadComplete() {
    loading_screen->OnLoadComplete();
}

void GMainWindow::OnExecuteProgram(std::size_t program_index) {
    ShutdownGame();
    BootGame(last_filename_booted, 0, program_index);
}

void GMainWindow::ErrorDisplayDisplayError(QString error_code, QString error_text) {
    OverlayDialog dialog(render_window, Core::System::GetInstance(), error_code, error_text,
                         QString{}, tr("OK"), Qt::AlignLeft | Qt::AlignVCenter);
    dialog.exec();

    emit ErrorDisplayFinished();
}

void GMainWindow::OnMenuReportCompatibility() {
    if (!Settings::values.yuzu_token.GetValue().empty() &&
        !Settings::values.yuzu_username.GetValue().empty()) {
        CompatDB compatdb{this};
        compatdb.exec();
    } else {
        QMessageBox::critical(
            this, tr("Missing yuzu Account"),
            tr("In order to submit a game compatibility test case, you must link your yuzu "
               "account.<br><br/>To link your yuzu account, go to Emulation &gt; Configuration "
               "&gt; "
               "Web."));
    }
}

void GMainWindow::OpenURL(const QUrl& url) {
    const bool open = QDesktopServices::openUrl(url);
    if (!open) {
        QMessageBox::warning(this, tr("Error opening URL"),
                             tr("Unable to open the URL \"%1\".").arg(url.toString()));
    }
}

void GMainWindow::OnOpenModsPage() {
    OpenURL(QUrl(QStringLiteral("https://github.com/yuzu-emu/yuzu/wiki/Switch-Mods")));
}

void GMainWindow::OnOpenQuickstartGuide() {
    OpenURL(QUrl(QStringLiteral("https://yuzu-emu.org/help/quickstart/")));
}

void GMainWindow::OnOpenFAQ() {
    OpenURL(QUrl(QStringLiteral("https://yuzu-emu.org/wiki/faq/")));
}

void GMainWindow::ToggleFullscreen() {
    if (!emulation_running) {
        return;
    }
    if (ui.action_Fullscreen->isChecked()) {
        ShowFullscreen();
    } else {
        HideFullscreen();
    }
}

void GMainWindow::ShowFullscreen() {
    if (ui.action_Single_Window_Mode->isChecked()) {
        UISettings::values.geometry = saveGeometry();

        ui.menubar->hide();
        statusBar()->hide();

        if (Settings::values.fullscreen_mode.GetValue() == 1) {
            showFullScreen();
            return;
        }

        hide();
        setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        const auto screen_geometry = QApplication::desktop()->screenGeometry(this);
        setGeometry(screen_geometry.x(), screen_geometry.y(), screen_geometry.width(),
                    screen_geometry.height() + 1);
        raise();
        showNormal();
    } else {
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();

        if (Settings::values.fullscreen_mode.GetValue() == 1) {
            render_window->showFullScreen();
            return;
        }

        render_window->hide();
        render_window->setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
        const auto screen_geometry = QApplication::desktop()->screenGeometry(this);
        render_window->setGeometry(screen_geometry.x(), screen_geometry.y(),
                                   screen_geometry.width(), screen_geometry.height() + 1);
        render_window->raise();
        render_window->showNormal();
    }
}

void GMainWindow::HideFullscreen() {
    if (ui.action_Single_Window_Mode->isChecked()) {
        if (Settings::values.fullscreen_mode.GetValue() == 1) {
            showNormal();
            restoreGeometry(UISettings::values.geometry);
        } else {
            hide();
            setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
            restoreGeometry(UISettings::values.geometry);
            raise();
            show();
        }

        statusBar()->setVisible(ui.action_Show_Status_Bar->isChecked());
        ui.menubar->show();
    } else {
        if (Settings::values.fullscreen_mode.GetValue() == 1) {
            render_window->showNormal();
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
        } else {
            render_window->hide();
            render_window->setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
            render_window->raise();
            render_window->show();
        }
    }
}

void GMainWindow::ToggleWindowMode() {
    if (ui.action_Single_Window_Mode->isChecked()) {
        // Render in the main window...
        render_window->BackupGeometry();
        ui.horizontalLayout->addWidget(render_window);
        render_window->setFocusPolicy(Qt::StrongFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->setFocus();
            game_list->hide();
        }

    } else {
        // Render in a separate window...
        ui.horizontalLayout->removeWidget(render_window);
        render_window->setParent(nullptr);
        render_window->setFocusPolicy(Qt::NoFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->RestoreGeometry();
            game_list->show();
        }
    }
}

void GMainWindow::ResetWindowSize720() {
    const auto aspect_ratio = Layout::EmulationAspectRatio(
        static_cast<Layout::AspectRatio>(Settings::values.aspect_ratio.GetValue()),
        static_cast<float>(Layout::ScreenUndocked::Height) / Layout::ScreenUndocked::Width);
    if (!ui.action_Single_Window_Mode->isChecked()) {
        render_window->resize(Layout::ScreenUndocked::Height / aspect_ratio,
                              Layout::ScreenUndocked::Height);
    } else {
        resize(Layout::ScreenUndocked::Height / aspect_ratio,
               Layout::ScreenUndocked::Height + menuBar()->height() +
                   (ui.action_Show_Status_Bar->isChecked() ? statusBar()->height() : 0));
    }
}

void GMainWindow::ResetWindowSize1080() {
    const auto aspect_ratio = Layout::EmulationAspectRatio(
        static_cast<Layout::AspectRatio>(Settings::values.aspect_ratio.GetValue()),
        static_cast<float>(Layout::ScreenDocked::Height) / Layout::ScreenDocked::Width);
    if (!ui.action_Single_Window_Mode->isChecked()) {
        render_window->resize(Layout::ScreenDocked::Height / aspect_ratio,
                              Layout::ScreenDocked::Height);
    } else {
        resize(Layout::ScreenDocked::Height / aspect_ratio,
               Layout::ScreenDocked::Height + menuBar()->height() +
                   (ui.action_Show_Status_Bar->isChecked() ? statusBar()->height() : 0));
    }
}

void GMainWindow::OnConfigure() {
    const auto old_theme = UISettings::values.theme;
    const bool old_discord_presence = UISettings::values.enable_discord_presence.GetValue();

    ConfigureDialog configure_dialog(this, hotkey_registry, input_subsystem.get());
    connect(&configure_dialog, &ConfigureDialog::LanguageChanged, this,
            &GMainWindow::OnLanguageChanged);

    const auto result = configure_dialog.exec();
    if (result != QDialog::Accepted && !UISettings::values.configuration_applied &&
        !UISettings::values.reset_to_defaults) {
        // Runs if the user hit Cancel or closed the window, and did not ever press the Apply button
        // or `Reset to Defaults` button
        return;
    } else if (result == QDialog::Accepted) {
        // Only apply new changes if user hit Okay
        // This is here to avoid applying changes if the user hit Apply, made some changes, then hit
        // Cancel
        configure_dialog.ApplyConfiguration();
    } else if (UISettings::values.reset_to_defaults) {
        LOG_INFO(Frontend, "Resetting all settings to defaults");
        if (!Common::FS::RemoveFile(config->GetConfigFilePath())) {
            LOG_WARNING(Frontend, "Failed to remove configuration file");
        }
        if (!Common::FS::RemoveDirContentsRecursively(
                Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir) / "custom")) {
            LOG_WARNING(Frontend, "Failed to remove custom configuration files");
        }
        if (!Common::FS::RemoveDirRecursively(
                Common::FS::GetYuzuPath(Common::FS::YuzuPath::CacheDir) / "game_list")) {
            LOG_WARNING(Frontend, "Failed to remove game metadata cache files");
        }

        // Explicitly save the game directories, since reinitializing config does not explicitly do
        // so.
        QVector<UISettings::GameDir> old_game_dirs = std::move(UISettings::values.game_dirs);
        QVector<u64> old_favorited_ids = std::move(UISettings::values.favorited_ids);

        Settings::values.disabled_addons.clear();

        config = std::make_unique<Config>();
        UISettings::values.reset_to_defaults = false;

        UISettings::values.game_dirs = std::move(old_game_dirs);
        UISettings::values.favorited_ids = std::move(old_favorited_ids);

        InitializeRecentFileMenuActions();

        SetDefaultUIGeometry();
        RestoreUIState();

        ShowTelemetryCallout();
    }
    controller_dialog->refreshConfiguration();
    InitializeHotkeys();

    if (UISettings::values.theme != old_theme) {
        UpdateUITheme();
    }
    if (UISettings::values.enable_discord_presence.GetValue() != old_discord_presence) {
        SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    }
    emit UpdateThemedIcons();

    const auto reload = UISettings::values.is_game_list_reload_pending.exchange(false);
    if (reload) {
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }

    UISettings::values.configuration_applied = false;

    config->Save();

    if ((UISettings::values.hide_mouse || Settings::values.mouse_panning) && emulation_running) {
        render_window->installEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, true);
    } else {
        render_window->removeEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, false);
    }

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }

    UpdateStatusButtons();
}

void GMainWindow::OnConfigurePerGame() {
    const u64 title_id = Core::System::GetInstance().CurrentProcess()->GetTitleID();
    OpenPerGameConfiguration(title_id, game_path.toStdString());
}

void GMainWindow::OpenPerGameConfiguration(u64 title_id, const std::string& file_name) {
    const auto v_file = Core::GetGameFileFromPath(vfs, file_name);
    const auto& system = Core::System::GetInstance();

    ConfigurePerGame dialog(this, title_id, file_name);
    dialog.LoadFromFile(v_file);
    const auto result = dialog.exec();

    if (result != QDialog::Accepted && !UISettings::values.configuration_applied) {
        Settings::RestoreGlobalState(system.IsPoweredOn());
        return;
    } else if (result == QDialog::Accepted) {
        dialog.ApplyConfiguration();
    }

    const auto reload = UISettings::values.is_game_list_reload_pending.exchange(false);
    if (reload) {
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }

    // Do not cause the global config to write local settings into the config file
    const bool is_powered_on = system.IsPoweredOn();
    Settings::RestoreGlobalState(is_powered_on);

    UISettings::values.configuration_applied = false;

    if (!is_powered_on) {
        config->Save();
    }
}

void GMainWindow::OnLoadAmiibo() {
    const QString extensions{QStringLiteral("*.bin")};
    const QString file_filter = tr("Amiibo File (%1);; All Files (*.*)").arg(extensions);
    const QString filename = QFileDialog::getOpenFileName(this, tr("Load Amiibo"), {}, file_filter);

    if (filename.isEmpty()) {
        return;
    }

    LoadAmiibo(filename);
}

void GMainWindow::LoadAmiibo(const QString& filename) {
    Core::System& system{Core::System::GetInstance()};
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFP::Module::Interface>("nfp:user");
    if (nfc == nullptr) {
        return;
    }

    QFile nfc_file{filename};
    if (!nfc_file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error opening Amiibo data file"),
                             tr("Unable to open Amiibo file \"%1\" for reading.").arg(filename));
        return;
    }

    const u64 nfc_file_size = nfc_file.size();
    std::vector<u8> buffer(nfc_file_size);
    const u64 read_size = nfc_file.read(reinterpret_cast<char*>(buffer.data()), nfc_file_size);
    if (nfc_file_size != read_size) {
        QMessageBox::warning(this, tr("Error reading Amiibo data file"),
                             tr("Unable to fully read Amiibo data. Expected to read %1 bytes, but "
                                "was only able to read %2 bytes.")
                                 .arg(nfc_file_size)
                                 .arg(read_size));
        return;
    }

    if (!nfc->LoadAmiibo(buffer)) {
        QMessageBox::warning(this, tr("Error loading Amiibo data"),
                             tr("Unable to load Amiibo data."));
    }
}

void GMainWindow::OnOpenYuzuFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(Common::FS::GetYuzuPathString(Common::FS::YuzuPath::YuzuDir))));
}

void GMainWindow::OnAbout() {
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void GMainWindow::OnToggleFilterBar() {
    game_list->SetFilterVisible(ui.action_Show_Filter_Bar->isChecked());
    if (ui.action_Show_Filter_Bar->isChecked()) {
        game_list->SetFilterFocus();
    } else {
        game_list->ClearFilter();
    }
}

void GMainWindow::OnCaptureScreenshot() {
    OnPauseGame();

    const u64 title_id = Core::System::GetInstance().CurrentProcess()->GetTitleID();
    const auto screenshot_path =
        QString::fromStdString(Common::FS::GetYuzuPathString(Common::FS::YuzuPath::ScreenshotsDir));
    const auto date =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz"));
    QString filename = QStringLiteral("%1%2_%3.png")
                           .arg(screenshot_path)
                           .arg(title_id, 16, 16, QLatin1Char{'0'})
                           .arg(date);

    if (!Common::FS::CreateDir(screenshot_path.toStdString())) {
        OnStartGame();
        return;
    }

#ifdef _WIN32
    if (UISettings::values.enable_screenshot_save_as) {
        filename = QFileDialog::getSaveFileName(this, tr("Capture Screenshot"), filename,
                                                tr("PNG Image (*.png)"));
        if (filename.isEmpty()) {
            OnStartGame();
            return;
        }
    }
#endif
    render_window->CaptureScreenshot(UISettings::values.screenshot_resolution_factor.GetValue(),
                                     filename);
    OnStartGame();
}

// TODO: Written 2020-10-01: Remove per-game config migration code when it is irrelevant
void GMainWindow::MigrateConfigFiles() {
    const auto config_dir_fs_path = Common::FS::GetYuzuPath(Common::FS::YuzuPath::ConfigDir);
    const QDir config_dir =
        QDir(QString::fromStdString(Common::FS::PathToUTF8String(config_dir_fs_path)));
    const QStringList config_dir_list = config_dir.entryList(QStringList(QStringLiteral("*.ini")));

    if (!Common::FS::CreateDirs(config_dir_fs_path / "custom")) {
        LOG_ERROR(Frontend, "Failed to create new config file directory");
    }

    for (auto it = config_dir_list.constBegin(); it != config_dir_list.constEnd(); ++it) {
        const auto filename = it->toStdString();
        if (filename.find_first_not_of("0123456789abcdefACBDEF", 0) < 16) {
            continue;
        }
        const auto origin = config_dir_fs_path / filename;
        const auto destination = config_dir_fs_path / "custom" / filename;
        LOG_INFO(Frontend, "Migrating config file from {} to {}", origin, destination);
        if (!Common::FS::RenameFile(origin, destination)) {
            // Delete the old config file if one already exists in the new location.
            Common::FS::RemoveFile(origin);
        }
    }
}

void GMainWindow::UpdateWindowTitle(std::string_view title_name, std::string_view title_version,
                                    std::string_view gpu_vendor) {
    const auto branch_name = std::string(Common::g_scm_branch);
    const auto description = std::string(Common::g_scm_desc);
    const auto build_id = std::string(Common::g_build_id);

    const auto yuzu_title = fmt::format("yuzu | {}-{}", branch_name, description);
    const auto override_title =
        fmt::format(fmt::runtime(std::string(Common::g_title_bar_format_idle)), build_id);
    const auto window_title = override_title.empty() ? yuzu_title : override_title;

    if (title_name.empty()) {
        setWindowTitle(QString::fromStdString(window_title));
    } else {
        const auto run_title =
            fmt::format("{} | {} | {} | {}", window_title, title_name, title_version, gpu_vendor);
        setWindowTitle(QString::fromStdString(run_title));
    }
}

void GMainWindow::UpdateStatusBar() {
    if (emu_thread == nullptr) {
        status_bar_update_timer.stop();
        return;
    }

    auto results = Core::System::GetInstance().GetAndResetPerfStats();
    auto& shader_notify = Core::System::GetInstance().GPU().ShaderNotify();
    const auto shaders_building = shader_notify.GetShadersBuilding();

    if (shaders_building != 0) {
        shader_building_label->setText(
            tr("Building: %n shader(s)", "", static_cast<int>(shaders_building)));
        shader_building_label->setVisible(true);
    } else {
        shader_building_label->setVisible(false);
    }

    if (Settings::values.use_frame_limit.GetValue()) {
        emu_speed_label->setText(tr("Speed: %1% / %2%")
                                     .arg(results.emulation_speed * 100.0, 0, 'f', 0)
                                     .arg(Settings::values.frame_limit.GetValue()));
    } else {
        emu_speed_label->setText(tr("Speed: %1%").arg(results.emulation_speed * 100.0, 0, 'f', 0));
    }
    if (Settings::values.disable_fps_limit) {
        game_fps_label->setText(
            tr("Game: %1 FPS (Limit off)").arg(results.average_game_fps, 0, 'f', 0));
    } else {
        game_fps_label->setText(tr("Game: %1 FPS").arg(results.average_game_fps, 0, 'f', 0));
    }
    emu_frametime_label->setText(tr("Frame: %1 ms").arg(results.frametime * 1000.0, 0, 'f', 2));

    emu_speed_label->setVisible(!Settings::values.use_multi_core.GetValue());
    game_fps_label->setVisible(true);
    emu_frametime_label->setVisible(true);
}

void GMainWindow::UpdateStatusButtons() {
    dock_status_button->setChecked(Settings::values.use_docked_mode.GetValue());
    multicore_status_button->setChecked(Settings::values.use_multi_core.GetValue());
    async_status_button->setChecked(Settings::values.use_asynchronous_gpu_emulation.GetValue());
    renderer_status_button->setChecked(Settings::values.renderer_backend.GetValue() ==
                                       Settings::RendererBackend::Vulkan);
}

void GMainWindow::UpdateUISettings() {
    if (!ui.action_Fullscreen->isChecked()) {
        UISettings::values.geometry = saveGeometry();
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
    }
    UISettings::values.state = saveState();
#if MICROPROFILE_ENABLED
    UISettings::values.microprofile_geometry = microProfileDialog->saveGeometry();
    UISettings::values.microprofile_visible = microProfileDialog->isVisible();
#endif
    UISettings::values.single_window_mode = ui.action_Single_Window_Mode->isChecked();
    UISettings::values.fullscreen = ui.action_Fullscreen->isChecked();
    UISettings::values.display_titlebar = ui.action_Display_Dock_Widget_Headers->isChecked();
    UISettings::values.show_filter_bar = ui.action_Show_Filter_Bar->isChecked();
    UISettings::values.show_status_bar = ui.action_Show_Status_Bar->isChecked();
    UISettings::values.first_start = false;
}

void GMainWindow::HideMouseCursor() {
    if (emu_thread == nullptr && UISettings::values.hide_mouse) {
        mouse_hide_timer.stop();
        ShowMouseCursor();
        return;
    }
    render_window->setCursor(QCursor(Qt::BlankCursor));
}

void GMainWindow::ShowMouseCursor() {
    render_window->unsetCursor();
    if (emu_thread != nullptr && UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }
}

void GMainWindow::OnMouseActivity() {
    if (!Settings::values.mouse_panning) {
        ShowMouseCursor();
    }
}

void GMainWindow::OnCoreError(Core::System::ResultStatus result, std::string details) {
    QMessageBox::StandardButton answer;
    QString status_message;
    const QString common_message =
        tr("The game you are trying to load requires additional files from your Switch to be "
           "dumped "
           "before playing.<br/><br/>For more information on dumping these files, please see the "
           "following wiki page: <a "
           "href='https://yuzu-emu.org/wiki/"
           "dumping-system-archives-and-the-shared-fonts-from-a-switch-console/'>Dumping System "
           "Archives and the Shared Fonts from a Switch Console</a>.<br/><br/>Would you like to "
           "quit "
           "back to the game list? Continuing emulation may result in crashes, corrupted save "
           "data, or other bugs.");
    switch (result) {
    case Core::System::ResultStatus::ErrorSystemFiles: {
        QString message;
        if (details.empty()) {
            message =
                tr("yuzu was unable to locate a Switch system archive. %1").arg(common_message);
        } else {
            message = tr("yuzu was unable to locate a Switch system archive: %1. %2")
                          .arg(QString::fromStdString(details), common_message);
        }

        answer = QMessageBox::question(this, tr("System Archive Not Found"), message,
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        status_message = tr("System Archive Missing");
        break;
    }

    case Core::System::ResultStatus::ErrorSharedFont: {
        const QString message =
            tr("yuzu was unable to locate the Switch shared fonts. %1").arg(common_message);
        answer = QMessageBox::question(this, tr("Shared Fonts Not Found"), message,
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        status_message = tr("Shared Font Missing");
        break;
    }

    default:
        answer = QMessageBox::question(
            this, tr("Fatal Error"),
            tr("yuzu has encountered a fatal error, please see the log for more details. "
               "For more information on accessing the log, please see the following page: "
               "<a href='https://community.citra-emu.org/t/how-to-upload-the-log-file/296'>How "
               "to "
               "Upload the Log File</a>.<br/><br/>Would you like to quit back to the game "
               "list? "
               "Continuing emulation may result in crashes, corrupted save data, or other "
               "bugs."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        status_message = tr("Fatal Error encountered");
        break;
    }

    if (answer == QMessageBox::Yes) {
        if (emu_thread) {
            ShutdownGame();

            Settings::RestoreGlobalState(Core::System::GetInstance().IsPoweredOn());
            UpdateStatusButtons();
        }
    } else {
        // Only show the message if the game is still running.
        if (emu_thread) {
            emu_thread->SetRunning(true);
            message_label->setText(status_message);
        }
    }
}

void GMainWindow::OnReinitializeKeys(ReinitializeKeyBehavior behavior) {
    if (behavior == ReinitializeKeyBehavior::Warning) {
        const auto res = QMessageBox::information(
            this, tr("Confirm Key Rederivation"),
            tr("You are about to force rederive all of your keys. \nIf you do not know what "
               "this "
               "means or what you are doing, \nthis is a potentially destructive action. "
               "\nPlease "
               "make sure this is what you want \nand optionally make backups.\n\nThis will "
               "delete "
               "your autogenerated key files and re-run the key derivation module."),
            QMessageBox::StandardButtons{QMessageBox::Ok, QMessageBox::Cancel});

        if (res == QMessageBox::Cancel)
            return;

        const auto keys_dir = Common::FS::GetYuzuPath(Common::FS::YuzuPath::KeysDir);

        Common::FS::RemoveFile(keys_dir / "prod.keys_autogenerated");
        Common::FS::RemoveFile(keys_dir / "console.keys_autogenerated");
        Common::FS::RemoveFile(keys_dir / "title.keys_autogenerated");
    }

    Core::Crypto::KeyManager& keys = Core::Crypto::KeyManager::Instance();
    if (keys.BaseDeriveNecessary()) {
        Core::Crypto::PartitionDataManager pdm{vfs->OpenDirectory("", FileSys::Mode::Read)};

        const auto function = [this, &keys, &pdm] {
            keys.PopulateFromPartitionData(pdm);

            auto& system = Core::System::GetInstance();
            system.GetFileSystemController().CreateFactories(*vfs);
            keys.DeriveETicket(pdm, system.GetContentProvider());
        };

        QString errors;
        if (!pdm.HasFuses()) {
            errors += tr("Missing fuses");
        }
        if (!pdm.HasBoot0()) {
            errors += tr(" - Missing BOOT0");
        }
        if (!pdm.HasPackage2()) {
            errors += tr(" - Missing BCPKG2-1-Normal-Main");
        }
        if (!pdm.HasProdInfo()) {
            errors += tr(" - Missing PRODINFO");
        }
        if (!errors.isEmpty()) {
            QMessageBox::warning(
                this, tr("Derivation Components Missing"),
                tr("Components are missing that may hinder key derivation from completing. "
                   "<br>Please follow <a href='https://yuzu-emu.org/help/quickstart/'>the yuzu "
                   "quickstart guide</a> to get all your keys and "
                   "games.<br><br><small>(%1)</small>")
                    .arg(errors));
        }

        QProgressDialog prog(this);
        prog.setRange(0, 0);
        prog.setLabelText(tr("Deriving keys...\nThis may take up to a minute depending \non your "
                             "system's performance."));
        prog.setWindowTitle(tr("Deriving Keys"));

        prog.show();

        auto future = QtConcurrent::run(function);
        while (!future.isFinished()) {
            QCoreApplication::processEvents();
        }

        prog.close();
    }

    Core::System::GetInstance().GetFileSystemController().CreateFactories(*vfs);

    if (behavior == ReinitializeKeyBehavior::Warning) {
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }
}

std::optional<u64> GMainWindow::SelectRomFSDumpTarget(const FileSys::ContentProvider& installed,
                                                      u64 program_id) {
    const auto dlc_entries =
        installed.ListEntriesFilter(FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
    std::vector<FileSys::ContentProviderEntry> dlc_match;
    dlc_match.reserve(dlc_entries.size());
    std::copy_if(dlc_entries.begin(), dlc_entries.end(), std::back_inserter(dlc_match),
                 [&program_id, &installed](const FileSys::ContentProviderEntry& entry) {
                     return FileSys::GetBaseTitleID(entry.title_id) == program_id &&
                            installed.GetEntry(entry)->GetStatus() == Loader::ResultStatus::Success;
                 });

    std::vector<u64> romfs_tids;
    romfs_tids.push_back(program_id);
    for (const auto& entry : dlc_match) {
        romfs_tids.push_back(entry.title_id);
    }

    if (romfs_tids.size() > 1) {
        QStringList list{QStringLiteral("Base")};
        for (std::size_t i = 1; i < romfs_tids.size(); ++i) {
            list.push_back(QStringLiteral("DLC %1").arg(romfs_tids[i] & 0x7FF));
        }

        bool ok;
        const auto res = QInputDialog::getItem(
            this, tr("Select RomFS Dump Target"),
            tr("Please select which RomFS you would like to dump."), list, 0, false, &ok);
        if (!ok) {
            return {};
        }

        return romfs_tids[list.indexOf(res)];
    }

    return program_id;
}

bool GMainWindow::ConfirmClose() {
    if (emu_thread == nullptr || !UISettings::values.confirm_before_closing)
        return true;

    QMessageBox::StandardButton answer =
        QMessageBox::question(this, tr("yuzu"), tr("Are you sure you want to close yuzu?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
}

void GMainWindow::closeEvent(QCloseEvent* event) {
    if (!ConfirmClose()) {
        event->ignore();
        return;
    }

    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    hotkey_registry.SaveHotkeys();

    // Shutdown session if the emu thread is active...
    if (emu_thread != nullptr) {
        ShutdownGame();

        Settings::RestoreGlobalState(Core::System::GetInstance().IsPoweredOn());
        UpdateStatusButtons();
    }

    render_window->close();

    QWidget::closeEvent(event);
}

static bool IsSingleFileDropEvent(const QMimeData* mime) {
    return mime->hasUrls() && mime->urls().length() == 1;
}

void GMainWindow::AcceptDropEvent(QDropEvent* event) {
    if (IsSingleFileDropEvent(event->mimeData())) {
        event->setDropAction(Qt::DropAction::LinkAction);
        event->accept();
    }
}

bool GMainWindow::DropAction(QDropEvent* event) {
    if (!IsSingleFileDropEvent(event->mimeData())) {
        return false;
    }

    const QMimeData* mime_data = event->mimeData();
    const QString& filename = mime_data->urls().at(0).toLocalFile();

    if (emulation_running && QFileInfo(filename).suffix() == QStringLiteral("bin")) {
        // Amiibo
        LoadAmiibo(filename);
    } else {
        // Game
        if (ConfirmChangeGame()) {
            BootGame(filename);
        }
    }
    return true;
}

void GMainWindow::dropEvent(QDropEvent* event) {
    DropAction(event);
}

void GMainWindow::dragEnterEvent(QDragEnterEvent* event) {
    AcceptDropEvent(event);
}

void GMainWindow::dragMoveEvent(QDragMoveEvent* event) {
    AcceptDropEvent(event);
}

bool GMainWindow::ConfirmChangeGame() {
    if (emu_thread == nullptr)
        return true;

    const auto answer = QMessageBox::question(
        this, tr("yuzu"),
        tr("Are you sure you want to stop the emulation? Any unsaved progress will be lost."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
}

bool GMainWindow::ConfirmForceLockedExit() {
    if (emu_thread == nullptr)
        return true;

    const auto answer =
        QMessageBox::question(this, tr("yuzu"),
                              tr("The currently running application has requested yuzu to not "
                                 "exit.\n\nWould you like to bypass this and exit anyway?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
}

void GMainWindow::RequestGameExit() {
    auto& sm{Core::System::GetInstance().ServiceManager()};
    auto applet_oe = sm.GetService<Service::AM::AppletOE>("appletOE");
    auto applet_ae = sm.GetService<Service::AM::AppletAE>("appletAE");
    bool has_signalled = false;

    if (applet_oe != nullptr) {
        applet_oe->GetMessageQueue()->RequestExit();
        has_signalled = true;
    }

    if (applet_ae != nullptr && !has_signalled) {
        applet_ae->GetMessageQueue()->RequestExit();
    }
}

void GMainWindow::filterBarSetChecked(bool state) {
    ui.action_Show_Filter_Bar->setChecked(state);
    emit(OnToggleFilterBar());
}

void GMainWindow::UpdateUITheme() {
    const QString default_icons = QStringLiteral("default");
    const QString& current_theme = UISettings::values.theme;
    const bool is_default_theme = current_theme == QString::fromUtf8(UISettings::themes[0].second);
    QStringList theme_paths(default_theme_paths);

    if (is_default_theme || current_theme.isEmpty()) {
        const QString theme_uri(QStringLiteral(":default/style.qss"));
        QFile f(theme_uri);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
            setStyleSheet(ts.readAll());
        } else {
            qApp->setStyleSheet({});
            setStyleSheet({});
        }
        QIcon::setThemeName(default_icons);
    } else {
        const QString theme_uri(QLatin1Char{':'} + current_theme + QStringLiteral("/style.qss"));
        QFile f(theme_uri);
        if (f.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
            setStyleSheet(ts.readAll());
        } else {
            LOG_ERROR(Frontend, "Unable to set style, stylesheet file not found");
        }
        QIcon::setThemeName(current_theme);
    }

    QIcon::setThemeSearchPaths(theme_paths);
}

void GMainWindow::LoadTranslation() {
    // If the selected language is English, no need to install any translation
    if (UISettings::values.language == QStringLiteral("en")) {
        return;
    }

    bool loaded;

    if (UISettings::values.language.isEmpty()) {
        // If the selected language is empty, use system locale
        loaded = translator.load(QLocale(), {}, {}, QStringLiteral(":/languages/"));
    } else {
        // Otherwise load from the specified file
        loaded = translator.load(UISettings::values.language, QStringLiteral(":/languages/"));
    }

    if (loaded) {
        qApp->installTranslator(&translator);
    } else {
        UISettings::values.language = QStringLiteral("en");
    }
}

void GMainWindow::OnLanguageChanged(const QString& locale) {
    if (UISettings::values.language != QStringLiteral("en")) {
        qApp->removeTranslator(&translator);
    }

    UISettings::values.language = locale;
    LoadTranslation();
    ui.retranslateUi(this);
    UpdateWindowTitle();

    if (emulation_running)
        ui.action_Start->setText(tr("&Continue"));
}

void GMainWindow::SetDiscordEnabled([[maybe_unused]] bool state) {
#ifdef USE_DISCORD_PRESENCE
    if (state) {
        discord_rpc = std::make_unique<DiscordRPC::DiscordImpl>();
    } else {
        discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
    }
#else
    discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
#endif
    discord_rpc->Update();
}

#ifdef main
#undef main
#endif

int main(int argc, char* argv[]) {
    Common::DetachedTasks detached_tasks;
    MicroProfileOnThreadCreate("Frontend");
    SCOPE_EXIT({ MicroProfileShutdown(); });

    Common::ConfigureNvidiaEnvironmentFlags();

    // Init settings params
    QCoreApplication::setOrganizationName(QStringLiteral("yuzu team"));
    QCoreApplication::setApplicationName(QStringLiteral("yuzu"));

#ifdef _WIN32
    // Increases the maximum open file limit to 4096
    _setmaxstdio(4096);
#endif

#ifdef __APPLE__
    // If you start a bundle (binary) on OSX without the Terminal, the working directory is "/".
    // But since we require the working directory to be the executable path for the location of
    // the user folder in the Qt Frontend, we need to cd into that working directory
    const auto bin_path = Common::FS::GetBundleDirectory() / "..";
    chdir(Common::FS::PathToUTF8String(bin_path).c_str());
#endif

#ifdef __linux__
    // Set the DISPLAY variable in order to open web browsers
    // TODO (lat9nq): Find a better solution for AppImages to start external applications
    if (QString::fromLocal8Bit(qgetenv("DISPLAY")).isEmpty()) {
        qputenv("DISPLAY", ":0");
    }
#endif

    // Enables the core to make the qt created contexts current on std::threads
    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);
    QApplication app(argc, argv);

    // Qt changes the locale and causes issues in float conversion using std::to_string() when
    // generating shaders
    setlocale(LC_ALL, "C");

    GMainWindow main_window;
    // After settings have been loaded by GMainWindow, apply the filter
    main_window.show();

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &main_window,
                     &GMainWindow::OnAppFocusStateChanged);

    int result = app.exec();
    detached_tasks.WaitForAllTasks();
    return result;
}
