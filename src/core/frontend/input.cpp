// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/frontend/input.h"

namespace Input {

std::function<void(void)> tas_update_callback;

void Input::SetTASUpdateCallback(std::function<void(void)> callback) {
    tas_update_callback = callback;
}

void Input::RequestTASUpdate() {
    if (tas_update_callback != nullptr) {
        tas_update_callback();
    }
}

} // namespace Input
