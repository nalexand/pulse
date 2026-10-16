// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
namespace pulse {
struct MetronomeSettings {
    int sound=0,level=100;
    bool accent=true;
    bool operator==(const MetronomeSettings&)const=default;
};
constexpr int metronomeSoundCount=4;
juce::String metronomeSoundName(int);
juce::Result validateMetronome(const MetronomeSettings&);
uint32_t packMetronome(const MetronomeSettings&);
MetronomeSettings unpackMetronome(uint32_t);
juce::Result loadMetronome(const juce::File&,MetronomeSettings&);
juce::Result saveMetronome(const juce::File&,const MetronomeSettings&);
// A fixed-storage click voice. Settings are latched at each onset.
class MetronomeClick {
public:
    void start(double sampleRate,bool barStart,const MetronomeSettings&);
    void reset(){remaining=0;}
    float next();
private:
    int remaining=0,sound=0,total=1;
    double rate=48000,frequency=1500;
    float level=.08f,previousNoise=0;
    uint32_t noise=1;
};
}
