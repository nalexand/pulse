// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <set>
namespace pulse {
struct SectionClipStats {
    std::vector<int> sourceIndices,resultIndices;
    int patternClips=0,audioClips=0,automationClips=0,automationPoints=0;
    double start=0,end=0,destination=0;
};
juce::Result inspectSectionClips(const Project&,const juce::String& section,SectionClipStats&);
juce::Result transformSectionClips(const Project&,const juce::String& section,double destination,bool move,bool allowOverlap,Project& output,SectionClipStats* =nullptr);
struct PlaylistGroupStats {std::vector<int> sourceIndices,resultIndices;double start=0,end=0;int lowTrack=0,highTrack=0;};
juce::Result inspectPlaylistGroup(const Project&,const std::set<int>&,PlaylistGroupStats&);
juce::Result transformPlaylistGroup(const Project&,const std::set<int>&,double delta,int trackDelta,bool copy,bool allowOverlap,Project&,PlaylistGroupStats* =nullptr);
juce::Result insertPlaylistGroup(const Project&,const std::vector<Clip>&,double delta,int trackDelta,bool allowOverlap,Project&,PlaylistGroupStats* =nullptr);
juce::Result deletePlaylistGroup(const Project&,const std::set<int>&,Project&);

inline constexpr const char* playlistLoopMarkerId="pulse-playlist-loop";
juce::Result setPlaylistLoopRange(const Project&,double start,double end,Project&);
juce::Result clearPlaylistLoopRange(const Project&,Project&);
struct AudioCrossfadePair {int outgoing=-1,incoming=-1;double start=0,end=0;};
struct AudioCrossfadeStats {std::vector<AudioCrossfadePair> pairs;std::set<int> changed;};
juce::Result crossfadeSelectedAudio(const Project&,const std::set<int>&,bool clear,Project&,AudioCrossfadeStats* =nullptr,FadeCurve curve=FadeCurve::linear);
}
