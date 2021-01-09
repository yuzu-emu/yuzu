// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <stdexcept>
#include <utility>

#include <fmt/format.h>

namespace Shader {

class LogicError : public std::logic_error {
public:
    template <typename... Args>
    LogicError(const char* message, Args&&... args)
        : std::logic_error{fmt::format(message, std::forward<Args>(args)...)} {}
};

class RuntimeError : public std::runtime_error {
public:
    template <typename... Args>
    RuntimeError(const char* message, Args&&... args)
        : std::runtime_error{fmt::format(message, std::forward<Args>(args)...)} {}
};

class NotImplementedException : public std::logic_error {
public:
    template <typename... Args>
    NotImplementedException(const char* message, Args&&... args)
        : std::logic_error{fmt::format(message, std::forward<Args>(args)...)} {}
};

class InvalidArgument : public std::invalid_argument {
public:
    template <typename... Args>
    InvalidArgument(const char* message, Args&&... args)
        : std::invalid_argument{fmt::format(message, std::forward<Args>(args)...)} {}
};

} // namespace Shader
