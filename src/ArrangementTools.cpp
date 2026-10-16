// SPDX-License-Identifier: AGPL-3.0-only
#include "ArrangementTools.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
bool within(double value,double lo,double hi){return std::isfinite(value)&&value>=lo&&value<=hi;}
bool overlaps(double first,double firstEnd,double second,double secondEnd){return first<secondEnd&&firstEnd>second;}
Result validateClips(const Project& p,int& automation,int& points){
    if(p.tracks.empty()||p.tracks.size()>maxTracks||p.patterns.empty()||p.patterns.size()>maxPatterns||p.clips.size()>maxClips||p.activePattern<0||p.activePattern>=(int)p.patterns.size())return Result::fail("The project exceeds arrangement limits.");
    for(const auto& pattern:p.patterns)if(!within(pattern.length,1,64)||pattern.lanes.size()!=p.tracks.size()||std::any_of(pattern.lanes.begin(),pattern.lanes.end(),[](const auto& lane){return lane.size()>maxNotesPerLane;}))return Result::fail("Invalid pattern lanes; section edit cancelled.");
    automation=points=0;
    for(const auto& c:p.clips){
        if(c.track<0||c.track>=(int)p.tracks.size()||!within(c.beat,0,4096)||(!std::isfinite(c.length)||c.length<=0||c.length>600)||!std::isfinite(c.beat+c.length)||c.automationTarget<-1||c.automationTarget>5)return Result::fail("Invalid Playlist clip; section edit cancelled.");
        if(c.isAudio()){
            if(c.isAutomation()||!within(c.sourceOffset,0,120)||!within(c.gain,0,2)||!within(c.fadeIn,0,c.length)||!within(c.fadeOut,0,c.length))return Result::fail("Invalid audio clip; section edit cancelled.");
        }else if(c.isAutomation()){
            if((c.automationTarget!=5&&(c.automationTrack<0||c.automationTrack>=(int)p.tracks.size()))||c.points.size()<2||c.points.size()>maxAutomationPoints)return Result::fail("Invalid automation clip; section edit cancelled.");
            double last=-1;for(const auto& point:c.points){if(!within(point.beat,0,600)||point.beat<=last||!within(point.value,0,1))return Result::fail("Invalid automation points; section edit cancelled.");last=point.beat;}
            ++automation;points+=(int)c.points.size();
        }else if(c.pattern<0||c.pattern>=(int)p.patterns.size())return Result::fail("Missing pattern reference; section edit cancelled.");
    }
    if(automation>maxAutomationClips||points>maxTotalAutomationPoints)return Result::fail("The project exceeds automation clip/point limits.");return Result::ok();
}
}
Result inspectSectionClips(const Project& source,const String& id,SectionClipStats& output){
    auto valid=validateSongMarkers(source);if(valid.failed())return valid;int automation=0,points=0;valid=validateClips(source,automation,points);if(valid.failed())return valid;
    auto* section=findSongMarker(source,id);if(!section||!section->isSection())return Result::fail("Choose a Song section range first.");
    SectionClipStats next;next.start=section->beat;next.end=section->endBeat;
    for(int i=0;i<(int)source.clips.size();++i){const auto& c=source.clips[(size_t)i];if(!overlaps(c.beat,c.beat+c.length,next.start,next.end))continue;
        if(c.beat<next.start||c.beat+c.length>next.end)return Result::fail("Playlist clip "+String(i+1)+" crosses a section boundary. Split that clip or adjust the section before copying/moving; nothing was changed.");
        next.sourceIndices.push_back(i);if(c.isAudio())++next.audioClips;else if(c.isAutomation()){++next.automationClips;next.automationPoints+=(int)c.points.size();}else ++next.patternClips;
    }
    if(next.sourceIndices.empty())return Result::fail("This section contains no complete Playlist clips.");output=std::move(next);return Result::ok();
}
Result transformSectionClips(const Project& source,const String& id,double destination,bool move,bool allowOverlap,Project& output,SectionClipStats* stats){
    SectionClipStats scope;auto valid=inspectSectionClips(source,id,scope);if(valid.failed())return valid;double span=scope.end-scope.start;
    if(!within(destination,0,4096)||destination+span>4096)return Result::fail("The full destination section must fit inside beats 0-4096.");
    if(destination==scope.start)return Result::fail("Choose a different destination; the clips are already at this section start.");
    if(!move){
        if(source.clips.size()+scope.sourceIndices.size()>maxClips)return Result::fail("Copy exceeds 512 Playlist clips. Nothing was changed.");
        int automation=0,points=0;for(const auto& c:source.clips)if(c.isAutomation()){++automation;points+=(int)c.points.size();}
        if(automation+scope.automationClips>maxAutomationClips||points+scope.automationPoints>maxTotalAutomationPoints)return Result::fail("Copy exceeds automation clip/point limits. Nothing was changed.");
    }
    std::array<bool,maxClips> moving{};if(move)for(int i:scope.sourceIndices)moving[(size_t)i]=true;
    if(!allowOverlap)for(int index:scope.sourceIndices){const auto& c=source.clips[(size_t)index];double start=destination+(c.beat-scope.start),end=start+c.length;
        for(int i=0;i<(int)source.clips.size();++i){const auto& other=source.clips[(size_t)i];bool sameAutomation=c.isAutomation()&&other.isAutomation()&&c.automationTarget==other.automationTarget&&(c.automationTarget==5||c.automationTrack==other.automationTrack);if(!moving[(size_t)i]&&(c.track==other.track||sameAutomation)&&overlaps(start,end,other.beat,other.beat+other.length))return Result::fail("Destination overlaps Playlist clip "+String(i+1)+" on the same lane or automation parameter. Choose an empty destination or explicitly allow layering; nothing was changed.");}
    }
    auto next=source;scope.destination=destination;
    for(int index:scope.sourceIndices){if(move){next.clips[(size_t)index].beat=destination+(source.clips[(size_t)index].beat-scope.start);scope.resultIndices.push_back(index);}else{auto copy=source.clips[(size_t)index];copy.beat=destination+(copy.beat-scope.start);scope.resultIndices.push_back((int)next.clips.size());next.clips.push_back(std::move(copy));}}
    output=std::move(next);if(stats)*stats=std::move(scope);return Result::ok();
}
}
