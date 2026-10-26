// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <thread>
#include <optional>
#include "TakeRecovery.h"

namespace pulse {
// One device callback produces samples; a dedicated worker writes the WAV.
// Capture never allocates, waits for disk, or changes project/UI state.
class Recorder {
public:
    ~Recorder();
    juce::Result start(const juce::File&,double rate,int inputChannels,double maximumSeconds=120,const TakeCheckpoint* checkpoint=nullptr);
    void capture(const float* left,const float* right,int frames);
    juce::Result finish();
    bool isActive() const { return enabled.load(); }
    uint64_t frames() const { return captured.load(); }
    uint64_t flushedFrames()const{return flushed.load();}
    float peak() const { return inputPeak.load(); }
    bool needsStop() const { return failed.load() || limitReached.load(); }
    double rate() const { return sampleRate; }
    double limitSeconds()const{return frameLimit/sampleRate;}
    uint64_t maxFrames()const{return frameLimit;}
    bool reachedLimit()const{return limitReached.load();}
private:
    juce::AudioBuffer<float> ring;
    juce::AbstractFifo fifo{1};
    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::thread worker;
    juce::FileOutputStream* outputFile=nullptr; // Owned by writer; control/worker only.
    int64_t dataOffset=0;std::optional<TakeCheckpoint> checkpoint;
    std::atomic<bool> enabled{false},done{false},failed{false},limitReached{false};
    std::atomic<int> readers{0};
    std::atomic<uint64_t> captured{0},flushed{0};
    std::atomic<float> inputPeak{0};
    double sampleRate=48000;
    int channels=1;uint64_t frameLimit=0;
};
}
