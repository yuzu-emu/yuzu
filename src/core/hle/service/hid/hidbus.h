// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/hid/hidbus/ringcon.h"
#include "core/hle/service/service.h"

namespace Core::Timing {
struct EventType;
} // namespace Core::Timing

namespace Core {
class System;
} // namespace Core

namespace Kernel {
class KEvent;
class KReadableEvent;
} // namespace Kernel

namespace Service::HID {

enum class HidBusControllerTypes : std::size_t {
    RingController,
    Starlink,

    MaxControllers,
};

class HidBus final : public ServiceFramework<HidBus> {
public:
    explicit HidBus(Core::System& system_);
    ~HidBus() override;

    enum class BusType : u8 {
        LeftJoyRail,
        RightJoyRail,
        InternalBus,

        MaxBusType,
    };

    struct HidbusBusHandle {
        u32_le abstracted_pad_id;
        u8 internal_index;
        u8 player_number;
        BusType bus_type;
        bool is_valid;
    };
    static_assert(sizeof(HidbusBusHandle) == 0x8, "HidbusBusHandle is an invalid size");

    struct JoyPollingReceivedData {
        std::array<u8, 0x30> data;
        u64 out_size;
        u64 sampling_number;
    };
    static_assert(sizeof(JoyPollingReceivedData) == 0x40,
                  "JoyPollingReceivedData is an invalid size");

    struct HidbusStatusManagerEntry {
        u8 is_connected{};
        INSERT_PADDING_BYTES(0x3);
        ResultCode is_connected_result{0};
        u8 is_enabled{};
        u8 is_in_focus{};
        u8 is_polling_mode{};
        u8 reserved{};
        JoyPollingMode polling_mode{};
        std::array<u8, 0x70> data{};
    };
    static_assert(sizeof(HidbusStatusManagerEntry) == 0x80,
                  "HidbusStatusManagerEntry is an invalid size");

    struct HidbusStatusManager {
        std::array<HidbusStatusManagerEntry, 19> entries{};
    };
    static_assert(sizeof(HidbusStatusManager) <= 0x1000, "HidbusStatusManager is an invalid size");

private:
    void GetBusHandle(Kernel::HLERequestContext& ctx);
    void IsExternalDeviceConnected(Kernel::HLERequestContext& ctx);
    void Initialize(Kernel::HLERequestContext& ctx);
    void Finalize(Kernel::HLERequestContext& ctx);
    void EnableExternalDevice(Kernel::HLERequestContext& ctx);
    void GetExternalDeviceId(Kernel::HLERequestContext& ctx);
    void SendCommandAsync(Kernel::HLERequestContext& ctx);
    void GetSendCommandAsynceResult(Kernel::HLERequestContext& ctx);
    void SetEventForSendCommandAsycResult(Kernel::HLERequestContext& ctx);
    void GetSharedMemoryHandle(Kernel::HLERequestContext& ctx);
    void EnableJoyPollingReceiveMode(Kernel::HLERequestContext& ctx);
    void DisableJoyPollingReceiveMode(Kernel::HLERequestContext& ctx);
    void SetStatusManagerType(Kernel::HLERequestContext& ctx);

    void UpdateHidbus(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);

    bool is_hidbus_enabled{false};
    RingController ringcon{};
    HidbusBusHandle bus_handle{};
    HidbusStatusManager hidbus_status{};
    Kernel::KEvent* send_command_asyc_event;
    std::shared_ptr<Core::Timing::EventType> hidbus_update_event;
};

} // namespace Service::HID
