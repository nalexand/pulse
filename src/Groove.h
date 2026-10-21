// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <set>
namespace pulse {
struct GrooveStep {double offset=0;float velocity=.8f;bool active=true;bool operator==(const GrooveStep&)const=default;};
struct GroovePattern {double grid=.25;std::vector<GrooveStep> steps=std::vector<GrooveStep>(16);bool operator==(const GroovePattern&)const=default;};
struct GrooveSettings {GroovePattern pattern;double timing=1,velocity=0,window=.5;bool useWindow=false;bool operator==(const GrooveSettings&)const=default;};
struct GrooveStats {int changed=0,cells=0,notes=0;};
juce::Result validateGroove(const GrooveSettings&);
juce::Result applyGroove(const std::vector<Note>&,double,const std::set<int>&,const GrooveSettings&,std::vector<Note>&,GrooveStats* =nullptr);
juce::Result extractGroove(const std::vector<Note>&,double,const std::set<int>&,double start,double grid,int steps,GroovePattern&,GrooveStats* =nullptr);
struct GroovePreset {juce::String id,name;GrooveSettings settings;};
constexpr int maxGroovePresets=128,maxGroovePresetBytes=1024*1024;
juce::Result validateGroovePreset(const GroovePreset&);
juce::var groovePresetToJson(const GroovePreset&);
juce::Result groovePresetFromJson(const juce::var&,GroovePreset&);
juce::String groovePresetToken(const juce::File&);
juce::Result loadGroovePresets(const juce::File&,std::vector<GroovePreset>&);
juce::Result saveGroovePresets(const juce::File&,const std::vector<GroovePreset>&,const juce::String& expected={});
juce::Result putGroovePreset(const juce::File&,const GroovePreset&,bool replace,const juce::String& expected={});
juce::Result removeGroovePreset(const juce::File&,const juce::String&,const juce::String& expected={});
std::vector<GroovePreset> factoryGroovePresets();
}
