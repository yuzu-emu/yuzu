// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/acc/acc_u0.h"

namespace Service::Account {

ACC_U0::ACC_U0(std::shared_ptr<Module> interface_module,
               std::shared_ptr<ProfileManager> profile_manager, Core::System& system)
    : Module::Interface(std::move(interface_module), std::move(profile_manager), system, "acc:u0") {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ACC_U0::GetUserCount, "GetUserCount"},
        {1, &ACC_U0::GetUserExistence, "GetUserExistence"},
        {2, &ACC_U0::ListAllUsers, "ListAllUsers"},
        {3, &ACC_U0::ListOpenUsers, "ListOpenUsers"},
        {4, &ACC_U0::GetLastOpenedUser, "GetLastOpenedUser"},
        {5, &ACC_U0::GetProfile, "GetProfile"},
        {6, nullptr, "GetProfileDigest"}, // 3.0.0+
        {50, &ACC_U0::IsUserRegistrationRequestPermitted, "IsUserRegistrationRequestPermitted"},
        {51, &ACC_U0::TrySelectUserWithoutInteraction, "TrySelectUserWithoutInteraction"},
        {60, &ACC_U0::ListOpenContextStoredUsers, "ListOpenContextStoredUsers"}, // 5.0.0 - 5.1.0
        {99, nullptr, "DebugActivateOpenContextRetention"}, // 6.0.0+
        {100, &ACC_U0::InitializeApplicationInfo, "InitializeApplicationInfo"},
        {101, &ACC_U0::GetBaasAccountManagerForApplication, "GetBaasAccountManagerForApplication"},
        {102, nullptr, "AuthenticateApplicationAsync"},
        {103, nullptr, "CheckNetworkServiceAvailabilityAsync"}, // 4.0.0+
        {110, nullptr, "StoreSaveDataThumbnail"},
        {111, nullptr, "ClearSaveDataThumbnail"},
        {120, nullptr, "CreateGuestLoginRequest"},
        {130, &ACC_U0::LoadOpenContext, "LoadOpenContext"}, // 5.0.0+
        {131, &ACC_U0::ListOpenContextStoredUsers, "ListOpenContextStoredUsers"}, // 6.0.0+
        {140, &ACC_U0::InitializeApplicationInfoRestricted, "InitializeApplicationInfoRestricted"}, // 6.0.0+
        {141, &ACC_U0::ListQualifiedUsers, "ListQualifiedUsers"}, // 6.0.0+
        {150, &ACC_U0::IsUserAccountSwitchLocked, "IsUserAccountSwitchLocked"}, // 6.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

ACC_U0::~ACC_U0() = default;

} // namespace Service::Account
