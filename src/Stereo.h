// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
#include <array>
namespace pulse {
struct StereoSettings {
    bool enabled=false,mono=false,swap=false,invertLeft=false,invertRight=false;
    float width=1,trim=0;
    bool operator==(const StereoSettings&)const=default;
};
juce::Result validateStereo(const StereoSettings&);
juce::var stereoToJson(const StereoSettings&);
juce::Result stereoFromJson(const juce::var&,StereoSettings&);
// Post-chain, pre-fader stereo matrix. Coefficients smooth on the audio thread.
class StereoUtility {
public:
    void prepare(double sampleRate);
    void reset();
    void setSettings(const StereoSettings&);
    void process(float& left,float& right) noexcept;
private:
    std::array<double,4> target{1,0,0,1},current{1,0,0,1};
    double coefficient=1;
    bool configured=false;
};
}
