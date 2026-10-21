// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <set>
namespace pulse {
constexpr int harmonyScaleCount=13,chordQualityCount=15;
struct HarmonyGuide {int root=0,scale=0;bool enabled=true;bool operator==(const HarmonyGuide&)const=default;};
enum class ScaleDirection {nearest,down,up};
struct ChordOptions {int root=60,quality=0,inversion=0,octaves=1;double beat=0,length=1;float velocity=.8f;bool skipExact=true;};
struct ProgressionOptions {
    std::vector<int> degrees{1,5,6,4};
    int octave=3;
    double beat=0,step=1,gate=.9;
    float velocity=.8f;
    bool sevenths=false,voiceLeading=true,skipExact=true;
};
juce::Result parseProgression(const juce::String&,std::vector<int>&);
juce::String progressionDegrees(const std::vector<int>&);
juce::Result buildProgression(const std::vector<Note>&,double length,const HarmonyGuide&,const ProgressionOptions&,std::vector<Note>&);
juce::String harmonyPitchClass(int);
juce::String harmonyScaleName(int);
juce::String chordQualityName(int);
juce::Result validateHarmonyGuide(const HarmonyGuide&);
bool harmonyContains(int pitch,const HarmonyGuide&);
juce::Result fitNotesToScale(const std::vector<Note>&,double length,const std::set<int>&,const HarmonyGuide&,ScaleDirection,std::vector<Note>&);
juce::Result buildChord(const std::vector<Note>&,double length,const HarmonyGuide&,const ChordOptions&,std::vector<Note>&);
juce::String harmonyGuideToken(const juce::File&);
juce::Result loadHarmonyGuide(const juce::File&,HarmonyGuide&);
juce::Result saveHarmonyGuide(const juce::File&,const HarmonyGuide&,const juce::String& expected={});
}
