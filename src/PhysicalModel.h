// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
namespace pulse {
// A finite modal model of a displaced string or struck free bar. T60 is seconds.
struct PhysicalSettings {
    int mode=0;
    float decay=3,damping=.35f,brightness=.65f,position=.3f,body=.25f,stiffness=.05f;
    bool operator==(const PhysicalSettings&)const=default;
};
class PhysicalVoice {
public:
    void start(double sampleRate);
    double process(const PhysicalSettings&,double frequency);
private:
    struct Mode {double real=0,imag=-1,c=1,s=0,gain=0,target=0;};
    std::array<Mode,16> modes{};
    std::array<Mode,3> body{};
    double rate=48000,smoothing=.01,bodyMix=.25;
    uint32_t frame=0;
    PhysicalSettings cached;double lastFrequency=-1;
};
}
