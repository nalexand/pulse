// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <set>
namespace pulse {
enum class ArpOrder { up,down,upDown,downUp,source,random };
struct ArpOptions {
    ArpOrder order=ArpOrder::up;
    double rate=.25,gate=.75;
    int octaves=1,transpose=0;
    float velocity=1;
    bool restart=true;
    uint32_t seed=12345;
};
struct ArpStats { int inputNotes=0,generated=0,restSteps=0,shortSteps=0; };
juce::Result arpeggiateNotes(const std::vector<Note>& source,double patternLength,const std::set<int>& selection,
                            const ArpOptions&,std::vector<Note>& output,ArpStats* stats=nullptr);
}
