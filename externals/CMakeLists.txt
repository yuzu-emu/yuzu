# Definitions for all external bundled libraries

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/externals/find-modules")
include(DownloadExternals)

# xbyak
if (ARCHITECTURE_x86 OR ARCHITECTURE_x86_64)
    add_library(xbyak INTERFACE)
    target_include_directories(xbyak SYSTEM INTERFACE ./xbyak/xbyak)
    target_compile_definitions(xbyak INTERFACE XBYAK_NO_OP_NAMES)
elseif(ARCHITECTURE_ARM64)
    add_library(xbyak_aarch64 INTERFACE)
    target_include_directories(xbyak_aarch64 SYSTEM INTERFACE ./xbyak_aarch64/xbyak_aarch64)
    target_compile_definitions(xbyak_aarch64 INTERFACE XBYAK_NO_OP_NAMES)
endif()

# Catch
add_library(catch-single-include INTERFACE)
target_include_directories(catch-single-include INTERFACE catch/single_include)

# Dynarmic
if (ARCHITECTURE_x86_64 OR ARCHITECTURE_ARM64)
    set(DYNARMIC_TESTS OFF)
    set(DYNARMIC_NO_BUNDLED_FMT ON)
    add_subdirectory(dynarmic)
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt)
endif()

# Glad
add_subdirectory(glad)

# inih
add_subdirectory(inih)

# mbedtls
add_subdirectory(mbedtls EXCLUDE_FROM_ALL)
target_include_directories(mbedtls PUBLIC ./mbedtls/include)

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)

# Unicorn
add_library(unicorn-headers INTERFACE)
target_include_directories(unicorn-headers INTERFACE ./unicorn/include)

# SoundTouch
add_subdirectory(soundtouch)

# Cubeb
if(ENABLE_CUBEB)
    set(BUILD_TESTS OFF CACHE BOOL "")
    add_subdirectory(cubeb EXCLUDE_FROM_ALL)
endif()

# DiscordRPC
if (USE_DISCORD_PRESENCE)
    add_subdirectory(discord-rpc EXCLUDE_FROM_ALL)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
endif()

# Sirit
add_subdirectory(sirit)

# libzip
find_package(libzip 1.5)
if (NOT libzip_FOUND)
    message(STATUS "libzip 1.5 or newer not found, falling back to externals")
    add_subdirectory(libzip EXCLUDE_FROM_ALL)
endif()

if (ENABLE_WEB_SERVICE)
    find_package(OpenSSL 1.1)
    if (OPENSSL_FOUND)
        set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    else()
        # LibreSSL
        set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "")
        set(OPENSSLDIR "/etc/ssl/")
        add_subdirectory(libressl EXCLUDE_FROM_ALL)
        target_include_directories(ssl INTERFACE ./libressl/include)
        target_compile_definitions(ssl PRIVATE -DHAVE_INET_NTOP)
        get_directory_property(OPENSSL_LIBRARIES
            DIRECTORY libressl
            DEFINITION OPENSSL_LIBS)
    endif()

    # httplib
    add_library(httplib INTERFACE)
    target_include_directories(httplib INTERFACE ./httplib)
    target_compile_definitions(httplib INTERFACE -DCPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE ${OPENSSL_LIBRARIES})
    if (WIN32)
        target_link_libraries(httplib INTERFACE crypt32 cryptui ws2_32)
    endif()
endif()

# Opus
find_package(opus 1.3)
if (NOT opus_FOUND)
    message(STATUS "opus 1.3 or newer not found, falling back to externals")
    add_subdirectory(opus EXCLUDE_FROM_ALL)
endif()
