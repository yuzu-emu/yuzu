// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_types.h"
#include "core/frontend/input.h"
#include "input_common/main.h"

namespace TasInput {

constexpr size_t PLAYER_NUMBER = 8;

using TasAnalog = std::pair<float, float>;

enum class TasState {
    Running,
    Recording,
    Stopped,
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

    void RecordInput(u32 buttons, const std::array<std::pair<float, float>, 2>& axes);
    void UpdateThread();

    void StartStop();
    void Reset();
    void Record();

    /**
     * Returns the current status values of TAS playback/recording
     * @return Tuple of
     * TasState indicating the current state out of Running, Recording or Stopped ;
     * Current playback progress or amount of frames (so far) for Recording ;
     * Total length of script file currently loaded or amount of frames (so far) for Recording
     */
    std::tuple<TasState, size_t, size_t> GetStatus() const;
    InputCommon::ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage& params) const;
    InputCommon::AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage& params) const;
    [[nodiscard]] const TasData& GetTasState(std::size_t pad) const;

private:
    struct TASCommand {
        u32 buttons{};
        TasAnalog l_axis{};
        TasAnalog r_axis{};
    };
    void LoadTasFiles();
    void LoadTasFile(size_t player_index);
    void WriteTasFile();
    TasAnalog ReadCommandAxis(const std::string& line) const;
    u32 ReadCommandButtons(const std::string& line) const;
    std::string WriteCommandButtons(u32 data) const;
    std::string WriteCommandAxis(TasAnalog data) const;

    std::pair<float, float> FlipAxisY(std::pair<float, float> old);

    std::string DebugButtons(u32 buttons) const;
    std::string DebugJoystick(float x, float y) const;
    std::string DebugInput(const TasData& data) const;
    std::string DebugInputs(const std::array<TasData, PLAYER_NUMBER>& arr) const;
    std::string ButtonsToString(u32 button) const;

    size_t script_length{0};
    std::array<TasData, PLAYER_NUMBER> tas_data;
    bool refresh_tas_fle{false};
    bool is_recording{false};
    bool is_running{false};
    bool needs_reset{false};
    std::array<std::vector<TASCommand>, PLAYER_NUMBER> commands{};
    std::vector<TASCommand> record_commands{};
    size_t current_command{0};
    TASCommand last_input{}; // only used for recording
};
} // namespace TasInput
