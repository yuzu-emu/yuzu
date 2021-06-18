// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <mutex>
#include <thread>

#include "common/common_types.h"
#include "core/frontend/input.h"
#include "input_common/main.h"

namespace TasInput {

constexpr int PLAYER_NUMBER = 8;

using TasAnalog = std::pair<float, float>;

enum class TasState {
    RUNNING,
    RECORDING,
    STOPPED,
};

enum class TasButton : u32 {
    BUTTON_A = 0x000001,
    BUTTON_B = 0x000002,
    BUTTON_X = 0x000004,
    BUTTON_Y = 0x000008,
    STICK_L = 0x000010,
    STICK_R = 0x000020,
    TRIGGER_L = 0x000040,
    TRIGGER_R = 0x000080,
    TRIGGER_ZL = 0x000100,
    TRIGGER_ZR = 0x000200,
    BUTTON_PLUS = 0x000400,
    BUTTON_MINUS = 0x000800,
    BUTTON_LEFT = 0x001000,
    BUTTON_UP = 0x002000,
    BUTTON_RIGHT = 0x004000,
    BUTTON_DOWN = 0x008000,
    BUTTON_SL = 0x010000,
    BUTTON_SR = 0x020000,
    BUTTON_HOME = 0x040000,
    BUTTON_CAPTURE = 0x080000,
};

enum class TasAxes : u8 {
    StickX,
    StickY,
    SubstickX,
    SubstickY,
    Undefined,
};

struct TasData {
    u32 buttons{};
    std::array<float, 4> axis{};
};

class Tas {
public:
    Tas();
    ~Tas();

    static std::string ButtonsToString(u32 button) {
        std::string returns;
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_A)) != 0)
            returns += ", A";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_B)) != 0)
            returns += ", B";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_X)) != 0)
            returns += ", X";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_Y)) != 0)
            returns += ", Y";
        if ((button & static_cast<u32>(TasInput::TasButton::STICK_L)) != 0)
            returns += ", STICK_L";
        if ((button & static_cast<u32>(TasInput::TasButton::STICK_R)) != 0)
            returns += ", STICK_R";
        if ((button & static_cast<u32>(TasInput::TasButton::TRIGGER_L)) != 0)
            returns += ", TRIGGER_L";
        if ((button & static_cast<u32>(TasInput::TasButton::TRIGGER_R)) != 0)
            returns += ", TRIGGER_R";
        if ((button & static_cast<u32>(TasInput::TasButton::TRIGGER_ZL)) != 0)
            returns += ", TRIGGER_ZL";
        if ((button & static_cast<u32>(TasInput::TasButton::TRIGGER_ZR)) != 0)
            returns += ", TRIGGER_ZR";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_PLUS)) != 0)
            returns += ", PLUS";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_MINUS)) != 0)
            returns += ", MINUS";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_LEFT)) != 0)
            returns += ", LEFT";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_UP)) != 0)
            returns += ", UP";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_RIGHT)) != 0)
            returns += ", RIGHT";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_DOWN)) != 0)
            returns += ", DOWN";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_SL)) != 0)
            returns += ", SL";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_SR)) != 0)
            returns += ", SR";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_HOME)) != 0)
            returns += ", HOME";
        if ((button & static_cast<u32>(TasInput::TasButton::BUTTON_CAPTURE)) != 0)
            returns += ", CAPTURE";
        return returns.empty() ? "" : returns.substr(2);
    }

    void RefreshTasFile();
    void LoadTasFiles();
    void RecordInput(u32 buttons, const std::array<std::pair<float, float>, 2>& axes);
    void UpdateThread();
    std::tuple<TasState, size_t, size_t> GetStatus();

    InputCommon::ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage& params) const;
    InputCommon::AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage& params) const;
    [[nodiscard]] const TasData& GetTasState(std::size_t pad) const;

private:
    struct TASCommand {
        u32 buttons{};
        TasAnalog l_axis{};
        TasAnalog r_axis{};
    };
    void LoadTasFile(size_t player_index);
    void WriteTasFile();
    TasAnalog ReadCommandAxis(const std::string& line) const;
    u32 ReadCommandButtons(const std::string& line) const;
    std::string WriteCommandButtons(u32 data) const;
    std::string WriteCommandAxis(TasAnalog data) const;

    size_t script_length{0};
    std::array<TasData, PLAYER_NUMBER> tas_data;
    bool update_thread_running{true};
    bool refresh_tas_fle{false};
    std::array<std::vector<TASCommand>, PLAYER_NUMBER> commands{};
    std::vector<TASCommand> record_commands{};
    std::size_t current_command{0};
    TASCommand last_input{}; // only used for recording
};
} // namespace TasInput
