// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/fatal/fatal_u.h"

namespace Service::Fatal {

Fatal_U::Fatal_U(std::shared_ptr<Module> interface_module, Core::System& system)
    : Module::Interface(std::move(interface_module), system, "fatal:u") {
    static const FunctionInfo functions[] = {
        {0, &Fatal_U::ThrowFatal, "ThrowFatal"},
        {1, &Fatal_U::ThrowFatalWithPolicy, "ThrowFatalWithPolicy"},
        {2, &Fatal_U::ThrowFatalWithCpuContext, "ThrowFatalWithCpuContext"},
    };
    RegisterHandlers(functions);
}

Fatal_U::~Fatal_U() = default;

} // namespace Service::Fatal
