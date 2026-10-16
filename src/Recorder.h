// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <thread>

namespace pulse {
// One device callback produces samples; a dedicated worker writes the WAV.
// Capture never allocates, waits for disk, or changes project/UI state.
class Recorder {
public:
    ~Recorder();
    juce::Result start(const juce::File&,double rate,int inputChannels);
    void capture(const float* left,const float* right,int frames);
    juce::Result finish();
    bool isActive() const { return enabled.load(); }
    uint64_t frames() const { return captured.load(); }
    float peak() const { return inputPeak.load(); }
    bool needsStop() const { return failed.load() || limitReached.load(); }
    double rate() const { return sampleRate; }
private:
    juce::AudioBuffer<float> ring;
    juce::AbstractFifo fifo{1};
    std::unique_ptr<juce::AudioFormatWriter> writer;
    std::thread worker;
    std::atomic<bool> enabled{false},done{false},failed{false},limitReached{false};
    std::atomic<int> readers{0};
    std::atomic<uint64_t> captured{0};
    std::atomic<float> inputPeak{0};
    double sampleRate=48000;
    int channels=1;
};
}
