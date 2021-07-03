// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>

#include <fmt/format.h>
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/service/nvdrv/devices/nvdevice.h"
#include "core/hle/service/nvdrv/devices/nvdisp_disp0.h"
#include "core/hle/service/nvdrv/devices/nvhost_as_gpu.h"
#include "core/hle/service/nvdrv/devices/nvhost_ctrl.h"
#include "core/hle/service/nvdrv/devices/nvhost_ctrl_gpu.h"
#include "core/hle/service/nvdrv/devices/nvhost_gpu.h"
#include "core/hle/service/nvdrv/devices/nvhost_nvdec.h"
#include "core/hle/service/nvdrv/devices/nvhost_nvjpg.h"
#include "core/hle/service/nvdrv/devices/nvhost_vic.h"
#include "core/hle/service/nvdrv/devices/nvmap.h"
#include "core/hle/service/nvdrv/interface.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvdrv/nvmemp.h"
#include "core/hle/service/nvdrv/syncpoint_manager.h"
#include "core/hle/service/nvflinger/nvflinger.h"

namespace Service::Nvidia {

void InstallInterfaces(SM::ServiceManager& service_manager, NVFlinger::NVFlinger& nvflinger,
                       Core::System& system) {
    auto module_ = std::make_shared<Module>(system);
    std::make_shared<NVDRV>(system, module_, "nvdrv")->InstallAsService(service_manager);
    std::make_shared<NVDRV>(system, module_, "nvdrv:a")->InstallAsService(service_manager);
    std::make_shared<NVDRV>(system, module_, "nvdrv:s")->InstallAsService(service_manager);
    std::make_shared<NVDRV>(system, module_, "nvdrv:t")->InstallAsService(service_manager);
    std::make_shared<NVMEMP>(system)->InstallAsService(service_manager);
    nvflinger.SetNVDrvInstance(module_);
}

Module::Module(Core::System& system)
    : syncpoint_manager{system.GPU()}, service_context{system, "nvdrv"} {
    for (u32 i = 0; i < MaxNvEvents; i++) {
        events_interface.events[i].event =
            service_context.CreateEvent(fmt::format("NVDRV::NvEvent_{}", i));
        events_interface.status[i] = EventState::Free;
        events_interface.registered[i] = false;
    }
    auto nvmap_dev = std::make_shared<Devices::nvmap>(system);
    devices["/dev/nvhost-as-gpu"] = std::make_shared<Devices::nvhost_as_gpu>(system, nvmap_dev);
    devices["/dev/nvhost-gpu"] =
        std::make_shared<Devices::nvhost_gpu>(system, nvmap_dev, syncpoint_manager);
    devices["/dev/nvhost-ctrl-gpu"] = std::make_shared<Devices::nvhost_ctrl_gpu>(system);
    devices["/dev/nvmap"] = nvmap_dev;
    devices["/dev/nvdisp_disp0"] = std::make_shared<Devices::nvdisp_disp0>(system, nvmap_dev);
    devices["/dev/nvhost-ctrl"] =
        std::make_shared<Devices::nvhost_ctrl>(system, events_interface, syncpoint_manager);
    devices["/dev/nvhost-nvdec"] =
        std::make_shared<Devices::nvhost_nvdec>(system, nvmap_dev, syncpoint_manager);
    devices["/dev/nvhost-nvjpg"] = std::make_shared<Devices::nvhost_nvjpg>(system);
    devices["/dev/nvhost-vic"] =
        std::make_shared<Devices::nvhost_vic>(system, nvmap_dev, syncpoint_manager);
}

Module::~Module() {
    for (u32 i = 0; i < MaxNvEvents; i++) {
        service_context.CloseEvent(events_interface.events[i].event);
    }
}

NvResult Module::VerifyFD(DeviceFD fd) const {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    if (open_files.find(fd) == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    return NvResult::Success;
}

DeviceFD Module::Open(const std::string& device_name) {
    if (devices.find(device_name) == devices.end()) {
        LOG_ERROR(Service_NVDRV, "Trying to open unknown device {}", device_name);
        return INVALID_NVDRV_FD;
    }

    auto device = devices[device_name];
    const DeviceFD fd = next_fd++;

    device->OnOpen(fd);

    open_files[fd] = std::move(device);

    return fd;
}

NvResult Module::Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                        std::vector<u8>& output) {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    const auto itr = open_files.find(fd);

    if (itr == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    return itr->second->Ioctl1(fd, command, input, output);
}

NvResult Module::Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                        const std::vector<u8>& inline_input, std::vector<u8>& output) {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    const auto itr = open_files.find(fd);

    if (itr == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    return itr->second->Ioctl2(fd, command, input, inline_input, output);
}

NvResult Module::Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                        std::vector<u8>& output, std::vector<u8>& inline_output) {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    const auto itr = open_files.find(fd);

    if (itr == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    return itr->second->Ioctl3(fd, command, input, output, inline_output);
}

NvResult Module::Close(DeviceFD fd) {
    if (fd < 0) {
        LOG_ERROR(Service_NVDRV, "Invalid DeviceFD={}!", fd);
        return NvResult::InvalidState;
    }

    const auto itr = open_files.find(fd);

    if (itr == open_files.end()) {
        LOG_ERROR(Service_NVDRV, "Could not find DeviceFD={}!", fd);
        return NvResult::NotImplemented;
    }

    itr->second->OnClose(fd);

    open_files.erase(itr);

    return NvResult::Success;
}

void Module::SignalSyncpt(const u32 syncpoint_id, const u32 value) {
    for (u32 i = 0; i < MaxNvEvents; i++) {
        if (events_interface.assigned_syncpt[i] == syncpoint_id &&
            events_interface.assigned_value[i] == value) {
            events_interface.LiberateEvent(i);
            events_interface.events[i].event->GetWritableEvent().Signal();
        }
    }
}

Kernel::KReadableEvent& Module::GetEvent(const u32 event_id) {
    return events_interface.events[event_id].event->GetReadableEvent();
}

Kernel::KWritableEvent& Module::GetEventWriteable(const u32 event_id) {
    return events_interface.events[event_id].event->GetWritableEvent();
}

} // namespace Service::Nvidia
