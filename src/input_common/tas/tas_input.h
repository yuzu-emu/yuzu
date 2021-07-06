// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_types.h"
#include "common/settings_input.h"
#include "core/frontend/input.h"
#include "input_common/main.h"

/*
To play back TAS scripts on Yuzu, select the folder with scripts in the configuration menu below
Emulation -> Configure TAS. The file itself has normal text format and has to be called
script0-1.txt for controller 1, script0-2.txt for controller 2 and so forth (with max. 8 players).

A script file has the same format as TAS-nx uses, so final files will look like this:

1 KEY_B 0;0 0;0
6 KEY_ZL 0;0 0;0
41 KEY_ZL;KEY_Y 0;0 0;0
43 KEY_X;KEY_A 32767;0 0;0
44 KEY_A 32767;0 0;0
45 KEY_A 32767;0 0;0
46 KEY_A 32767;0 0;0
47 KEY_A 32767;0 0;0

After placing the file at the correct location, it can be read into Yuzu with the (default) hotkey
CTRL+F6 (refresh). In the bottom left corner, it will display the amount of frames the script file
has. Playback can be started or stopped using CTRL+F5.

However, for playback to actually work, the correct input device has to be selected: In the Controls
menu, select TAS from the device list for the controller that the script should be played on.

Recording a new script file is really simple: Just make sure that the proper device (not TAS) is
connected on P1, and press CTRL+F7 to start recording. When done, just press the same keystroke
again (CTRL+F7). The new script will be saved at the location previously selected, as the filename
record.txt.

For debugging purposes, the common controller debugger can be used (View -> Debugging -> Controller
P1).
*/

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

    // Changes the input status that will be stored in each frame
    void RecordInput(u32 buttons, const std::array<std::pair<float, float>, 2>& axes);

    // Main loop that records or executes input
    void UpdateThread();

    //  Sets the flag to start or stop the TAS command excecution and swaps controllers profiles
    void StartStop();

    // Sets the flag to reload the file and start from the begining in the next update
    void Reset();

    /**
     * Sets the flag to enable or disable recording of inputs
     * @return Returns true if the current recording status is enabled
     */
    bool Record();

    // Saves contents of record_commands on a file if overwrite is enabled player 1 will be
    // overwritten with the recorded commands
    void SaveRecording(bool overwrite_file);

    /**
     * Returns the current status values of TAS playback/recording
     * @return Tuple of
     * TasState indicating the current state out of Running, Recording or Stopped ;
     * Current playback progress or amount of frames (so far) for Recording ;
     * Total length of script file currently loaded or amount of frames (so far) for Recording
     */
    std::tuple<TasState, size_t, size_t> GetStatus() const;

    // Retuns an array of the default button mappings
    InputCommon::ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage& params) const;

    // Retuns an array of the default analog mappings
    InputCommon::AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage& params) const;
    [[nodiscard]] const TasData& GetTasState(std::size_t pad) const;

private:
    struct TASCommand {
        u32 buttons{};
        TasAnalog l_axis{};
        TasAnalog r_axis{};
    };

    // Loads TAS files from all players
    void LoadTasFiles();

    // Loads TAS file from the specified player
    void LoadTasFile(size_t player_index);

    // Writes a TAS file from the recorded commands
    void WriteTasFile(std::u8string file_name);

    /**
     * Parses a string containing the axis values with the following format "x;y"
     * X and Y have a range from -32767 to 32767
     * @return Returns a TAS analog object with axis values with range from -1.0 to 1.0
     */
    TasAnalog ReadCommandAxis(const std::string& line) const;

    /**
     * Parses a string containing the button values with the following format "a;b;c;d..."
     * Each button is represented by it's text format specified in text_to_tas_button array
     * @return Returns a u32 with each bit representing the status of a button
     */
    u32 ReadCommandButtons(const std::string& line) const;

    /**
     * Converts an u32 containing the button status into the text equivalent
     * @return Returns a string with the name of the buttons to be written to the file
     */
    std::string WriteCommandButtons(u32 data) const;

    /**
     * Converts an TAS analog object containing the axis status into the text equivalent
     * @return Returns a string with the value of the axis to be written to the file
     */
    std::string WriteCommandAxis(TasAnalog data) const;

    // Inverts the Y axis polarity
    std::pair<float, float> FlipAxisY(std::pair<float, float> old);

    /**
     * Converts an u32 containing the button status into the text equivalent
     * @return Returns a string with the name of the buttons to be printed on console
     */
    std::string DebugButtons(u32 buttons) const;

    /**
     * Converts an TAS analog object containing the axis status into the text equivalent
     * @return Returns a string with the value of the axis to be printed on console
     */
    std::string DebugJoystick(float x, float y) const;

    /**
     * Converts the given TAS status into the text equivalent
     * @return Returns a string with the value of the TAS status to be printed on console
     */
    std::string DebugInput(const TasData& data) const;

    /**
     * Converts the given TAS status of multiple players into the text equivalent
     * @return Returns a string with the value of the status of all TAS players to be printed on
     * console
     */
    std::string DebugInputs(const std::array<TasData, PLAYER_NUMBER>& arr) const;

    /**
     * Converts an u32 containing the button status into the text equivalent
     * @return Returns a string with the name of the buttons
     */
    std::string ButtonsToString(u32 button) const;

    // Stores current controller configuration and sets a TAS controller for every active controller
    // to the current config
    void SwapToTasController();

    // Sets the stored controller configuration to the current config
    void SwapToStoredController();

    size_t script_length{0};
    std::array<TasData, PLAYER_NUMBER> tas_data;
    bool is_recording{false};
    bool is_running{false};
    bool needs_reset{false};
    std::array<std::vector<TASCommand>, PLAYER_NUMBER> commands{};
    std::vector<TASCommand> record_commands{};
    size_t current_command{0};
    TASCommand last_input{}; // only used for recording

    // Old settings for swapping controllers
    std::array<Settings::PlayerInput, 10> player_mappings;
};
} // namespace TasInput
