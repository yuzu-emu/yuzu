// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/spl/module.h"

namespace Service::SPL {

class CSRNG final : public Module::Interface {
public:
    explicit CSRNG(std::shared_ptr<Module> interface_module);
    ~CSRNG() override;
};

} // namespace Service::SPL
