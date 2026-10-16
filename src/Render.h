// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Engine.h"

namespace pulse {
enum class RenderFormat { wav,flac };
enum class RenderSource { current,song,pattern };
struct RenderSettings {
    RenderFormat format=RenderFormat::wav;
    RenderSource source=RenderSource::current;
    int sampleRate=48000,bits=24,channels=2;
    double startBeat=0,endBeat=-1,tailSeconds=2,targetDb=-1;
    bool normalize=false,dither=false;
};
struct RenderPlan {
    double endBeat=0;
    juce::int64 firstFrame=0,endFrame=0,tailFrames=0,outputFrames=0;
    bool songMode=false;
};
juce::Result planRender(const Project&,const RenderSettings&,RenderPlan&);
juce::Result renderAudio(const Project&,Assets&,const juce::File& base,const juce::File& target,
                         const RenderSettings&,int onlyTrack=-1,std::atomic<bool>* cancel=nullptr,
                         std::atomic<float>* progress=nullptr);
}
