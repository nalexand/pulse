// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "SpectralEditing.h"
#include "AssetFingerprints.h"
#include <juce_dsp/juce_dsp.h>
namespace pulse {
enum class RestorationKind { dc,hum,clicks,spectral };
struct RestorationOptions {
    RestorationKind kind=RestorationKind::dc;
    double strength=1,humHz=50,q=20,clickThreshold=6;
    int harmonics=3,clickWidth=3;
    SpectralOptions spectral;
    bool operator==(const RestorationOptions&)const=default;
};
struct RestorationStats {
    std::array<double,2> dcBefore{},dcAfter{},peakBefore{},peakAfter{};
    std::array<uint64_t,2> changed{},clicks{};
};
juce::Result validateRestoration(const RestorationOptions&);
juce::var restorationToJson(const RestorationOptions&);
juce::Result restorationFromJson(const juce::var&,RestorationOptions&);
juce::Result saveRestorationOptions(const juce::File&,const RestorationOptions&);
juce::Result loadRestorationOptions(const juce::File&,RestorationOptions&);
juce::String restorationName(RestorationKind);
juce::String restorationSummary(const RestorationStats&,const RestorationOptions&,double rate,int first,int end);
// Offline processing only; stereo, 8-192 kHz, at most 120 seconds. Selection
// endpoints are half-open frames. Source and output stay unchanged on failure.
juce::Result restoreWave(const juce::AudioBuffer<float>&,double rate,int first,int end,const RestorationOptions&,juce::AudioBuffer<float>&,RestorationStats&,std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
juce::String restorationFileStamp(const juce::File&);
class RestorationTask final:public juce::Thread {
public:
    RestorationTask(juce::AudioBuffer<float> audio,double sampleRate,int a,int b,RestorationOptions settings,juce::File source,juce::File target):Thread("Audio restoration"),first(a),end(b),options(settings),sourceFile(source),outputFile(target),input(std::move(audio)),rate(sampleRate){}
    ~RestorationTask()override{cancel.store(true);waitForThreadToExit(-1);}
    void run()override;
    std::atomic<bool> cancel{false},done{false};std::atomic<float> progress{0};
    juce::Result result=juce::Result::fail("Not processed");RestorationStats stats;
    juce::String sourceStamp,outputStamp;SpectralImages images;
    int first=0,end=0;RestorationOptions options;juce::File sourceFile,outputFile;
private:juce::AudioBuffer<float> input;double rate=48000;
};
}
