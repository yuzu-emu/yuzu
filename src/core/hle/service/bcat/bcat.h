// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/bcat/module.h"

namespace Core {
class System;
}

namespace Service::BCAT {

class BCAT final : public Module::Interface {
public:
    explicit BCAT(Core::System& system, std::shared_ptr<Module> interface_module,
                  FileSystem::FileSystemController& fsc, const char* name);
    ~BCAT() override;
};

} // namespace Service::BCAT
