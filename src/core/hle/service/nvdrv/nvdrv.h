// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/nvdrv/syncpoint_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class KEvent;
}

namespace Service::NVFlinger {
class NVFlinger;
}

namespace Service::Nvidia {

class SyncpointManager;

namespace Devices {
class nvdevice;
}

/// Represents an Nvidia event
struct NvEvent {
    Kernel::KEvent* event{};
    Fence fence{};
};

struct EventInterface {
    // Mask representing currently busy events
    u64 events_mask{};
    // Each kernel event associated to an NV event
    std::array<NvEvent, MaxNvEvents> events;
    // The status of the current NVEvent
    std::array<EventState, MaxNvEvents> status{};
    // Tells if an NVEvent is registered or not
    std::array<bool, MaxNvEvents> registered{};
    // When an NVEvent is waiting on GPU interrupt, this is the sync_point
    // associated with it.
    std::array<u32, MaxNvEvents> assigned_syncpt{};
    // This is the value of the GPU interrupt for which the NVEvent is waiting
    // for.
    std::array<u32, MaxNvEvents> assigned_value{};
    // Constant to denote an unasigned syncpoint.
    static constexpr u32 unassigned_syncpt = 0xFFFFFFFF;
    std::optional<u32> GetFreeEvent() const {
        u64 mask = events_mask;
        for (u32 i = 0; i < MaxNvEvents; i++) {
            const bool is_free = (mask & 0x1) == 0;
            if (is_free) {
                if (status[i] == EventState::Registered || status[i] == EventState::Free) {
                    return {i};
                }
            }
            mask = mask >> 1;
        }
        return std::nullopt;
    }
    void SetEventStatus(const u32 event_id, EventState new_status) {
        EventState old_status = status[event_id];
        if (old_status == new_status) {
            return;
        }
        status[event_id] = new_status;
        if (new_status == EventState::Registered) {
            registered[event_id] = true;
        }
        if (new_status == EventState::Waiting || new_status == EventState::Busy) {
            events_mask |= (1ULL << event_id);
        }
    }
    void RegisterEvent(const u32 event_id) {
        registered[event_id] = true;
        if (status[event_id] == EventState::Free) {
            status[event_id] = EventState::Registered;
        }
    }
    void UnregisterEvent(const u32 event_id) {
        registered[event_id] = false;
        if (status[event_id] == EventState::Registered) {
            status[event_id] = EventState::Free;
        }
    }
    void LiberateEvent(const u32 event_id) {
        status[event_id] = registered[event_id] ? EventState::Registered : EventState::Free;
        events_mask &= ~(1ULL << event_id);
        assigned_syncpt[event_id] = unassigned_syncpt;
        assigned_value[event_id] = 0;
    }
};

class Module final {
public:
    explicit Module(Core::System& system_);
    ~Module();

    /// Returns a pointer to one of the available devices, identified by its name.
    template <typename T>
    std::shared_ptr<T> GetDevice(const std::string& name) {
        auto itr = devices.find(name);
        if (itr == devices.end())
            return nullptr;
        return std::static_pointer_cast<T>(itr->second);
    }

    NvResult VerifyFD(DeviceFD fd) const;

    /// Opens a device node and returns a file descriptor to it.
    DeviceFD Open(const std::string& device_name);

    /// Sends an ioctl command to the specified file descriptor.
    NvResult Ioctl1(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    std::vector<u8>& output);

    NvResult Ioctl2(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    const std::vector<u8>& inline_input, std::vector<u8>& output);

    NvResult Ioctl3(DeviceFD fd, Ioctl command, const std::vector<u8>& input,
                    std::vector<u8>& output, std::vector<u8>& inline_output);

    /// Closes a device file descriptor and returns operation success.
    NvResult Close(DeviceFD fd);

    void SignalSyncpt(const u32 syncpoint_id, const u32 value);

    Kernel::KReadableEvent& GetEvent(u32 event_id);

    Kernel::KWritableEvent& GetEventWriteable(u32 event_id);

private:
    /// Manages syncpoints on the host
    SyncpointManager syncpoint_manager;

    /// Id to use for the next open file descriptor.
    DeviceFD next_fd = 1;

    /// Mapping of file descriptors to the devices they reference.
    std::unordered_map<DeviceFD, std::shared_ptr<Devices::nvdevice>> open_files;

    /// Mapping of device node names to their implementation.
    std::unordered_map<std::string, std::shared_ptr<Devices::nvdevice>> devices;

    EventInterface events_interface;

    KernelHelpers::ServiceContext service_context;
};

/// Registers all NVDRV services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager, NVFlinger::NVFlinger& nvflinger,
                       Core::System& system);

} // namespace Service::Nvidia
