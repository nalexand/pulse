// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
enum class AutomationWave { sine, triangle, square, steps, randomHold, randomSmooth };
struct AutomationGenerator {
    AutomationWave wave=AutomationWave::sine;
    double period=4,phase=0,low=0,high=1;
    int resolution=16;
    uint32_t seed=12345;
    std::vector<float> steps{0,1,.25f,.75f};
};
const char* automationWaveName(AutomationWave);
juce::Result validateAutomationPoints(const std::vector<AutomationPoint>&,double length,bool requireEndpoints=true);
juce::Result generateAutomation(double length,const AutomationGenerator&,std::vector<AutomationPoint>&);
juce::Result replaceAutomationPoints(Project&,int clip,const std::vector<AutomationPoint>&,bool requireEndpoints=true);
}
