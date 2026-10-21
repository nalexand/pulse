// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <set>
namespace pulse {
enum class NoteToolMode { quantize,humanize,euclidean,arpeggiator,fitScale,buildChord,buildProgression,groove };
struct NoteToolOptions {
    NoteToolMode mode=NoteToolMode::quantize;
    double grid=.25,positionStrength=1,lengthStrength=0,swing=0,window=.125;
    bool useWindow=false,keepChords=true;
    double timing=.02,length=.02,velocity=.05;
    uint32_t seed=12345;
};
juce::Result processNotes(const std::vector<Note>& source,double patternLength,const std::set<int>& selection,
                          const NoteToolOptions&,std::vector<Note>& output);
}
