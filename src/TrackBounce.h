// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Render.h"
#include "ProjectAssets.h"
namespace pulse {
struct BouncePlan {
    Project source;
    RenderSettings settings;
    int track=-1, replaced=0;
    double length=0;
};
juce::Result planTrackBounce(const Project&,int track,double tailSeconds,uint64_t availableBytes,BouncePlan&);
juce::Result applyTrackBounce(const Project&,int track,const juce::File&,double length,Project&);
struct BounceDependency {juce::File file;juce::int64 bytes=0,modified=0;juce::String hash;};
class BounceTask final:public juce::Thread {
public:
    BounceTask(BouncePlan plan,juce::File directory):Thread("Channel source bounce"),plan(std::move(plan)),directory(directory){}
    ~BounceTask()override{cancel.store(true);waitForThreadToExit(-1);}
    void run()override;
    std::atomic<bool> cancel{false},done{false};std::atomic<float> progress{0};
    juce::Result result=juce::Result::fail("Not rendered");
    bool ownsDirectory=false; // Read by UI only after done/join.
    BouncePlan plan;juce::File directory,audioFile;juce::String audioHash;
    std::vector<BounceDependency> dependencies;
    juce::int64 audioBytes=0,audioModified=0;
};
}
