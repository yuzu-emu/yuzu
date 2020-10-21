// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/applets/error.h"

namespace Core::Frontend {

ErrorApplet::~ErrorApplet() = default;

void DefaultErrorApplet::ShowError(ResultCode error, std::function<void()> finished) const {
    LOG_CRITICAL(Service_Fatal, "Application requested error display: {:04}-{:04} (raw={:08X})",
                 static_cast<u32>(error.error_module.Value()), error.description.Value(),
                 error.raw);
}

void DefaultErrorApplet::ShowErrorWithTimestamp(ResultCode error, std::chrono::seconds time,
                                                std::function<void()> finished) const {
    LOG_CRITICAL(
        Service_Fatal,
        "Application requested error display: {:04X}-{:04X} (raw={:08X}) with timestamp={:016X}",
        static_cast<u32>(error.error_module.Value()), error.description.Value(), error.raw,
        time.count());
}

void DefaultErrorApplet::ShowCustomErrorText(ResultCode error, std::string main_text,
                                             std::string detail_text,
                                             std::function<void()> finished) const {
    LOG_CRITICAL(Service_Fatal,
                 "Application requested custom error with error_code={:04X}-{:04X} (raw={:08X})",
                 static_cast<u32>(error.error_module.Value()), error.description.Value(),
                 error.raw);
    LOG_CRITICAL(Service_Fatal, "    Main Text: {}", main_text);
    LOG_CRITICAL(Service_Fatal, "    Detail Text: {}", detail_text);
}

} // namespace Core::Frontend
