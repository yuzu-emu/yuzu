// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace Core {
class System;
}

namespace Kernel {
class KernelCore;
class KEvent;
class KProcess;
} // namespace Kernel

namespace Service::KernelHelpers {

class ServiceContext {
public:
    ServiceContext(Core::System& system_, std::string name_);
    ~ServiceContext();

    Kernel::KEvent* CreateEvent(std::string&& name);

    void CloseEvent(Kernel::KEvent* event);

private:
    Kernel::KernelCore& kernel;
    Kernel::KProcess* process{};
};

} // namespace Service::KernelHelpers
