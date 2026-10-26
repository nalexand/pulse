// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <array>
#include <memory>
#include <vector>
namespace pulse {
// Long Playlist sources use independent cursors even when clips share a file.
// All page storage is allocated before publication. Only the worker accesses the
// real-time cursor's reader; the callback pins immutable pages with bounded CAS.
struct StreamingBudget {
    static constexpr uint64_t limit=128ull*1024*1024;
    std::atomic<uint64_t> bytes{0},sources{0},missedFrames{0},readErrors{0};
};
class AudioStreaming;
class StreamedAudio final {
public:
    static constexpr int pageFrames=8192,pageCount=6;
    ~StreamedAudio();
    const double sampleRate;
    const int64_t frames;
    const bool offline;
    bool sample(double position,float& left,float& right);
    float peak(int64_t start,int count)const;
    bool failed()const{return readFailed.load(std::memory_order_relaxed);}
    bool waveformReady()const{return peaksComplete.load(std::memory_order_acquire);}
    void requestPosition(double seconds){wanted.store((int64_t)(seconds*sampleRate),std::memory_order_relaxed);}
    void requestLoop(double seconds){loopFrame.store((int64_t)(seconds*sampleRate),std::memory_order_relaxed);}
private:
    friend class AudioStreaming;
    struct Page {
        // 0 empty, 1 ready, 2 callback-pinned, 3 worker-writing. base/data are
        // accessed only while exclusively pinned, or after offline ownership.
        std::atomic<int> state{0};std::atomic<int64_t> base{-1};juce::AudioBuffer<float> data;
    };
    StreamedAudio(std::unique_ptr<juce::AudioFormatReader>,double,bool,std::shared_ptr<StreamingBudget>,uint64_t);
    bool readPage(Page&,int64_t);
    bool readSource(juce::AudioBuffer<float>&,int count,int64_t start);
    juce::File sourceFile;int64_t sourceBytes=0,sourceModified=0;
    void fail();
    bool service(juce::AudioBuffer<float>& scratch);
    bool prime();
    bool copy(Page&,double,float&,float&)const;
    std::unique_ptr<juce::AudioFormatReader> reader;
    const std::shared_ptr<StreamingBudget> budget;
    const uint64_t allocationBytes;
    std::array<Page,pageCount> pages;
    std::atomic<int64_t> wanted{0},loopFrame{0};
    const int64_t seedPage;
    const int peakCount;
    std::unique_ptr<std::atomic<float>[]> peaks;
    int scanPage=0;
    std::atomic<bool> readFailed{false},peaksComplete{false};
};
class AudioStreaming final:private juce::Thread {
public:
    AudioStreaming();~AudioStreaming()override;
    std::shared_ptr<StreamedAudio> open(std::unique_ptr<juce::AudioFormatReader>,double offset,bool offline,juce::String& error);
    struct Statistics {uint64_t bytes=0,sources=0,missedFrames=0,readErrors=0;};
    Statistics statistics()const;
private:
    void run()override;
    std::shared_ptr<StreamingBudget> budget=std::make_shared<StreamingBudget>();
    juce::CriticalSection listLock; // UI/worker only. Never acquired by sample().
    std::vector<std::weak_ptr<StreamedAudio>> cursors;
};
}
