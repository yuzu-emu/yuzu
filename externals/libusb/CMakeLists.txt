if (MINGW OR (${CMAKE_SYSTEM_NAME} MATCHES "Linux") OR APPLE)
    set(LIBUSB_FOUND ON CACHE BOOL "libusb is present" FORCE)
    set(LIBUSB_VERSION "1.0.24" CACHE STRING "libusb version string" FORCE)

    # GNU toolchains for some reason doesn't work with the later half of this CMakeLists after
    # updating to 1.0.24, so we do it the old-fashioned way for now.

    # Require autoconf and libtoolize here, rather than crash during compilation
    find_program(AUTOCONF autoconf)
    if ("${AUTOCONF}" STREQUAL "AUTOCONF-NOTFOUND")
        message(FATAL_ERROR "Required program `autoconf` not found.")
    endif()

    find_program(LIBTOOLIZE libtoolize)
    if ("${LIBTOOLIZE}" STREQUAL "LIBTOOLIZE-NOTFOUND")
        message(FATAL_ERROR "Required program `libtoolize` not found.")
    endif()

    set(LIBUSB_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/libusb")
    set(LIBUSB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusb")

    # Workarounds for MSYS/MinGW
    if (MSYS)
        # CMake on Windows passes `C:/`, but we need `/C/` or `/c/` to use `configure`
        string(REPLACE ":/" "/" LIBUSB_SRC_DIR "${LIBUSB_SRC_DIR}")
        set(LIBUSB_SRC_DIR "/${LIBUSB_SRC_DIR}")

        # And now that we are using /C/ for srcdir but everything else is using C:/, we need to
        # compile everything in the source directory, else `configure` won't think the build
        # environment is sane.
        set(LIBUSB_PREFIX "${LIBUSB_SRC_DIR}")
    endif()

    set(LIBUSB_CONFIGURE "${LIBUSB_SRC_DIR}/configure")
    set(LIBUSB_MAKEFILE "${LIBUSB_PREFIX}/Makefile")

    if (MINGW)
        set(LIBUSB_LIBRARIES "${LIBUSB_PREFIX}/libusb/.libs/libusb-1.0.dll.a" CACHE PATH "libusb library path" FORCE)
        set(LIBUSB_SHARED_LIBRARY "${LIBUSB_PREFIX}/libusb/.libs/libusb-1.0.dll")
        set(LIBUSB_SHARED_LIBRARY_DEST "${CMAKE_BINARY_DIR}/bin/libusb-1.0.dll")

        set(LIBUSB_CONFIGURE_ARGS --host=x86_64-w64-mingw32 --build=x86_64-windows)
    else()
        set(LIBUSB_LIBRARIES "${LIBUSB_PREFIX}/libusb/.libs/libusb-1.0.a" CACHE PATH "libusb library path" FORCE)
    endif()

    set(LIBUSB_INCLUDE_DIRS "${LIBUSB_SRC_DIR}/libusb" CACHE PATH "libusb headers path" FORCE)

    # MINGW: causes "externals/libusb/libusb/libusb/os/windows_winusb.c:1427:2: error: conversion to non-scalar type requested", so cannot statically link it for now.
    if (NOT MINGW)
        set(LIBUSB_CFLAGS "-DGUID_DEVINTERFACE_USB_DEVICE=\\(GUID\\){0xA5DCBF10,0x6530,0x11D2,{0x90,0x1F,0x00,0xC0,0x4F,0xB9,0x51,0xED}}")
    endif()

    make_directory("${LIBUSB_PREFIX}")

    add_custom_command(
        OUTPUT
            "${LIBUSB_LIBRARIES}"
        COMMAND
            make
        WORKING_DIRECTORY
            "${LIBUSB_PREFIX}"
    )

    add_custom_command(
        OUTPUT
            "${LIBUSB_MAKEFILE}"
        COMMAND
            env
                CC="${CMAKE_C_COMPILER}"
                CXX="${CMAKE_CXX_COMPILER}"
                CFLAGS="${LIBUSB_CFLAGS}"
            sh "${LIBUSB_CONFIGURE}"
                ${LIBUSB_CONFIGURE_ARGS}
                --srcdir="${LIBUSB_SRC_DIR}"
        WORKING_DIRECTORY
            "${LIBUSB_PREFIX}"
    )

    add_custom_command(
        OUTPUT
            "${LIBUSB_CONFIGURE}"
        COMMAND
            sh "${LIBUSB_SRC_DIR}/bootstrap.sh"
        WORKING_DIRECTORY
            "${LIBUSB_SRC_DIR}"
    )

    add_custom_command(
        OUTPUT
            "${LIBUSB_SHARED_LIBRARY_DEST}"
        COMMAND
            cp "${LIBUSB_SHARED_LIBRARY}" "${LIBUSB_SHARED_LIBRARY_DEST}"
    )

    add_custom_target(usb-bootstrap DEPENDS "${LIBUSB_CONFIGURE}")
    add_custom_target(usb-configure DEPENDS "${LIBUSB_MAKEFILE}" usb-bootstrap)
    add_custom_target(usb-build ALL DEPENDS "${LIBUSB_LIBRARIES}" usb-configure)
    # Workaround since static linking didn't work out -- We need to copy the DLL to the bin directory
    add_custom_target(usb-copy ALL DEPENDS "${LIBUSB_SHARED_LIBRARY_DEST}" usb-build)

    add_library(usb INTERFACE)
    add_dependencies(usb usb-copy)
    target_link_libraries(usb INTERFACE "${LIBUSB_LIBRARIES}")
    target_include_directories(usb INTERFACE "${LIBUSB_INCLUDE_DIRS}")

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        Include(FindPkgConfig)
        pkg_check_modules(LIBUDEV REQUIRED libudev)

        if (LIBUDEV_FOUND)
            target_include_directories(usb INTERFACE "${LIBUDEV_INCLUDE_DIRS}")
            target_link_libraries(usb INTERFACE "${LIBUDEV_STATIC_LIBRARIES}")
        endif()
    endif()
else() # MINGW OR (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # Ensure libusb compiles with UTF-8 encoding on MSVC
    if(MSVC)
        add_compile_options(/utf-8)
    endif()

    add_library(usb STATIC EXCLUDE_FROM_ALL
        libusb/libusb/core.c
        libusb/libusb/core.c
        libusb/libusb/descriptor.c
        libusb/libusb/hotplug.c
        libusb/libusb/io.c
        libusb/libusb/strerror.c
        libusb/libusb/sync.c
    )
    set_target_properties(usb PROPERTIES VERSION 1.0.24)
    if(WIN32)
        target_include_directories(usb
            BEFORE
            PUBLIC
              libusb/libusb

            PRIVATE
              "${CMAKE_CURRENT_BINARY_DIR}"
        )

        if (NOT MINGW)
            target_include_directories(usb BEFORE PRIVATE libusb/msvc)
        endif()

        # Works around other libraries providing their own definition of USB GUIDs (e.g. SDL2)
        target_compile_definitions(usb PRIVATE "-DGUID_DEVINTERFACE_USB_DEVICE=(GUID){ 0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}}")
    else()
        target_include_directories(usb
            # turns out other projects also have "config.h", so make sure the
            # LibUSB one comes first
            BEFORE

            PUBLIC
              libusb/libusb

            PRIVATE
              "${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()

    if(WIN32 OR CYGWIN)
        target_sources(usb PRIVATE
          libusb/libusb/os/threads_windows.c
          libusb/libusb/os/windows_winusb.c
          libusb/libusb/os/windows_usbdk.c
          libusb/libusb/os/windows_common.c
        )
        set(OS_WINDOWS TRUE)
    elseif(APPLE)
        target_sources(usb PRIVATE
            libusb/libusb/os/darwin_usb.c
        )
        find_library(COREFOUNDATION_LIBRARY CoreFoundation)
        find_library(IOKIT_LIBRARY IOKit)
        find_library(OBJC_LIBRARY objc)
        target_link_libraries(usb PRIVATE
            ${COREFOUNDATION_LIBRARY}
            ${IOKIT_LIBRARY}
            ${OBJC_LIBRARY}
        )
        set(OS_DARWIN TRUE)
    elseif(ANDROID)
        target_sources(usb PRIVATE
            libusb/libusb/os/linux_usbfs.c
            libusb/libusb/os/linux_netlink.c
        )
        find_library(LOG_LIBRARY log)
        target_link_libraries(usb PRIVATE ${LOG_LIBRARY})
        set(OS_LINUX TRUE)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_sources(usb PRIVATE
            libusb/libusb/os/linux_usbfs.c
        )
        find_package(Libudev)
        if(LIBUDEV_FOUND)
            target_sources(usb PRIVATE
                libusb/libusb/os/linux_udev.c
            )
            target_link_libraries(usb PRIVATE "${LIBUDEV_LIBRARIES}")
            target_include_directories(usb PRIVATE "${LIBUDEV_INCLUDE_DIR}")
            set(HAVE_LIBUDEV TRUE)
            set(USE_UDEV TRUE)
        else()
            target_sources(usb PRIVATE
                libusb/libusb/os/linux_netlink.c
            )
        endif()
        set(OS_LINUX TRUE)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
        target_sources(usb PRIVATE
            libusb/libusb/os/netbsd_usb.c
        )
        set(OS_NETBSD TRUE)
    elseif(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
        target_sources(usb PRIVATE
            libusb/libusb/os/openbsd_usb.c
        )
        set(OS_OPENBSD TRUE)
    endif()

    if(UNIX)
        target_sources(usb PRIVATE
            libusb/libusb/os/events_posix.c
            libusb/libusb/os/threads_posix.c
        )
        find_package(Threads REQUIRED)
        if(THREADS_HAVE_PTHREAD_ARG)
          target_compile_options(usb PUBLIC "-pthread")
        endif()
        if(CMAKE_THREAD_LIBS_INIT)
          target_link_libraries(usb PRIVATE "${CMAKE_THREAD_LIBS_INIT}")
        endif()
        set(THREADS_POSIX TRUE)
    elseif(WIN32)
        target_sources(usb PRIVATE
            libusb/libusb/os/events_windows.c
            libusb/libusb/os/threads_windows.c
        )
    endif()

    include(CheckFunctionExists)
    include(CheckIncludeFiles)
    include(CheckTypeSize)
    check_include_files(asm/types.h HAVE_ASM_TYPES_H)
    check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
    check_include_files(linux/filter.h HAVE_LINUX_FILTER_H)
    check_include_files(linux/netlink.h HAVE_LINUX_NETLINK_H)
    check_include_files(poll.h HAVE_POLL_H)
    check_include_files(signal.h HAVE_SIGNAL_H)
    check_include_files(strings.h HAVE_STRINGS_H)
    check_type_size("struct timespec" STRUCT_TIMESPEC)
    check_function_exists(syslog HAVE_SYSLOG_FUNC)
    check_include_files(syslog.h HAVE_SYSLOG_H)
    check_include_files(sys/socket.h HAVE_SYS_SOCKET_H)
    check_include_files(sys/time.h HAVE_SYS_TIME_H)
    check_include_files(sys/types.h HAVE_SYS_TYPES_H)

    set(CMAKE_EXTRA_INCLUDE_FILES poll.h)
    check_type_size("nfds_t" nfds_t)
    unset(CMAKE_EXTRA_INCLUDE_FILES)
    if(HAVE_NFDS_T)
        set(POLL_NFDS_TYPE "nfds_t")
    else()
        set(POLL_NFDS_TYPE "unsigned int")
    endif()

    check_include_files(sys/timerfd.h USBI_TIMERFD_AVAILABLE)


    configure_file(config.h.in config.h)
endif() # MINGW OR (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
