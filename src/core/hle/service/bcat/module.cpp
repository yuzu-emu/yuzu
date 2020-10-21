// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cctype>
#include <mbedtls/md5.h>
#include "backend/boxcat.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/vfs.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/writable_event.h"
#include "core/hle/service/bcat/backend/backend.h"
#include "core/hle/service/bcat/bcat.h"
#include "core/hle/service/bcat/module.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/settings.h"

namespace Service::BCAT {

constexpr ResultCode ERROR_INVALID_ARGUMENT{ErrorModule::BCAT, 1};
constexpr ResultCode ERROR_FAILED_OPEN_ENTITY{ErrorModule::BCAT, 2};
constexpr ResultCode ERROR_ENTITY_ALREADY_OPEN{ErrorModule::BCAT, 6};
constexpr ResultCode ERROR_NO_OPEN_ENTITY{ErrorModule::BCAT, 7};

// The command to clear the delivery cache just calls fs IFileSystem DeleteFile on all of the files
// and if any of them have a non-zero result it just forwards that result. This is the FS error code
// for permission denied, which is the closest approximation of this scenario.
constexpr ResultCode ERROR_FAILED_CLEAR_CACHE{ErrorModule::FS, 6400};

using BCATDigest = std::array<u8, 0x10>;

namespace {

u64 GetCurrentBuildID(const Core::System::CurrentBuildProcessID& id) {
    u64 out{};
    std::memcpy(&out, id.data(), sizeof(u64));
    return out;
}

// The digest is only used to determine if a file is unique compared to others of the same name.
// Since the algorithm isn't ever checked in game, MD5 is safe.
BCATDigest DigestFile(const FileSys::VirtualFile& file) {
    BCATDigest out{};
    const auto bytes = file->ReadAllBytes();
    mbedtls_md5_ret(bytes.data(), bytes.size(), out.data());
    return out;
}

// For a name to be valid it must be non-empty, must have a null terminating character as the final
// char, can only contain numbers, letters, underscores and a hyphen if directory and a period if
// file.
bool VerifyNameValidInternal(Kernel::HLERequestContext& ctx, std::array<char, 0x20> name,
                             char match_char) {
    const auto null_chars = std::count(name.begin(), name.end(), 0);
    const auto bad_chars = std::count_if(name.begin(), name.end(), [match_char](char c) {
        return !std::isalnum(static_cast<u8>(c)) && c != '_' && c != match_char && c != '\0';
    });
    if (null_chars == 0x20 || null_chars == 0 || bad_chars != 0 || name[0x1F] != '\0') {
        LOG_ERROR(Service_BCAT, "Name passed was invalid!");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ERROR_INVALID_ARGUMENT);
        return false;
    }

    return true;
}

bool VerifyNameValidDir(Kernel::HLERequestContext& ctx, DirectoryName name) {
    return VerifyNameValidInternal(ctx, name, '-');
}

bool VerifyNameValidFile(Kernel::HLERequestContext& ctx, FileName name) {
    return VerifyNameValidInternal(ctx, name, '.');
}

} // Anonymous namespace

struct DeliveryCacheDirectoryEntry {
    FileName name;
    u64 size;
    BCATDigest digest;
};

class IDeliveryCacheProgressService final : public ServiceFramework<IDeliveryCacheProgressService> {
public:
    IDeliveryCacheProgressService(std::shared_ptr<Kernel::ReadableEvent> event,
                                  const DeliveryCacheProgressImpl& impl)
        : ServiceFramework{"IDeliveryCacheProgressService"}, event(std::move(event)), impl(impl) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDeliveryCacheProgressService::GetEvent, "GetEvent"},
            {1, &IDeliveryCacheProgressService::GetImpl, "GetImpl"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetEvent(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(event);
    }

    void GetImpl(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        ctx.WriteBuffer(impl);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    std::shared_ptr<Kernel::ReadableEvent> event;
    const DeliveryCacheProgressImpl& impl;
};

class IBcatService final : public ServiceFramework<IBcatService> {
public:
    explicit IBcatService(Core::System& system_, Backend& backend_)
        : ServiceFramework("IBcatService"), system{system_}, backend{backend_},
          progress{{
              ProgressServiceBackend{system_.Kernel(), "Normal"},
              ProgressServiceBackend{system_.Kernel(), "Directory"},
          }} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {10100, &IBcatService::RequestSyncDeliveryCache, "RequestSyncDeliveryCache"},
            {10101, &IBcatService::RequestSyncDeliveryCacheWithDirectoryName, "RequestSyncDeliveryCacheWithDirectoryName"},
            {10200, nullptr, "CancelSyncDeliveryCacheRequest"},
            {20100, nullptr, "RequestSyncDeliveryCacheWithApplicationId"},
            {20101, nullptr, "RequestSyncDeliveryCacheWithApplicationIdAndDirectoryName"},
            {20300, nullptr, "GetDeliveryCacheStorageUpdateNotifier"},
            {20301, nullptr, "RequestSuspendDeliveryTask"},
            {20400, nullptr, "RegisterSystemApplicationDeliveryTask"},
            {20401, nullptr, "UnregisterSystemApplicationDeliveryTask"},
            {20410, nullptr, "SetSystemApplicationDeliveryTaskTimer"},
            {30100, &IBcatService::SetPassphrase, "SetPassphrase"},
            {30101, nullptr, "Unknown"},
            {30102, nullptr, "Unknown2"},
            {30200, nullptr, "RegisterBackgroundDeliveryTask"},
            {30201, nullptr, "UnregisterBackgroundDeliveryTask"},
            {30202, nullptr, "BlockDeliveryTask"},
            {30203, nullptr, "UnblockDeliveryTask"},
            {30210, nullptr, "SetDeliveryTaskTimer"},
            {30300, nullptr, "RegisterSystemApplicationDeliveryTasks"},
            {90100, nullptr, "EnumerateBackgroundDeliveryTask"},
            {90200, nullptr, "GetDeliveryList"},
            {90201, &IBcatService::ClearDeliveryCacheStorage, "ClearDeliveryCacheStorage"},
            {90202, nullptr, "ClearDeliveryTaskSubscriptionStatus"},
            {90300, nullptr, "GetPushNotificationLog"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }

private:
    enum class SyncType {
        Normal,
        Directory,
        Count,
    };

    std::shared_ptr<IDeliveryCacheProgressService> CreateProgressService(SyncType type) {
        auto& backend{progress.at(static_cast<std::size_t>(type))};
        return std::make_shared<IDeliveryCacheProgressService>(backend.GetEvent(),
                                                               backend.GetImpl());
    }

    void RequestSyncDeliveryCache(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        backend.Synchronize({system.CurrentProcess()->GetTitleID(),
                             GetCurrentBuildID(system.GetCurrentProcessBuildID())},
                            progress.at(static_cast<std::size_t>(SyncType::Normal)));

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface(CreateProgressService(SyncType::Normal));
    }

    void RequestSyncDeliveryCacheWithDirectoryName(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto name_raw = rp.PopRaw<DirectoryName>();
        const auto name =
            Common::StringFromFixedZeroTerminatedBuffer(name_raw.data(), name_raw.size());

        LOG_DEBUG(Service_BCAT, "called, name={}", name);

        backend.SynchronizeDirectory({system.CurrentProcess()->GetTitleID(),
                                      GetCurrentBuildID(system.GetCurrentProcessBuildID())},
                                     name,
                                     progress.at(static_cast<std::size_t>(SyncType::Directory)));

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface(CreateProgressService(SyncType::Directory));
    }

    void SetPassphrase(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto title_id = rp.PopRaw<u64>();

        const auto passphrase_raw = ctx.ReadBuffer();

        LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, passphrase={}", title_id,
                  Common::HexToString(passphrase_raw));

        if (title_id == 0) {
            LOG_ERROR(Service_BCAT, "Invalid title ID!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_ARGUMENT);
        }

        if (passphrase_raw.size() > 0x40) {
            LOG_ERROR(Service_BCAT, "Passphrase too large!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_ARGUMENT);
            return;
        }

        Passphrase passphrase{};
        std::memcpy(passphrase.data(), passphrase_raw.data(),
                    std::min(passphrase.size(), passphrase_raw.size()));

        backend.SetPassphrase(title_id, passphrase);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void ClearDeliveryCacheStorage(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto title_id = rp.PopRaw<u64>();

        LOG_DEBUG(Service_BCAT, "called, title_id={:016X}", title_id);

        if (title_id == 0) {
            LOG_ERROR(Service_BCAT, "Invalid title ID!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_ARGUMENT);
            return;
        }

        if (!backend.Clear(title_id)) {
            LOG_ERROR(Service_BCAT, "Could not clear the directory successfully!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_FAILED_CLEAR_CACHE);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    Core::System& system;
    Backend& backend;

    std::array<ProgressServiceBackend, static_cast<std::size_t>(SyncType::Count)> progress;
};

void Module::Interface::CreateBcatService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_BCAT, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IBcatService>(system, *backend);
}

class IDeliveryCacheFileService final : public ServiceFramework<IDeliveryCacheFileService> {
public:
    IDeliveryCacheFileService(FileSys::VirtualDir root_)
        : ServiceFramework{"IDeliveryCacheFileService"}, root(std::move(root_)) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDeliveryCacheFileService::Open, "Open"},
            {1, &IDeliveryCacheFileService::Read, "Read"},
            {2, &IDeliveryCacheFileService::GetSize, "GetSize"},
            {3, &IDeliveryCacheFileService::GetDigest, "GetDigest"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Open(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto dir_name_raw = rp.PopRaw<DirectoryName>();
        const auto file_name_raw = rp.PopRaw<FileName>();

        const auto dir_name =
            Common::StringFromFixedZeroTerminatedBuffer(dir_name_raw.data(), dir_name_raw.size());
        const auto file_name =
            Common::StringFromFixedZeroTerminatedBuffer(file_name_raw.data(), file_name_raw.size());

        LOG_DEBUG(Service_BCAT, "called, dir_name={}, file_name={}", dir_name, file_name);

        if (!VerifyNameValidDir(ctx, dir_name_raw) || !VerifyNameValidFile(ctx, file_name_raw))
            return;

        if (current_file != nullptr) {
            LOG_ERROR(Service_BCAT, "A file has already been opened on this interface!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_ENTITY_ALREADY_OPEN);
            return;
        }

        const auto dir = root->GetSubdirectory(dir_name);

        if (dir == nullptr) {
            LOG_ERROR(Service_BCAT, "The directory of name={} couldn't be opened!", dir_name);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_FAILED_OPEN_ENTITY);
            return;
        }

        current_file = dir->GetFile(file_name);

        if (current_file == nullptr) {
            LOG_ERROR(Service_BCAT, "The file of name={} couldn't be opened!", file_name);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_FAILED_OPEN_ENTITY);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void Read(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto offset{rp.PopRaw<u64>()};

        auto size = ctx.GetWriteBufferSize();

        LOG_DEBUG(Service_BCAT, "called, offset={:016X}, size={:016X}", offset, size);

        if (current_file == nullptr) {
            LOG_ERROR(Service_BCAT, "There is no file currently open!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_NO_OPEN_ENTITY);
        }

        size = std::min<u64>(current_file->GetSize() - offset, size);
        const auto buffer = current_file->ReadBytes(size, offset);
        ctx.WriteBuffer(buffer);

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u64>(buffer.size());
    }

    void GetSize(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        if (current_file == nullptr) {
            LOG_ERROR(Service_BCAT, "There is no file currently open!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_NO_OPEN_ENTITY);
        }

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u64>(current_file->GetSize());
    }

    void GetDigest(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        if (current_file == nullptr) {
            LOG_ERROR(Service_BCAT, "There is no file currently open!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_NO_OPEN_ENTITY);
        }

        IPC::ResponseBuilder rb{ctx, 6};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(DigestFile(current_file));
    }

    FileSys::VirtualDir root;
    FileSys::VirtualFile current_file;
};

class IDeliveryCacheDirectoryService final
    : public ServiceFramework<IDeliveryCacheDirectoryService> {
public:
    IDeliveryCacheDirectoryService(FileSys::VirtualDir root_)
        : ServiceFramework{"IDeliveryCacheDirectoryService"}, root(std::move(root_)) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDeliveryCacheDirectoryService::Open, "Open"},
            {1, &IDeliveryCacheDirectoryService::Read, "Read"},
            {2, &IDeliveryCacheDirectoryService::GetCount, "GetCount"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Open(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto name_raw = rp.PopRaw<DirectoryName>();
        const auto name =
            Common::StringFromFixedZeroTerminatedBuffer(name_raw.data(), name_raw.size());

        LOG_DEBUG(Service_BCAT, "called, name={}", name);

        if (!VerifyNameValidDir(ctx, name_raw))
            return;

        if (current_dir != nullptr) {
            LOG_ERROR(Service_BCAT, "A file has already been opened on this interface!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_ENTITY_ALREADY_OPEN);
            return;
        }

        current_dir = root->GetSubdirectory(name);

        if (current_dir == nullptr) {
            LOG_ERROR(Service_BCAT, "Failed to open the directory name={}!", name);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_FAILED_OPEN_ENTITY);
            return;
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void Read(Kernel::HLERequestContext& ctx) {
        auto write_size = ctx.GetWriteBufferSize() / sizeof(DeliveryCacheDirectoryEntry);

        LOG_DEBUG(Service_BCAT, "called, write_size={:016X}", write_size);

        if (current_dir == nullptr) {
            LOG_ERROR(Service_BCAT, "There is no open directory!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_NO_OPEN_ENTITY);
            return;
        }

        const auto files = current_dir->GetFiles();
        write_size = std::min<u64>(write_size, files.size());
        std::vector<DeliveryCacheDirectoryEntry> entries(write_size);
        std::transform(
            files.begin(), files.begin() + write_size, entries.begin(), [](const auto& file) {
                FileName name{};
                std::memcpy(name.data(), file->GetName().data(),
                            std::min(file->GetName().size(), name.size()));
                return DeliveryCacheDirectoryEntry{name, file->GetSize(), DigestFile(file)};
            });

        ctx.WriteBuffer(entries);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(static_cast<u32>(write_size * sizeof(DeliveryCacheDirectoryEntry)));
    }

    void GetCount(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        if (current_dir == nullptr) {
            LOG_ERROR(Service_BCAT, "There is no open directory!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_NO_OPEN_ENTITY);
            return;
        }

        const auto files = current_dir->GetFiles();

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(static_cast<u32>(files.size()));
    }

    FileSys::VirtualDir root;
    FileSys::VirtualDir current_dir;
};

class IDeliveryCacheStorageService final : public ServiceFramework<IDeliveryCacheStorageService> {
public:
    IDeliveryCacheStorageService(FileSys::VirtualDir root_)
        : ServiceFramework{"IDeliveryCacheStorageService"}, root(std::move(root_)) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IDeliveryCacheStorageService::CreateFileService, "CreateFileService"},
            {1, &IDeliveryCacheStorageService::CreateDirectoryService, "CreateDirectoryService"},
            {10, &IDeliveryCacheStorageService::EnumerateDeliveryCacheDirectory, "EnumerateDeliveryCacheDirectory"},
        };
        // clang-format on

        RegisterHandlers(functions);

        for (const auto& subdir : root->GetSubdirectories()) {
            DirectoryName name{};
            std::memcpy(name.data(), subdir->GetName().data(),
                        std::min(sizeof(DirectoryName) - 1, subdir->GetName().size()));
            entries.push_back(name);
        }
    }

private:
    void CreateFileService(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IDeliveryCacheFileService>(root);
    }

    void CreateDirectoryService(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BCAT, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IDeliveryCacheDirectoryService>(root);
    }

    void EnumerateDeliveryCacheDirectory(Kernel::HLERequestContext& ctx) {
        auto size = ctx.GetWriteBufferSize() / sizeof(DirectoryName);

        LOG_DEBUG(Service_BCAT, "called, size={:016X}", size);

        size = std::min<u64>(size, entries.size() - next_read_index);
        ctx.WriteBuffer(entries.data() + next_read_index, size * sizeof(DirectoryName));
        next_read_index += size;

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(static_cast<u32>(size));
    }

    FileSys::VirtualDir root;
    std::vector<DirectoryName> entries;
    u64 next_read_index = 0;
};

void Module::Interface::CreateDeliveryCacheStorageService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_BCAT, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IDeliveryCacheStorageService>(
        fsc.GetBCATDirectory(system.CurrentProcess()->GetTitleID()));
}

void Module::Interface::CreateDeliveryCacheStorageServiceWithApplicationId(
    Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto title_id = rp.PopRaw<u64>();

    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}", title_id);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<IDeliveryCacheStorageService>(fsc.GetBCATDirectory(title_id));
}

std::unique_ptr<Backend> CreateBackendFromSettings([[maybe_unused]] Core::System& system,
                                                   DirectoryGetter getter) {
#ifdef YUZU_ENABLE_BOXCAT
    if (Settings::values.bcat_backend == "boxcat") {
        return std::make_unique<Boxcat>(system.GetAppletManager(), std::move(getter));
    }
#endif

    return std::make_unique<NullBackend>(std::move(getter));
}

Module::Interface::Interface(Core::System& system_, std::shared_ptr<Module> interface_module,
                             FileSystem::FileSystemController& fsc_, const char* name)
    : ServiceFramework(name), fsc{fsc_}, interface_module{std::move(interface_module)},
      backend{CreateBackendFromSettings(system_,
                                        [&fsc_](u64 tid) { return fsc_.GetBCATDirectory(tid); })},
      system{system_} {}

Module::Interface::~Interface() = default;

void InstallInterfaces(Core::System& system) {
    auto interface_module = std::make_shared<Module>();
    std::make_shared<BCAT>(system, interface_module, system.GetFileSystemController(), "bcat:a")
        ->InstallAsService(system.ServiceManager());
    std::make_shared<BCAT>(system, interface_module, system.GetFileSystemController(), "bcat:m")
        ->InstallAsService(system.ServiceManager());
    std::make_shared<BCAT>(system, interface_module, system.GetFileSystemController(), "bcat:u")
        ->InstallAsService(system.ServiceManager());
    std::make_shared<BCAT>(system, interface_module, system.GetFileSystemController(), "bcat:s")
        ->InstallAsService(system.ServiceManager());
}

} // namespace Service::BCAT
