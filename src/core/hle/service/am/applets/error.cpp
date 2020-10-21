// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/error.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applets/error.h"
#include "core/reporter.h"

namespace Service::AM::Applets {

#pragma pack(push, 4)
struct ShowError {
    u8 mode;
    bool jump;
    INSERT_UNION_PADDING_BYTES(4);
    bool use_64bit_error_code;
    INSERT_UNION_PADDING_BYTES(1);
    u64 error_code_64;
    u32 error_code_32;
};
static_assert(sizeof(ShowError) == 0x14, "ShowError has incorrect size.");
#pragma pack(pop)

struct ShowErrorRecord {
    u8 mode;
    bool jump;
    INSERT_UNION_PADDING_BYTES(6);
    u64 error_code_64;
    u64 posix_time;
};
static_assert(sizeof(ShowErrorRecord) == 0x18, "ShowErrorRecord has incorrect size.");

struct SystemErrorArg {
    u8 mode;
    bool jump;
    INSERT_UNION_PADDING_BYTES(6);
    u64 error_code_64;
    std::array<char, 8> language_code;
    std::array<char, 0x800> main_text;
    std::array<char, 0x800> detail_text;
};
static_assert(sizeof(SystemErrorArg) == 0x1018, "SystemErrorArg has incorrect size.");

struct ApplicationErrorArg {
    u8 mode;
    bool jump;
    INSERT_UNION_PADDING_BYTES(6);
    u32 error_code;
    std::array<char, 8> language_code;
    std::array<char, 0x800> main_text;
    std::array<char, 0x800> detail_text;
};
static_assert(sizeof(ApplicationErrorArg) == 0x1014, "ApplicationErrorArg has incorrect size.");

union Error::ErrorArguments {
    ShowError error;
    ShowErrorRecord error_record;
    SystemErrorArg system_error;
    ApplicationErrorArg application_error;
    std::array<u8, 0x1018> raw{};
};

namespace {
template <typename T>
void CopyArgumentData(const std::vector<u8>& data, T& variable) {
    ASSERT(data.size() >= sizeof(T));
    std::memcpy(&variable, data.data(), sizeof(T));
}

ResultCode Decode64BitError(u64 error) {
    const auto description = (error >> 32) & 0x1FFF;
    auto error_module = error & 0x3FF;
    if (error_module >= 2000)
        error_module -= 2000;
    error_module &= 0x1FF;
    return {static_cast<ErrorModule>(error_module), static_cast<u32>(description)};
}

} // Anonymous namespace

Error::Error(Core::System& system_, const Core::Frontend::ErrorApplet& frontend_)
    : Applet{system_.Kernel()}, frontend(frontend_), system{system_} {}

Error::~Error() = default;

void Error::Initialize() {
    Applet::Initialize();
    args = std::make_unique<ErrorArguments>();
    complete = false;

    const auto storage = broker.PopNormalDataToApplet();
    ASSERT(storage != nullptr);
    const auto data = storage->GetData();

    ASSERT(!data.empty());
    std::memcpy(&mode, data.data(), sizeof(ErrorAppletMode));

    switch (mode) {
    case ErrorAppletMode::ShowError:
        CopyArgumentData(data, args->error);
        if (args->error.use_64bit_error_code) {
            error_code = Decode64BitError(args->error.error_code_64);
        } else {
            error_code = ResultCode(args->error.error_code_32);
        }
        break;
    case ErrorAppletMode::ShowSystemError:
        CopyArgumentData(data, args->system_error);
        error_code = ResultCode(Decode64BitError(args->system_error.error_code_64));
        break;
    case ErrorAppletMode::ShowApplicationError:
        CopyArgumentData(data, args->application_error);
        error_code = ResultCode(args->application_error.error_code);
        break;
    case ErrorAppletMode::ShowErrorRecord:
        CopyArgumentData(data, args->error_record);
        error_code = Decode64BitError(args->error_record.error_code_64);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented LibAppletError mode={:02X}!", static_cast<u8>(mode));
    }
}

bool Error::TransactionComplete() const {
    return complete;
}

ResultCode Error::GetStatus() const {
    return RESULT_SUCCESS;
}

void Error::ExecuteInteractive() {
    UNREACHABLE_MSG("Unexpected interactive applet data!");
}

void Error::Execute() {
    if (complete) {
        return;
    }

    const auto callback = [this] { DisplayCompleted(); };
    const auto title_id = system.CurrentProcess()->GetTitleID();
    const auto& reporter{system.GetReporter()};

    switch (mode) {
    case ErrorAppletMode::ShowError:
        reporter.SaveErrorReport(title_id, error_code);
        frontend.ShowError(error_code, callback);
        break;
    case ErrorAppletMode::ShowSystemError:
    case ErrorAppletMode::ShowApplicationError: {
        const auto system = mode == ErrorAppletMode::ShowSystemError;
        const auto& main_text =
            system ? args->system_error.main_text : args->application_error.main_text;
        const auto& detail_text =
            system ? args->system_error.detail_text : args->application_error.detail_text;

        const auto main_text_string =
            Common::StringFromFixedZeroTerminatedBuffer(main_text.data(), main_text.size());
        const auto detail_text_string =
            Common::StringFromFixedZeroTerminatedBuffer(detail_text.data(), detail_text.size());

        reporter.SaveErrorReport(title_id, error_code, main_text_string, detail_text_string);
        frontend.ShowCustomErrorText(error_code, main_text_string, detail_text_string, callback);
        break;
    }
    case ErrorAppletMode::ShowErrorRecord:
        reporter.SaveErrorReport(title_id, error_code,
                                 fmt::format("{:016X}", args->error_record.posix_time));
        frontend.ShowErrorWithTimestamp(
            error_code, std::chrono::seconds{args->error_record.posix_time}, callback);
        break;
    default:
        UNIMPLEMENTED_MSG("Unimplemented LibAppletError mode={:02X}!", static_cast<u8>(mode));
        DisplayCompleted();
    }
}

void Error::DisplayCompleted() {
    complete = true;
    broker.PushNormalDataFromApplet(std::make_shared<IStorage>(std::vector<u8>{}));
    broker.SignalStateChanged();
}

} // namespace Service::AM::Applets
