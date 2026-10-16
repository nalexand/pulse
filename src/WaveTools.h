// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace pulse {
enum class WaveOperation { trim, erase, silence, insert, replace, duplicate };
juce::Result validateWaveAudio(const juce::AudioBuffer<float>&,double rate);
juce::Result copyWaveRange(const juce::AudioBuffer<float>&,double rate,int first,int end,juce::AudioBuffer<float>& output);
// Offline, band-limited conversion. Same-rate input is copied exactly. Cancellation never commits output.
juce::Result resampleWave(const juce::AudioBuffer<float>&,double rate,double targetRate,juce::AudioBuffer<float>& output,const std::function<bool()>& cancelled={});
// Outputs are committed only on success; half-open frame ranges keep splice boundaries exact.
juce::Result spliceWave(const juce::AudioBuffer<float>&,double rate,int first,int end,WaveOperation,
                        const juce::AudioBuffer<float>& clipboard,double clipboardRate,juce::AudioBuffer<float>& output,int& selectionFirst,int& selectionEnd,const std::function<bool()>& cancelled={});
juce::Result sliceWaveToPlaylist(const Project&,int track,const juce::String& path,double rate,int frames,int first,int end,double stepBeats,double at,Project& output);
struct TransientOptions { double thresholdDb=-36,ratio=2,minGapMs=50; };
juce::Result findWaveTransients(const juce::AudioBuffer<float>&,double rate,int first,int end,const TransientOptions&,std::vector<int>& markers);
juce::Result sliceWaveFramesToPlaylist(const Project&,int track,const juce::String& path,double rate,int frames,const std::vector<int>& boundaries,double at,Project& output);
}
