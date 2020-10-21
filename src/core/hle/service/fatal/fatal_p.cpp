// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/fatal/fatal_p.h"

namespace Service::Fatal {

Fatal_P::Fatal_P(std::shared_ptr<Module> interface_module, Core::System& system)
    : Module::Interface(std::move(interface_module), system, "fatal:p") {}

Fatal_P::~Fatal_P() = default;

} // namespace Service::Fatal
