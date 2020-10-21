// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include "common/common_paths.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/constants.h"
#include "core/core_timing.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/acc/acc.h"
#include "core/hle/service/acc/acc_aa.h"
#include "core/hle/service/acc/acc_su.h"
#include "core/hle/service/acc/acc_u0.h"
#include "core/hle/service/acc/acc_u1.h"
#include "core/hle/service/acc/errors.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/glue/arp.h"
#include "core/hle/service/glue/manager.h"
#include "core/hle/service/sm/sm.h"
#include "core/loader/loader.h"

namespace Service::Account {

constexpr ResultCode ERR_INVALID_BUFFER_SIZE{ErrorModule::Account, 30};
constexpr ResultCode ERR_FAILED_SAVE_DATA{ErrorModule::Account, 100};

static std::string GetImagePath(Common::UUID uuid) {
    return Common::FS::GetUserPath(Common::FS::UserPath::NANDDir) +
           "/system/save/8000000000000010/su/avators/" + uuid.FormatSwitch() + ".jpg";
}

static constexpr u32 SanitizeJPEGSize(std::size_t size) {
    constexpr std::size_t max_jpeg_image_size = 0x20000;
    return static_cast<u32>(std::min(size, max_jpeg_image_size));
}

class IManagerForSystemService final : public ServiceFramework<IManagerForSystemService> {
public:
    explicit IManagerForSystemService(Common::UUID user_id)
        : ServiceFramework("IManagerForSystemService") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CheckAvailability"},
            {1, nullptr, "GetAccountId"},
            {2, nullptr, "EnsureIdTokenCacheAsync"},
            {3, nullptr, "LoadIdTokenCache"},
            {100, nullptr, "SetSystemProgramIdentification"},
            {101, nullptr, "RefreshNotificationTokenAsync"}, // 7.0.0+
            {110, nullptr, "GetServiceEntryRequirementCache"}, // 4.0.0+
            {111, nullptr, "InvalidateServiceEntryRequirementCache"}, // 4.0.0+
            {112, nullptr, "InvalidateTokenCache"}, // 4.0.0 - 6.2.0
            {113, nullptr, "GetServiceEntryRequirementCacheForOnlinePlay"}, // 6.1.0+
            {120, nullptr, "GetNintendoAccountId"},
            {121, nullptr, "CalculateNintendoAccountAuthenticationFingerprint"}, // 9.0.0+
            {130, nullptr, "GetNintendoAccountUserResourceCache"},
            {131, nullptr, "RefreshNintendoAccountUserResourceCacheAsync"},
            {132, nullptr, "RefreshNintendoAccountUserResourceCacheAsyncIfSecondsElapsed"},
            {133, nullptr, "GetNintendoAccountVerificationUrlCache"}, // 9.0.0+
            {134, nullptr, "RefreshNintendoAccountVerificationUrlCache"}, // 9.0.0+
            {135, nullptr, "RefreshNintendoAccountVerificationUrlCacheAsyncIfSecondsElapsed"}, // 9.0.0+
            {140, nullptr, "GetNetworkServiceLicenseCache"}, // 5.0.0+
            {141, nullptr, "RefreshNetworkServiceLicenseCacheAsync"}, // 5.0.0+
            {142, nullptr, "RefreshNetworkServiceLicenseCacheAsyncIfSecondsElapsed"}, // 5.0.0+
            {150, nullptr, "CreateAuthorizationRequest"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

// 3.0.0+
class IFloatingRegistrationRequest final : public ServiceFramework<IFloatingRegistrationRequest> {
public:
    explicit IFloatingRegistrationRequest(Common::UUID user_id)
        : ServiceFramework("IFloatingRegistrationRequest") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetSessionId"},
            {12, nullptr, "GetAccountId"},
            {13, nullptr, "GetLinkedNintendoAccountId"},
            {14, nullptr, "GetNickname"},
            {15, nullptr, "GetProfileImage"},
            {21, nullptr, "LoadIdTokenCache"},
            {100, nullptr, "RegisterUser"}, // [1.0.0-3.0.2] RegisterAsync
            {101, nullptr, "RegisterUserWithUid"}, // [1.0.0-3.0.2] RegisterWithUidAsync
            {102, nullptr, "RegisterNetworkServiceAccountAsync"}, // 4.0.0+
            {103, nullptr, "RegisterNetworkServiceAccountWithUidAsync"}, // 4.0.0+
            {110, nullptr, "SetSystemProgramIdentification"},
            {111, nullptr, "EnsureIdTokenCacheAsync"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IAdministrator final : public ServiceFramework<IAdministrator> {
public:
    explicit IAdministrator(Common::UUID user_id) : ServiceFramework("IAdministrator") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CheckAvailability"},
            {1, nullptr, "GetAccountId"},
            {2, nullptr, "EnsureIdTokenCacheAsync"},
            {3, nullptr, "LoadIdTokenCache"},
            {100, nullptr, "SetSystemProgramIdentification"},
            {101, nullptr, "RefreshNotificationTokenAsync"}, // 7.0.0+
            {110, nullptr, "GetServiceEntryRequirementCache"}, // 4.0.0+
            {111, nullptr, "InvalidateServiceEntryRequirementCache"}, // 4.0.0+
            {112, nullptr, "InvalidateTokenCache"}, // 4.0.0 - 6.2.0
            {113, nullptr, "GetServiceEntryRequirementCacheForOnlinePlay"}, // 6.1.0+
            {120, nullptr, "GetNintendoAccountId"},
            {121, nullptr, "CalculateNintendoAccountAuthenticationFingerprint"}, // 9.0.0+
            {130, nullptr, "GetNintendoAccountUserResourceCache"},
            {131, nullptr, "RefreshNintendoAccountUserResourceCacheAsync"},
            {132, nullptr, "RefreshNintendoAccountUserResourceCacheAsyncIfSecondsElapsed"},
            {133, nullptr, "GetNintendoAccountVerificationUrlCache"}, // 9.0.0+
            {134, nullptr, "RefreshNintendoAccountVerificationUrlCacheAsync"}, // 9.0.0+
            {135, nullptr, "RefreshNintendoAccountVerificationUrlCacheAsyncIfSecondsElapsed"}, // 9.0.0+
            {140, nullptr, "GetNetworkServiceLicenseCache"}, // 5.0.0+
            {141, nullptr, "RefreshNetworkServiceLicenseCacheAsync"}, // 5.0.0+
            {142, nullptr, "RefreshNetworkServiceLicenseCacheAsyncIfSecondsElapsed"}, // 5.0.0+
            {150, nullptr, "CreateAuthorizationRequest"},
            {200, nullptr, "IsRegistered"},
            {201, nullptr, "RegisterAsync"},
            {202, nullptr, "UnregisterAsync"},
            {203, nullptr, "DeleteRegistrationInfoLocally"},
            {220, nullptr, "SynchronizeProfileAsync"},
            {221, nullptr, "UploadProfileAsync"},
            {222, nullptr, "SynchronizaProfileAsyncIfSecondsElapsed"},
            {250, nullptr, "IsLinkedWithNintendoAccount"},
            {251, nullptr, "CreateProcedureToLinkWithNintendoAccount"},
            {252, nullptr, "ResumeProcedureToLinkWithNintendoAccount"},
            {255, nullptr, "CreateProcedureToUpdateLinkageStateOfNintendoAccount"},
            {256, nullptr, "ResumeProcedureToUpdateLinkageStateOfNintendoAccount"},
            {260, nullptr, "CreateProcedureToLinkNnidWithNintendoAccount"}, // 3.0.0+
            {261, nullptr, "ResumeProcedureToLinkNnidWithNintendoAccount"}, // 3.0.0+
            {280, nullptr, "ProxyProcedureToAcquireApplicationAuthorizationForNintendoAccount"},
            {290, nullptr, "GetRequestForNintendoAccountUserResourceView"}, // 8.0.0+
            {300, nullptr, "TryRecoverNintendoAccountUserStateAsync"}, // 6.0.0+
            {400, nullptr, "IsServiceEntryRequirementCacheRefreshRequiredForOnlinePlay"}, // 6.1.0+
            {401, nullptr, "RefreshServiceEntryRequirementCacheForOnlinePlayAsync"}, // 6.1.0+
            {900, nullptr, "GetAuthenticationInfoForWin"}, // 9.0.0+
            {901, nullptr, "ImportAsyncForWin"}, // 9.0.0+
            {997, nullptr, "DebugUnlinkNintendoAccountAsync"},
            {998, nullptr, "DebugSetAvailabilityErrorDetail"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IAuthorizationRequest final : public ServiceFramework<IAuthorizationRequest> {
public:
    explicit IAuthorizationRequest(Common::UUID user_id)
        : ServiceFramework("IAuthorizationRequest") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetSessionId"},
            {10, nullptr, "InvokeWithoutInteractionAsync"},
            {19, nullptr, "IsAuthorized"},
            {20, nullptr, "GetAuthorizationCode"},
            {21, nullptr, "GetIdToken"},
            {22, nullptr, "GetState"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IOAuthProcedure final : public ServiceFramework<IOAuthProcedure> {
public:
    explicit IOAuthProcedure(Common::UUID user_id) : ServiceFramework("IOAuthProcedure") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "PrepareAsync"},
            {1, nullptr, "GetRequest"},
            {2, nullptr, "ApplyResponse"},
            {3, nullptr, "ApplyResponseAsync"},
            {10, nullptr, "Suspend"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

// 3.0.0+
class IOAuthProcedureForExternalNsa final : public ServiceFramework<IOAuthProcedureForExternalNsa> {
public:
    explicit IOAuthProcedureForExternalNsa(Common::UUID user_id)
        : ServiceFramework("IOAuthProcedureForExternalNsa") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "PrepareAsync"},
            {1, nullptr, "GetRequest"},
            {2, nullptr, "ApplyResponse"},
            {3, nullptr, "ApplyResponseAsync"},
            {10, nullptr, "Suspend"},
            {100, nullptr, "GetAccountId"},
            {101, nullptr, "GetLinkedNintendoAccountId"},
            {102, nullptr, "GetNickname"},
            {103, nullptr, "GetProfileImage"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IOAuthProcedureForNintendoAccountLinkage final
    : public ServiceFramework<IOAuthProcedureForNintendoAccountLinkage> {
public:
    explicit IOAuthProcedureForNintendoAccountLinkage(Common::UUID user_id)
        : ServiceFramework("IOAuthProcedureForNintendoAccountLinkage") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "PrepareAsync"},
            {1, nullptr, "GetRequest"},
            {2, nullptr, "ApplyResponse"},
            {3, nullptr, "ApplyResponseAsync"},
            {10, nullptr, "Suspend"},
            {100, nullptr, "GetRequestWithTheme"},
            {101, nullptr, "IsNetworkServiceAccountReplaced"},
            {199, nullptr, "GetUrlForIntroductionOfExtraMembership"}, // 2.0.0 - 5.1.0
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class INotifier final : public ServiceFramework<INotifier> {
public:
    explicit INotifier(Common::UUID user_id) : ServiceFramework("INotifier") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetSystemEvent"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IProfileCommon : public ServiceFramework<IProfileCommon> {
public:
    explicit IProfileCommon(const char* name, bool editor_commands, Common::UUID user_id,
                            ProfileManager& profile_manager)
        : ServiceFramework(name), profile_manager(profile_manager), user_id(user_id) {
        static const FunctionInfo functions[] = {
            {0, &IProfileCommon::Get, "Get"},
            {1, &IProfileCommon::GetBase, "GetBase"},
            {10, &IProfileCommon::GetImageSize, "GetImageSize"},
            {11, &IProfileCommon::LoadImage, "LoadImage"},
        };

        RegisterHandlers(functions);

        if (editor_commands) {
            static const FunctionInfo editor_functions[] = {
                {100, &IProfileCommon::Store, "Store"},
                {101, &IProfileCommon::StoreWithImage, "StoreWithImage"},
            };

            RegisterHandlers(editor_functions);
        }
    }

protected:
    void Get(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_ACC, "called user_id={}", user_id.Format());
        ProfileBase profile_base{};
        ProfileData data{};
        if (profile_manager.GetProfileBaseAndData(user_id, profile_base, data)) {
            ctx.WriteBuffer(data);
            IPC::ResponseBuilder rb{ctx, 16};
            rb.Push(RESULT_SUCCESS);
            rb.PushRaw(profile_base);
        } else {
            LOG_ERROR(Service_ACC, "Failed to get profile base and data for user={}",
                      user_id.Format());
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(RESULT_UNKNOWN); // TODO(ogniK): Get actual error code
        }
    }

    void GetBase(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_ACC, "called user_id={}", user_id.Format());
        ProfileBase profile_base{};
        if (profile_manager.GetProfileBase(user_id, profile_base)) {
            IPC::ResponseBuilder rb{ctx, 16};
            rb.Push(RESULT_SUCCESS);
            rb.PushRaw(profile_base);
        } else {
            LOG_ERROR(Service_ACC, "Failed to get profile base for user={}", user_id.Format());
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(RESULT_UNKNOWN); // TODO(ogniK): Get actual error code
        }
    }

    void LoadImage(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_ACC, "called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);

        const Common::FS::IOFile image(GetImagePath(user_id), "rb");
        if (!image.IsOpen()) {
            LOG_WARNING(Service_ACC,
                        "Failed to load user provided image! Falling back to built-in backup...");
            ctx.WriteBuffer(Core::Constants::ACCOUNT_BACKUP_JPEG);
            rb.Push(SanitizeJPEGSize(Core::Constants::ACCOUNT_BACKUP_JPEG.size()));
            return;
        }

        const u32 size = SanitizeJPEGSize(image.GetSize());
        std::vector<u8> buffer(size);
        image.ReadBytes(buffer.data(), buffer.size());

        ctx.WriteBuffer(buffer);
        rb.Push<u32>(size);
    }

    void GetImageSize(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_ACC, "called");
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);

        const Common::FS::IOFile image(GetImagePath(user_id), "rb");

        if (!image.IsOpen()) {
            LOG_WARNING(Service_ACC,
                        "Failed to load user provided image! Falling back to built-in backup...");
            rb.Push(SanitizeJPEGSize(Core::Constants::ACCOUNT_BACKUP_JPEG.size()));
        } else {
            rb.Push(SanitizeJPEGSize(image.GetSize()));
        }
    }

    void Store(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto base = rp.PopRaw<ProfileBase>();

        const auto user_data = ctx.ReadBuffer();

        LOG_DEBUG(Service_ACC, "called, username='{}', timestamp={:016X}, uuid={}",
                  Common::StringFromFixedZeroTerminatedBuffer(
                      reinterpret_cast<const char*>(base.username.data()), base.username.size()),
                  base.timestamp, base.user_uuid.Format());

        if (user_data.size() < sizeof(ProfileData)) {
            LOG_ERROR(Service_ACC, "ProfileData buffer too small!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_INVALID_BUFFER_SIZE);
            return;
        }

        ProfileData data;
        std::memcpy(&data, user_data.data(), sizeof(ProfileData));

        if (!profile_manager.SetProfileBaseAndData(user_id, base, data)) {
            LOG_ERROR(Service_ACC, "Failed to update profile data and base!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_FAILED_SAVE_DATA);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void StoreWithImage(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto base = rp.PopRaw<ProfileBase>();

        const auto user_data = ctx.ReadBuffer();
        const auto image_data = ctx.ReadBuffer(1);

        LOG_DEBUG(Service_ACC, "called, username='{}', timestamp={:016X}, uuid={}",
                  Common::StringFromFixedZeroTerminatedBuffer(
                      reinterpret_cast<const char*>(base.username.data()), base.username.size()),
                  base.timestamp, base.user_uuid.Format());

        if (user_data.size() < sizeof(ProfileData)) {
            LOG_ERROR(Service_ACC, "ProfileData buffer too small!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_INVALID_BUFFER_SIZE);
            return;
        }

        ProfileData data;
        std::memcpy(&data, user_data.data(), sizeof(ProfileData));

        Common::FS::IOFile image(GetImagePath(user_id), "wb");

        if (!image.IsOpen() || !image.Resize(image_data.size()) ||
            image.WriteBytes(image_data.data(), image_data.size()) != image_data.size() ||
            !profile_manager.SetProfileBaseAndData(user_id, base, data)) {
            LOG_ERROR(Service_ACC, "Failed to update profile data, base, and image!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERR_FAILED_SAVE_DATA);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    ProfileManager& profile_manager;
    Common::UUID user_id{Common::INVALID_UUID}; ///< The user id this profile refers to.
};

class IProfile final : public IProfileCommon {
public:
    IProfile(Common::UUID user_id, ProfileManager& profile_manager)
        : IProfileCommon("IProfile", false, user_id, profile_manager) {}
};

class IProfileEditor final : public IProfileCommon {
public:
    IProfileEditor(Common::UUID user_id, ProfileManager& profile_manager)
        : IProfileCommon("IProfileEditor", true, user_id, profile_manager) {}
};

class IAsyncContext final : public ServiceFramework<IAsyncContext> {
public:
    explicit IAsyncContext(Common::UUID user_id) : ServiceFramework("IAsyncContext") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetSystemEvent"},
            {1, nullptr, "Cancel"},
            {2, nullptr, "HasDone"},
            {3, nullptr, "GetResult"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class ISessionObject final : public ServiceFramework<ISessionObject> {
public:
    explicit ISessionObject(Common::UUID user_id) : ServiceFramework("ISessionObject") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {999, nullptr, "Dummy"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IGuestLoginRequest final : public ServiceFramework<IGuestLoginRequest> {
public:
    explicit IGuestLoginRequest(Common::UUID) : ServiceFramework("IGuestLoginRequest") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetSessionId"},
            {11, nullptr, "Unknown"}, // 1.0.0 - 2.3.0 (the name is blank on Switchbrew)
            {12, nullptr, "GetAccountId"},
            {13, nullptr, "GetLinkedNintendoAccountId"},
            {14, nullptr, "GetNickname"},
            {15, nullptr, "GetProfileImage"},
            {21, nullptr, "LoadIdTokenCache"}, // 3.0.0+
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IManagerForApplication final : public ServiceFramework<IManagerForApplication> {
public:
    explicit IManagerForApplication(Common::UUID user_id)
        : ServiceFramework("IManagerForApplication"), user_id(user_id) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IManagerForApplication::CheckAvailability, "CheckAvailability"},
            {1, &IManagerForApplication::GetAccountId, "GetAccountId"},
            {2, nullptr, "EnsureIdTokenCacheAsync"},
            {3, nullptr, "LoadIdTokenCache"},
            {130, nullptr, "GetNintendoAccountUserResourceCacheForApplication"},
            {150, nullptr, "CreateAuthorizationRequest"},
            {160, &IManagerForApplication::StoreOpenContext, "StoreOpenContext"},
            {170, nullptr, "LoadNetworkServiceLicenseKindAsync"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CheckAvailability(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_ACC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(false); // TODO: Check when this is supposed to return true and when not
    }

    void GetAccountId(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_ACC, "called");

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<u64>(user_id.GetNintendoID());
    }

    void StoreOpenContext(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_ACC, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    Common::UUID user_id;
};

// 6.0.0+
class IAsyncNetworkServiceLicenseKindContext final
    : public ServiceFramework<IAsyncNetworkServiceLicenseKindContext> {
public:
    explicit IAsyncNetworkServiceLicenseKindContext(Common::UUID user_id)
        : ServiceFramework("IAsyncNetworkServiceLicenseKindContext") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetSystemEvent"},
            {1, nullptr, "Cancel"},
            {2, nullptr, "HasDone"},
            {3, nullptr, "GetResult"},
            {4, nullptr, "GetNetworkServiceLicenseKind"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

// 8.0.0+
class IOAuthProcedureForUserRegistration final
    : public ServiceFramework<IOAuthProcedureForUserRegistration> {
public:
    explicit IOAuthProcedureForUserRegistration(Common::UUID user_id)
        : ServiceFramework("IOAuthProcedureForUserRegistration") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "PrepareAsync"},
            {1, nullptr, "GetRequest"},
            {2, nullptr, "ApplyResponse"},
            {3, nullptr, "ApplyResponseAsync"},
            {10, nullptr, "Suspend"},
            {100, nullptr, "GetAccountId"},
            {101, nullptr, "GetLinkedNintendoAccountId"},
            {102, nullptr, "GetNickname"},
            {103, nullptr, "GetProfileImage"},
            {110, nullptr, "RegisterUserAsync"},
            {111, nullptr, "GetUid"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class DAUTH_O final : public ServiceFramework<DAUTH_O> {
public:
    explicit DAUTH_O(Common::UUID) : ServiceFramework("dauth:o") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "EnsureAuthenticationTokenCacheAsync"}, // [5.0.0-5.1.0] GeneratePostData
            {1, nullptr, "LoadAuthenticationTokenCache"}, // 6.0.0+
            {2, nullptr, "InvalidateAuthenticationTokenCache"}, // 6.0.0+
            {10, nullptr, "EnsureEdgeTokenCacheAsync"}, // 6.0.0+
            {11, nullptr, "LoadEdgeTokenCache"}, // 6.0.0+
            {12, nullptr, "InvalidateEdgeTokenCache"}, // 6.0.0+
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

// 6.0.0+
class IAsyncResult final : public ServiceFramework<IAsyncResult> {
public:
    explicit IAsyncResult(Common::UUID user_id) : ServiceFramework("IAsyncResult") {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetResult"},
            {1, nullptr, "Cancel"},
            {2, nullptr, "IsAvailable"},
            {3, nullptr, "GetSystemEvent"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void Module::Interface::GetUserCount(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(static_cast<u32>(profile_manager->GetUserCount()));
}

void Module::Interface::GetUserExistence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    Common::UUID user_id = rp.PopRaw<Common::UUID>();
    LOG_DEBUG(Service_ACC, "called user_id={}", user_id.Format());

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push(profile_manager->UserExists(user_id));
}

void Module::Interface::ListAllUsers(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");
    ctx.WriteBuffer(profile_manager->GetAllUsers());
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::ListOpenUsers(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");
    ctx.WriteBuffer(profile_manager->GetOpenUsers());
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::GetLastOpenedUser(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");
    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<Common::UUID>(profile_manager->GetLastOpenedUser());
}

void Module::Interface::GetProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    Common::UUID user_id = rp.PopRaw<Common::UUID>();
    LOG_DEBUG(Service_ACC, "called user_id={}", user_id.Format());

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IProfile>(user_id, *profile_manager);
}

void Module::Interface::IsUserRegistrationRequestPermitted(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_ACC, "(STUBBED) called");
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push(profile_manager->CanSystemRegisterUser());
}

void Module::Interface::InitializeApplicationInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    LOG_DEBUG(Service_ACC, "called");
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(InitializeApplicationInfoBase());
}

void Module::Interface::InitializeApplicationInfoRestricted(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    LOG_WARNING(Service_ACC, "(Partial implementation) called");

    // TODO(ogniK): We require checking if the user actually owns the title and what not. As of
    // currently, we assume the user owns the title. InitializeApplicationInfoBase SHOULD be called
    // first then we do extra checks if the game is a digital copy.

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(InitializeApplicationInfoBase());
}

ResultCode Module::Interface::InitializeApplicationInfoBase() {
    if (application_info) {
        LOG_ERROR(Service_ACC, "Application already initialized");
        return ERR_ACCOUNTINFO_ALREADY_INITIALIZED;
    }

    // TODO(ogniK): This should be changed to reflect the target process for when we have multiple
    // processes emulated. As we don't actually have pid support we should assume we're just using
    // our own process
    const auto& current_process = system.Kernel().CurrentProcess();
    const auto launch_property =
        system.GetARPManager().GetLaunchProperty(current_process->GetTitleID());

    if (launch_property.Failed()) {
        LOG_ERROR(Service_ACC, "Failed to get launch property");
        return ERR_ACCOUNTINFO_BAD_APPLICATION;
    }

    switch (launch_property->base_game_storage_id) {
    case FileSys::StorageId::GameCard:
        application_info.application_type = ApplicationType::GameCard;
        break;
    case FileSys::StorageId::Host:
    case FileSys::StorageId::NandUser:
    case FileSys::StorageId::SdCard:
    case FileSys::StorageId::None: // Yuzu specific, differs from hardware
        application_info.application_type = ApplicationType::Digital;
        break;
    default:
        LOG_ERROR(Service_ACC, "Invalid game storage ID! storage_id={}",
                  launch_property->base_game_storage_id);
        return ERR_ACCOUNTINFO_BAD_APPLICATION;
    }

    LOG_WARNING(Service_ACC, "ApplicationInfo init required");
    // TODO(ogniK): Actual initalization here

    return RESULT_SUCCESS;
}

void Module::Interface::GetBaasAccountManagerForApplication(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IManagerForApplication>(profile_manager->GetLastOpenedUser());
}

void Module::Interface::IsUserAccountSwitchLocked(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");
    FileSys::NACP nacp;
    const auto res = system.GetAppLoader().ReadControlData(nacp);

    bool is_locked = false;

    if (res != Loader::ResultStatus::Success) {
        FileSys::PatchManager pm{system.CurrentProcess()->GetTitleID()};
        auto nacp_unique = pm.GetControlMetadata().first;

        if (nacp_unique != nullptr) {
            is_locked = nacp_unique->GetUserAccountSwitchLock();
        } else {
            LOG_ERROR(Service_ACC, "nacp_unique is null!");
        }
    } else {
        is_locked = nacp.GetUserAccountSwitchLock();
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(RESULT_SUCCESS);
    rb.Push(is_locked);
}

void Module::Interface::GetProfileEditor(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    Common::UUID user_id = rp.PopRaw<Common::UUID>();

    LOG_DEBUG(Service_ACC, "called, user_id={}", user_id.Format());

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IProfileEditor>(user_id, *profile_manager);
}

void Module::Interface::ListQualifiedUsers(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");

    // All users should be qualified. We don't actually have parental control or anything to do with
    // nintendo online currently. We're just going to assume the user running the game has access to
    // the game regardless of parental control settings.
    ctx.WriteBuffer(profile_manager->GetAllUsers());
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::LoadOpenContext(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_ACC, "(STUBBED) called");

    // This is similar to GetBaasAccountManagerForApplication
    // This command is used concurrently with ListOpenContextStoredUsers
    // TODO: Find the differences between this and GetBaasAccountManagerForApplication
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IManagerForApplication>(profile_manager->GetLastOpenedUser());
}

void Module::Interface::ListOpenContextStoredUsers(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_ACC, "(STUBBED) called");

    // TODO(ogniK): Handle open contexts
    ctx.WriteBuffer(profile_manager->GetOpenUsers());
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::TrySelectUserWithoutInteraction(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_ACC, "called");
    // A u8 is passed into this function which we can safely ignore. It's to determine if we have
    // access to use the network or not by the looks of it
    IPC::ResponseBuilder rb{ctx, 6};
    if (profile_manager->GetUserCount() != 1) {
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw<u128>(Common::INVALID_UUID);
        return;
    }

    const auto user_list = profile_manager->GetAllUsers();
    if (std::all_of(user_list.begin(), user_list.end(),
                    [](const auto& user) { return user.uuid == Common::INVALID_UUID; })) {
        rb.Push(RESULT_UNKNOWN); // TODO(ogniK): Find the correct error code
        rb.PushRaw<u128>(Common::INVALID_UUID);
        return;
    }

    // Select the first user we have
    rb.Push(RESULT_SUCCESS);
    rb.PushRaw<u128>(profile_manager->GetUser(0)->uuid);
}

Module::Interface::Interface(std::shared_ptr<Module> interface_module,
                             std::shared_ptr<ProfileManager> profile_manager, Core::System& system,
                             const char* name)
    : ServiceFramework(name), interface_module(std::move(interface_module)),
      profile_manager(std::move(profile_manager)), system(system) {}

Module::Interface::~Interface() = default;

void InstallInterfaces(Core::System& system) {
    auto interface_module = std::make_shared<Module>();
    auto profile_manager = std::make_shared<ProfileManager>();

    std::make_shared<ACC_AA>(interface_module, profile_manager, system)
        ->InstallAsService(system.ServiceManager());
    std::make_shared<ACC_SU>(interface_module, profile_manager, system)
        ->InstallAsService(system.ServiceManager());
    std::make_shared<ACC_U0>(interface_module, profile_manager, system)
        ->InstallAsService(system.ServiceManager());
    std::make_shared<ACC_U1>(interface_module, profile_manager, system)
        ->InstallAsService(system.ServiceManager());
}

} // namespace Service::Account
