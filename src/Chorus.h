// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
namespace pulse {
struct ChorusSettings {
    bool enabled=false;
    float mix=.35f,rate=.8f,depth=.45f,delay=15,feedback=.1f,width=1;
    bool operator==(const ChorusSettings&)const=default;
};
juce::Result validateChorus(const ChorusSettings&);
juce::var chorusToJson(const ChorusSettings&);
juce::Result chorusFromJson(const juce::var&,ChorusSettings&);
struct ChorusParameterSpec {const char* name;double low,high,initial;bool logarithmic;const char* unit;};
const std::array<ChorusParameterSpec,6>& chorusParameterSpecs();
double chorusValue(const ChorusSettings&,int);
double chorusNormal(int,double);
double chorusFromNormal(int,double);
juce::Result setChorusValue(ChorusSettings&,int,double);
// Stereo variable-delay chorus. Storage is allocated only during prepare().
// Dry has zero latency; wet is deliberately delayed, with no cross-channel summing.
class StereoChorus {
public:
    void prepare(double sampleRate);
    void reset();
    void setSettings(const ChorusSettings&);
    void process(float& left,float& right) noexcept;
private:
    std::array<std::vector<float>,2> history;
    ChorusSettings target;
    std::array<double,6> smooth{};
    double rate=48000,coefficient=.001,fadeCoefficient=.002,phase=0,wetGain=0;
    size_t write=0,filled=0;
    bool running=false;
    float read(int channel,double delaySamples)const noexcept;
};
}
