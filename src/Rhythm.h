// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
enum class RhythmWriteMode { replacePitch,fill };
struct RhythmOptions {
    int steps=16,pulses=5,rotation=0,cycles=1,pitch=36;
    double start=0,end=8,gate=.7;
    float velocity=.8f,variation=.1f;
    uint32_t seed=12345;
    RhythmWriteMode write=RhythmWriteMode::replacePitch;
};
struct RhythmStats { int added=0,removed=0,skipped=0; };
juce::Result generateEuclidean(const std::vector<Note>& source,double patternLength,const RhythmOptions&,
                              std::vector<Note>& output,RhythmStats* stats=nullptr);
}
