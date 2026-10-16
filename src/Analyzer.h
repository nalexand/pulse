// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
namespace pulse {
// One audio producer and one UI consumer. Full queues drop analysis, never audio.
class AnalyzerCapture {
public:
    struct Frame {float left=0,right=0,rate=48000;uint32_t token=0;};
    static constexpr unsigned capacity=16384;
    std::atomic<bool> enabled{false};
    uint32_t selectSource(int source){auto previous=selection.load(std::memory_order_relaxed);auto next=((previous+32u)&~31u)|(uint32_t)(juce::jlimit(-1,15,source)+1);selection.store(next,std::memory_order_release);return next;}
    uint32_t token()const{return selection.load(std::memory_order_acquire);}
    static int source(uint32_t token){return(int)(token&31u)-1;}
    void push(float left,float right,float rate,uint32_t token)noexcept;
    bool pop(Frame&)noexcept;
    void discard()noexcept{read.store(write.load(std::memory_order_acquire),std::memory_order_release);}
    uint64_t dropped()const{return lost.load(std::memory_order_relaxed);}
private:
    std::array<Frame,capacity> frames;
    std::atomic<unsigned> write{0},read{0};
    std::atomic<uint32_t> selection{32};
    std::atomic<uint64_t> lost{0};
};
// UI-thread analysis only. Stereo FFT is power-averaged, so anti-phase does not disappear.
class AnalyzerModel {
public:
    static constexpr int fftOrder=11,size=1<<fftOrder,bins=size/2+1;
    AnalyzerModel();
    void reset(double sampleRate=48000);
    void beginBatch();
    void feed(float left,float right,double sampleRate);
    void finishBatch();
    double sampleRate()const{return rate;}
    uint64_t samples()const{return count;}
    bool spectrumReady()const{return count>=size;}
    double rms(int channel)const{return std::sqrt(power[(size_t)channel]);}
    float peak(int channel)const{return displayPeak[(size_t)channel];}
    float peakHold(int channel)const{return hold[(size_t)channel];}
    uint64_t clips(int channel)const{return clipped[(size_t)channel];}
    uint64_t invalidSamples()const{return invalid;}
    bool correlationReady()const{return power[0]>1e-12&&power[1]>1e-12;}
    double correlation()const{return correlationReady()?juce::jlimit(-1.,1.,cross/std::sqrt(power[0]*power[1])):0;}
    float magnitude(int bin)const{return amplitudes[(size_t)juce::jlimit(0,bins-1,bin)];}
    double frequency(int bin)const{return juce::jlimit(0,bins-1,bin)*rate/size;}
    static double decibels(double amplitude){return amplitude>1e-6?20*std::log10(amplitude):-120;}
private:
    juce::dsp::FFT fft{fftOrder};
    std::array<std::array<float,size>,2> ring{};
    std::array<float,size> window{};
    std::array<std::array<float,size*2>,2> work{};
    std::array<float,bins> amplitudes{};
    std::array<double,2> power{};
    std::array<float,2> displayPeak{},hold{},batchPeak{};
    std::array<uint64_t,2> clipped{};
    double rate=48000,coefficient=0,cross=0,windowSum=0;
    uint64_t count=0,invalid=0,batchFrames=0;
    int position=0;
};
}
