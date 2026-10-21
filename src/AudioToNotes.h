// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include "AssetFingerprints.h"
#include <atomic>
namespace pulse {
struct TranscriptionOptions {
    double bpm=120,gateDb=-42,confidence=.85,minMs=80;
    bool operator==(const TranscriptionOptions&)const=default;
};
struct TranscribedNote { Note note;double confidence=0; };
struct MelodyTranscript {
    std::vector<TranscribedNote> notes;
    double seconds=0,bpm=120;
    int channel=0,windows=0,voiced=0;
};
juce::Result validateTranscription(const TranscriptionOptions&);
juce::Result transcribeMelody(const juce::AudioBuffer<float>&,double rate,const TranscriptionOptions&,
    MelodyTranscript&,std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
juce::Result appendTranscription(const Project&,const MelodyTranscript&,int track,const juce::String& name,Project&);
juce::String transcriptionSummary(const MelodyTranscript&);
juce::var transcriptionToJson(const MelodyTranscript&,const TranscriptionOptions&,const juce::File&,const juce::String& hash,int first,double rate);
juce::Result saveTranscriptionReport(const juce::File&,const MelodyTranscript&,const TranscriptionOptions&,const juce::File& source,const juce::String& hash,int first,double rate);
class TranscriptionTask final:public juce::Thread {
public:
    TranscriptionTask(juce::AudioBuffer<float> selected,double rate,int first,juce::File file,TranscriptionOptions o)
        :Thread("Audio to notes"),audio(std::move(selected)),sampleRate(rate),sourceFirst(first),source(file),options(o){}
    ~TranscriptionTask()override{cancel.store(true);waitForThreadToExit(-1);}
    void run()override;
    std::atomic<bool> cancel{false},done{false};std::atomic<float> progress{0};
    juce::Result result=juce::Result::fail("Not transcribed");MelodyTranscript transcript;
    juce::String sourceHash,sourceStamp;
private:
    juce::AudioBuffer<float> audio;double sampleRate;int sourceFirst;juce::File source;TranscriptionOptions options;
};
}
