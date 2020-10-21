// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::PCTL {

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> interface_module, const char* name);
        ~Interface() override;

        void CreateService(Kernel::HLERequestContext& ctx);
        void CreateServiceWithoutInitialize(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> interface_module;
    };
};

/// Registers all PCTL services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace Service::PCTL
