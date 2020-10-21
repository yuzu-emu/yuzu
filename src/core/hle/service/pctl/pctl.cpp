// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/pctl/pctl.h"

namespace Service::PCTL {

PCTL::PCTL(std::shared_ptr<Module> interface_module, const char* name)
    : Module::Interface(std::move(interface_module), name) {
    static const FunctionInfo functions[] = {
        {0, &PCTL::CreateService, "CreateService"},
        {1, &PCTL::CreateServiceWithoutInitialize, "CreateServiceWithoutInitialize"},
    };
    RegisterHandlers(functions);
}

PCTL::~PCTL() = default;
} // namespace Service::PCTL
