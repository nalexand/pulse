// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Render.h"
#include "AssetFingerprints.h"
namespace pulse {
struct MixMeasurements {
    uint64_t frames=0;
    std::array<double,2> peak{},rms{},dc{};
    std::array<uint64_t,2> fullScale{};
    std::array<double,3> bandFraction{};
    double correlation=0;
    bool correlationValid=false;
};
// Worker-only, fixed-size measurement storage. Frequency fractions use stereo
// Hann-window FFT power at 20-200 / 200-4000 / 4000-20000 Hz, not LUFS.
class MixMeter {
public:
    bool feed(float left,float right);
    MixMeasurements finish();
private:
    void spectrum();
    juce::dsp::FFT fft{11};
    std::array<std::array<float,4096>,2> work{};
    std::array<double,2> mean{},variance{},power{},peaks{};
    std::array<uint64_t,2> clipped{};
    std::array<double,3> bands{};
    double cross=0;uint64_t count=0;int at=0;
};
struct MixAnalysisReport {
    MixMeasurements measured;
    AssetFingerprints assets;
    juce::String projectHash,title,levelReason;
    bool songMode=false,canReduce=false;
    double masterBefore=0,masterAfter=0,reductionDb=0;
};
juce::String mixProjectHash(const Project&,const juce::File& base);
bool sameMixAssets(const AssetFingerprints&,const AssetFingerprints&);
juce::Result analyzeMix(const Project&,const juce::File& base,MixAnalysisReport&,std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
juce::String mixAnalysisText(const MixAnalysisReport&);
juce::var mixAnalysisToJson(const MixAnalysisReport&);
juce::Result saveMixAnalysisReport(const juce::File&,const MixAnalysisReport&);
// Owns all worker data and decoded assets; never uses the live Engine/Assets.
class MixAnalysisTask final:public juce::Thread {
public:
    MixAnalysisTask(Project source,juce::File base):Thread("Offline mix analysis"),project(std::move(source)),directory(std::move(base)){}
    ~MixAnalysisTask()override{cancel.store(true);waitForThreadToExit(-1);}
    void run()override{result=analyzeMix(project,directory,report,&cancel,&progress);done.store(true,std::memory_order_release);}
    std::atomic<bool> cancel{false},done{false};std::atomic<float> progress{0};
    juce::Result result=juce::Result::fail("Not analyzed");MixAnalysisReport report;
    Project project;
private:juce::File directory;
};
}
