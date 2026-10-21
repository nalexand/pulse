// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cmath>
#include <algorithm>
namespace pulse {
constexpr int additivePartials=16;
using HarmonicLevels=std::array<float,additivePartials>;
inline constexpr HarmonicLevels defaultHarmonics{1};
// Sine recurrence: fixed work and storage, with a fade below Nyquist.
// Normalization is supplied by the voice so removing inaudible partials cannot boost others.
inline double additiveWave(const HarmonicLevels& levels,double phase,double step,double gain=1) noexcept {
    if(!std::isfinite(phase)||!std::isfinite(step)||step<=0||step>=.5)return 0;
    constexpr double tau=6.28318530717958647692;double angle=tau*(phase-std::floor(phase));
    double current=std::sin(angle),previous=0,twiceCos=2*std::cos(angle),out=0;
    for(int i=0;i<additivePartials;++i){double frequency=(i+1)*step;if(frequency>=.48)break;double band=std::clamp((.48-frequency)/.03,0.,1.);out+=levels[(size_t)i]*band*current;double next=twiceCos*current-previous;previous=current;current=next;}
    return out*gain;
}
}
