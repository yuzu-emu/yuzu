// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Fatal {

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> interface_module, Core::System& system,
                           const char* name);
        ~Interface() override;

        void ThrowFatal(Kernel::HLERequestContext& ctx);
        void ThrowFatalWithPolicy(Kernel::HLERequestContext& ctx);
        void ThrowFatalWithCpuContext(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> interface_module;
        Core::System& system;
    };
};

void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system);

} // namespace Service::Fatal
