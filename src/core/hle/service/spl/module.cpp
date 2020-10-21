// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <vector>
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/spl/csrng.h"
#include "core/hle/service/spl/module.h"
#include "core/hle/service/spl/spl.h"
#include "core/settings.h"

namespace Service::SPL {

Module::Interface::Interface(std::shared_ptr<Module> interface_module, const char* name)
    : ServiceFramework(name), interface_module(std::move(interface_module)),
      rng(Settings::values.rng_seed.GetValue().value_or(std::time(nullptr))) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetRandomBytes(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SPL, "called");

    const std::size_t size = ctx.GetWriteBufferSize();

    std::uniform_int_distribution<u16> distribution(0, std::numeric_limits<u8>::max());
    std::vector<u8> data(size);
    std::generate(data.begin(), data.end(), [&] { return static_cast<u8>(distribution(rng)); });

    ctx.WriteBuffer(data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto interface_module = std::make_shared<Module>();
    std::make_shared<CSRNG>(interface_module)->InstallAsService(service_manager);
    std::make_shared<SPL>(interface_module)->InstallAsService(service_manager);
}

} // namespace Service::SPL
