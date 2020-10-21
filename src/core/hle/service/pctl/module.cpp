// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/pctl/module.h"
#include "core/hle/service/pctl/pctl.h"

namespace Service::PCTL {

class IParentalControlService final : public ServiceFramework<IParentalControlService> {
public:
    IParentalControlService() : ServiceFramework("IParentalControlService") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, &IParentalControlService::Initialize, "Initialize"},
            {1001, &IParentalControlService::CheckFreeCommunicationPermission, "CheckFreeCommunicationPermission"},
            {1002, nullptr, "ConfirmLaunchApplicationPermission"},
            {1003, nullptr, "ConfirmResumeApplicationPermission"},
            {1004, nullptr, "ConfirmSnsPostPermission"},
            {1005, nullptr, "ConfirmSystemSettingsPermission"},
            {1006, nullptr, "IsRestrictionTemporaryUnlocked"},
            {1007, nullptr, "RevertRestrictionTemporaryUnlocked"},
            {1008, nullptr, "EnterRestrictedSystemSettings"},
            {1009, nullptr, "LeaveRestrictedSystemSettings"},
            {1010, nullptr, "IsRestrictedSystemSettingsEntered"},
            {1011, nullptr, "RevertRestrictedSystemSettingsEntered"},
            {1012, nullptr, "GetRestrictedFeatures"},
            {1013, nullptr, "ConfirmStereoVisionPermission"},
            {1014, nullptr, "ConfirmPlayableApplicationVideoOld"},
            {1015, nullptr, "ConfirmPlayableApplicationVideo"},
            {1016, nullptr, "ConfirmShowNewsPermission"},
            {1017, nullptr, "EndFreeCommunication"},
            {1018, nullptr, "IsFreeCommunicationAvailable"},
            {1031, nullptr, "IsRestrictionEnabled"},
            {1032, nullptr, "GetSafetyLevel"},
            {1033, nullptr, "SetSafetyLevel"},
            {1034, nullptr, "GetSafetyLevelSettings"},
            {1035, nullptr, "GetCurrentSettings"},
            {1036, nullptr, "SetCustomSafetyLevelSettings"},
            {1037, nullptr, "GetDefaultRatingOrganization"},
            {1038, nullptr, "SetDefaultRatingOrganization"},
            {1039, nullptr, "GetFreeCommunicationApplicationListCount"},
            {1042, nullptr, "AddToFreeCommunicationApplicationList"},
            {1043, nullptr, "DeleteSettings"},
            {1044, nullptr, "GetFreeCommunicationApplicationList"},
            {1045, nullptr, "UpdateFreeCommunicationApplicationList"},
            {1046, nullptr, "DisableFeaturesForReset"},
            {1047, nullptr, "NotifyApplicationDownloadStarted"},
            {1048, nullptr, "NotifyNetworkProfileCreated"},
            {1061, nullptr, "ConfirmStereoVisionRestrictionConfigurable"},
            {1062, nullptr, "GetStereoVisionRestriction"},
            {1063, nullptr, "SetStereoVisionRestriction"},
            {1064, nullptr, "ResetConfirmedStereoVisionPermission"},
            {1065, nullptr, "IsStereoVisionPermitted"},
            {1201, nullptr, "UnlockRestrictionTemporarily"},
            {1202, nullptr, "UnlockSystemSettingsRestriction"},
            {1203, nullptr, "SetPinCode"},
            {1204, nullptr, "GenerateInquiryCode"},
            {1205, nullptr, "CheckMasterKey"},
            {1206, nullptr, "GetPinCodeLength"},
            {1207, nullptr, "GetPinCodeChangedEvent"},
            {1208, nullptr, "GetPinCode"},
            {1403, nullptr, "IsPairingActive"},
            {1406, nullptr, "GetSettingsLastUpdated"},
            {1411, nullptr, "GetPairingAccountInfo"},
            {1421, nullptr, "GetAccountNickname"},
            {1424, nullptr, "GetAccountState"},
            {1425, nullptr, "RequestPostEvents"},
            {1432, nullptr, "GetSynchronizationEvent"},
            {1451, nullptr, "StartPlayTimer"},
            {1452, nullptr, "StopPlayTimer"},
            {1453, nullptr, "IsPlayTimerEnabled"},
            {1454, nullptr, "GetPlayTimerRemainingTime"},
            {1455, nullptr, "IsRestrictedByPlayTimer"},
            {1456, nullptr, "GetPlayTimerSettings"},
            {1457, nullptr, "GetPlayTimerEventToRequestSuspension"},
            {1458, nullptr, "IsPlayTimerAlarmDisabled"},
            {1471, nullptr, "NotifyWrongPinCodeInputManyTimes"},
            {1472, nullptr, "CancelNetworkRequest"},
            {1473, nullptr, "GetUnlinkedEvent"},
            {1474, nullptr, "ClearUnlinkedEvent"},
            {1601, nullptr, "DisableAllFeatures"},
            {1602, nullptr, "PostEnableAllFeatures"},
            {1603, nullptr, "IsAllFeaturesDisabled"},
            {1901, nullptr, "DeleteFromFreeCommunicationApplicationListForDebug"},
            {1902, nullptr, "ClearFreeCommunicationApplicationListForDebug"},
            {1903, nullptr, "GetExemptApplicationListCountForDebug"},
            {1904, nullptr, "GetExemptApplicationListForDebug"},
            {1905, nullptr, "UpdateExemptApplicationListForDebug"},
            {1906, nullptr, "AddToExemptApplicationListForDebug"},
            {1907, nullptr, "DeleteFromExemptApplicationListForDebug"},
            {1908, nullptr, "ClearExemptApplicationListForDebug"},
            {1941, nullptr, "DeletePairing"},
            {1951, nullptr, "SetPlayTimerSettingsForDebug"},
            {1952, nullptr, "GetPlayTimerSpentTimeForTest"},
            {1953, nullptr, "SetPlayTimerAlarmDisabledForDebug"},
            {2001, nullptr, "RequestPairingAsync"},
            {2002, nullptr, "FinishRequestPairing"},
            {2003, nullptr, "AuthorizePairingAsync"},
            {2004, nullptr, "FinishAuthorizePairing"},
            {2005, nullptr, "RetrievePairingInfoAsync"},
            {2006, nullptr, "FinishRetrievePairingInfo"},
            {2007, nullptr, "UnlinkPairingAsync"},
            {2008, nullptr, "FinishUnlinkPairing"},
            {2009, nullptr, "GetAccountMiiImageAsync"},
            {2010, nullptr, "FinishGetAccountMiiImage"},
            {2011, nullptr, "GetAccountMiiImageContentTypeAsync"},
            {2012, nullptr, "FinishGetAccountMiiImageContentType"},
            {2013, nullptr, "SynchronizeParentalControlSettingsAsync"},
            {2014, nullptr, "FinishSynchronizeParentalControlSettings"},
            {2015, nullptr, "FinishSynchronizeParentalControlSettingsWithLastUpdated"},
            {2016, nullptr, "RequestUpdateExemptionListAsync"},
        };
        RegisterHandlers(functions);
    }

private:
    void Initialize(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_PCTL, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 0};
        rb.Push(RESULT_SUCCESS);
    }

    void CheckFreeCommunicationPermission(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_PCTL, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }
};

void Module::Interface::CreateService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IParentalControlService>();
}

void Module::Interface::CreateServiceWithoutInitialize(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IParentalControlService>();
}

Module::Interface::Interface(std::shared_ptr<Module> interface_module, const char* name)
    : ServiceFramework(name), interface_module(std::move(interface_module)) {}

Module::Interface::~Interface() = default;

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto interface_module = std::make_shared<Module>();
    std::make_shared<PCTL>(interface_module, "pctl")->InstallAsService(service_manager);
    std::make_shared<PCTL>(interface_module, "pctl:a")->InstallAsService(service_manager);
    std::make_shared<PCTL>(interface_module, "pctl:r")->InstallAsService(service_manager);
    std::make_shared<PCTL>(interface_module, "pctl:s")->InstallAsService(service_manager);
}

} // namespace Service::PCTL
