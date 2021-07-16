cmake_minimum_required(VERSION 3.15)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/find-modules")
include(DownloadExternals)
include(CMakeDependentOption)

project(yuzu)

# Set bundled sdl2/qt as dependent options.
# OFF by default, but if ENABLE_SDL2 and MSVC are true then ON
option(ENABLE_SDL2 "Enable the SDL2 frontend" ON)
CMAKE_DEPENDENT_OPTION(YUZU_USE_BUNDLED_SDL2 "Download bundled SDL2 binaries" ON "ENABLE_SDL2;MSVC" OFF)
# On Linux system SDL2 is likely to be lacking HIDAPI support which have drawbacks but is needed for SDL motion
option(YUZU_ALLOW_SYSTEM_SDL2 "Try using system SDL2 before fallling back to one from externals" OFF)

option(ENABLE_QT "Enable the Qt frontend" ON)
option(ENABLE_QT_TRANSLATION "Enable translations for the Qt frontend" OFF)
CMAKE_DEPENDENT_OPTION(YUZU_USE_BUNDLED_QT "Download bundled Qt binaries" "${MSVC}" "ENABLE_QT" OFF)

option(ENABLE_WEB_SERVICE "Enable web services (telemetry, etc.)" ON)

option(YUZU_USE_BUNDLED_BOOST "Download bundled Boost" OFF)

option(YUZU_USE_BUNDLED_LIBUSB "Compile bundled libusb" OFF)

CMAKE_DEPENDENT_OPTION(YUZU_USE_BUNDLED_FFMPEG "Download/Build bundled FFmpeg" ON "WIN32" OFF)

option(YUZU_USE_QT_WEB_ENGINE "Use QtWebEngine for web applet implementation" OFF)

option(YUZU_ENABLE_BOXCAT "Enable the Boxcat service, a yuzu high-level implementation of BCAT" ON)

option(ENABLE_CUBEB "Enables the cubeb audio backend" ON)

option(USE_DISCORD_PRESENCE "Enables Discord Rich Presence" OFF)

if (NOT ENABLE_WEB_SERVICE)
    set(YUZU_ENABLE_BOXCAT OFF)
endif()

# Default to a Release build
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
    message(STATUS "Defaulting to a Release build")
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/hooks/pre-commit AND NOT EXISTS ${PROJECT_SOURCE_DIR}/.git/hooks/pre-commit)
    if (EXISTS ${PROJECT_SOURCE_DIR}/.git/)
        message(STATUS "Copying pre-commit hook")
        file(COPY hooks/pre-commit DESTINATION ${PROJECT_SOURCE_DIR}/.git/hooks)
    endif()
endif()

# Sanity check : Check that all submodules are present
# =======================================================================

function(check_submodules_present)
    file(READ "${PROJECT_SOURCE_DIR}/.gitmodules" gitmodules)
    string(REGEX MATCHALL "path *= *[^ \t\r\n]*" gitmodules ${gitmodules})
    foreach(module ${gitmodules})
        string(REGEX REPLACE "path *= *" "" module ${module})
        if (NOT EXISTS "${PROJECT_SOURCE_DIR}/${module}/.git")
            message(FATAL_ERROR "Git submodule ${module} not found. "
                    "Please run: git submodule update --init --recursive")
        endif()
    endforeach()
endfunction()

if(EXISTS ${PROJECT_SOURCE_DIR}/.gitmodules)
    check_submodules_present()
endif()
configure_file(${PROJECT_SOURCE_DIR}/dist/compatibility_list/compatibility_list.qrc
               ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.qrc
               COPYONLY)
if (ENABLE_COMPATIBILITY_LIST_DOWNLOAD AND NOT EXISTS ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
    message(STATUS "Downloading compatibility list for yuzu...")
    file(DOWNLOAD
        https://api.yuzu-emu.org/gamedb/
        "${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json" SHOW_PROGRESS)
endif()
if (NOT EXISTS ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
    file(WRITE ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json "")
endif()

# Detect current compilation architecture and create standard definitions
# =======================================================================

include(CheckSymbolExists)
function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE)
            add_definitions(-DARCHITECTURE_${arch}=1)
        endif()
    endif()
endfunction()

if (NOT ENABLE_GENERIC)
    if (MSVC)
        detect_architecture("_M_AMD64" x86_64)
        detect_architecture("_M_IX86" x86)
        detect_architecture("_M_ARM" ARM)
        detect_architecture("_M_ARM64" ARM64)
    else()
        detect_architecture("__x86_64__" x86_64)
        detect_architecture("__i386__" x86)
        detect_architecture("__arm__" ARM)
        detect_architecture("__aarch64__" ARM64)
    endif()
endif()

if (NOT DEFINED ARCHITECTURE)
    set(ARCHITECTURE "GENERIC")
    set(ARCHITECTURE_GENERIC 1)
    add_definitions(-DARCHITECTURE_GENERIC=1)
endif()
message(STATUS "Target architecture: ${ARCHITECTURE}")

if (UNIX)
    add_definitions(-DYUZU_UNIX=1)
endif()

# Configure C++ standard
# ===========================

# boost asio's concept usage doesn't play nicely with some compilers yet.
add_definitions(-DBOOST_ASIO_DISABLE_CONCEPTS)
if (MSVC)
    add_compile_options(/std:c++latest)

    # cubeb and boost still make use of deprecated result_of.
    add_definitions(-D_HAS_DEPRECATED_RESULT_OF)
else()
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Output binaries to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# System imported libraries
# If not found, download any missing through Conan
# =======================================================================
set(CONAN_CMAKE_SILENT_OUTPUT TRUE)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)
if (YUZU_CONAN_INSTALLED)
    if (IS_MULTI_CONFIG)
        include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    else()
        include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    endif()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")
    conan_basic_setup()
    message(STATUS "Adding conan installed libraries to the search path")
endif()

macro(yuzu_find_packages)
    set(options FORCE_REQUIRED)
    cmake_parse_arguments(FN "${options}" "" "" ${ARGN})

    # Cmake has a *serious* lack of 2D array or associative array...
    # Capitalization matters here. We need the naming to match the generated paths from Conan
    set(REQUIRED_LIBS
    #    Cmake Pkg Prefix  Version     Conan Pkg
        "Catch2            2.13        catch2/2.13.0"
        "fmt               8.0         fmt/8.0.0"
        "lz4               1.8         lz4/1.9.2"
        "nlohmann_json     3.8         nlohmann_json/3.8.0"
        "ZLIB              1.2         zlib/1.2.11"
        "zstd              1.5         zstd/1.5.0"
    # can't use opus until AVX check is fixed: https://github.com/yuzu-emu/yuzu/pull/4068
        #"opus              1.3         opus/1.3.1"
    )

    foreach(PACKAGE ${REQUIRED_LIBS})
        string(REGEX REPLACE "[ \t\r\n]+" ";" PACKAGE_SPLIT ${PACKAGE})
        list(GET PACKAGE_SPLIT 0 PACKAGE_PREFIX)
        list(GET PACKAGE_SPLIT 1 PACKAGE_VERSION)
        list(GET PACKAGE_SPLIT 2 PACKAGE_CONAN)
        # This function is called twice, once to check if the packages exist on the system already
        # and a second time to check if conan installed them properly. The second check passes in FORCE_REQUIRED
        if (NOT ${PACKAGE_PREFIX}_FOUND)
            if (FN_FORCE_REQUIRED)
                find_package(${PACKAGE_PREFIX} ${PACKAGE_VERSION} REQUIRED)
            else()
                find_package(${PACKAGE_PREFIX} ${PACKAGE_VERSION})
            endif()
        endif()
        if (NOT ${PACKAGE_PREFIX}_FOUND)
            list(APPEND CONAN_REQUIRED_LIBS ${PACKAGE_CONAN})
        else()
            # Set a legacy findPackage.cmake style PACKAGE_LIBRARIES variable for subprojects that rely on this
            set(${PACKAGE_PREFIX}_LIBRARIES "${PACKAGE_PREFIX}::${PACKAGE_PREFIX}")
        endif()
    endforeach()
    unset(FN_FORCE_REQUIRED)
endmacro()

if (NOT YUZU_USE_BUNDLED_BOOST)
    find_package(Boost 1.73.0 CONFIG COMPONENTS context headers QUIET)
endif()
if (Boost_FOUND)
    set(Boost_LIBRARIES Boost::boost)
    # Conditionally add Boost::context only if the active version of the Conan or system Boost package provides it
    # The old version is missing Boost::context, so we want to avoid adding in that case
    # The new version requires adding Boost::context to prevent linking issues
    #
    # This one is used by Conan on subsequent CMake configures, not the first configure.
    if (TARGET Boost::context)
        list(APPEND Boost_LIBRARIES Boost::context)
    endif()
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR YUZU_USE_BUNDLED_BOOST)
    message(STATUS "Boost 1.73.0 or newer not found, falling back to externals")
    set(YUZU_USE_BUNDLED_BOOST ON CACHE BOOL "Download bundled Boost" FORCE)

    # Use yuzu Boost binaries
    set(Boost_EXT_NAME "boost_1_75_0")
    set(Boost_PATH "${CMAKE_BINARY_DIR}/externals/${Boost_EXT_NAME}")
    download_bundled_external("boost/" ${Boost_EXT_NAME} "")
    set(Boost_USE_DEBUG_RUNTIME FALSE)
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost 1.75.0 CONFIG REQUIRED COMPONENTS context headers PATHS ${Boost_PATH} NO_DEFAULT_PATH)
    # Manually set the include dirs since the find_package sets it incorrectly
    set(Boost_INCLUDE_DIRS ${Boost_PATH}/include CACHE PATH "Path to Boost headers" FORCE)
    include_directories(SYSTEM "${Boost_INCLUDE_DIRS}")
else()
    message(STATUS "Boost 1.73.0 or newer not found, falling back to Conan")
    list(APPEND CONAN_REQUIRED_LIBS "boost/1.73.0")
endif()

# Attempt to locate any packages that are required and report the missing ones in CONAN_REQUIRED_LIBS
yuzu_find_packages()

# Qt5 requires that we find components, so it doesn't fit our pretty little find package function
if(ENABLE_QT)
    set(QT_VERSION 5.12)
    # We want to load the generated conan qt config so that we get the QT_ROOT var so that we can use the official
    # Qt5Config inside the root folder instead of the conan generated one.
    if(EXISTS ${CMAKE_BINARY_DIR}/qtConfig.cmake)
        include(${CMAKE_BINARY_DIR}/qtConfig.cmake)
        list(APPEND CMAKE_MODULE_PATH "${CONAN_QT_ROOT_RELEASE}")
        list(APPEND CMAKE_PREFIX_PATH "${CONAN_QT_ROOT_RELEASE}")
    endif()

    # Check for system Qt on Linux, fallback to bundled Qt
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        if (NOT YUZU_USE_BUNDLED_QT)
            find_package(Qt5 ${QT_VERSION} COMPONENTS Widgets)
        endif()
        if (NOT Qt5_FOUND OR YUZU_USE_BUNDLED_QT)
            # Check for dependencies, then enable bundled Qt download

            # Check that the system GLIBCXX version is compatible
            find_program(OBJDUMP objdump)
            if ("${OBJDUMP}" STREQUAL "OBJDUMP-NOTFOUND")
                message(FATAL_ERROR "Required program `objdump` not found.")
            endif()
            find_library(LIBSTDCXX libstdc++.so.6)
            execute_process(
                COMMAND
                    ${OBJDUMP} -T ${LIBSTDCXX}
                COMMAND
                    grep GLIBCXX_3.4.28
                COMMAND
                    sed "s/[0-9a-f]*.*  //"
                COMMAND
                    sed "s/ .*//"
                COMMAND
                    sort -u
                OUTPUT_VARIABLE
                    GLIBCXX_MET
            )
            if (NOT GLIBCXX_MET)
                message(FATAL_ERROR "Qt too old or not found, and bundled Qt package is not \
                    compatible with this system. Either install Qt ${QT_VERSION}, or provide the path \
                    to Qt by setting the variable Qt5_ROOT.")
            endif()

            # Check for headers
            Include(FindPkgConfig REQUIRED)
            pkg_check_modules(QT_DEP_GLU QUIET glu>=9.0.0)
            if (NOT QT_DEP_GLU_FOUND)
                message(FATAL_ERROR "Qt bundled pacakge dependency `glu` not found. \
                    Perhaps `libglu1-mesa-dev` needs to be installed?")
            endif()
            pkg_check_modules(QT_DEP_MESA QUIET dri>=20.0.8)
            if (NOT QT_DEP_MESA_FOUND)
                message(FATAL_ERROR "Qt bundled pacakge dependency `dri` not found. \
                    Perhaps `mesa-common-dev` needs to be installed?")
            endif()

            # Check for X libraries
            set(BUNDLED_QT_REQUIREMENTS
                libxcb-icccm.so.4
                libxcb-image.so.0
                libxcb-keysyms.so.1
                libxcb-randr.so.0
                libxcb-render-util.so.0
                libxcb-render.so.0
                libxcb-shape.so.0
                libxcb-shm.so.0
                libxcb-sync.so.1
                libxcb-xfixes.so.0
                libxcb-xinerama.so.0
                libxcb-xkb.so.1
                libxcb.so.1
                libxkbcommon-x11.so.0
                libxkbcommon.so.0
            )
            set(UNRESOLVED_QT_DEPS "")
            foreach (REQUIREMENT ${BUNDLED_QT_REQUIREMENTS})
                find_library(BUNDLED_QT_${REQUIREMENT} ${REQUIREMENT})
                if ("${BUNDLED_QT_${REQUIREMENT}}" STREQUAL "BUNDLED_QT_${REQUIREMENT}-NOTFOUND")
                    set(UNRESOLVED_QT_DEPS ${UNRESOLVED_QT_DEPS} ${REQUIREMENT})
                endif()
                unset(BUNDLED_QT_${REQUIREMENT})
            endforeach()
            unset(BUNDLED_QT_REQUIREMENTS)

            if (NOT "${UNRESOLVED_QT_DEPS}" STREQUAL "")
                message(FATAL_ERROR "Bundled Qt package missing required dependencies: ${UNRESOLVED_QT_DEPS}")
            endif()

            set(YUZU_USE_BUNDLED_QT ON CACHE BOOL "Download bundled Qt" FORCE)
        endif()
        if (YUZU_USE_BUNDLED_QT)
            # Binary package currently does not support Qt webengine, so make sure it's disabled
            set(YUZU_USE_QT_WEB_ENGINE OFF CACHE BOOL "Use Qt Webengine" FORCE)
        endif()
    endif()

    set(YUZU_QT_NO_CMAKE_SYSTEM_PATH)

    # Workaround for an issue where conan tries to build Qt from scratch instead of download prebuilt binaries
    set(QT_PREFIX_HINT)

    if(YUZU_USE_BUNDLED_QT)
        if ((MSVC_VERSION GREATER_EQUAL 1910 AND MSVC_VERSION LESS 1930) AND ARCHITECTURE_x86_64)
            set(QT_BUILD qt-5.12.8-msvc2017_64)
        elseif ((${CMAKE_SYSTEM_NAME} STREQUAL "Linux") AND NOT MINGW AND ARCHITECTURE_x86_64)
            set(QT_BUILD qt5_5_15_2)
        else()
            message(FATAL_ERROR "No bundled Qt binaries for your toolchain. Disable YUZU_USE_BUNDLED_QT and provide your own.")
        endif()

        if (DEFINED QT_BUILD)
            download_bundled_external("qt/" ${QT_BUILD} QT_PREFIX)
        endif()

        set(QT_PREFIX_HINT HINTS "${QT_PREFIX}")

        set(YUZU_QT_NO_CMAKE_SYSTEM_PATH "NO_CMAKE_SYSTEM_PATH")
    endif()
    find_package(Qt5 ${QT_VERSION} REQUIRED COMPONENTS Widgets ${QT_PREFIX_HINT} ${YUZU_QT_NO_CMAKE_SYSTEM_PATH})
    if (YUZU_USE_QT_WEB_ENGINE)
        find_package(Qt5 COMPONENTS WebEngineCore WebEngineWidgets)
    endif()

    if (ENABLE_QT_TRANSLATION)
        find_package(Qt5 REQUIRED COMPONENTS LinguistTools ${QT_PREFIX_HINT})
    endif()
endif()

# find SDL2 exports a bunch of variables that are needed, so its easier to do this outside of the yuzu_find_package
if (ENABLE_SDL2)
    if (YUZU_USE_BUNDLED_SDL2)
        # Detect toolchain and platform
        if ((MSVC_VERSION GREATER_EQUAL 1910 AND MSVC_VERSION LESS 1930) AND ARCHITECTURE_x86_64)
            set(SDL2_VER "SDL2-2.0.15-prerelease")
        else()
            message(FATAL_ERROR "No bundled SDL2 binaries for your toolchain. Disable YUZU_USE_BUNDLED_SDL2 and provide your own.")
        endif()

        if (DEFINED SDL2_VER)
            download_bundled_external("sdl2/" ${SDL2_VER} SDL2_PREFIX)
        endif()

        set(SDL2_FOUND YES)
        set(SDL2_INCLUDE_DIR "${SDL2_PREFIX}/include" CACHE PATH "Path to SDL2 headers")
        set(SDL2_LIBRARY "${SDL2_PREFIX}/lib/x64/SDL2.lib" CACHE PATH "Path to SDL2 library")
        set(SDL2_DLL_DIR "${SDL2_PREFIX}/lib/x64/" CACHE PATH "Path to SDL2.dll")

        add_library(SDL2 INTERFACE)
        target_link_libraries(SDL2 INTERFACE "${SDL2_LIBRARY}")
        target_include_directories(SDL2 INTERFACE "${SDL2_INCLUDE_DIR}")
    else()
        if (YUZU_ALLOW_SYSTEM_SDL2)
            find_package(SDL2 2.0.15 QUIET)

            if (SDL2_FOUND)
                # Some installations don't set SDL2_LIBRARIES
                if("${SDL2_LIBRARIES}" STREQUAL "")
                    message(WARNING "SDL2_LIBRARIES wasn't set, manually setting to SDL2::SDL2")
                    set(SDL2_LIBRARIES "SDL2::SDL2")
                endif()

                include_directories(SYSTEM ${SDL2_INCLUDE_DIRS})
                add_library(SDL2 INTERFACE)
                target_link_libraries(SDL2 INTERFACE "${SDL2_LIBRARIES}")
            else()
                message(STATUS "SDL2 2.0.15 or newer not found, falling back to externals.")
            endif()
        else()
            message(STATUS "Using SDL2 from externals.")
        endif()
    endif()
endif()

# Install any missing dependencies with conan install
if (CONAN_REQUIRED_LIBS)
    message(STATUS "Packages ${CONAN_REQUIRED_LIBS} not found!")
    # Use Conan to fetch the libraries that aren't found
    # Download conan.cmake automatically, you can also just copy the conan.cmake file
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
        file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.15/conan.cmake"
                        "${CMAKE_BINARY_DIR}/conan.cmake")
    endif()
    include(${CMAKE_BINARY_DIR}/conan.cmake)

    set(CONAN_LIB_OPTIONS
        libzip:with_openssl=False
        libzip:enable_windows_crypto=False
    )

    conan_check(VERSION 1.24.0 REQUIRED)

    # Manually add iconv to fix a dep conflict between qt and sdl2
    # We don't need to add it through find_package or anything since the other two can find it just fine
    if ("${CONAN_REQUIRED_LIBS}" MATCHES "qt" AND "${CONAN_REQUIRED_LIBS}" MATCHES "sdl")
        list(APPEND CONAN_REQUIRED_LIBS "libiconv/1.16")
    endif()
    if (IS_MULTI_CONFIG)
        conan_cmake_run(REQUIRES ${CONAN_REQUIRED_LIBS}
                        OPTIONS ${CONAN_LIB_OPTIONS}
                        BUILD missing
                        CONFIGURATION_TYPES "Release;Debug"
                        GENERATORS cmake_multi cmake_find_package_multi)
        include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    else()
        conan_cmake_run(REQUIRES ${CONAN_REQUIRED_LIBS}
                        OPTIONS ${CONAN_LIB_OPTIONS}
                        BUILD missing
                        GENERATORS cmake cmake_find_package_multi)
        include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    endif()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")
    conan_basic_setup()

    set(YUZU_CONAN_INSTALLED TRUE CACHE BOOL "If true, the following builds will add conan to the lib search path" FORCE)

    # Now that we've installed what we are missing, try to locate them again,
    # this time with required, so we bail if its not found.
    yuzu_find_packages(FORCE_REQUIRED)

    if (NOT Boost_FOUND)
        find_package(Boost 1.73.0 REQUIRED COMPONENTS context headers)
        set(Boost_LIBRARIES Boost::boost)
        # Conditionally add Boost::context only if the active version of the Conan Boost package provides it
        # The old version is missing Boost::context, so we want to avoid adding in that case
        # The new version requires adding Boost::context to prevent linking issues
        if (TARGET Boost::context)
            list(APPEND Boost_LIBRARIES Boost::context)
        endif()
    endif()

    # Due to issues with variable scopes in functions, we need to also find_package(qt5) outside of the function
    if(ENABLE_QT)
        list(APPEND CMAKE_MODULE_PATH "${CONAN_QT_ROOT_RELEASE}")
        list(APPEND CMAKE_PREFIX_PATH "${CONAN_QT_ROOT_RELEASE}")
        find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets)
        if (YUZU_USE_QT_WEB_ENGINE)
            find_package(Qt5 REQUIRED COMPONENTS WebEngineCore WebEngineWidgets)
        endif()
    endif()

endif()

# Reexport some targets that are named differently when using the upstream CmakeConfig vs the generated Conan config
# In order to ALIAS targets to a new name, they first need to be IMPORTED_GLOBAL
# Dynarmic checks for target `boost` and so we want to make sure it can find it through our system instead of using their external
if (TARGET Boost::Boost)
    set_target_properties(Boost::Boost PROPERTIES IMPORTED_GLOBAL TRUE)
    add_library(Boost::boost ALIAS Boost::Boost)
    add_library(boost ALIAS Boost::Boost)
elseif (TARGET Boost::boost)
    set_target_properties(Boost::boost PROPERTIES IMPORTED_GLOBAL TRUE)
    add_library(boost ALIAS Boost::boost)
endif()

# Ensure libusb is properly configured (based on dolphin libusb include)
if(NOT APPLE AND NOT YUZU_USE_BUNDLED_LIBUSB)
    include(FindPkgConfig)
    if (PKG_CONFIG_FOUND)
        pkg_check_modules(LIBUSB QUIET libusb-1.0>=1.0.24)
    else()
        find_package(LibUSB)
    endif()

    if (LIBUSB_FOUND)
        add_library(usb INTERFACE)
        target_include_directories(usb INTERFACE "${LIBUSB_INCLUDE_DIRS}")
        target_link_libraries(usb INTERFACE "${LIBUSB_LIBRARIES}")
    else()
        message(WARNING "libusb not found, falling back to externals")
        set(YUZU_USE_BUNDLED_LIBUSB ON)
    endif()
endif()

# List of all FFmpeg components required
set(FFmpeg_COMPONENTS
    avcodec
    avutil
    swscale)

if (NOT YUZU_USE_BUNDLED_FFMPEG)
    # Use system installed FFmpeg
    find_package(FFmpeg QUIET COMPONENTS ${FFmpeg_COMPONENTS})

    if (FFmpeg_FOUND)
        # Overwrite aggregate defines from FFmpeg module to avoid over-linking libraries.
        # Prevents shipping too many libraries with the AppImage.
        set(FFmpeg_LIBRARIES "")
        set(FFmpeg_INCLUDE_DIR "")

        foreach(COMPONENT ${FFmpeg_COMPONENTS})
            set(FFmpeg_LIBRARIES ${FFmpeg_LIBRARIES} ${FFmpeg_LIBRARY_${COMPONENT}} CACHE PATH "Paths to FFmpeg libraries" FORCE)
            set(FFmpeg_INCLUDE_DIR ${FFmpeg_INCLUDE_DIR} ${FFmpeg_INCLUDE_${COMPONENT}} CACHE PATH "Path to FFmpeg headers" FORCE)
        endforeach()
    else()
        message(WARNING "FFmpeg not found, falling back to externals")
        set(YUZU_USE_BUNDLED_FFMPEG ON)
    endif()
endif()

if (YUZU_USE_BUNDLED_FFMPEG)
    if (NOT WIN32)
        # Build FFmpeg from externals
        message(STATUS "Using FFmpeg from externals")

        # FFmpeg has source that requires one of nasm or yasm to assemble it.
        # REQUIRED throws an error if not found here during configuration rather than during compilation.
        find_program(ASSEMBLER NAMES nasm yasm)
        if ("${ASSEMBLER}" STREQUAL "ASSEMBLER-NOTFOUND")
            message(FATAL_ERROR "One of either `nasm` or `yasm` not found but is required.")
        endif()

        find_program(AUTOCONF autoconf)
        if ("${AUTOCONF}" STREQUAL "AUTOCONF-NOTFOUND")
            message(FATAL_ERROR "Required program `autoconf` not found.")
        endif()

        set(FFmpeg_PREFIX ${PROJECT_SOURCE_DIR}/externals/ffmpeg)
        set(FFmpeg_BUILD_DIR ${PROJECT_BINARY_DIR}/externals/ffmpeg)
        set(FFmpeg_MAKEFILE ${FFmpeg_BUILD_DIR}/Makefile)
        make_directory(${FFmpeg_BUILD_DIR})

        # Read version string from external
        file(READ ${FFmpeg_PREFIX}/RELEASE FFmpeg_VERSION)
        set(FFmpeg_FOUND NO)
        if (NOT FFmpeg_VERSION STREQUAL "")
            set(FFmpeg_FOUND YES)
        endif()

        unset(FFmpeg_LIBRARIES CACHE)
        foreach(COMPONENT ${FFmpeg_COMPONENTS})
            set(FFmpeg_${COMPONENT}_PREFIX "${FFmpeg_BUILD_DIR}/lib${COMPONENT}")
            set(FFmpeg_${COMPONENT}_LIB_NAME "lib${COMPONENT}.a")
            set(FFmpeg_${COMPONENT}_LIBRARY "${FFmpeg_${COMPONENT}_PREFIX}/${FFmpeg_${COMPONENT}_LIB_NAME}")

            set(FFmpeg_LIBRARIES
                ${FFmpeg_LIBRARIES}
                ${FFmpeg_${COMPONENT}_LIBRARY}
                CACHE PATH "Paths to FFmpeg libraries" FORCE)
        endforeach()

        set(FFmpeg_INCLUDE_DIR
            "${FFmpeg_PREFIX};${FFmpeg_BUILD_DIR}"
            CACHE PATH "Path to FFmpeg headers" FORCE)

        # `configure` parameters builds only exactly what yuzu needs from FFmpeg
        # `--disable-{vaapi,vdpau}` is needed to avoid linking issues
        add_custom_command(
            OUTPUT
                ${FFmpeg_MAKEFILE}
            COMMAND
                /bin/bash ${FFmpeg_PREFIX}/configure
                    --disable-avdevice
                    --disable-avfilter
                    --disable-avformat
                    --disable-doc
                    --disable-everything
                    --disable-ffmpeg
                    --disable-ffprobe
                    --disable-network
                    --disable-postproc
                    --disable-swresample
                    --disable-vaapi
                    --disable-vdpau
                    --enable-decoder=h264
                    --enable-decoder=vp9
            WORKING_DIRECTORY
                ${FFmpeg_BUILD_DIR}
        )

        # Workaround for Ubuntu 18.04's older version of make not being able to call make as a child
        # with context of the jobserver. Also helps ninja users.
        execute_process(
            COMMAND
                nproc
            OUTPUT_VARIABLE
                SYSTEM_THREADS)

        add_custom_command(
            OUTPUT
                ${FFmpeg_LIBRARIES}
            COMMAND
                make -j${SYSTEM_THREADS}
            WORKING_DIRECTORY
                ${FFmpeg_BUILD_DIR}
        )

        # ALL makes this custom target build every time
        # but it won't actually build if the DEPENDS parameter is up to date
        add_custom_target(ffmpeg-configure ALL DEPENDS ${FFmpeg_MAKEFILE})
        add_custom_target(ffmpeg-build ALL DEPENDS ${FFmpeg_LIBRARIES} ffmpeg-configure)

        if (FFmpeg_FOUND)
            message(STATUS "Found FFmpeg version ${FFmpeg_VERSION}")
        else()
            message(FATAL_ERROR "FFmpeg not found")
        endif()
    else() # WIN32
        # Use yuzu FFmpeg binaries
        set(FFmpeg_EXT_NAME "ffmpeg-4.3.1")
        set(FFmpeg_PATH "${CMAKE_BINARY_DIR}/externals/${FFmpeg_EXT_NAME}")
        download_bundled_external("ffmpeg/" ${FFmpeg_EXT_NAME} "")
        set(FFmpeg_FOUND YES)
        set(FFmpeg_INCLUDE_DIR "${FFmpeg_PATH}/include" CACHE PATH "Path to FFmpeg headers" FORCE)
        set(FFmpeg_LIBRARY_DIR "${FFmpeg_PATH}/bin" CACHE PATH "Path to FFmpeg library directory" FORCE)
        set(FFmpeg_DLL_DIR "${FFmpeg_PATH}/bin" CACHE PATH "Path to FFmpeg dll's" FORCE)
        set(FFmpeg_LIBRARIES
            ${FFmpeg_LIBRARY_DIR}/swscale.lib
            ${FFmpeg_LIBRARY_DIR}/avcodec.lib
            ${FFmpeg_LIBRARY_DIR}/avutil.lib
            CACHE PATH "Paths to FFmpeg libraries" FORCE)
    endif()
endif()

unset(FFmpeg_COMPONENTS)

# Prefer the -pthread flag on Linux.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Platform-specific library requirements
# ======================================

if (APPLE)
    # Umbrella framework for everything GUI-related
    find_library(COCOA_LIBRARY Cocoa)
    set(PLATFORM_LIBRARIES ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
elseif (WIN32)
    # WSAPoll and SHGetKnownFolderPath (AppData/Roaming) didn't exist before WinNT 6.x (Vista)
    add_definitions(-D_WIN32_WINNT=0x0600 -DWINVER=0x0600)
    set(PLATFORM_LIBRARIES winmm ws2_32)
    if (MINGW)
        # PSAPI is the Process Status API
        set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} psapi imm32 version)
    endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU|SunOS)$")
    set(PLATFORM_LIBRARIES rt)
endif()

# Setup a custom clang-format target (if clang-format can be found) that will run
# against all the src files. This should be used before making a pull request.
# =======================================================================

set(CLANG_FORMAT_POSTFIX "-10")
find_program(CLANG_FORMAT
    NAMES clang-format${CLANG_FORMAT_POSTFIX}
          clang-format
    PATHS ${PROJECT_BINARY_DIR}/externals)
# if find_program doesn't find it, try to download from externals
if (NOT CLANG_FORMAT)
    if (WIN32 AND NOT CMAKE_CROSSCOMPILING)
        message(STATUS "Clang format not found! Downloading...")
        set(CLANG_FORMAT "${PROJECT_BINARY_DIR}/externals/clang-format${CLANG_FORMAT_POSTFIX}.exe")
        file(DOWNLOAD
            https://github.com/yuzu-emu/ext-windows-bin/raw/master/clang-format${CLANG_FORMAT_POSTFIX}.exe
            "${CLANG_FORMAT}" SHOW_PROGRESS
            STATUS DOWNLOAD_SUCCESS)
        if (NOT DOWNLOAD_SUCCESS EQUAL 0)
            message(WARNING "Could not download clang format! Disabling the clang format target")
            file(REMOVE ${CLANG_FORMAT})
            unset(CLANG_FORMAT)
        endif()
    else()
        message(WARNING "Clang format not found! Disabling the clang format target")
    endif()
endif()

if (CLANG_FORMAT)
    set(SRCS ${PROJECT_SOURCE_DIR}/src)
    set(CCOMMENT "Running clang format against all the .h and .cpp files in src/")
    if (WIN32)
        add_custom_target(clang-format
            COMMAND powershell.exe -Command "Get-ChildItem '${SRCS}/*' -Include *.cpp,*.h -Recurse | Foreach {&'${CLANG_FORMAT}' -i $_.fullname}"
            COMMENT ${CCOMMENT})
    elseif(MINGW)
        add_custom_target(clang-format
            COMMAND find `cygpath -u ${SRCS}` -iname *.h -o -iname *.cpp | xargs `cygpath -u ${CLANG_FORMAT}` -i
            COMMENT ${CCOMMENT})
    else()
        add_custom_target(clang-format
            COMMAND find ${SRCS} -iname *.h -o -iname *.cpp | xargs ${CLANG_FORMAT} -i
            COMMENT ${CCOMMENT})
    endif()
    unset(SRCS)
    unset(CCOMMENT)
endif()

# Include source code
# ===================

# This function should be passed a list of all files in a target. It will automatically generate
# file groups following the directory hierarchy, so that the layout of the files in IDEs matches the
# one in the filesystem.
function(create_target_directory_groups target_name)
    # Place any files that aren't in the source list in a separate group so that they don't get in
    # the way.
    source_group("Other Files" REGULAR_EXPRESSION ".")

    get_target_property(target_sources "${target_name}" SOURCES)

    foreach(file_name IN LISTS target_sources)
        get_filename_component(dir_name "${file_name}" PATH)
        # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
        string(REPLACE "/" "\\" group_name "${dir_name}")
        source_group("${group_name}" FILES "${file_name}")
    endforeach()
endfunction()

# Prevent boost from linking against libs when building
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY
    -DBOOST_SYSTEM_NO_LIB
    -DBOOST_DATE_TIME_NO_LIB
    -DBOOST_REGEX_NO_LIB
)

enable_testing()
add_subdirectory(externals)
add_subdirectory(src)

# Set yuzu project or yuzu-cmd project as default StartUp Project in Visual Studio depending on whether QT is enabled or not
if(ENABLE_QT)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT yuzu)
else()
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT yuzu-cmd)
endif()


# Installation instructions
# =========================

# Install freedesktop.org metadata files, following those specifications:
# http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html
# http://standards.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html
# http://standards.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html
if(ENABLE_QT AND UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/dist/yuzu.desktop"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    install(FILES "${PROJECT_SOURCE_DIR}/dist/yuzu.svg"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
    install(FILES "${PROJECT_SOURCE_DIR}/dist/yuzu.xml"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/mime/packages")
endif()
