// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/bcat/bcat.h"

namespace Service::BCAT {

BCAT::BCAT(Core::System& system, std::shared_ptr<Module> interface_module,
           FileSystem::FileSystemController& fsc, const char* name)
    : Interface(system, std::move(interface_module), fsc, name) {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &BCAT::CreateBcatService, "CreateBcatService"},
        {1, &BCAT::CreateDeliveryCacheStorageService, "CreateDeliveryCacheStorageService"},
        {2, &BCAT::CreateDeliveryCacheStorageServiceWithApplicationId, "CreateDeliveryCacheStorageServiceWithApplicationId"},
        {3, nullptr, "CreateDeliveryCacheProgressService"},
        {4, nullptr, "CreateDeliveryCacheProgressServiceWithApplicationId"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

BCAT::~BCAT() = default;
} // namespace Service::BCAT
