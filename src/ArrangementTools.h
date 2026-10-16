// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
struct SectionClipStats {
    std::vector<int> sourceIndices,resultIndices;
    int patternClips=0,audioClips=0,automationClips=0,automationPoints=0;
    double start=0,end=0,destination=0;
};
juce::Result inspectSectionClips(const Project&,const juce::String& section,SectionClipStats&);
juce::Result transformSectionClips(const Project&,const juce::String& section,double destination,bool move,bool allowOverlap,Project& output,SectionClipStats* =nullptr);
}
