// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/acc/acc.h"

namespace Service::Account {

class ACC_U0 final : public Module::Interface {
public:
    explicit ACC_U0(std::shared_ptr<Module> interface_module,
                    std::shared_ptr<ProfileManager> profile_manager, Core::System& system);
    ~ACC_U0() override;
};

} // namespace Service::Account
