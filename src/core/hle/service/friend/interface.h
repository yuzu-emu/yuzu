// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/friend/friend.h"

namespace Service::Friend {

class Friend final : public Module::Interface {
public:
    explicit Friend(std::shared_ptr<Module> interface_module, Core::System& system,
                    const char* name);
    ~Friend() override;
};

} // namespace Service::Friend
