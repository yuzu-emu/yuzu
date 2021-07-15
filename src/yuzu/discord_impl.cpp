// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <string>
#include <discord_rpc.h>
#include "common/common_types.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "yuzu/discord_impl.h"
#include "yuzu/uisettings.h"

namespace DiscordRPC {

DiscordImpl::DiscordImpl() {
    DiscordEventHandlers handlers{};

    // The number is the client ID for yuzu, it's used for images and the
    // application name
    Discord_Initialize("712465656758665259", &handlers, 1, nullptr);
}

DiscordImpl::~DiscordImpl() {
    Discord_ClearPresence();
    Discord_Shutdown();
}

void DiscordImpl::Pause() {
    Discord_ClearPresence();
}

void DiscordImpl::Update() {
    s64 start_time = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    std::string title;
    if (Core::System::GetInstance().IsPoweredOn())
        Core::System::GetInstance().GetAppLoader().ReadTitle(title);
    DiscordRichPresence presence{};
    presence.largeImageKey = "yuzu_logo_ea";
    presence.largeImageText = "yuzu is an emulator for the Nintendo Switch";
    if (Core::System::GetInstance().IsPoweredOn()) {
        presence.state = title.c_str();
        presence.details = "Currently in game";
    } else {
        presence.details = "Not in game";
    }
    presence.startTimestamp = start_time;
    Discord_UpdatePresence(&presence);
}
} // namespace DiscordRPC
