// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <regex>

#include "common/fs/file.h"
#include "common/fs/fs_types.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "input_common/tas/tas_input.h"

namespace TasInput {

// Supported keywords and buttons from a TAS file
constexpr std::array<std::pair<std::string_view, TasButton>, 20> text_to_tas_button = {
    std::pair{"KEY_A", TasButton::BUTTON_A},
    {"KEY_B", TasButton::BUTTON_B},
    {"KEY_X", TasButton::BUTTON_X},
    {"KEY_Y", TasButton::BUTTON_Y},
    {"KEY_LSTICK", TasButton::STICK_L},
    {"KEY_RSTICK", TasButton::STICK_R},
    {"KEY_L", TasButton::TRIGGER_L},
    {"KEY_R", TasButton::TRIGGER_R},
    {"KEY_PLUS", TasButton::BUTTON_PLUS},
    {"KEY_MINUS", TasButton::BUTTON_MINUS},
    {"KEY_DLEFT", TasButton::BUTTON_LEFT},
    {"KEY_DUP", TasButton::BUTTON_UP},
    {"KEY_DRIGHT", TasButton::BUTTON_RIGHT},
    {"KEY_DDOWN", TasButton::BUTTON_DOWN},
    {"KEY_SL", TasButton::BUTTON_SL},
    {"KEY_SR", TasButton::BUTTON_SR},
    {"KEY_CAPTURE", TasButton::BUTTON_CAPTURE},
    {"KEY_HOME", TasButton::BUTTON_HOME},
    {"KEY_ZL", TasButton::TRIGGER_ZL},
    {"KEY_ZR", TasButton::TRIGGER_ZR},
};

Tas::Tas() {
    if (!Settings::values.tas_enable) {
        return;
    }
    LoadTasFiles();
}

Tas::~Tas() = default;

void Tas::LoadTasFiles() {
    script_length = 0;
    for (size_t i = 0; i < commands.size(); i++) {
        LoadTasFile(i);
        if (commands[i].size() > script_length) {
            script_length = commands[i].size();
        }
    }
}

void Tas::LoadTasFile(size_t player_index) {
    if (!commands[player_index].empty()) {
        commands[player_index].clear();
    }
    std::string file =
        Common::FS::ReadStringFromFile(Common::FS::GetYuzuPath(Common::FS::YuzuPath::TASDir) /
                                           fmt::format("script0-{}.txt", player_index + 1),
                                       Common::FS::FileType::BinaryFile);
    std::stringstream command_line(file);
    std::string line;
    int frame_no = 0;
    while (std::getline(command_line, line, '\n')) {
        if (line.empty()) {
            continue;
        }
        LOG_DEBUG(Input, "Loading line: {}", line);
        std::smatch m;

        std::stringstream linestream(line);
        std::string segment;
        std::vector<std::string> seglist;

        while (std::getline(linestream, segment, ' ')) {
            seglist.push_back(segment);
        }

        if (seglist.size() < 4) {
            continue;
        }

        while (frame_no < std::stoi(seglist.at(0))) {
            commands[player_index].push_back({});
            frame_no++;
        }

        TASCommand command = {
            .buttons = ReadCommandButtons(seglist.at(1)),
            .l_axis = ReadCommandAxis(seglist.at(2)),
            .r_axis = ReadCommandAxis(seglist.at(3)),
        };
        commands[player_index].push_back(command);
        frame_no++;
    }
    LOG_INFO(Input, "TAS file loaded! {} frames", frame_no);
}

void Tas::WriteTasFile(std::u8string file_name) {
    std::string output_text;
    for (size_t frame = 0; frame < record_commands.size(); frame++) {
        if (!output_text.empty()) {
            output_text += "\n";
        }
        const TASCommand& line = record_commands[frame];
        output_text += std::to_string(frame) + " " + WriteCommandButtons(line.buttons) + " " +
                       WriteCommandAxis(line.l_axis) + " " + WriteCommandAxis(line.r_axis);
    }
    const auto bytes_written = Common::FS::WriteStringToFile(
        Common::FS::GetYuzuPath(Common::FS::YuzuPath::TASDir) / file_name,
        Common::FS::FileType::TextFile, output_text);
    if (bytes_written == output_text.size()) {
        LOG_INFO(Input, "TAS file written to file!");
    } else {
        LOG_ERROR(Input, "Writing the TAS-file has failed! {} / {} bytes written", bytes_written,
                  output_text.size());
    }
}

std::pair<float, float> Tas::FlipAxisY(std::pair<float, float> old) {
    auto [x, y] = old;
    return {x, -y};
}

void Tas::RecordInput(u32 buttons, const std::array<std::pair<float, float>, 2>& axes) {
    last_input = {buttons, FlipAxisY(axes[0]), FlipAxisY(axes[1])};
}

std::tuple<TasState, size_t, size_t> Tas::GetStatus() const {
    TasState state;
    if (is_recording) {
        return {TasState::Recording, 0, record_commands.size()};
    }

    if (is_running) {
        state = TasState::Running;
    } else {
        state = TasState::Stopped;
    }

    return {state, current_command, script_length};
}

std::string Tas::DebugButtons(u32 buttons) const {
    return fmt::format("{{ {} }}", TasInput::Tas::ButtonsToString(buttons));
}

std::string Tas::DebugJoystick(float x, float y) const {
    return fmt::format("[ {} , {} ]", std::to_string(x), std::to_string(y));
}

std::string Tas::DebugInput(const TasData& data) const {
    return fmt::format("{{ {} , {} , {} }}", DebugButtons(data.buttons),
                       DebugJoystick(data.axis[0], data.axis[1]),
                       DebugJoystick(data.axis[2], data.axis[3]));
}

std::string Tas::DebugInputs(const std::array<TasData, PLAYER_NUMBER>& arr) const {
    std::string returns = "[ ";
    for (size_t i = 0; i < arr.size(); i++) {
        returns += DebugInput(arr[i]);
        if (i != arr.size() - 1) {
            returns += " , ";
        }
    }
    return returns + "]";
}

std::string Tas::ButtonsToString(u32 button) const {
    std::string returns;
    for (auto [text_button, tas_button] : text_to_tas_button) {
        if ((button & static_cast<u32>(tas_button)) != 0)
            returns += fmt::format(", {}", text_button.substr(4));
    }
    return returns.empty() ? "" : returns.substr(2);
}

void Tas::UpdateThread() {
    if (!Settings::values.tas_enable) {
        return;
    }

    if (is_recording) {
        record_commands.push_back(last_input);
    }
    if (needs_reset) {
        current_command = 0;
        needs_reset = false;
        LoadTasFiles();
        LOG_DEBUG(Input, "tas_reset done");
    }

    if (!is_running) {
        tas_data.fill({});
        return;
    }
    if (current_command < script_length) {
        LOG_DEBUG(Input, "Playing TAS {}/{}", current_command, script_length);
        size_t frame = current_command++;
        for (size_t i = 0; i < commands.size(); i++) {
            if (frame < commands[i].size()) {
                TASCommand command = commands[i][frame];
                tas_data[i].buttons = command.buttons;
                auto [l_axis_x, l_axis_y] = command.l_axis;
                tas_data[i].axis[0] = l_axis_x;
                tas_data[i].axis[1] = l_axis_y;
                auto [r_axis_x, r_axis_y] = command.r_axis;
                tas_data[i].axis[2] = r_axis_x;
                tas_data[i].axis[3] = r_axis_y;
            } else {
                tas_data[i] = {};
            }
        }
    } else {
        is_running = Settings::values.tas_loop.GetValue();
        current_command = 0;
        tas_data.fill({});
        if (!is_running) {
            SwapToStoredController();
        }
    }
    LOG_DEBUG(Input, "TAS inputs: {}", DebugInputs(tas_data));
}

TasAnalog Tas::ReadCommandAxis(const std::string& line) const {
    std::stringstream linestream(line);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(linestream, segment, ';')) {
        seglist.push_back(segment);
    }

    const float x = std::stof(seglist.at(0)) / 32767.0f;
    const float y = std::stof(seglist.at(1)) / 32767.0f;

    return {x, y};
}

u32 Tas::ReadCommandButtons(const std::string& data) const {
    std::stringstream button_text(data);
    std::string line;
    u32 buttons = 0;
    while (std::getline(button_text, line, ';')) {
        for (auto [text, tas_button] : text_to_tas_button) {
            if (text == line) {
                buttons |= static_cast<u32>(tas_button);
                break;
            }
        }
    }
    return buttons;
}

std::string Tas::WriteCommandAxis(TasAnalog data) const {
    auto [x, y] = data;
    std::string line;
    line += std::to_string(static_cast<int>(x * 32767));
    line += ";";
    line += std::to_string(static_cast<int>(y * 32767));
    return line;
}

std::string Tas::WriteCommandButtons(u32 data) const {
    if (data == 0) {
        return "NONE";
    }

    std::string line;
    u32 index = 0;
    while (data > 0) {
        if ((data & 1) == 1) {
            for (auto [text, tas_button] : text_to_tas_button) {
                if (tas_button == static_cast<TasButton>(1 << index)) {
                    if (line.size() > 0) {
                        line += ";";
                    }
                    line += text;
                    break;
                }
            }
        }
        index++;
        data >>= 1;
    }
    return line;
}

void Tas::StartStop() {
    is_running = !is_running;
    if (is_running) {
        SwapToTasController();
    } else {
        SwapToStoredController();
    }
}

void Tas::SwapToTasController() {
    if (!Settings::values.tas_swap_controllers) {
        return;
    }
    auto& players = Settings::values.players.GetValue();
    for (std::size_t index = 0; index < players.size(); index++) {
        auto& player = players[index];
        player_mappings[index] = player;

        // Only swap active controllers
        if (!player.connected) {
            continue;
        }

        auto tas_param = Common::ParamPackage{{"pad", static_cast<u8>(index)}};
        auto button_mapping = GetButtonMappingForDevice(tas_param);
        auto analog_mapping = GetAnalogMappingForDevice(tas_param);
        auto& buttons = player.buttons;
        auto& analogs = player.analogs;

        for (std::size_t i = 0; i < buttons.size(); ++i) {
            buttons[i] = button_mapping[static_cast<Settings::NativeButton::Values>(i)].Serialize();
        }
        for (std::size_t i = 0; i < analogs.size(); ++i) {
            analogs[i] = analog_mapping[static_cast<Settings::NativeAnalog::Values>(i)].Serialize();
        }
    }
    Settings::values.is_device_reload_pending.store(true);
}

void Tas::SwapToStoredController() {
    if (!Settings::values.tas_swap_controllers) {
        return;
    }
    auto& players = Settings::values.players.GetValue();
    for (std::size_t index = 0; index < players.size(); index++) {
        players[index] = player_mappings[index];
    }
    Settings::values.is_device_reload_pending.store(true);
}

void Tas::Reset() {
    needs_reset = true;
}

bool Tas::Record() {
    is_recording = !is_recording;
    return is_recording;
}

void Tas::SaveRecording(bool overwrite_file) {
    if (is_recording) {
        return;
    }
    if (record_commands.empty()) {
        return;
    }
    WriteTasFile(u8"record.txt");
    if (overwrite_file) {
        WriteTasFile(u8"script0-1.txt");
    }
    needs_reset = true;
    record_commands.clear();
}

InputCommon::ButtonMapping Tas::GetButtonMappingForDevice(
    const Common::ParamPackage& params) const {
    // This list is missing ZL/ZR since those are not considered buttons.
    // We will add those afterwards
    // This list also excludes any button that can't be really mapped
    static constexpr std::array<std::pair<Settings::NativeButton::Values, TasButton>, 20>
        switch_to_tas_button = {
            std::pair{Settings::NativeButton::A, TasButton::BUTTON_A},
            {Settings::NativeButton::B, TasButton::BUTTON_B},
            {Settings::NativeButton::X, TasButton::BUTTON_X},
            {Settings::NativeButton::Y, TasButton::BUTTON_Y},
            {Settings::NativeButton::LStick, TasButton::STICK_L},
            {Settings::NativeButton::RStick, TasButton::STICK_R},
            {Settings::NativeButton::L, TasButton::TRIGGER_L},
            {Settings::NativeButton::R, TasButton::TRIGGER_R},
            {Settings::NativeButton::Plus, TasButton::BUTTON_PLUS},
            {Settings::NativeButton::Minus, TasButton::BUTTON_MINUS},
            {Settings::NativeButton::DLeft, TasButton::BUTTON_LEFT},
            {Settings::NativeButton::DUp, TasButton::BUTTON_UP},
            {Settings::NativeButton::DRight, TasButton::BUTTON_RIGHT},
            {Settings::NativeButton::DDown, TasButton::BUTTON_DOWN},
            {Settings::NativeButton::SL, TasButton::BUTTON_SL},
            {Settings::NativeButton::SR, TasButton::BUTTON_SR},
            {Settings::NativeButton::Screenshot, TasButton::BUTTON_CAPTURE},
            {Settings::NativeButton::Home, TasButton::BUTTON_HOME},
            {Settings::NativeButton::ZL, TasButton::TRIGGER_ZL},
            {Settings::NativeButton::ZR, TasButton::TRIGGER_ZR},
        };

    InputCommon::ButtonMapping mapping{};
    for (const auto& [switch_button, tas_button] : switch_to_tas_button) {
        Common::ParamPackage button_params({{"engine", "tas"}});
        button_params.Set("pad", params.Get("pad", 0));
        button_params.Set("button", static_cast<int>(tas_button));
        mapping.insert_or_assign(switch_button, std::move(button_params));
    }

    return mapping;
}

InputCommon::AnalogMapping Tas::GetAnalogMappingForDevice(
    const Common::ParamPackage& params) const {

    InputCommon::AnalogMapping mapping = {};
    Common::ParamPackage left_analog_params;
    left_analog_params.Set("engine", "tas");
    left_analog_params.Set("pad", params.Get("pad", 0));
    left_analog_params.Set("axis_x", static_cast<int>(TasAxes::StickX));
    left_analog_params.Set("axis_y", static_cast<int>(TasAxes::StickY));
    mapping.insert_or_assign(Settings::NativeAnalog::LStick, std::move(left_analog_params));
    Common::ParamPackage right_analog_params;
    right_analog_params.Set("engine", "tas");
    right_analog_params.Set("pad", params.Get("pad", 0));
    right_analog_params.Set("axis_x", static_cast<int>(TasAxes::SubstickX));
    right_analog_params.Set("axis_y", static_cast<int>(TasAxes::SubstickY));
    mapping.insert_or_assign(Settings::NativeAnalog::RStick, std::move(right_analog_params));
    return mapping;
}

const TasData& Tas::GetTasState(std::size_t pad) const {
    return tas_data[pad];
}
} // namespace TasInput
