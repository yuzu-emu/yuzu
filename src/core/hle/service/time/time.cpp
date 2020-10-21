// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/hardware_properties.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/service/time/interface.h"
#include "core/hle/service/time/time.h"
#include "core/hle/service/time/time_sharedmemory.h"
#include "core/hle/service/time/time_zone_service.h"

namespace Service::Time {

class ISystemClock final : public ServiceFramework<ISystemClock> {
public:
    explicit ISystemClock(Clock::SystemClockCore& clock_core, Core::System& system)
        : ServiceFramework("ISystemClock"), clock_core{clock_core}, system{system} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ISystemClock::GetCurrentTime, "GetCurrentTime"},
            {1, nullptr, "SetCurrentTime"},
            {2,  &ISystemClock::GetSystemClockContext, "GetSystemClockContext"},
            {3, nullptr, "SetSystemClockContext"},
            {4, nullptr, "GetOperationEventReadableHandle"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetCurrentTime(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Time, "called");

        if (!clock_core.IsInitialized()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_UNINITIALIZED_CLOCK);
            return;
        }

        s64 posix_time{};
        if (const ResultCode result{clock_core.GetCurrentTime(system, posix_time)};
            result.IsError()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(result);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.Push<s64>(posix_time);
    }

    void GetSystemClockContext(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Time, "called");

        if (!clock_core.IsInitialized()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_UNINITIALIZED_CLOCK);
            return;
        }

        Clock::SystemClockContext system_clock_context{};
        if (const ResultCode result{clock_core.GetClockContext(system, system_clock_context)};
            result.IsError()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(result);
            return;
        }

        IPC::ResponseBuilder rb{ctx, sizeof(Clock::SystemClockContext) / 4 + 2};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(system_clock_context);
    }

    Clock::SystemClockCore& clock_core;
    Core::System& system;
};

class ISteadyClock final : public ServiceFramework<ISteadyClock> {
public:
    explicit ISteadyClock(Clock::SteadyClockCore& clock_core, Core::System& system)
        : ServiceFramework("ISteadyClock"), clock_core{clock_core}, system{system} {
        static const FunctionInfo functions[] = {
            {0, &ISteadyClock::GetCurrentTimePoint, "GetCurrentTimePoint"},
            {2, nullptr, "GetTestOffset"},
            {3, nullptr, "SetTestOffset"},
            {100, nullptr, "GetRtcValue"},
            {101, nullptr, "IsRtcResetDetected"},
            {102, nullptr, "GetSetupResultValue"},
            {200, nullptr, "GetInternalOffset"},
            {201, nullptr, "SetInternalOffset"},
        };
        RegisterHandlers(functions);
    }

private:
    void GetCurrentTimePoint(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Time, "called");

        if (!clock_core.IsInitialized()) {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_UNINITIALIZED_CLOCK);
            return;
        }

        const Clock::SteadyClockTimePoint time_point{clock_core.GetCurrentTimePoint(system)};
        IPC::ResponseBuilder rb{ctx, (sizeof(Clock::SteadyClockTimePoint) / 4) + 2};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(time_point);
    }

    Clock::SteadyClockCore& clock_core;
    Core::System& system;
};

ResultCode Module::Interface::GetClockSnapshotFromSystemClockContextInternal(
    Kernel::Thread* thread, Clock::SystemClockContext user_context,
    Clock::SystemClockContext network_context, u8 type, Clock::ClockSnapshot& clock_snapshot) {

    auto& time_manager{interface_module->GetTimeManager()};

    clock_snapshot.is_automatic_correction_enabled =
        time_manager.GetStandardUserSystemClockCore().IsAutomaticCorrectionEnabled();
    clock_snapshot.user_context = user_context;
    clock_snapshot.network_context = network_context;

    if (const ResultCode result{
            time_manager.GetTimeZoneContentManager().GetTimeZoneManager().GetDeviceLocationName(
                clock_snapshot.location_name)};
        result != RESULT_SUCCESS) {
        return result;
    }

    const auto current_time_point{
        time_manager.GetStandardSteadyClockCore().GetCurrentTimePoint(system)};
    if (const ResultCode result{Clock::ClockSnapshot::GetCurrentTime(
            clock_snapshot.user_time, current_time_point, clock_snapshot.user_context)};
        result != RESULT_SUCCESS) {
        return result;
    }

    TimeZone::CalendarInfo userCalendarInfo{};
    if (const ResultCode result{
            time_manager.GetTimeZoneContentManager().GetTimeZoneManager().ToCalendarTimeWithMyRules(
                clock_snapshot.user_time, userCalendarInfo)};
        result != RESULT_SUCCESS) {
        return result;
    }

    clock_snapshot.user_calendar_time = userCalendarInfo.time;
    clock_snapshot.user_calendar_additional_time = userCalendarInfo.additiona_info;

    if (Clock::ClockSnapshot::GetCurrentTime(clock_snapshot.network_time, current_time_point,
                                             clock_snapshot.network_context) != RESULT_SUCCESS) {
        clock_snapshot.network_time = 0;
    }

    TimeZone::CalendarInfo networkCalendarInfo{};
    if (const ResultCode result{
            time_manager.GetTimeZoneContentManager().GetTimeZoneManager().ToCalendarTimeWithMyRules(
                clock_snapshot.network_time, networkCalendarInfo)};
        result != RESULT_SUCCESS) {
        return result;
    }

    clock_snapshot.network_calendar_time = networkCalendarInfo.time;
    clock_snapshot.network_calendar_additional_time = networkCalendarInfo.additiona_info;
    clock_snapshot.type = type;

    return RESULT_SUCCESS;
}

void Module::Interface::GetStandardUserSystemClock(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISystemClock>(
        interface_module->GetTimeManager().GetStandardUserSystemClockCore(), system);
}

void Module::Interface::GetStandardNetworkSystemClock(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISystemClock>(
        interface_module->GetTimeManager().GetStandardNetworkSystemClockCore(), system);
}

void Module::Interface::GetStandardSteadyClock(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISteadyClock>(
        interface_module->GetTimeManager().GetStandardSteadyClockCore(), system);
}

void Module::Interface::GetTimeZoneService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ITimeZoneService>(
        interface_module->GetTimeManager().GetTimeZoneContentManager());
}

void Module::Interface::GetStandardLocalSystemClock(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISystemClock>(
        interface_module->GetTimeManager().GetStandardLocalSystemClockCore(), system);
}

void Module::Interface::IsStandardNetworkSystemClockAccuracySufficient(
    Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    auto& clock_core{interface_module->GetTimeManager().GetStandardNetworkSystemClockCore()};
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(clock_core.IsStandardNetworkSystemClockAccuracySufficient(system));
}

void Module::Interface::CalculateMonotonicSystemClockBaseTimePoint(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");

    auto& steady_clock_core{interface_module->GetTimeManager().GetStandardSteadyClockCore()};
    if (!steady_clock_core.IsInitialized()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ERROR_UNINITIALIZED_CLOCK);
        return;
    }

    IPC::RequestParser rp{ctx};
    const auto context{rp.PopRaw<Clock::SystemClockContext>()};
    const auto current_time_point{steady_clock_core.GetCurrentTimePoint(system)};

    if (current_time_point.clock_source_id == context.steady_time_point.clock_source_id) {
        const auto ticks{Clock::TimeSpanType::FromTicks(system.CoreTiming().GetClockTicks(),
                                                        Core::Hardware::CNTFREQ)};
        const s64 base_time_point{context.offset + current_time_point.time_point -
                                  ticks.ToSeconds()};
        IPC::ResponseBuilder rb{ctx, (sizeof(s64) / 4) + 2};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(base_time_point);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ERROR_TIME_MISMATCH);
}

void Module::Interface::GetClockSnapshot(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::RequestParser rp{ctx};
    const auto type{rp.PopRaw<u8>()};

    Clock::SystemClockContext user_context{};
    if (const ResultCode result{
            interface_module->GetTimeManager().GetStandardUserSystemClockCore().GetClockContext(
                system, user_context)};
        result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }
    Clock::SystemClockContext network_context{};
    if (const ResultCode result{
            interface_module->GetTimeManager().GetStandardNetworkSystemClockCore().GetClockContext(
                system, network_context)};
        result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    Clock::ClockSnapshot clock_snapshot{};
    if (const ResultCode result{GetClockSnapshotFromSystemClockContextInternal(
            &ctx.GetThread(), user_context, network_context, type, clock_snapshot)};
        result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
    ctx.WriteBuffer(clock_snapshot);
}

void Module::Interface::GetClockSnapshotFromSystemClockContext(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::RequestParser rp{ctx};
    const auto type{rp.PopRaw<u8>()};
    rp.AlignWithPadding();

    const Clock::SystemClockContext user_context{rp.PopRaw<Clock::SystemClockContext>()};
    const Clock::SystemClockContext network_context{rp.PopRaw<Clock::SystemClockContext>()};

    Clock::ClockSnapshot clock_snapshot{};
    if (const ResultCode result{GetClockSnapshotFromSystemClockContextInternal(
            &ctx.GetThread(), user_context, network_context, type, clock_snapshot)};
        result != RESULT_SUCCESS) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
    ctx.WriteBuffer(clock_snapshot);
}

void Module::Interface::CalculateStandardUserSystemClockDifferenceByUser(
    Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");

    IPC::RequestParser rp{ctx};
    const auto snapshot_a = rp.PopRaw<Clock::ClockSnapshot>();
    const auto snapshot_b = rp.PopRaw<Clock::ClockSnapshot>();

    auto time_span_type{Clock::TimeSpanType::FromSeconds(snapshot_b.user_context.offset -
                                                         snapshot_a.user_context.offset)};

    if ((snapshot_b.user_context.steady_time_point.clock_source_id !=
         snapshot_a.user_context.steady_time_point.clock_source_id) ||
        (snapshot_b.is_automatic_correction_enabled &&
         snapshot_a.is_automatic_correction_enabled)) {
        time_span_type.nanoseconds = 0;
    }

    IPC::ResponseBuilder rb{ctx, (sizeof(s64) / 4) + 2};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(time_span_type.nanoseconds);
}

void Module::Interface::CalculateSpanBetween(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");

    IPC::RequestParser rp{ctx};
    const auto snapshot_a = rp.PopRaw<Clock::ClockSnapshot>();
    const auto snapshot_b = rp.PopRaw<Clock::ClockSnapshot>();

    Clock::TimeSpanType time_span_type{};
    s64 span{};
    if (const ResultCode result{snapshot_a.steady_clock_time_point.GetSpanBetween(
            snapshot_b.steady_clock_time_point, span)};
        result != RESULT_SUCCESS) {
        if (snapshot_a.network_time && snapshot_b.network_time) {
            time_span_type =
                Clock::TimeSpanType::FromSeconds(snapshot_b.network_time - snapshot_a.network_time);
        } else {
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_TIME_NOT_FOUND);
            return;
        }
    } else {
        time_span_type = Clock::TimeSpanType::FromSeconds(span);
    }

    IPC::ResponseBuilder rb{ctx, (sizeof(s64) / 4) + 2};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(time_span_type.nanoseconds);
}

void Module::Interface::GetSharedMemoryNativeHandle(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called");
    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(
        interface_module->GetTimeManager().GetSharedMemory().GetSharedMemoryHolder());
}

Module::Interface::Interface(std::shared_ptr<Module> interface_module, Core::System& system,
                             const char* name)
    : ServiceFramework(name), interface_module{std::move(interface_module)}, system{system} {}

Module::Interface::~Interface() = default;

void InstallInterfaces(Core::System& system) {
    auto interface_module{std::make_shared<Module>(system)};
    std::make_shared<Time>(interface_module, system, "time:a")
        ->InstallAsService(system.ServiceManager());
    std::make_shared<Time>(interface_module, system, "time:s")
        ->InstallAsService(system.ServiceManager());
    std::make_shared<Time>(interface_module, system, "time:u")
        ->InstallAsService(system.ServiceManager());
}

} // namespace Service::Time
