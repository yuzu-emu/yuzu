// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nfp/nfp_user.h"

namespace Service::NFP {

NFP_User::NFP_User(std::shared_ptr<Module> interface_module, Core::System& system)
    : Module::Interface(std::move(interface_module), system, "nfp:user") {
    static const FunctionInfo functions[] = {
        {0, &NFP_User::CreateUserInterface, "CreateUserInterface"},
    };
    RegisterHandlers(functions);
}

NFP_User::~NFP_User() = default;

} // namespace Service::NFP
