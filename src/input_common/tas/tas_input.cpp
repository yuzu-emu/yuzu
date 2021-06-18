// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <cstring>
#include <functional>
#include <random>
#include <regex>
#include <thread>
#include <boost/asio.hpp>

#include "common/fs/file.h"
#include "common/fs/fs_types.h"
#include "common/fs/path_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "input_common/tas/tas_input.h"

namespace TasInput {

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
    LoadTasFiles();
}

Tas::~Tas() {
    update_thread_running = false;
}

void Tas::RefreshTasFile() {
    refresh_tas_fle = true;
}
void Tas::LoadTasFiles() {
    script_length = 0;
    for (int i = 0; i < PLAYER_NUMBER; i++) {
        LoadTasFile(i);
        if (commands[i].size() > script_length) {
            script_length = commands[i].size();
        }
    }
}
void Tas::LoadTasFile(int player_index) {
    LOG_DEBUG(Input, "LoadTasFile()");
    if (!commands[player_index].empty()) {
        commands[player_index].clear();
    }
    std::string file = Common::FS::ReadStringFromFile(
        Common::FS::GetYuzuPathString(Common::FS::YuzuPath::TASFile) + "script0-" +
            std::to_string(player_index + 1) + ".txt",
        Common::FS::FileType::BinaryFile);
    std::stringstream command_line(file);
    std::string line;
    int frameNo = 0;
    TASCommand empty = {.buttons = 0, .l_axis = {0.f, 0.f}, .r_axis = {0.f, 0.f}};
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

        while (frameNo < std::stoi(seglist.at(0))) {
            commands[player_index].push_back(empty);
            frameNo++;
        }

        TASCommand command = {
            .buttons = ReadCommandButtons(seglist.at(1)),
            .l_axis = ReadCommandAxis(seglist.at(2)),
            .r_axis = ReadCommandAxis(seglist.at(3)),
        };
        commands[player_index].push_back(command);
        frameNo++;
    }
    LOG_INFO(Input, "TAS file loaded! {} frames", frameNo);
}

void Tas::WriteTasFile() {
    LOG_DEBUG(Input, "WriteTasFile()");
    std::string output_text = "";
    for (int frame = 0; frame < (signed)record_commands.size(); frame++) {
        if (!output_text.empty()) {
            output_text += "\n";
        }
        TASCommand line = record_commands.at(frame);
        output_text += std::to_string(frame) + " " + WriteCommandButtons(line.buttons) + " " +
                       WriteCommandAxis(line.l_axis) + " " + WriteCommandAxis(line.r_axis);
    }
    size_t bytesWritten = Common::FS::WriteStringToFile(
        Common::FS::GetYuzuPathString(Common::FS::YuzuPath::TASFile) + "record.txt",
        Common::FS::FileType::TextFile, output_text);
    if (bytesWritten == output_text.size()) {
        LOG_INFO(Input, "TAS file written to file!");
    } else {
        LOG_ERROR(Input, "Writing the TAS-file has failed! {} / {} bytes written", bytesWritten,
                  output_text.size());
    }
}

static std::pair<float, float> FlipY(std::pair<float, float> old) {
    auto [x, y] = old;
    return {x, -y};
}

void Tas::RecordInput(u32 buttons, const std::array<std::pair<float, float>, 2>& axes) {
    last_input = {buttons, FlipY(axes[0]), FlipY(axes[1])};
}

std::string Tas::GetStatusDescription() {
    if (Settings::values.tas_record) {
        return "Recording TAS: " + std::to_string(record_commands.size());
    }
    if (Settings::values.tas_enable) {
        return "Playing TAS: " + std::to_string(current_command) + "/" +
               std::to_string(script_length);
    }
    return "TAS not running: " + std::to_string(current_command) + "/" +
           std::to_string(script_length);
}

static std::string DebugButtons(u32 buttons) {
    return "{ " + TasInput::Tas::ButtonsToString(buttons) + " }";
}

static std::string DebugJoystick(float x, float y) {
    return "[ " + std::to_string(x) + "," + std::to_string(y) + " ]";
}

static std::string DebugInput(const TasData& data) {
    return "{ " + DebugButtons(data.buttons) + " , " + DebugJoystick(data.axis[0], data.axis[1]) +
           " , " + DebugJoystick(data.axis[2], data.axis[3]) + " }";
}

static std::string DebugInputs(const std::array<TasData, PLAYER_NUMBER>& arr) {
    std::string returns = "[ ";
    for (size_t i = 0; i < arr.size(); i++) {
        returns += DebugInput(arr[i]);
        if (i != arr.size() - 1) {
            returns += " , ";
        }
    }
    return returns + "]";
}

void Tas::UpdateThread() {
    if (update_thread_running) {
        if (Settings::values.pause_tas_on_load && Settings::values.is_cpu_boosted) {
            for (int i = 0; i < PLAYER_NUMBER; i++) {
                tas_data[i].buttons = 0;
                tas_data[i].axis = {};
            }
        }

        if (Settings::values.tas_record) {
            record_commands.push_back(last_input);
        }
        if (!Settings::values.tas_record && !record_commands.empty()) {
            WriteTasFile();
            Settings::values.tas_reset = true;
            refresh_tas_fle = true;
            record_commands.clear();
        }
        if (Settings::values.tas_reset) {
            current_command = 0;
            if (refresh_tas_fle) {
                LoadTasFiles();
                refresh_tas_fle = false;
            }
            Settings::values.tas_reset = false;
            LoadTasFiles();
            LOG_DEBUG(Input, "tas_reset done");
        }
        if (Settings::values.tas_enable) {
            if ((signed)current_command < script_length) {
                LOG_INFO(Input, "Playing TAS {}/{}", current_command, script_length);
                size_t frame = current_command++;
                for (int i = 0; i < PLAYER_NUMBER; i++) {
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
                        tas_data[i].buttons = 0;
                        tas_data[i].axis = {};
                    }
                }
            } else {
                Settings::values.tas_enable = false;
                current_command = 0;
                for (int i = 0; i < PLAYER_NUMBER; i++) {
                    tas_data[i].buttons = 0;
                    tas_data[i].axis = {};
                }
            }
        } else {
            for (int i = 0; i < PLAYER_NUMBER; i++) {
                tas_data[i].buttons = 0;
                tas_data[i].axis = {};
            }
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

    const float x = std::stof(seglist.at(0)) / 32767.f;
    const float y = std::stof(seglist.at(1)) / 32767.f;

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
    if (data == 0)
        return "NONE";

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
