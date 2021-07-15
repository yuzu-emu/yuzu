// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <filesystem>
#include <vector>

#include "common/fs/fs_util.h"

namespace Common::FS {

enum class YuzuPath {
    YuzuDir,        // Where yuzu stores its data.
    CacheDir,       // Where cached filesystem data is stored.
    ConfigDir,      // Where config files are stored.
    DumpDir,        // Where dumped data is stored.
    KeysDir,        // Where key files are stored.
    LoadDir,        // Where cheat/mod files are stored.
    LogDir,         // Where log files are stored.
    NANDDir,        // Where the emulated NAND is stored.
    ScreenshotsDir, // Where yuzu screenshots are stored.
    SDMCDir,        // Where the emulated SDMC is stored.
    ShaderDir,      // Where shaders are stored.
    TASDir,         // Where TAS scripts are stored.
};

/**
 * Validates a given path.
 *
 * A given path is valid if it meets these conditions:
 * - The path is not empty
 * - The path is not too long
 *
 * @param path Filesystem path
 *
 * @returns True if the path is valid, false otherwise.
 */
[[nodiscard]] bool ValidatePath(const std::filesystem::path& path);

#ifdef _WIN32
template <typename Path>
[[nodiscard]] bool ValidatePath(const Path& path) {
    if constexpr (IsChar<typename Path::value_type>) {
        return ValidatePath(ToU8String(path));
    } else {
        return ValidatePath(std::filesystem::path{path});
    }
}
#endif

/**
 * Concatenates two filesystem paths together.
 *
 * This is needed since the following occurs when using std::filesystem::path's operator/:
 * first: "/first/path"
 * second: "/second/path" (Note that the second path has a directory separator in the front)
 * first / second yields "/second/path" when the desired result is first/path/second/path
 *
 * @param first First filesystem path
 * @param second Second filesystem path
 *
 * @returns A concatenated filesystem path.
 */
[[nodiscard]] std::filesystem::path ConcatPath(const std::filesystem::path& first,
                                               const std::filesystem::path& second);

#ifdef _WIN32
template <typename Path1, typename Path2>
[[nodiscard]] std::filesystem::path ConcatPath(const Path1& first, const Path2& second) {
    using ValueType1 = typename Path1::value_type;
    using ValueType2 = typename Path2::value_type;
    if constexpr (IsChar<ValueType1> && IsChar<ValueType2>) {
        return ConcatPath(ToU8String(first), ToU8String(second));
    } else if constexpr (IsChar<ValueType1> && !IsChar<ValueType2>) {
        return ConcatPath(ToU8String(first), second);
    } else if constexpr (!IsChar<ValueType1> && IsChar<ValueType2>) {
        return ConcatPath(first, ToU8String(second));
    } else {
        return ConcatPath(std::filesystem::path{first}, std::filesystem::path{second});
    }
}
#endif

/**
 * Safe variant of ConcatPath that takes in a base path and an offset path from the given base path.
 *
 * If ConcatPath(base, offset) resolves to a path that is sandboxed within the base path,
 * this will return the concatenated path. Otherwise this will return the base path.
 *
 * @param base Base filesystem path
 * @param offset Offset filesystem path
 *
 * @returns A concatenated filesystem path if it is within the base path,
 *          returns the base path otherwise.
 */
[[nodiscard]] std::filesystem::path ConcatPathSafe(const std::filesystem::path& base,
                                                   const std::filesystem::path& offset);

#ifdef _WIN32
template <typename Path1, typename Path2>
[[nodiscard]] std::filesystem::path ConcatPathSafe(const Path1& base, const Path2& offset) {
    using ValueType1 = typename Path1::value_type;
    using ValueType2 = typename Path2::value_type;
    if constexpr (IsChar<ValueType1> && IsChar<ValueType2>) {
        return ConcatPathSafe(ToU8String(base), ToU8String(offset));
    } else if constexpr (IsChar<ValueType1> && !IsChar<ValueType2>) {
        return ConcatPathSafe(ToU8String(base), offset);
    } else if constexpr (!IsChar<ValueType1> && IsChar<ValueType2>) {
        return ConcatPathSafe(base, ToU8String(offset));
    } else {
        return ConcatPathSafe(std::filesystem::path{base}, std::filesystem::path{offset});
    }
}
#endif

/**
 * Checks whether a given path is sandboxed within a given base path.
 *
 * @param base Base filesystem path
 * @param path Filesystem path
 *
 * @returns True if the given path is sandboxed within the given base path, false otherwise.
 */
[[nodiscard]] bool IsPathSandboxed(const std::filesystem::path& base,
                                   const std::filesystem::path& path);

#ifdef _WIN32
template <typename Path1, typename Path2>
[[nodiscard]] bool IsPathSandboxed(const Path1& base, const Path2& path) {
    using ValueType1 = typename Path1::value_type;
    using ValueType2 = typename Path2::value_type;
    if constexpr (IsChar<ValueType1> && IsChar<ValueType2>) {
        return IsPathSandboxed(ToU8String(base), ToU8String(path));
    } else if constexpr (IsChar<ValueType1> && !IsChar<ValueType2>) {
        return IsPathSandboxed(ToU8String(base), path);
    } else if constexpr (!IsChar<ValueType1> && IsChar<ValueType2>) {
        return IsPathSandboxed(base, ToU8String(path));
    } else {
        return IsPathSandboxed(std::filesystem::path{base}, std::filesystem::path{path});
    }
}
#endif

/**
 * Checks if a character is a directory separator (either a forward slash or backslash).
 *
 * @param character Character
 *
 * @returns True if the character is a directory separator, false otherwise.
 */
[[nodiscard]] bool IsDirSeparator(char character);

/**
 * Checks if a character is a directory separator (either a forward slash or backslash).
 *
 * @param character Character
 *
 * @returns True if the character is a directory separator, false otherwise.
 */
[[nodiscard]] bool IsDirSeparator(char8_t character);

/**
 * Removes any trailing directory separators if they exist in the given path.
 *
 * @param path Filesystem path
 *
 * @returns The filesystem path without any trailing directory separators.
 */
[[nodiscard]] std::filesystem::path RemoveTrailingSeparators(const std::filesystem::path& path);

#ifdef _WIN32
template <typename Path>
[[nodiscard]] std::filesystem::path RemoveTrailingSeparators(const Path& path) {
    if constexpr (IsChar<typename Path::value_type>) {
        return RemoveTrailingSeparators(ToU8String(path));
    } else {
        return RemoveTrailingSeparators(std::filesystem::path{path});
    }
}
#endif

/**
 * Gets the filesystem path associated with the YuzuPath enum.
 *
 * @param yuzu_path YuzuPath enum
 *
 * @returns The filesystem path associated with the YuzuPath enum.
 */
[[nodiscard]] const std::filesystem::path& GetYuzuPath(YuzuPath yuzu_path);

/**
 * Gets the filesystem path associated with the YuzuPath enum as a UTF-8 encoded std::string.
 *
 * @param yuzu_path YuzuPath enum
 *
 * @returns The filesystem path associated with the YuzuPath enum as a UTF-8 encoded std::string.
 */
[[nodiscard]] std::string GetYuzuPathString(YuzuPath yuzu_path);

/**
 * Sets a new filesystem path associated with the YuzuPath enum.
 * If the filesystem object at new_path is not a directory, this function will not do anything.
 *
 * @param yuzu_path YuzuPath enum
 * @param new_path New filesystem path
 */
void SetYuzuPath(YuzuPath yuzu_path, const std::filesystem::path& new_path);

#ifdef _WIN32
template <typename Path>
void SetYuzuPath(YuzuPath yuzu_path, const Path& new_path) {
    if constexpr (IsChar<typename Path::value_type>) {
        SetYuzuPath(yuzu_path, ToU8String(new_path));
    } else {
        SetYuzuPath(yuzu_path, std::filesystem::path{new_path});
    }
}
#endif

#ifdef _WIN32

/**
 * Gets the path of the directory containing the executable of the current process.
 *
 * @returns The path of the directory containing the executable of the current process.
 */
[[nodiscard]] std::filesystem::path GetExeDirectory();

/**
 * Gets the path of the current user's %APPDATA% directory (%USERPROFILE%/AppData/Roaming).
 *
 * @returns The path of the current user's %APPDATA% directory.
 */
[[nodiscard]] std::filesystem::path GetAppDataRoamingDirectory();

#else

/**
 * Gets the path of the directory specified by the #HOME environment variable.
 * If $HOME is not defined, it will attempt to query the user database in passwd instead.
 *
 * @returns The path of the current user's home directory.
 */
[[nodiscard]] std::filesystem::path GetHomeDirectory();

/**
 * Gets the relevant paths for yuzu to store its data based on the given XDG environment variable.
 * See https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
 * Defaults to $HOME/.local/share for main application data,
 * $HOME/.cache for cached data, and $HOME/.config for configuration files.
 *
 * @param env_name XDG environment variable name
 *
 * @returns The path where yuzu should store its data.
 */
[[nodiscard]] std::filesystem::path GetDataDirectory(const std::string& env_name);

#endif

#ifdef __APPLE__

[[nodiscard]] std::filesystem::path GetBundleDirectory();

#endif

// vvvvvvvvvv Deprecated vvvvvvvvvv //

// Removes the final '/' or '\' if one exists
[[nodiscard]] std::string_view RemoveTrailingSlash(std::string_view path);

enum class DirectorySeparator {
    ForwardSlash,
    BackwardSlash,
    PlatformDefault,
};

// Splits the path on '/' or '\' and put the components into a vector
// i.e. "C:\Users\Yuzu\Documents\save.bin" becomes {"C:", "Users", "Yuzu", "Documents", "save.bin" }
[[nodiscard]] std::vector<std::string> SplitPathComponents(std::string_view filename);

// Removes trailing slash, makes all '\\' into '/', and removes duplicate '/'. Makes '/' into '\\'
// depending if directory_separator is BackwardSlash or PlatformDefault and running on windows
[[nodiscard]] std::string SanitizePath(
    std::string_view path,
    DirectorySeparator directory_separator = DirectorySeparator::ForwardSlash);

// Gets all of the text up to the last '/' or '\' in the path.
[[nodiscard]] std::string_view GetParentPath(std::string_view path);

// Gets all of the text after the first '/' or '\' in the path.
[[nodiscard]] std::string_view GetPathWithoutTop(std::string_view path);

// Gets the filename of the path
[[nodiscard]] std::string_view GetFilename(std::string_view path);

// Gets the extension of the filename
[[nodiscard]] std::string_view GetExtensionFromFilename(std::string_view name);

} // namespace Common::FS
