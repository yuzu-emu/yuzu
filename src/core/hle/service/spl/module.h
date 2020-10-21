// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <random>
#include "core/hle/service/service.h"

namespace Service::SPL {

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> interface_module, const char* name);
        ~Interface() override;

        void GetRandomBytes(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> interface_module;

    private:
        std::mt19937 rng;
    };
};

/// Registers all SPL services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace Service::SPL
