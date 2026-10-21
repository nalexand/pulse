// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
namespace pulse {
enum class DynamicsMode { compressor,limiter,gate,expander };
struct DynamicsOptions {
    DynamicsMode mode=DynamicsMode::compressor;
    float range=60,hysteresis=6,hold=50,knee=0;
    bool operator==(const DynamicsOptions&)const=default;
};
juce::String dynamicsName(DynamicsMode);
juce::Result validateDynamics(const DynamicsOptions&);
juce::var dynamicsToJson(const DynamicsOptions&);
juce::Result dynamicsFromJson(const juce::var&,DynamicsOptions&);
// Fixed storage, zero lookahead/latency. Legacy compressor stays in MixerDSP.
class PeakDynamics {
public:
    void prepare(double rate){sampleRate=rate;reset();}
    void configure(const DynamicsOptions&,float threshold,float ratio,float attackMs,float releaseMs,float makeupDb);
    void reset();
    float process(float detector,float& left,float& right);
private:
    double sampleRate=48000;
    DynamicsOptions settings;
    float threshold=0,closeThreshold=0,ceiling=1,ratio=4,attack=0,release=0,inputGain=1,floorGain=.001f,gain=1,thresholdDb=-18;
    uint64_t holdSamples=0,remaining=0;
    bool open=false;
};
}
