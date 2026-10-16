// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <set>
namespace pulse {
enum class PatternMergePolicy { keepAll,skipExact,preferFirst,preferSecond };
juce::String patternMergePolicyName(PatternMergePolicy);
struct PatternEditStats { std::vector<int> created;int relinked=0;int removed=-1,movedFrom=-1,movedTo=-1;std::vector<int> removedClips;int skippedNotes=0,extractedNotes=0,movedNotes=0,sourceTrack=-1;double shiftBeats=0; };
juce::Result makeClipPatternUnique(const Project&,int clip,Project& output,PatternEditStats* =nullptr);
juce::Result splitPatternChannels(const Project&,int pattern,Project& output,PatternEditStats* =nullptr);
juce::Result mergePatternLayers(const Project&,int first,int second,Project& output,PatternEditStats* =nullptr,PatternMergePolicy=PatternMergePolicy::keepAll);
juce::Result extractPatternNotes(const Project&,int pattern,int track,const std::set<int>& selection,bool move,bool alignStart,Project& output,PatternEditStats* =nullptr);
juce::Result mergePatternSequence(const Project&,int first,int second,Project& output,PatternEditStats* =nullptr);
int patternUsage(const Project&,int pattern);
juce::Result movePattern(const Project&,int from,int to,Project& output,PatternEditStats* =nullptr);
juce::Result deleteUnusedPattern(const Project&,int pattern,Project& output,PatternEditStats* =nullptr);
juce::Result deletePatternWithClips(const Project&,int pattern,Project& output,PatternEditStats* =nullptr);
}
