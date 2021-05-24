// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/system_archive/system_version.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/set/set_sys.h"

namespace Service::Set {

namespace {
constexpr u64 SYSTEM_VERSION_FILE_MINOR_REVISION_OFFSET = 0x05;

enum class GetFirmwareVersionType {
    Version1,
    Version2,
};

void GetFirmwareVersionImpl(Kernel::HLERequestContext& ctx, GetFirmwareVersionType type) {
    LOG_WARNING(Service_SET, "called - Using hardcoded firmware version '{}'",
                FileSys::SystemArchive::GetLongDisplayVersion());

    ASSERT_MSG(ctx.GetWriteBufferSize() == 0x100,
               "FirmwareVersion output buffer must be 0x100 bytes in size!");

    // Instead of using the normal procedure of checking for the real system archive and if it
    // doesn't exist, synthesizing one, I feel that that would lead to strange bugs because a
    // used is using a really old or really new SystemVersion title. The synthesized one ensures
    // consistence (currently reports as 5.1.0-0.0)
    const auto archive = FileSys::SystemArchive::SystemVersion();

    const auto early_exit_failure = [&ctx](std::string_view desc, ResultCode code) {
        LOG_ERROR(Service_SET, "General failure while attempting to resolve firmware version ({}).",
                  desc);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(code);
    };

    if (archive == nullptr) {
        early_exit_failure("The system version archive couldn't be synthesized.",
                           FileSys::ERROR_FAILED_MOUNT_ARCHIVE);
        return;
    }

    const auto ver_file = archive->GetFile("file");
    if (ver_file == nullptr) {
        early_exit_failure("The system version archive didn't contain the file 'file'.",
                           FileSys::ERROR_INVALID_ARGUMENT);
        return;
    }

    auto data = ver_file->ReadAllBytes();
    if (data.size() != 0x100) {
        early_exit_failure("The system version file 'file' was not the correct size.",
                           FileSys::ERROR_OUT_OF_BOUNDS);
        return;
    }

    // If the command is GetFirmwareVersion (as opposed to GetFirmwareVersion2), hardware will
    // zero out the REVISION_MINOR field.
    if (type == GetFirmwareVersionType::Version1) {
        data[SYSTEM_VERSION_FILE_MINOR_REVISION_OFFSET] = 0;
    }

    ctx.WriteBuffer(data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}
} // Anonymous namespace

void SET_SYS::GetFirmwareVersion(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");
    GetFirmwareVersionImpl(ctx, GetFirmwareVersionType::Version1);
}

void SET_SYS::GetFirmwareVersion2(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");
    GetFirmwareVersionImpl(ctx, GetFirmwareVersionType::Version2);
}

void SET_SYS::GetColorSetId(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::ResponseBuilder rb{ctx, 3};

    rb.Push(ResultSuccess);
    rb.PushEnum(color_set);
}

void SET_SYS::SetColorSetId(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");

    IPC::RequestParser rp{ctx};
    color_set = rp.PopEnum<ColorSet>();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void SET_SYS::GetDeviceNickName(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_SET, "called");
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
    ctx.WriteBuffer(Settings::values.device_name);
}

SET_SYS::SET_SYS(Core::System& system_) : ServiceFramework{system_, "set:sys"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "SetLanguageCode"},
        {1, nullptr, "SetNetworkSettings"},
        {2, nullptr, "GetNetworkSettings"},
        {3, &SET_SYS::GetFirmwareVersion, "GetFirmwareVersion"},
        {4, &SET_SYS::GetFirmwareVersion2, "GetFirmwareVersion2"},
        {5, nullptr, "GetFirmwareVersionDigest"},
        {7, nullptr, "GetLockScreenFlag"},
        {8, nullptr, "SetLockScreenFlag"},
        {9, nullptr, "GetBacklightSettings"},
        {10, nullptr, "SetBacklightSettings"},
        {11, nullptr, "SetBluetoothDevicesSettings"},
        {12, nullptr, "GetBluetoothDevicesSettings"},
        {13, nullptr, "GetExternalSteadyClockSourceId"},
        {14, nullptr, "SetExternalSteadyClockSourceId"},
        {15, nullptr, "GetUserSystemClockContext"},
        {16, nullptr, "SetUserSystemClockContext"},
        {17, nullptr, "GetAccountSettings"},
        {18, nullptr, "SetAccountSettings"},
        {19, nullptr, "GetAudioVolume"},
        {20, nullptr, "SetAudioVolume"},
        {21, nullptr, "GetEulaVersions"},
        {22, nullptr, "SetEulaVersions"},
        {23, &SET_SYS::GetColorSetId, "GetColorSetId"},
        {24, &SET_SYS::SetColorSetId, "SetColorSetId"},
        {25, nullptr, "GetConsoleInformationUploadFlag"},
        {26, nullptr, "SetConsoleInformationUploadFlag"},
        {27, nullptr, "GetAutomaticApplicationDownloadFlag"},
        {28, nullptr, "SetAutomaticApplicationDownloadFlag"},
        {29, nullptr, "GetNotificationSettings"},
        {30, nullptr, "SetNotificationSettings"},
        {31, nullptr, "GetAccountNotificationSettings"},
        {32, nullptr, "SetAccountNotificationSettings"},
        {35, nullptr, "GetVibrationMasterVolume"},
        {36, nullptr, "SetVibrationMasterVolume"},
        {37, nullptr, "GetSettingsItemValueSize"},
        {38, nullptr, "GetSettingsItemValue"},
        {39, nullptr, "GetTvSettings"},
        {40, nullptr, "SetTvSettings"},
        {41, nullptr, "GetEdid"},
        {42, nullptr, "SetEdid"},
        {43, nullptr, "GetAudioOutputMode"},
        {44, nullptr, "SetAudioOutputMode"},
        {45, nullptr, "IsForceMuteOnHeadphoneRemoved"},
        {46, nullptr, "SetForceMuteOnHeadphoneRemoved"},
        {47, nullptr, "GetQuestFlag"},
        {48, nullptr, "SetQuestFlag"},
        {49, nullptr, "GetDataDeletionSettings"},
        {50, nullptr, "SetDataDeletionSettings"},
        {51, nullptr, "GetInitialSystemAppletProgramId"},
        {52, nullptr, "GetOverlayDispProgramId"},
        {53, nullptr, "GetDeviceTimeZoneLocationName"},
        {54, nullptr, "SetDeviceTimeZoneLocationName"},
        {55, nullptr, "GetWirelessCertificationFileSize"},
        {56, nullptr, "GetWirelessCertificationFile"},
        {57, nullptr, "SetRegionCode"},
        {58, nullptr, "GetNetworkSystemClockContext"},
        {59, nullptr, "SetNetworkSystemClockContext"},
        {60, nullptr, "IsUserSystemClockAutomaticCorrectionEnabled"},
        {61, nullptr, "SetUserSystemClockAutomaticCorrectionEnabled"},
        {62, nullptr, "GetDebugModeFlag"},
        {63, nullptr, "GetPrimaryAlbumStorage"},
        {64, nullptr, "SetPrimaryAlbumStorage"},
        {65, nullptr, "GetUsb30EnableFlag"},
        {66, nullptr, "SetUsb30EnableFlag"},
        {67, nullptr, "GetBatteryLot"},
        {68, nullptr, "GetSerialNumber"},
        {69, nullptr, "GetNfcEnableFlag"},
        {70, nullptr, "SetNfcEnableFlag"},
        {71, nullptr, "GetSleepSettings"},
        {72, nullptr, "SetSleepSettings"},
        {73, nullptr, "GetWirelessLanEnableFlag"},
        {74, nullptr, "SetWirelessLanEnableFlag"},
        {75, nullptr, "GetInitialLaunchSettings"},
        {76, nullptr, "SetInitialLaunchSettings"},
        {77, &SET_SYS::GetDeviceNickName, "GetDeviceNickName"},
        {78, nullptr, "SetDeviceNickName"},
        {79, nullptr, "GetProductModel"},
        {80, nullptr, "GetLdnChannel"},
        {81, nullptr, "SetLdnChannel"},
        {82, nullptr, "AcquireTelemetryDirtyFlagEventHandle"},
        {83, nullptr, "GetTelemetryDirtyFlags"},
        {84, nullptr, "GetPtmBatteryLot"},
        {85, nullptr, "SetPtmBatteryLot"},
        {86, nullptr, "GetPtmFuelGaugeParameter"},
        {87, nullptr, "SetPtmFuelGaugeParameter"},
        {88, nullptr, "GetBluetoothEnableFlag"},
        {89, nullptr, "SetBluetoothEnableFlag"},
        {90, nullptr, "GetMiiAuthorId"},
        {91, nullptr, "SetShutdownRtcValue"},
        {92, nullptr, "GetShutdownRtcValue"},
        {93, nullptr, "AcquireFatalDirtyFlagEventHandle"},
        {94, nullptr, "GetFatalDirtyFlags"},
        {95, nullptr, "GetAutoUpdateEnableFlag"},
        {96, nullptr, "SetAutoUpdateEnableFlag"},
        {97, nullptr, "GetNxControllerSettings"},
        {98, nullptr, "SetNxControllerSettings"},
        {99, nullptr, "GetBatteryPercentageFlag"},
        {100, nullptr, "SetBatteryPercentageFlag"},
        {101, nullptr, "GetExternalRtcResetFlag"},
        {102, nullptr, "SetExternalRtcResetFlag"},
        {103, nullptr, "GetUsbFullKeyEnableFlag"},
        {104, nullptr, "SetUsbFullKeyEnableFlag"},
        {105, nullptr, "SetExternalSteadyClockInternalOffset"},
        {106, nullptr, "GetExternalSteadyClockInternalOffset"},
        {107, nullptr, "GetBacklightSettingsEx"},
        {108, nullptr, "SetBacklightSettingsEx"},
        {109, nullptr, "GetHeadphoneVolumeWarningCount"},
        {110, nullptr, "SetHeadphoneVolumeWarningCount"},
        {111, nullptr, "GetBluetoothAfhEnableFlag"},
        {112, nullptr, "SetBluetoothAfhEnableFlag"},
        {113, nullptr, "GetBluetoothBoostEnableFlag"},
        {114, nullptr, "SetBluetoothBoostEnableFlag"},
        {115, nullptr, "GetInRepairProcessEnableFlag"},
        {116, nullptr, "SetInRepairProcessEnableFlag"},
        {117, nullptr, "GetHeadphoneVolumeUpdateFlag"},
        {118, nullptr, "SetHeadphoneVolumeUpdateFlag"},
        {119, nullptr, "NeedsToUpdateHeadphoneVolume"},
        {120, nullptr, "GetPushNotificationActivityModeOnSleep"},
        {121, nullptr, "SetPushNotificationActivityModeOnSleep"},
        {122, nullptr, "GetServiceDiscoveryControlSettings"},
        {123, nullptr, "SetServiceDiscoveryControlSettings"},
        {124, nullptr, "GetErrorReportSharePermission"},
        {125, nullptr, "SetErrorReportSharePermission"},
        {126, nullptr, "GetAppletLaunchFlags"},
        {127, nullptr, "SetAppletLaunchFlags"},
        {128, nullptr, "GetConsoleSixAxisSensorAccelerationBias"},
        {129, nullptr, "SetConsoleSixAxisSensorAccelerationBias"},
        {130, nullptr, "GetConsoleSixAxisSensorAngularVelocityBias"},
        {131, nullptr, "SetConsoleSixAxisSensorAngularVelocityBias"},
        {132, nullptr, "GetConsoleSixAxisSensorAccelerationGain"},
        {133, nullptr, "SetConsoleSixAxisSensorAccelerationGain"},
        {134, nullptr, "GetConsoleSixAxisSensorAngularVelocityGain"},
        {135, nullptr, "SetConsoleSixAxisSensorAngularVelocityGain"},
        {136, nullptr, "GetKeyboardLayout"},
        {137, nullptr, "SetKeyboardLayout"},
        {138, nullptr, "GetWebInspectorFlag"},
        {139, nullptr, "GetAllowedSslHosts"},
        {140, nullptr, "GetHostFsMountPoint"},
        {141, nullptr, "GetRequiresRunRepairTimeReviser"},
        {142, nullptr, "SetRequiresRunRepairTimeReviser"},
        {143, nullptr, "SetBlePairingSettings"},
        {144, nullptr, "GetBlePairingSettings"},
        {145, nullptr, "GetConsoleSixAxisSensorAngularVelocityTimeBias"},
        {146, nullptr, "SetConsoleSixAxisSensorAngularVelocityTimeBias"},
        {147, nullptr, "GetConsoleSixAxisSensorAngularAcceleration"},
        {148, nullptr, "SetConsoleSixAxisSensorAngularAcceleration"},
        {149, nullptr, "GetRebootlessSystemUpdateVersion"},
        {150, nullptr, "GetDeviceTimeZoneLocationUpdatedTime"},
        {151, nullptr, "SetDeviceTimeZoneLocationUpdatedTime"},
        {152, nullptr, "GetUserSystemClockAutomaticCorrectionUpdatedTime"},
        {153, nullptr, "SetUserSystemClockAutomaticCorrectionUpdatedTime"},
        {154, nullptr, "GetAccountOnlineStorageSettings"},
        {155, nullptr, "SetAccountOnlineStorageSettings"},
        {156, nullptr, "GetPctlReadyFlag"},
        {157, nullptr, "SetPctlReadyFlag"},
        {158, nullptr, "GetAnalogStickUserCalibrationL"},
        {159, nullptr, "SetAnalogStickUserCalibrationL"},
        {160, nullptr, "GetAnalogStickUserCalibrationR"},
        {161, nullptr, "SetAnalogStickUserCalibrationR"},
        {162, nullptr, "GetPtmBatteryVersion"},
        {163, nullptr, "SetPtmBatteryVersion"},
        {164, nullptr, "GetUsb30HostEnableFlag"},
        {165, nullptr, "SetUsb30HostEnableFlag"},
        {166, nullptr, "GetUsb30DeviceEnableFlag"},
        {167, nullptr, "SetUsb30DeviceEnableFlag"},
        {168, nullptr, "GetThemeId"},
        {169, nullptr, "SetThemeId"},
        {170, nullptr, "GetChineseTraditionalInputMethod"},
        {171, nullptr, "SetChineseTraditionalInputMethod"},
        {172, nullptr, "GetPtmCycleCountReliability"},
        {173, nullptr, "SetPtmCycleCountReliability"},
        {174, nullptr, "GetHomeMenuScheme"},
        {175, nullptr, "GetThemeSettings"},
        {176, nullptr, "SetThemeSettings"},
        {177, nullptr, "GetThemeKey"},
        {178, nullptr, "SetThemeKey"},
        {179, nullptr, "GetZoomFlag"},
        {180, nullptr, "SetZoomFlag"},
        {181, nullptr, "GetT"},
        {182, nullptr, "SetT"},
        {183, nullptr, "GetPlatformRegion"},
        {184, nullptr, "SetPlatformRegion"},
        {185, nullptr, "GetHomeMenuSchemeModel"},
        {186, nullptr, "GetMemoryUsageRateFlag"},
        {187, nullptr, "GetTouchScreenMode"},
        {188, nullptr, "SetTouchScreenMode"},
        {189, nullptr, "GetButtonConfigSettingsFull"},
        {190, nullptr, "SetButtonConfigSettingsFull"},
        {191, nullptr, "GetButtonConfigSettingsEmbedded"},
        {192, nullptr, "SetButtonConfigSettingsEmbedded"},
        {193, nullptr, "GetButtonConfigSettingsLeft"},
        {194, nullptr, "SetButtonConfigSettingsLeft"},
        {195, nullptr, "GetButtonConfigSettingsRight"},
        {196, nullptr, "SetButtonConfigSettingsRight"},
        {197, nullptr, "GetButtonConfigRegisteredSettingsEmbedded"},
        {198, nullptr, "SetButtonConfigRegisteredSettingsEmbedded"},
        {199, nullptr, "GetButtonConfigRegisteredSettings"},
        {200, nullptr, "SetButtonConfigRegisteredSettings"},
        {201, nullptr, "GetFieldTestingFlag"},
        {202, nullptr, "SetFieldTestingFlag"},
        {203, nullptr, "GetPanelCrcMode"},
        {204, nullptr, "SetPanelCrcMode"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

SET_SYS::~SET_SYS() = default;

} // namespace Service::Set
