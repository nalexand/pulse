// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Harmony.h"
namespace pulse {
enum class HarmonyPresetKind {fitScale,chord,progression};
struct HarmonyPreset {
    juce::String id,name;
    HarmonyPresetKind kind=HarmonyPresetKind::fitScale;
    HarmonyGuide guide;
    ScaleDirection direction=ScaleDirection::nearest;
    ChordOptions chord; // The insertion beat belongs to the current target, not the preset.
    ProgressionOptions progression; // Likewise, recall retains the target progression beat.
};
constexpr int maxHarmonyPresets=128;
juce::String harmonyPresetKindName(HarmonyPresetKind);
juce::Result validateHarmonyPreset(const HarmonyPreset&);
juce::var harmonyPresetToJson(const HarmonyPreset&);
juce::Result harmonyPresetFromJson(const juce::var&,HarmonyPreset&);
juce::String harmonyPresetToken(const juce::File&);
juce::Result loadHarmonyPresets(const juce::File&,std::vector<HarmonyPreset>&);
juce::Result saveHarmonyPresets(const juce::File&,const std::vector<HarmonyPreset>&,const juce::String& expected={});
juce::Result putHarmonyPreset(const juce::File&,const HarmonyPreset&,bool replace,const juce::String& expected={});
juce::Result removeHarmonyPreset(const juce::File&,const juce::String&,const juce::String& expected={});
std::vector<HarmonyPreset> factoryHarmonyPresets(HarmonyPresetKind);
}
