// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

#include <fmt/format.h>

#include "shader_recompiler/frontend/ir/flow_test.h"

namespace Shader::IR {

std::string NameOf(FlowTest flow_test) {
    switch (flow_test) {
    case FlowTest::F:
        return "F";
    case FlowTest::LT:
        return "LT";
    case FlowTest::EQ:
        return "EQ";
    case FlowTest::LE:
        return "LE";
    case FlowTest::GT:
        return "GT";
    case FlowTest::NE:
        return "NE";
    case FlowTest::GE:
        return "GE";
    case FlowTest::NUM:
        return "NUM";
    case FlowTest::NaN:
        return "NAN";
    case FlowTest::LTU:
        return "LTU";
    case FlowTest::EQU:
        return "EQU";
    case FlowTest::LEU:
        return "LEU";
    case FlowTest::GTU:
        return "GTU";
    case FlowTest::NEU:
        return "NEU";
    case FlowTest::GEU:
        return "GEU";
    case FlowTest::T:
        return "T";
    case FlowTest::OFF:
        return "OFF";
    case FlowTest::LO:
        return "LO";
    case FlowTest::SFF:
        return "SFF";
    case FlowTest::LS:
        return "LS";
    case FlowTest::HI:
        return "HI";
    case FlowTest::SFT:
        return "SFT";
    case FlowTest::HS:
        return "HS";
    case FlowTest::OFT:
        return "OFT";
    case FlowTest::CSM_TA:
        return "CSM_TA";
    case FlowTest::CSM_TR:
        return "CSM_TR";
    case FlowTest::CSM_MX:
        return "CSM_MX";
    case FlowTest::FCSM_TA:
        return "FCSM_TA";
    case FlowTest::FCSM_TR:
        return "FCSM_TR";
    case FlowTest::FCSM_MX:
        return "FCSM_MX";
    case FlowTest::RLE:
        return "RLE";
    case FlowTest::RGT:
        return "RGT";
    }
    return fmt::format("<invalid flow test {}>", static_cast<int>(flow_test));
}

} // namespace Shader::IR
