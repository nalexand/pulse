// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "MixAnalysis.h"
namespace pulse {
struct ReferenceOptions { double mixBeat=0,referenceSeconds=0,duration=4;bool operator==(const ReferenceOptions&)const=default; };
struct ReferenceReport {
    ReferenceOptions options;
    MixMeasurements mix,reference;
    AssetFingerprints sources;
    juce::String projectHash,referenceHash,title;
    juce::File referenceFile,mixFile,referenceCopy;
    juce::String mixStamp,copyStamp;
    bool songMode=false;
};
juce::Result validateReferenceOptions(const ReferenceOptions&);
std::array<float,2> referenceGains(const ReferenceReport&,bool match);
juce::String referenceSummary(const ReferenceReport&);
juce::var referenceToJson(const ReferenceReport&);
juce::Result saveReferenceReport(const juce::File&,const ReferenceReport&);
// New, caller-owned directory only. 48 kHz float stereo snapshots; sources are
// read-only. Failures/cancellation retain output report and remove owned files.
juce::Result compareReference(const Project&,const juce::File& base,const juce::File& reference,
    const ReferenceOptions&,const juce::File& newDirectory,ReferenceReport&,
    std::atomic<bool>* cancel=nullptr,std::atomic<float>* progress=nullptr);
class ReferenceTask final:public juce::Thread {
public:
    ReferenceTask(Project p,juce::File base,juce::File ref,ReferenceOptions options,juce::File directory)
      :Thread("Reference comparison"),project(std::move(p)),baseFile(base),reference(ref),folder(directory),settings(options){}
    ~ReferenceTask()override{cancel.store(true);waitForThreadToExit(-1);}
    void run()override;
    std::atomic<bool> cancel{false},done{false};std::atomic<float> progress{0};
    juce::Result result=juce::Result::fail("Not compared");ReferenceReport report;
    Project project;juce::File baseFile,reference,folder;ReferenceOptions settings;
};
}
