// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <functional>
namespace pulse {
inline double samplerRatio(int pitch,int root,double cents){return std::pow(2.,(pitch-root+cents*.01)/12.);}
struct SamplePitch {int root=60;double cents=0,frequency=0,confidence=0;int windows=0;};
juce::String samplerNoteName(int);
juce::Result validateSamplerTuning(int root,double cents);
juce::Result detectSamplePitch(const juce::AudioBuffer<float>&,double rate,int first,int end,SamplePitch&,const std::function<bool()>& cancelled={});
}
