// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "SynthPresets.h"
namespace pulse {
struct AutomationParameter {
    juce::String key,name,group;
    double low=0,high=1;
    bool logarithmic=false,discrete=false,master=false;
};
const std::array<AutomationParameter,automationTargetCount>& automationParameters();
int automationParameterId(const juce::String&);
juce::StringArray automationParameterChoices(int);
juce::String automationParameterText(int,float normal);
juce::var automationParametersToJson();
double automationParameterValue(int,double normal)noexcept;
float automationParameterNormal(int,double value)noexcept;
float trackAutomationNormal(const Track&,int);
float projectAutomationNormal(const Project&,int);
float projectAutomationAt(const Project&,int target,int track,double beat);
inline float trimAutomationValue(float baseline,float offset)noexcept{return juce::jlimit(0.f,1.f,baseline+(offset-.5f)*2.f);}
juce::String automationClipValueText(const Clip&,float);
float mixerAutomationNormal(const Track::Mixer&,int);
void applyMixerAutomation(Track::Mixer&,int,float normal)noexcept;
void applySynthAutomation(SynthPatch&,float& attack,float& release,int,float normal)noexcept;
}
