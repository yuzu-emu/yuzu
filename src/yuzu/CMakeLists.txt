set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

# Set the RPATH for Qt Libraries
# This must be done before the `yuzu` target is created
if (YUZU_USE_BUNDLED_QT AND (${CMAKE_SYSTEM_NAME} STREQUAL "Linux"))
    set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/bin/lib/")
endif()

add_executable(yuzu
    Info.plist
    about_dialog.cpp
    about_dialog.h
    aboutdialog.ui
    applets/qt_controller.cpp
    applets/qt_controller.h
    applets/qt_controller.ui
    applets/qt_error.cpp
    applets/qt_error.h
    applets/qt_profile_select.cpp
    applets/qt_profile_select.h
    applets/qt_software_keyboard.cpp
    applets/qt_software_keyboard.h
    applets/qt_software_keyboard.ui
    applets/qt_web_browser.cpp
    applets/qt_web_browser.h
    applets/qt_web_browser_scripts.h
    bootmanager.cpp
    bootmanager.h
    compatdb.ui
    compatibility_list.cpp
    compatibility_list.h
    configuration/config.cpp
    configuration/config.h
    configuration/configuration_shared.cpp
    configuration/configuration_shared.h
    configuration/configure.ui
    configuration/configure_audio.cpp
    configuration/configure_audio.h
    configuration/configure_audio.ui
    configuration/configure_cpu.cpp
    configuration/configure_cpu.h
    configuration/configure_cpu.ui
    configuration/configure_cpu_debug.cpp
    configuration/configure_cpu_debug.h
    configuration/configure_cpu_debug.ui
    configuration/configure_debug.cpp
    configuration/configure_debug.h
    configuration/configure_debug.ui
    configuration/configure_debug_controller.cpp
    configuration/configure_debug_controller.h
    configuration/configure_debug_controller.ui
    configuration/configure_debug_tab.cpp
    configuration/configure_debug_tab.h
    configuration/configure_debug_tab.ui
    configuration/configure_dialog.cpp
    configuration/configure_dialog.h
    configuration/configure_filesystem.cpp
    configuration/configure_filesystem.h
    configuration/configure_filesystem.ui
    configuration/configure_general.cpp
    configuration/configure_general.h
    configuration/configure_general.ui
    configuration/configure_graphics.cpp
    configuration/configure_graphics.h
    configuration/configure_graphics.ui
    configuration/configure_graphics_advanced.cpp
    configuration/configure_graphics_advanced.h
    configuration/configure_graphics_advanced.ui
    configuration/configure_hotkeys.cpp
    configuration/configure_hotkeys.h
    configuration/configure_hotkeys.ui
    configuration/configure_input.cpp
    configuration/configure_input.h
    configuration/configure_input.ui
    configuration/configure_input_advanced.cpp
    configuration/configure_input_advanced.h
    configuration/configure_input_advanced.ui
    configuration/configure_input_player.cpp
    configuration/configure_input_player.h
    configuration/configure_input_player.ui
    configuration/configure_input_player_widget.cpp
    configuration/configure_input_player_widget.h
    configuration/configure_input_profile_dialog.cpp
    configuration/configure_input_profile_dialog.h
    configuration/configure_input_profile_dialog.ui
    configuration/configure_motion_touch.cpp
    configuration/configure_motion_touch.h
    configuration/configure_motion_touch.ui
    configuration/configure_mouse_advanced.cpp
    configuration/configure_mouse_advanced.h
    configuration/configure_mouse_advanced.ui
    configuration/configure_per_game.cpp
    configuration/configure_per_game.h
    configuration/configure_per_game.ui
    configuration/configure_per_game_addons.cpp
    configuration/configure_per_game_addons.h
    configuration/configure_per_game_addons.ui
    configuration/configure_profile_manager.cpp
    configuration/configure_profile_manager.h
    configuration/configure_profile_manager.ui
    configuration/configure_service.cpp
    configuration/configure_service.h
    configuration/configure_service.ui
    configuration/configure_system.cpp
    configuration/configure_system.h
    configuration/configure_system.ui
    configuration/configure_tas.cpp
    configuration/configure_tas.h
    configuration/configure_tas.ui
    configuration/configure_touch_from_button.cpp
    configuration/configure_touch_from_button.h
    configuration/configure_touch_from_button.ui
    configuration/configure_touchscreen_advanced.cpp
    configuration/configure_touchscreen_advanced.h
    configuration/configure_touchscreen_advanced.ui
    configuration/configure_touch_widget.h
    configuration/configure_ui.cpp
    configuration/configure_ui.h
    configuration/configure_ui.ui
    configuration/configure_vibration.cpp
    configuration/configure_vibration.h
    configuration/configure_vibration.ui
    configuration/configure_web.cpp
    configuration/configure_web.h
    configuration/configure_web.ui
    configuration/input_profiles.cpp
    configuration/input_profiles.h
    debugger/console.cpp
    debugger/console.h
    debugger/controller.cpp
    debugger/controller.h
    debugger/profiler.cpp
    debugger/profiler.h
    debugger/wait_tree.cpp
    debugger/wait_tree.h
    discord.h
    game_list.cpp
    game_list.h
    game_list_p.h
    game_list_worker.cpp
    game_list_worker.h
    hotkeys.cpp
    hotkeys.h
    install_dialog.cpp
    install_dialog.h
    loading_screen.cpp
    loading_screen.h
    loading_screen.ui
    main.cpp
    main.h
    main.ui
    uisettings.cpp
    uisettings.h
    util/limitable_input_dialog.cpp
    util/limitable_input_dialog.h
    util/overlay_dialog.cpp
    util/overlay_dialog.h
    util/overlay_dialog.ui
    util/sequence_dialog/sequence_dialog.cpp
    util/sequence_dialog/sequence_dialog.h
    util/url_request_interceptor.cpp
    util/url_request_interceptor.h
    util/util.cpp
    util/util.h
    compatdb.cpp
    compatdb.h
    yuzu.qrc
    yuzu.rc
)

file(GLOB COMPAT_LIST
     ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.qrc
     ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
file(GLOB_RECURSE ICONS ${PROJECT_SOURCE_DIR}/dist/icons/*)
file(GLOB_RECURSE THEMES ${PROJECT_SOURCE_DIR}/dist/qt_themes/*)

if (ENABLE_QT_TRANSLATION)
    set(YUZU_QT_LANGUAGES "${PROJECT_SOURCE_DIR}/dist/languages" CACHE PATH "Path to the translation bundle for the Qt frontend")
    option(GENERATE_QT_TRANSLATION "Generate en.ts as the translation source file" OFF)

    # Update source TS file if enabled
    if (GENERATE_QT_TRANSLATION)
        get_target_property(SRCS yuzu SOURCES)
        qt5_create_translation(QM_FILES ${SRCS} ${UIS} ${YUZU_QT_LANGUAGES}/en.ts)
        add_custom_target(translation ALL DEPENDS ${YUZU_QT_LANGUAGES}/en.ts)
    endif()

    # Find all TS files except en.ts
    file(GLOB_RECURSE LANGUAGES_TS ${YUZU_QT_LANGUAGES}/*.ts)
    list(REMOVE_ITEM LANGUAGES_TS ${YUZU_QT_LANGUAGES}/en.ts)

    # Compile TS files to QM files
    qt5_add_translation(LANGUAGES_QM ${LANGUAGES_TS})

    # Build a QRC file from the QM file list
    set(LANGUAGES_QRC ${CMAKE_CURRENT_BINARY_DIR}/languages.qrc)
    file(WRITE ${LANGUAGES_QRC} "<RCC><qresource prefix=\"languages\">\n")
    foreach (QM ${LANGUAGES_QM})
        get_filename_component(QM_FILE ${QM} NAME)
        file(APPEND ${LANGUAGES_QRC} "<file>${QM_FILE}</file>\n")
    endforeach (QM)
    file(APPEND ${LANGUAGES_QRC} "</qresource></RCC>")

    # Add the QRC file to package in all QM files
    qt5_add_resources(LANGUAGES ${LANGUAGES_QRC})
else()
    set(LANGUAGES)
endif()

target_sources(yuzu
    PRIVATE
        ${COMPAT_LIST}
        ${ICONS}
        ${LANGUAGES}
        ${THEMES}
)

if (APPLE)
    set(MACOSX_ICON "../../dist/yuzu.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(yuzu PRIVATE ${MACOSX_ICON})
    set_target_properties(yuzu PROPERTIES MACOSX_BUNDLE TRUE)
    set_target_properties(yuzu PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
elseif(WIN32)
    # compile as a win32 gui application instead of a console application
    target_link_libraries(yuzu PRIVATE Qt5::WinMain)
    if(MSVC)
        set_target_properties(yuzu PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    elseif(MINGW)
        set_target_properties(yuzu PROPERTIES LINK_FLAGS_RELEASE "-mwindows")
    endif()
endif()

create_target_directory_groups(yuzu)

target_link_libraries(yuzu PRIVATE common core input_common video_core)
target_link_libraries(yuzu PRIVATE Boost::boost glad Qt5::Widgets)
target_link_libraries(yuzu PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

target_include_directories(yuzu PRIVATE ../../externals/Vulkan-Headers/include)
if (NOT WIN32)
    target_include_directories(yuzu PRIVATE ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()

target_compile_definitions(yuzu PRIVATE
    # Use QStringBuilder for string concatenation to reduce
    # the overall number of temporary strings created.
    -DQT_USE_QSTRINGBUILDER

    # Disable implicit conversions from/to C strings
    -DQT_NO_CAST_FROM_ASCII
    -DQT_NO_CAST_TO_ASCII

    # Disable implicit type narrowing in signal/slot connect() calls.
    -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT

    # Disable unsafe overloads of QProcess' start() function.
    -DQT_NO_PROCESS_COMBINED_ARGUMENT_START

    # Disable implicit QString->QUrl conversions to enforce use of proper resolving functions.
    -DQT_NO_URL_CAST_FROM_STRING
)

if (YUZU_ENABLE_COMPATIBILITY_REPORTING)
    target_compile_definitions(yuzu PRIVATE -DYUZU_ENABLE_COMPATIBILITY_REPORTING)
endif()

if (USE_DISCORD_PRESENCE)
    target_sources(yuzu PUBLIC
        discord_impl.cpp
        discord_impl.h
    )
    target_link_libraries(yuzu PRIVATE discord-rpc)
    target_compile_definitions(yuzu PRIVATE -DUSE_DISCORD_PRESENCE)
endif()

if (YUZU_USE_QT_WEB_ENGINE)
    target_link_libraries(yuzu PRIVATE Qt5::WebEngineCore Qt5::WebEngineWidgets)
    target_compile_definitions(yuzu PRIVATE -DYUZU_USE_QT_WEB_ENGINE)
endif ()

if (YUZU_ENABLE_BOXCAT)
    target_compile_definitions(yuzu PRIVATE -DYUZU_ENABLE_BOXCAT)
endif ()

if(UNIX AND NOT APPLE)
    install(TARGETS yuzu RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()

if (YUZU_USE_BUNDLED_QT)
    include(CopyYuzuQt5Deps)
    copy_yuzu_Qt5_deps(yuzu)
endif()

if (MSVC)
    include(CopyYuzuSDLDeps)
    include(CopyYuzuFFmpegDeps)
    copy_yuzu_SDL_deps(yuzu)
    copy_yuzu_FFmpeg_deps(yuzu)
endif()

if (NOT APPLE)
    target_compile_definitions(yuzu PRIVATE HAS_OPENGL)
endif()
