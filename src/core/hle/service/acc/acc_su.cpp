// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/acc/acc_su.h"

namespace Service::Account {

ACC_SU::ACC_SU(std::shared_ptr<Module> interface_module,
               std::shared_ptr<ProfileManager> profile_manager, Core::System& system)
    : Module::Interface(std::move(interface_module), std::move(profile_manager), system, "acc:su") {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ACC_SU::GetUserCount, "GetUserCount"},
        {1, &ACC_SU::GetUserExistence, "GetUserExistence"},
        {2, &ACC_SU::ListAllUsers, "ListAllUsers"},
        {3, &ACC_SU::ListOpenUsers, "ListOpenUsers"},
        {4, &ACC_SU::GetLastOpenedUser, "GetLastOpenedUser"},
        {5, &ACC_SU::GetProfile, "GetProfile"},
        {6, nullptr, "GetProfileDigest"}, // 3.0.0+
        {50, &ACC_SU::IsUserRegistrationRequestPermitted, "IsUserRegistrationRequestPermitted"},
        {51, &ACC_SU::TrySelectUserWithoutInteraction, "TrySelectUserWithoutInteraction"},
        {60, &ACC_SU::ListOpenContextStoredUsers, "ListOpenContextStoredUsers"}, // 5.0.0 - 5.1.0
        {99, nullptr, "DebugActivateOpenContextRetention"}, // 6.0.0+
        {100, nullptr, "GetUserRegistrationNotifier"},
        {101, nullptr, "GetUserStateChangeNotifier"},
        {102, nullptr, "GetBaasAccountManagerForSystemService"},
        {103, nullptr, "GetBaasUserAvailabilityChangeNotifier"},
        {104, nullptr, "GetProfileUpdateNotifier"},
        {105, nullptr, "CheckNetworkServiceAvailabilityAsync"}, // 4.0.0+
        {106, nullptr, "GetProfileSyncNotifier"}, // 9.0.0+
        {110, nullptr, "StoreSaveDataThumbnail"},
        {111, nullptr, "ClearSaveDataThumbnail"},
        {112, nullptr, "LoadSaveDataThumbnail"},
        {113, nullptr, "GetSaveDataThumbnailExistence"}, // 5.0.0+
        {120, nullptr, "ListOpenUsersInApplication"}, // 10.0.0+
        {130, nullptr, "ActivateOpenContextRetention"}, // 6.0.0+
        {140, &ACC_SU::ListQualifiedUsers, "ListQualifiedUsers"}, // 6.0.0+
        {150, nullptr, "AuthenticateApplicationAsync"}, // 10.0.0+
        {190, nullptr, "GetUserLastOpenedApplication"}, // 1.0.0 - 9.2.0
        {191, nullptr, "ActivateOpenContextHolder"}, // 7.0.0+
        {200, nullptr, "BeginUserRegistration"},
        {201, nullptr, "CompleteUserRegistration"},
        {202, nullptr, "CancelUserRegistration"},
        {203, nullptr, "DeleteUser"},
        {204, nullptr, "SetUserPosition"},
        {205, &ACC_SU::GetProfileEditor, "GetProfileEditor"},
        {206, nullptr, "CompleteUserRegistrationForcibly"},
        {210, nullptr, "CreateFloatingRegistrationRequest"}, // 3.0.0+
        {211, nullptr, "CreateProcedureToRegisterUserWithNintendoAccount"}, // 8.0.0+
        {212, nullptr, "ResumeProcedureToRegisterUserWithNintendoAccount"}, // 8.0.0+
        {230, nullptr, "AuthenticateServiceAsync"},
        {250, nullptr, "GetBaasAccountAdministrator"},
        {290, nullptr, "ProxyProcedureForGuestLoginWithNintendoAccount"},
        {291, nullptr, "ProxyProcedureForFloatingRegistrationWithNintendoAccount"}, // 3.0.0+
        {299, nullptr, "SuspendBackgroundDaemon"},
        {997, nullptr, "DebugInvalidateTokenCacheForUser"}, // 3.0.0+
        {998, nullptr, "DebugSetUserStateClose"},
        {999, nullptr, "DebugSetUserStateOpen"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

ACC_SU::~ACC_SU() = default;

} // namespace Service::Account
