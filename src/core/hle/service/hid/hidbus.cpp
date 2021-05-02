// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/kernel/k_writable_event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/hid/errors.h"
#include "core/hle/service/hid/hidbus.h"
#include "core/hle/service/service.h"
#include "core/memory.h"

namespace Service::HID {
// (15ms, 66Hz)
constexpr auto hidbus_update_ns = std::chrono::nanoseconds{15 * 1000 * 1000};

HidBus::HidBus(Core::System& system_) : ServiceFramework{system_, "hidbus"} {
    // clang-format off
    static const FunctionInfo functions[] = {
            {1, &HidBus::GetBusHandle, "GetBusHandle"},
            {2, &HidBus::IsExternalDeviceConnected, "IsExternalDeviceConnected"},
            {3, &HidBus::Initialize, "Initialize"},
            {4, &HidBus::Finalize, "Finalize"},
            {5, &HidBus::EnableExternalDevice, "EnableExternalDevice"},
            {6, &HidBus::GetExternalDeviceId, "GetExternalDeviceId"},
            {7, &HidBus::SendCommandAsync, "SendCommandAsync"},
            {8, &HidBus::GetSendCommandAsynceResult, "GetSendCommandAsynceResult"},
            {9, &HidBus::SetEventForSendCommandAsycResult, "SetEventForSendCommandAsycResult"},
            {10, &HidBus::GetSharedMemoryHandle, "GetSharedMemoryHandle"},
            {11, &HidBus::EnableJoyPollingReceiveMode, "EnableJoyPollingReceiveMode"},
            {12, &HidBus::DisableJoyPollingReceiveMode, "DisableJoyPollingReceiveMode"},
            {13, nullptr, "GetPollingData"},
            {14, &HidBus::SetStatusManagerType, "SetStatusManagerType"},
    };
    // clang-format on

    RegisterHandlers(functions);

    send_command_asyc_event = Kernel::KEvent::Create(kernel);
    send_command_asyc_event->Initialize("Hidbus:SendCommandAsycEvent");

    // Register update callbacks
    hidbus_update_event = Core::Timing::CreateEvent(
        "Hidbus::UpdateCallback",
        [this](std::uintptr_t user_data, std::chrono::nanoseconds ns_late) {
            const auto guard = LockService();
            UpdateHidbus(user_data, ns_late);
        });

    system_.CoreTiming().ScheduleEvent(hidbus_update_ns, hidbus_update_event);
}

HidBus::~HidBus() {
    system.CoreTiming().UnscheduleEvent(hidbus_update_event, 0);
}

void HidBus::UpdateHidbus(std::uintptr_t user_data, std::chrono::nanoseconds ns_late) {
    auto& core_timing = system.CoreTiming();

    if (is_hidbus_enabled) {
        ringcon.Update();

        auto& cur_entry = hidbus_status.entries[0];
        cur_entry.is_polling_mode = ringcon.IsPollingMode();
        cur_entry.polling_mode = ringcon.GetPollingMode();
        cur_entry.is_enabled = ringcon.IsEnabled();

        u8* shared_memory = system.Kernel().GetHidBusSharedMem().GetPointer();
        std::memcpy(shared_memory, &hidbus_status, sizeof(hidbus_status));
    }

    // If ns_late is higher than the update rate ignore the delay
    if (ns_late > hidbus_update_ns) {
        ns_late = {};
    }

    core_timing.ScheduleEvent(hidbus_update_ns - ns_late, hidbus_update_event);
}

void HidBus::GetBusHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id{rp.PopRaw<u32>()};
    const auto bus_type{rp.PopRaw<u64>()};
    const auto applet_resource_user_id{rp.PopRaw<u64>()};

    // TODO: Assign and use handle values
    bus_handle = {
        .abstracted_pad_id = 0,
        .internal_index = 0,
        .player_number = static_cast<u8>(npad_id),
        .bus_type = static_cast<BusType>(bus_type),
        .is_valid = true,
    };

    LOG_INFO(Service_HID, "called. npad_id={} bus_type={} applet_resource_user_id={}", npad_id,
             bus_type, applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 5};
    rb.Push(ResultSuccess);
    rb.Push(true);
    rb.PushRaw(bus_handle);
}

void HidBus::IsExternalDeviceConnected(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};

    LOG_WARNING(Service_HID,
                "(STUBBED) called abstracted_pad_id={} bus_type={} internal_index={} "
                "player_number={} is_valid={}",
                bus_handle_.abstracted_pad_id, bus_handle_.bus_type, bus_handle_.internal_index,
                bus_handle_.player_number, bus_handle_.is_valid);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<bool>(true);
}

void HidBus::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};

    is_hidbus_enabled = true;
    ringcon.Enable(true);

    // Initialize first entry
    auto& cur_entry = hidbus_status.entries[0];
    cur_entry.is_in_focus = true;
    cur_entry.is_connected = true;
    cur_entry.is_connected_result = ResultSuccess;
    cur_entry.is_enabled = true;
    cur_entry.is_polling_mode = false;
    std::memcpy(system.Kernel().GetHidBusSharedMem().GetPointer(), &hidbus_status,
                sizeof(hidbus_status));

    LOG_WARNING(Service_HID,
                "(STUBBED) called abstracted_pad_id={} bus_type={} internal_index={} "
                "player_number={} is_valid={}",
                bus_handle_.abstracted_pad_id, bus_handle_.bus_type, bus_handle_.internal_index,
                bus_handle_.player_number, bus_handle_.is_valid);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void HidBus::Finalize(Kernel::HLERequestContext& ctx) {
    is_hidbus_enabled = false;
    ringcon.Enable(false);

    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void HidBus::EnableExternalDevice(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto enable{rp.PopRaw<bool>()};
    const auto pad{rp.PopRaw<std::array<u8, 7>>()};
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};
    const auto inval{rp.PopRaw<u64>()};
    const auto applet_resource_user_id{rp.PopRaw<u64>()};

    ringcon.Enable(enable);

    LOG_INFO(Service_HID,
             "called  enable={} pad={} abstracted_pad_id={} bus_type={} internal_index={} "
             "player_number={} is_valid={} inval={} applet_resource_user_id{}",
             enable, pad[0], bus_handle_.abstracted_pad_id, bus_handle_.bus_type,
             bus_handle_.internal_index, bus_handle_.player_number, bus_handle_.is_valid, inval,
             applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void HidBus::GetExternalDeviceId(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};
    const u8 device_id = ringcon.GetDeviceId();

    LOG_INFO(
        Service_HID,
        "called abstracted_pad_id={} bus_type={} internal_index={} player_number={} is_valid={}",
        bus_handle_.abstracted_pad_id, bus_handle_.bus_type, bus_handle_.internal_index,
        bus_handle_.player_number, bus_handle_.is_valid);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(device_id);
}

void HidBus::SendCommandAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto data = ctx.ReadBuffer();
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};

    ringcon.SetCommand(data);

    // Send the reply event right away no need to wait
    send_command_asyc_event->GetWritableEvent().Signal();

    LOG_INFO(Service_HID,
             "called data_size={}, abstracted_pad_id={} bus_type={} internal_index={} "
             "player_number={} is_valid={}",
             data.size(), bus_handle_.abstracted_pad_id, bus_handle_.bus_type,
             bus_handle_.internal_index, bus_handle_.player_number, bus_handle_.is_valid);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
};

void HidBus::GetSendCommandAsynceResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};
    const std::vector<u8> data = ringcon.GetReply();

    LOG_INFO(
        Service_HID,
        "called  abstracted_pad_id={} bus_type={} internal_index={} player_number={} is_valid={}",
        bus_handle_.abstracted_pad_id, bus_handle_.bus_type, bus_handle_.internal_index,
        bus_handle_.player_number, bus_handle_.is_valid);

    const u64 data_size = ctx.WriteBuffer(data);
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(data_size);
};

void HidBus::SetEventForSendCommandAsycResult(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};

    LOG_INFO(
        Service_HID,
        "called  abstracted_pad_id={} bus_type={} internal_index={} player_number={} is_valid={}",
        bus_handle_.abstracted_pad_id, bus_handle_.bus_type, bus_handle_.internal_index,
        bus_handle_.player_number, bus_handle_.is_valid);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(send_command_asyc_event->GetReadableEvent());
};

void HidBus::GetSharedMemoryHandle(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_HID, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(&system.Kernel().GetHidBusSharedMem());
}

void HidBus::EnableJoyPollingReceiveMode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto t_mem_size{rp.Pop<u32>()};
    const auto t_mem_handle{ctx.GetCopyHandle(0)};
    const auto polling_mode_{rp.PopRaw<JoyPollingMode>()};
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};

    ASSERT_MSG(t_mem_size == 0x1000, "t_mem_size is not 0x1000 bytes");

    auto t_mem =
        system.CurrentProcess()->GetHandleTable().GetObject<Kernel::KTransferMemory>(t_mem_handle);

    if (t_mem.IsNull()) {
        LOG_ERROR(Service_HID, "t_mem is a nullptr for handle=0x{:08X}", t_mem_handle);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    ASSERT_MSG(t_mem->GetSize() == 0x1000, "t_mem has incorrect size");

    ringcon.SetPollingMode(polling_mode_);
    ringcon.SetTransferMemoryPointer(system.Memory().GetPointer(t_mem->GetSourceAddress()));

    LOG_INFO(Service_HID,
             "called t_mem_handle=0x{:08X} polling_mode={} abstracted_pad_id={} bus_type={} "
             "internal_index={} player_number={} is_valid={}",
             t_mem_handle, static_cast<u8>(polling_mode_), bus_handle_.abstracted_pad_id,
             bus_handle_.bus_type, bus_handle_.internal_index, bus_handle_.player_number,
             bus_handle_.is_valid);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void HidBus::DisableJoyPollingReceiveMode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto bus_handle_{rp.PopRaw<HidbusBusHandle>()};

    ringcon.DisablePollingMode();

    LOG_INFO(Service_HID,
             "called abstracted_pad_id = {} bus_type = {} internal_index ={} player_number={} "
             "is_valid={}",
             bus_handle_.abstracted_pad_id, bus_handle_.bus_type, bus_handle_.internal_index,
             bus_handle_.player_number, bus_handle_.is_valid);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void HidBus::SetStatusManagerType(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto input{rp.PopRaw<u32>()};

    LOG_WARNING(Service_HID, "(STUBBED) called input={}", input);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
};
} // namespace Service::HID
