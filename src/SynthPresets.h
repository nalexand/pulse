// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
struct SynthPreset {juce::String name,category;Instrument instrument=Instrument::analog;SynthPatch patch;float attack=.005f,release=.18f;};
const std::vector<SynthPreset>& factorySynthPresets();
juce::Result validateSynthPreset(const SynthPreset&);
juce::var synthPresetToJson(const SynthPreset&);
juce::Result synthPresetFromJson(const juce::var&,SynthPreset&);
juce::Result saveSynthPreset(const SynthPreset&,const juce::File&);
juce::Result loadSynthPreset(const juce::File&,SynthPreset&);
struct SynthParameterSpec {const char* name;double low,high;bool logarithmic;const char* unit;};
constexpr int synthParameterCount=48;
const std::array<SynthParameterSpec,synthParameterCount>& synthParameterSpecs();
double synthParameterValue(const Track&,int);
double synthParameterNormal(int,double value);
double synthParameterFromNormal(int,double normal);
juce::Result setSynthParameter(Track&,int,double value);
}
