// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/settings.h"
#include "common/swap.h"
#include "core/frontend/input.h"
#include "core/hle/result.h"

namespace Service::HID {
enum class JoyPollingMode : u32 {
    SixAxisSensorDisable,
    SixAxisSensorEnable,
    ButtonOnly,
};

struct DataAccessorHeader {
    ResultCode result{ResultUnknown};
    INSERT_PADDING_WORDS(0x1);
    std::array<u8, 0x18> unused{};
    u64 latest_entry{};
    u64 total_entries{};
};
static_assert(sizeof(DataAccessorHeader) == 0x30, "DataAccessorHeader is an invalid size");

struct JoyDisableSixAxisPollingData {
    std::array<u8, 0x26> data;
    u8 out_size;
    INSERT_PADDING_BYTES(0x1);
    u64 sampling_number;
};
static_assert(sizeof(JoyDisableSixAxisPollingData) == 0x30,
              "JoyDisableSixAxisPollingData is an invalid size");

struct JoyEnableSixAxisPollingData {
    std::array<u8, 0x8> data;
    u8 out_size;
    INSERT_PADDING_BYTES(0x7);
    u64 sampling_number;
};
static_assert(sizeof(JoyEnableSixAxisPollingData) == 0x18,
              "JoyEnableSixAxisPollingData is an invalid size");

struct JoyButtonOnlyPollingData {
    std::array<u8, 0x2c> data;
    u8 out_size;
    INSERT_PADDING_BYTES(0x3);
    u64 sampling_number;
};
static_assert(sizeof(JoyButtonOnlyPollingData) == 0x38,
              "JoyButtonOnlyPollingData is an invalid size");

struct JoyDisableSixAxisPollingEntry {
    u64 sampling_number;
    JoyDisableSixAxisPollingData polling_data;
};
static_assert(sizeof(JoyDisableSixAxisPollingEntry) == 0x38,
              "JoyDisableSixAxisPollingEntry is an invalid size");

struct JoyEnableSixAxisPollingEntry {
    u64 sampling_number;
    JoyEnableSixAxisPollingData polling_data;
};
static_assert(sizeof(JoyEnableSixAxisPollingEntry) == 0x20,
              "JoyEnableSixAxisPollingEntry is an invalid size");

struct JoyButtonOnlyPollingEntry {
    u64 sampling_number;
    JoyButtonOnlyPollingData polling_data;
};
static_assert(sizeof(JoyButtonOnlyPollingEntry) == 0x40,
              "JoyButtonOnlyPollingEntry is an invalid size");

struct JoyDisableSixAxisDataAccessor {
    DataAccessorHeader header{};
    std::array<JoyDisableSixAxisPollingEntry, 0xb> entries{};
};
static_assert(sizeof(JoyDisableSixAxisDataAccessor) == 0x298,
              "JoyDisableSixAxisDataAccessor is an invalid size");

struct JoyEnableSixAxisDataAccessor {
    DataAccessorHeader header{};
    std::array<JoyEnableSixAxisPollingEntry, 0xb> entries{};
};
static_assert(sizeof(JoyEnableSixAxisDataAccessor) == 0x190,
              "JoyEnableSixAxisDataAccessor is an invalid size");

struct ButtonOnlyPollingDataAccessor {
    DataAccessorHeader header;
    std::array<JoyButtonOnlyPollingEntry, 0xb> entries;
};
static_assert(sizeof(ButtonOnlyPollingDataAccessor) == 0x2F0,
              "ButtonOnlyPollingDataAccessor is an invalid size");

class RingController {
public:
    enum class DataValid : u32 {
        Valid,
        BadCRC,
        Cal,
    };

    struct FirmwareVersion {
        u8 sub;
        u8 main;
    };

    struct FactoryCalibration {
        s32_le os_max;
        s32_le hk_max;
        s32_le zero_min;
        s32_le zero_max;
    };

    struct UserCalibration {
        s16_le os_max;
        u16 os_max_crc;
        s16_le hk_max;
        u16 hk_crc;
        s16_le zero;
        u16 zero_crc;
    };

    explicit RingController();
    ~RingController();

    // Updates ringcon transfer memory
    void Update();

    // Returns the device ID of the joycon
    u8 GetDeviceId() const;

    // Assigns a command from data
    bool SetCommand(const std::vector<u8>& data);

    // Enables/disables the device
    void Enable(bool enable);

    // returns true if ringcon is enabled
    bool IsEnabled() const;

    // returns true if polling mode enabled
    bool IsPollingMode() const;

    // returns polling mode
    JoyPollingMode GetPollingMode() const;

    // Sets and enables JoyPollingMode
    void SetPollingMode(JoyPollingMode mode);

    // Disables JoyPollingMode
    void DisablePollingMode();

    // Returns a reply from a command
    std::vector<u8> GetReply();

    // Loads controller input from settings
    void OnLoadInputDevices();

    // Called on EnableJoyPollingReceiveMode
    void SetTransferMemoryPointer(u8* t_mem);

private:
    enum class RingConCommands : u32 {
        GetFirmwareVersion = 0x00020000,
        ReadId = 0x00020100,
        JoyPolling = 0x00020101,
        Unknown1 = 0x00020104,
        c20105 = 0x00020105,
        Unknown2 = 0x00020204,
        Unknown3 = 0x00020304,
        Unknown4 = 0x00020404,
        ReadUnkCal = 0x00020504,
        ReadFactoryCal = 0x00020A04,
        Unknown5 = 0x00021104,
        Unknown6 = 0x00021204,
        Unknown7 = 0x00021304,
        ReadUserCal = 0x00021A04,
        Unknown8 = 0x00023104,
        ReadTotalPushCount = 0x00023204,
        Unknown9 = 0x04013104,
        Unknown10 = 0x04011104,
        Unknown11 = 0x04011204,
        Unknown12 = 0x04011304,
        Error = 0xFFFFFFFF,
    };

    struct FirmwareVersionReply {
        DataValid status;
        FirmwareVersion firmware;
        INSERT_PADDING_BYTES(0x2);
    };
    static_assert(sizeof(FirmwareVersionReply) == 0x8, "FirmwareVersionReply is an invalid size");

    struct Cmd020105Reply {
        DataValid status;
        u8 data;
        INSERT_PADDING_BYTES(0x3);
    };
    static_assert(sizeof(Cmd020105Reply) == 0x8, "Cmd020105Reply is an invalid size");

    struct GetThreeByteReply {
        DataValid status;
        std::array<u8, 3> data;
        u8 crc;
    };
    static_assert(sizeof(GetThreeByteReply) == 0x8, "GetThreeByteReply is an invalid size");

    struct ReadUnkCalReply {
        DataValid status;
        u16_le data;
        INSERT_PADDING_BYTES(0x2);
    };
    static_assert(sizeof(ReadUnkCalReply) == 0x8, "ReadUnkCalReply is an invalid size");

    struct ReadFactoryCalReply {
        DataValid status;
        FactoryCalibration calibration;
    };
    static_assert(sizeof(ReadFactoryCalReply) == 0x14, "ReadFactoryCalReply is an invalid size");

    struct ReadUserCalReply {
        DataValid status;
        UserCalibration calibration;
        INSERT_PADDING_BYTES(0x4);
    };
    static_assert(sizeof(ReadUserCalReply) == 0x14, "ReadUserCalReply is an invalid size");

    struct ReadIdReply {
        DataValid status;
        u16_le id_l_x0;
        u16_le id_l_x0_2;
        u16_le id_l_x4;
        u16_le id_h_x0;
        u16_le id_h_x0_2;
        u16_le id_h_x4;
    };
    static_assert(sizeof(ReadIdReply) == 0x10, "ReadIdReply is an invalid size");

    struct ErrorReply {
        DataValid status;
        INSERT_PADDING_BYTES(0x3);
    };
    static_assert(sizeof(ErrorReply) == 0x8, "ErrorReply is an invalid size");

    struct RingConData {
        DataValid status;
        s16_le data;
        INSERT_PADDING_BYTES(0x2);
    };
    static_assert(sizeof(RingConData) == 0x8, "RingConData is an invalid size");

    // Returns RingConData struct with pressure sensor values
    RingConData GetSensorValue() const;

    // Returns 8 byte reply with firmware version
    std::vector<u8> GetFirmwareVersionReply() const;

    // Returns 16 byte reply with ID values
    std::vector<u8> GetReadIdReply() const;

    // (STUBBED) Returns 8 byte reply
    std::vector<u8> GetC020105Reply() const;

    // (STUBBED) Returns 8 byte empty reply
    std::vector<u8> GetReadUnkCalReply() const;

    // Returns 20 byte reply with factory calibration values
    std::vector<u8> GetReadFactoryCalReply() const;

    // Returns 20 byte reply with user calibration values
    std::vector<u8> GetReadUserCalReply() const;

    // (STUBBED) Returns 8 byte reply
    std::vector<u8> GetReadTotalPushCountReply() const;

    // Returns 8 byte error reply
    std::vector<u8> GetErrorReply() const;

    // Returns 8 bit redundancy check from provided data
    u8 GetCrcValue(const std::vector<u8>& data) const;

    // Converts structs to an u8 vector equivalent
    template <typename T>
    std::vector<u8> GetDataVector(const T& reply) const;

    bool ringcon_enabled{};
    bool polling_mode_enabled{};
    JoyPollingMode polling_mode = {};
    JoyEnableSixAxisDataAccessor sixaxis_data{};
    RingConCommands command{RingConCommands::Error};

    u8* transfer_memory{nullptr};
    bool is_transfer_memory_set{};
    std::unique_ptr<Input::AnalogDevice> stick;
};
} // namespace Service::HID
