// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <unordered_map>

#include "common/fs/fs.h"
#include "common/fs/fs_paths.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"

#ifdef _WIN32
#include <shlobj.h> // Used in GetExeDirectory()
#else
#include <cstdlib>     // Used in Get(Home/Data)Directory()
#include <pwd.h>       // Used in GetHomeDirectory()
#include <sys/types.h> // Used in GetHomeDirectory()
#include <unistd.h>    // Used in GetDataDirectory()
#endif

#ifdef __APPLE__
#include <sys/param.h> // Used in GetBundleDirectory()

// CFURL contains __attribute__ directives that gcc does not know how to parse, so we need to just
// ignore them if we're not using clang. The macro is only used to prevent linking against
// functions that don't exist on older versions of macOS, and the worst case scenario is a linker
// error, so this is perfectly safe, just inconvenient.
#ifndef __clang__
#define availability(...)
#endif
#include <CoreFoundation/CFBundle.h> // Used in GetBundleDirectory()
#include <CoreFoundation/CFString.h> // Used in GetBundleDirectory()
#include <CoreFoundation/CFURL.h>    // Used in GetBundleDirectory()
#ifdef availability
#undef availability
#endif
#endif

#ifndef MAX_PATH
#ifdef _WIN32
// This is the maximum number of UTF-16 code units permissible in Windows file paths
#define MAX_PATH 260
#else
// This is the maximum number of UTF-8 code units permissible in all other OSes' file paths
#define MAX_PATH 1024
#endif
#endif

namespace Common::FS {

namespace fs = std::filesystem;

/**
 * The PathManagerImpl is a singleton allowing to manage the mapping of
 * YuzuPath enums to real filesystem paths.
 * This class provides 2 functions: GetYuzuPathImpl and SetYuzuPathImpl.
 * These are used by GetYuzuPath and SetYuzuPath respectively to get or modify
 * the path mapped by the YuzuPath enum.
 */
class PathManagerImpl {
public:
    static PathManagerImpl& GetInstance() {
        static PathManagerImpl path_manager_impl;

        return path_manager_impl;
    }

    PathManagerImpl(const PathManagerImpl&) = delete;
    PathManagerImpl& operator=(const PathManagerImpl&) = delete;

    PathManagerImpl(PathManagerImpl&&) = delete;
    PathManagerImpl& operator=(PathManagerImpl&&) = delete;

    [[nodiscard]] const fs::path& GetYuzuPathImpl(YuzuPath yuzu_path) {
        return yuzu_paths.at(yuzu_path);
    }

    void SetYuzuPathImpl(YuzuPath yuzu_path, const fs::path& new_path) {
        yuzu_paths.insert_or_assign(yuzu_path, new_path);
    }

private:
    PathManagerImpl() {
#ifdef _WIN32
        auto yuzu_path = GetExeDirectory() / PORTABLE_DIR;

        if (!IsDir(yuzu_path)) {
            yuzu_path = GetAppDataRoamingDirectory() / YUZU_DIR;
        }

        GenerateYuzuPath(YuzuPath::YuzuDir, yuzu_path);
        GenerateYuzuPath(YuzuPath::CacheDir, yuzu_path / CACHE_DIR);
        GenerateYuzuPath(YuzuPath::ConfigDir, yuzu_path / CONFIG_DIR);
#else
        auto yuzu_path = GetCurrentDir() / PORTABLE_DIR;

        if (Exists(yuzu_path) && IsDir(yuzu_path)) {
            GenerateYuzuPath(YuzuPath::YuzuDir, yuzu_path);
            GenerateYuzuPath(YuzuPath::CacheDir, yuzu_path / CACHE_DIR);
            GenerateYuzuPath(YuzuPath::ConfigDir, yuzu_path / CONFIG_DIR);
        } else {
            yuzu_path = GetDataDirectory("XDG_DATA_HOME") / YUZU_DIR;

            GenerateYuzuPath(YuzuPath::YuzuDir, yuzu_path);
            GenerateYuzuPath(YuzuPath::CacheDir, GetDataDirectory("XDG_CACHE_HOME") / YUZU_DIR);
            GenerateYuzuPath(YuzuPath::ConfigDir, GetDataDirectory("XDG_CONFIG_HOME") / YUZU_DIR);
        }
#endif

        GenerateYuzuPath(YuzuPath::DumpDir, yuzu_path / DUMP_DIR);
        GenerateYuzuPath(YuzuPath::KeysDir, yuzu_path / KEYS_DIR);
        GenerateYuzuPath(YuzuPath::LoadDir, yuzu_path / LOAD_DIR);
        GenerateYuzuPath(YuzuPath::LogDir, yuzu_path / LOG_DIR);
        GenerateYuzuPath(YuzuPath::NANDDir, yuzu_path / NAND_DIR);
        GenerateYuzuPath(YuzuPath::ScreenshotsDir, yuzu_path / SCREENSHOTS_DIR);
        GenerateYuzuPath(YuzuPath::SDMCDir, yuzu_path / SDMC_DIR);
        GenerateYuzuPath(YuzuPath::ShaderDir, yuzu_path / SHADER_DIR);
        GenerateYuzuPath(YuzuPath::TASDir, yuzu_path / TAS_DIR);
    }

    ~PathManagerImpl() = default;

    void GenerateYuzuPath(YuzuPath yuzu_path, const fs::path& new_path) {
        void(FS::CreateDir(new_path));

        SetYuzuPathImpl(yuzu_path, new_path);
    }

    std::unordered_map<YuzuPath, fs::path> yuzu_paths;
};

bool ValidatePath(const fs::path& path) {
    if (path.empty()) {
        LOG_ERROR(Common_Filesystem, "Input path is empty, path={}", PathToUTF8String(path));
        return false;
    }

#ifdef _WIN32
    if (path.u16string().size() >= MAX_PATH) {
        LOG_ERROR(Common_Filesystem, "Input path is too long, path={}", PathToUTF8String(path));
        return false;
    }
#else
    if (path.u8string().size() >= MAX_PATH) {
        LOG_ERROR(Common_Filesystem, "Input path is too long, path={}", PathToUTF8String(path));
        return false;
    }
#endif

    return true;
}

fs::path ConcatPath(const fs::path& first, const fs::path& second) {
    const bool second_has_dir_sep = IsDirSeparator(second.u8string().front());

    if (!second_has_dir_sep) {
        return (first / second).lexically_normal();
    }

    fs::path concat_path = first;
    concat_path += second;

    return concat_path.lexically_normal();
}

fs::path ConcatPathSafe(const fs::path& base, const fs::path& offset) {
    const auto concatenated_path = ConcatPath(base, offset);

    if (!IsPathSandboxed(base, concatenated_path)) {
        return base;
    }

    return concatenated_path;
}

bool IsPathSandboxed(const fs::path& base, const fs::path& path) {
    const auto base_string = RemoveTrailingSeparators(base.lexically_normal()).u8string();
    const auto path_string = RemoveTrailingSeparators(path.lexically_normal()).u8string();

    if (path_string.size() < base_string.size()) {
        return false;
    }

    return base_string.compare(0, base_string.size(), path_string, 0, base_string.size()) == 0;
}

bool IsDirSeparator(char character) {
    return character == '/' || character == '\\';
}

bool IsDirSeparator(char8_t character) {
    return character == u8'/' || character == u8'\\';
}

fs::path RemoveTrailingSeparators(const fs::path& path) {
    if (path.empty()) {
        return path;
    }

    auto string_path = path.u8string();

    while (IsDirSeparator(string_path.back())) {
        string_path.pop_back();
    }

    return fs::path{string_path};
}

const fs::path& GetYuzuPath(YuzuPath yuzu_path) {
    return PathManagerImpl::GetInstance().GetYuzuPathImpl(yuzu_path);
}

std::string GetYuzuPathString(YuzuPath yuzu_path) {
    return PathToUTF8String(GetYuzuPath(yuzu_path));
}

void SetYuzuPath(YuzuPath yuzu_path, const fs::path& new_path) {
    if (!FS::IsDir(new_path)) {
        LOG_ERROR(Common_Filesystem, "Filesystem object at new_path={} is not a directory",
                  PathToUTF8String(new_path));
        return;
    }

    PathManagerImpl::GetInstance().SetYuzuPathImpl(yuzu_path, new_path);
}

#ifdef _WIN32

fs::path GetExeDirectory() {
    wchar_t exe_path[MAX_PATH];

    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

    if (!exe_path) {
        LOG_ERROR(Common_Filesystem,
                  "Failed to get the path to the executable of the current process");
    }

    return fs::path{exe_path}.parent_path();
}

fs::path GetAppDataRoamingDirectory() {
    PWSTR appdata_roaming_path = nullptr;

    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_roaming_path);

    auto fs_appdata_roaming_path = fs::path{appdata_roaming_path};

    CoTaskMemFree(appdata_roaming_path);

    if (fs_appdata_roaming_path.empty()) {
        LOG_ERROR(Common_Filesystem, "Failed to get the path to the %APPDATA% directory");
    }

    return fs_appdata_roaming_path;
}

#else

fs::path GetHomeDirectory() {
    const char* home_env_var = getenv("HOME");

    if (home_env_var) {
        return fs::path{home_env_var};
    }

    LOG_INFO(Common_Filesystem,
             "$HOME is not defined in the environment variables, "
             "attempting to query passwd to get the home path of the current user");

    const auto* pw = getpwuid(getuid());

    if (!pw) {
        LOG_ERROR(Common_Filesystem, "Failed to get the home path of the current user");
        return {};
    }

    return fs::path{pw->pw_dir};
}

fs::path GetDataDirectory(const std::string& env_name) {
    const char* data_env_var = getenv(env_name.c_str());

    if (data_env_var) {
        return fs::path{data_env_var};
    }

    if (env_name == "XDG_DATA_HOME") {
        return GetHomeDirectory() / ".local/share";
    } else if (env_name == "XDG_CACHE_HOME") {
        return GetHomeDirectory() / ".cache";
    } else if (env_name == "XDG_CONFIG_HOME") {
        return GetHomeDirectory() / ".config";
    }

    return {};
}

#endif

#ifdef __APPLE__

fs::path GetBundleDirectory() {
    char app_bundle_path[MAXPATHLEN];

    // Get the main bundle for the app
    CFURLRef bundle_ref = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    CFStringRef bundle_path = CFURLCopyFileSystemPath(bundle_ref, kCFURLPOSIXPathStyle);

    CFStringGetFileSystemRepresentation(bundle_path, app_bundle_path, sizeof(app_bundle_path));

    CFRelease(bundle_ref);
    CFRelease(bundle_path);

    return fs::path{app_bundle_path};
}

#endif

// vvvvvvvvvv Deprecated vvvvvvvvvv //

std::string_view RemoveTrailingSlash(std::string_view path) {
    if (path.empty()) {
        return path;
    }

    if (path.back() == '\\' || path.back() == '/') {
        path.remove_suffix(1);
        return path;
    }

    return path;
}

std::vector<std::string> SplitPathComponents(std::string_view filename) {
    std::string copy(filename);
    std::replace(copy.begin(), copy.end(), '\\', '/');
    std::vector<std::string> out;

    std::stringstream stream(copy);
    std::string item;
    while (std::getline(stream, item, '/')) {
        out.push_back(std::move(item));
    }

    return out;
}

std::string SanitizePath(std::string_view path_, DirectorySeparator directory_separator) {
    std::string path(path_);
    char type1 = directory_separator == DirectorySeparator::BackwardSlash ? '/' : '\\';
    char type2 = directory_separator == DirectorySeparator::BackwardSlash ? '\\' : '/';

    if (directory_separator == DirectorySeparator::PlatformDefault) {
#ifdef _WIN32
        type1 = '/';
        type2 = '\\';
#endif
    }

    std::replace(path.begin(), path.end(), type1, type2);

    auto start = path.begin();
#ifdef _WIN32
    // allow network paths which start with a double backslash (e.g. \\server\share)
    if (start != path.end())
        ++start;
#endif
    path.erase(std::unique(start, path.end(),
                           [type2](char c1, char c2) { return c1 == type2 && c2 == type2; }),
               path.end());
    return std::string(RemoveTrailingSlash(path));
}

std::string_view GetParentPath(std::string_view path) {
    const auto name_bck_index = path.rfind('\\');
    const auto name_fwd_index = path.rfind('/');
    std::size_t name_index;

    if (name_bck_index == std::string_view::npos || name_fwd_index == std::string_view::npos) {
        name_index = std::min(name_bck_index, name_fwd_index);
    } else {
        name_index = std::max(name_bck_index, name_fwd_index);
    }

    return path.substr(0, name_index);
}

std::string_view GetPathWithoutTop(std::string_view path) {
    if (path.empty()) {
        return path;
    }

    while (path[0] == '\\' || path[0] == '/') {
        path.remove_prefix(1);
        if (path.empty()) {
            return path;
        }
    }

    const auto name_bck_index = path.find('\\');
    const auto name_fwd_index = path.find('/');
    return path.substr(std::min(name_bck_index, name_fwd_index) + 1);
}

std::string_view GetFilename(std::string_view path) {
    const auto name_index = path.find_last_of("\\/");

    if (name_index == std::string_view::npos) {
        return {};
    }

    return path.substr(name_index + 1);
}

std::string_view GetExtensionFromFilename(std::string_view name) {
    const std::size_t index = name.rfind('.');

    if (index == std::string_view::npos) {
        return {};
    }

    return name.substr(index + 1);
}

} // namespace Common::FS
