// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include <cmath>

namespace pulse {
enum class InputMode { stereo=0, first=1, second=2 };
struct InputSignal { const float* left=nullptr; const float* right=nullptr; int channels=0; };
inline int inputChannelCount(int available,InputMode mode){return available<1||(mode==InputMode::second&&available<2)?0:mode==InputMode::stereo?juce::jmin(2,available):1;}
inline InputSignal selectInput(const juce::AudioBuffer<float>& raw,int available,InputMode mode) {
    available=juce::jlimit(0,juce::jmin(2,raw.getNumChannels()),available);
    if(available==0||(mode==InputMode::second&&available<2))return {};
    auto* first=raw.getReadPointer(mode==InputMode::second?1:0);
    bool stereo=mode==InputMode::stereo&&available>1;
    return {first,stereo?raw.getReadPointer(1):first,stereo?2:1};
}
// Peak accumulation is independent of callback size. Only the UI consumes it.
class InputPeaks {
public:
    void measure(const juce::AudioBuffer<float>& raw,int available,int frames) {
        for(int ch=0;ch<2;++ch){float value=0;
            if(ch<available&&ch<raw.getNumChannels())for(int i=0;i<frames;++i){float x=raw.getSample(ch,i);if(std::isfinite(x))value=juce::jmax(value,std::abs(x));}
            auto old=peaks[(size_t)ch].load(std::memory_order_relaxed);
            while(old<value&&!peaks[(size_t)ch].compare_exchange_weak(old,value,std::memory_order_relaxed)){}
        }
    }
    float consume(int channel){return peaks[(size_t)channel].exchange(0,std::memory_order_relaxed);}
    void reset(){for(auto& peak:peaks)peak.store(0);}
private:
    std::array<std::atomic<float>,2> peaks{};
};
}
