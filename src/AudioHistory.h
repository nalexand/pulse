// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "InputAudio.h"
#include "Project.h"
#include <atomic>
#include <juce_cryptography/juce_cryptography.h>
namespace pulse {
struct HistoryOptions {int seconds=30,memoryMiB=32;InputMode mode=InputMode::stereo;bool operator==(const HistoryOptions&)const=default;};
struct HistorySpan {double firstBeat=0,bpm=120,rate=48000;int frames=0;uint64_t revision=0;bool valid=false;};
struct AudioHistorySnapshot {juce::AudioBuffer<float> audio;HistoryOptions options;double rate=48000,bpm=120,firstBeat=0,endBeat=0;uint64_t revision=0,invalidSamples=0;};
juce::Result validateHistoryOptions(const HistoryOptions&);
juce::Result saveHistoryOptions(const juce::File&,const HistoryOptions&);
juce::Result loadHistoryOptions(const juce::File&,HistoryOptions&);
juce::String historySourceName(InputMode);
// One callback producer. UI configure/disable/freeze first closes the gate and
// waits for existing readers. The callback never waits, allocates or writes disk.
// Snapshot runs off-thread only while paused; configure is blocked during it.
class AudioHistory final {
public:
    ~AudioHistory(){disable();}
    juce::Result start(double rate,const HistoryOptions&);
    void pause();void disable();
    void invalidate(){if(enabled.exchange(false)||published.load()>0)fault.store(true);}
    void capture(const juce::AudioBuffer<float>& raw,int available,const HistorySpan&);
    juce::Result copyFrozen(AudioHistorySnapshot&,std::atomic<bool>* cancel=nullptr)const;
    bool active()const{return enabled.load();}bool invalidated()const{return fault.load();}
    uint64_t frames()const{return published.load();}int capacity()const{return buffer.getNumSamples();}
    uint64_t bytes()const{return (uint64_t)buffer.getNumSamples()*buffer.getNumChannels()*sizeof(float);}
private:
    juce::AudioBuffer<float> buffer;HistoryOptions options;double rate=48000,bpm=120,endBeat=0;
    int count=0,write=0;uint64_t revision=0,invalidSamples=0;bool gap=true;
    std::atomic<bool> enabled{false},fault{false};std::atomic<int> readers{0};std::atomic<uint64_t> published{0};
};
juce::Result appendAudioHistory(const Project&,const AudioHistorySnapshot&,const juce::File&,int track,Project&);
juce::String historySummary(const AudioHistorySnapshot&);
juce::String historyFileStamp(const juce::File&);
class HistorySaveTask final:public juce::Thread {
public:
    HistorySaveTask(const AudioHistory& h,juce::File dir):Thread("Retrospective audio"),history(h),folder(dir){}
    ~HistorySaveTask()override{cancel.store(true);waitForThreadToExit(-1);}
    void run()override;
    std::atomic<bool> cancel{false},done{false};juce::Result result=juce::Result::fail("Not saved");
    AudioHistorySnapshot snapshot;juce::File folder;juce::String hash,stamp;
private:const AudioHistory& history;
};
}
