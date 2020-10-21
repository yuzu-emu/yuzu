// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/acc/acc_u1.h"

namespace Service::Account {

ACC_U1::ACC_U1(std::shared_ptr<Module> interface_module,
               std::shared_ptr<ProfileManager> profile_manager, Core::System& system)
    : Module::Interface(std::move(interface_module), std::move(profile_manager), system, "acc:u1") {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ACC_U1::GetUserCount, "GetUserCount"},
        {1, &ACC_U1::GetUserExistence, "GetUserExistence"},
        {2, &ACC_U1::ListAllUsers, "ListAllUsers"},
        {3, &ACC_U1::ListOpenUsers, "ListOpenUsers"},
        {4, &ACC_U1::GetLastOpenedUser, "GetLastOpenedUser"},
        {5, &ACC_U1::GetProfile, "GetProfile"},
        {6, nullptr, "GetProfileDigest"}, // 3.0.0+
        {50, &ACC_U1::IsUserRegistrationRequestPermitted, "IsUserRegistrationRequestPermitted"},
        {51, &ACC_U1::TrySelectUserWithoutInteraction, "TrySelectUserWithoutInteraction"},
        {60, &ACC_U1::ListOpenContextStoredUsers, "ListOpenContextStoredUsers"}, // 5.0.0 - 5.1.0
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
        {140, &ACC_U1::ListQualifiedUsers, "ListQualifiedUsers"}, // 6.0.0+
        {150, nullptr, "AuthenticateApplicationAsync"}, // 10.0.0+
        {190, nullptr, "GetUserLastOpenedApplication"}, // 1.0.0 - 9.2.0
        {191, nullptr, "ActivateOpenContextHolder"}, // 7.0.0+
        {997, nullptr, "DebugInvalidateTokenCacheForUser"}, // 3.0.0+
        {998, nullptr, "DebugSetUserStateClose"},
        {999, nullptr, "DebugSetUserStateOpen"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

ACC_U1::~ACC_U1() = default;

} // namespace Service::Account
