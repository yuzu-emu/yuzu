// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service {

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace BCAT {

class Backend;

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(Core::System& system_, std::shared_ptr<Module> interface_module,
                           FileSystem::FileSystemController& fsc_, const char* name);
        ~Interface() override;

        void CreateBcatService(Kernel::HLERequestContext& ctx);
        void CreateDeliveryCacheStorageService(Kernel::HLERequestContext& ctx);
        void CreateDeliveryCacheStorageServiceWithApplicationId(Kernel::HLERequestContext& ctx);

    protected:
        FileSystem::FileSystemController& fsc;

        std::shared_ptr<Module> interface_module;
        std::unique_ptr<Backend> backend;

    private:
        Core::System& system;
    };
};

/// Registers all BCAT services with the specified service manager.
void InstallInterfaces(Core::System& system);

} // namespace BCAT

} // namespace Service
