// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "WaveTools.h"
#include <juce_dsp/juce_dsp.h>
namespace pulse {
struct SpectralOptions {
    double lowHz=800,highHz=1200,gainDb=-24;
    int fftOrder=11;
    bool operator==(const SpectralOptions&)const=default;
};
struct SpectralImages {
    static constexpr int columns=192,rows=96;
    std::vector<float> original,edited;
    double sampleRate=0,firstSeconds=0,endSeconds=0;
    bool valid()const{return original.size()==columns*rows&&edited.size()==columns*rows&&sampleRate>=8000;}
};
juce::Result validateSpectral(const SpectralOptions&);
// Offline only. Whole source <=120 s; selection one FFT window to 30 s.
// A Hann window, 75% overlap and normalized difference overlap-add retain
// exact source samples outside the selection. Failure never replaces output.
juce::Result editSpectral(const juce::AudioBuffer<float>&,double rate,int first,int end,
    const SpectralOptions&,double strength,juce::AudioBuffer<float>&,
    std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
// Fixed-size, stereo-power spectrograms, -100..+12 dBFS, logarithmic 20 Hz
// to Nyquist axis. Pixel columns sample windows, not transient-peak guarantees.
juce::Result spectralImages(const juce::AudioBuffer<float>& original,
    const juce::AudioBuffer<float>& edited,double rate,int first,int end,int fftOrder,
    SpectralImages&,std::atomic<bool>* cancel=nullptr);
}
