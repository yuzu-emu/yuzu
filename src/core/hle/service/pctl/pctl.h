// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/pctl/module.h"

namespace Service::PCTL {

class PCTL final : public Module::Interface {
public:
    explicit PCTL(std::shared_ptr<Module> interface_module, const char* name);
    ~PCTL() override;
};

} // namespace Service::PCTL
