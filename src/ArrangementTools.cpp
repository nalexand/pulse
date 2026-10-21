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
        if(!validFadeCurve(c.fadeInCurve)||!validFadeCurve(c.fadeOutCurve))return Result::fail("Invalid audio fade curve.");
        if(c.track<0||c.track>=(int)p.tracks.size()||!within(c.beat,0,4096)||(!std::isfinite(c.length)||c.length<=0||c.length>maxClipLength(c))||!std::isfinite(c.beat+c.length)||c.automationTarget<-1||c.automationTarget>5)return Result::fail("Invalid Playlist clip; section edit cancelled.");
        if(c.isAudio()){
            if(c.isAutomation()||!within(c.sourceOffset,0,maxAudioSeconds)||!within(c.gain,0,2)||!within(c.fadeIn,0,c.length)||!within(c.fadeOut,0,c.length))return Result::fail("Invalid audio clip; section edit cancelled.");
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
Result inspectPlaylistGroup(const Project& source,const std::set<int>& indices,PlaylistGroupStats& output){
    int automation=0,points=0;auto valid=validateClips(source,automation,points);if(valid.failed())return valid;
    if(indices.empty())return Result::fail("Select Playlist clips first.");PlaylistGroupStats next;next.start=4096;next.end=0;next.lowTrack=maxTracks;next.highTrack=0;
    for(int index:indices){if(index<0||index>=(int)source.clips.size())return Result::fail("The Playlist selection changed. Select clips again.");const auto& c=source.clips[(size_t)index];next.sourceIndices.push_back(index);next.start=jmin(next.start,c.beat);next.end=jmax(next.end,c.beat+c.length);next.lowTrack=jmin(next.lowTrack,c.track);next.highTrack=jmax(next.highTrack,c.track);}
    output=std::move(next);return Result::ok();
}
namespace {
Result groupDestination(const Project& source,const std::vector<Clip>& incoming,const std::set<int>& moving,double delta,int trackDelta,bool copy,bool allowOverlap,Project& output,PlaylistGroupStats* stats){
    if(incoming.empty()||incoming.size()>maxClips||!std::isfinite(delta)||trackDelta< -maxTracks||trackDelta>maxTracks)return Result::fail("Invalid Playlist group or destination.");
    if(copy&&source.clips.size()+incoming.size()>maxClips)return Result::fail("Copy exceeds 512 Playlist clips. Nothing was changed.");
    auto next=source;PlaylistGroupStats result;result.start=4096;result.lowTrack=maxTracks;int moved=0;
    for(auto clip:incoming){clip.beat+=delta;clip.track+=trackDelta;if(!std::isfinite(clip.beat)||clip.beat<0||clip.beat+clip.length>4096||clip.track<0||clip.track>=(int)source.tracks.size())return Result::fail("The complete group must fit inside beats 0-4096 and existing Playlist lanes.");
        if(!allowOverlap)for(int i=0;i<(int)source.clips.size();++i){if(moving.count(i))continue;const auto& other=source.clips[(size_t)i];bool linked=clip.isAutomation()&&other.isAutomation()&&clip.automationTarget==other.automationTarget&&(clip.automationTarget==5||clip.automationTrack==other.automationTrack);if((clip.track==other.track||linked)&&overlaps(clip.beat,clip.beat+clip.length,other.beat,other.beat+other.length))return Result::fail("Destination overlaps another clip on the same lane or automation parameter. Choose empty space or explicitly allow layering.");}
        result.start=jmin(result.start,clip.beat);result.end=jmax(result.end,clip.beat+clip.length);result.lowTrack=jmin(result.lowTrack,clip.track);result.highTrack=jmax(result.highTrack,clip.track);
        if(copy){clip.editorId=Uuid().toString();result.resultIndices.push_back((int)next.clips.size());next.clips.push_back(std::move(clip));}
        else{auto it=moving.begin();std::advance(it,moved++);result.resultIndices.push_back(*it);next.clips[(size_t)*it]=std::move(clip);}
    }
    int automation=0,points=0;auto valid=validateClips(next,automation,points);if(valid.failed())return valid;output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
}
Result transformPlaylistGroup(const Project& source,const std::set<int>& indices,double delta,int trackDelta,bool copy,bool allowOverlap,Project& output,PlaylistGroupStats* stats){
    PlaylistGroupStats inspected;auto valid=inspectPlaylistGroup(source,indices,inspected);if(valid.failed())return valid;std::vector<Clip> clips;for(int index:indices)clips.push_back(source.clips[(size_t)index]);PlaylistGroupStats result;valid=groupDestination(source,clips,copy?std::set<int>{}:indices,delta,trackDelta,copy,allowOverlap,output,&result);if(valid.wasOk()&&stats){result.sourceIndices=std::move(inspected.sourceIndices);*stats=std::move(result);}return valid;
}
Result insertPlaylistGroup(const Project& source,const std::vector<Clip>& clips,double delta,int trackDelta,bool allowOverlap,Project& output,PlaylistGroupStats* stats){int automation=0,points=0;auto valid=validateClips(source,automation,points);return valid.failed()?valid:groupDestination(source,clips,{},delta,trackDelta,true,allowOverlap,output,stats);}
Result deletePlaylistGroup(const Project& source,const std::set<int>& indices,Project& output){PlaylistGroupStats stats;auto valid=inspectPlaylistGroup(source,indices,stats);if(valid.failed())return valid;auto next=source;for(auto i=indices.rbegin();i!=indices.rend();++i)next.clips.erase(next.clips.begin()+*i);output=std::move(next);return Result::ok();}

Result setPlaylistLoopRange(const Project& source,double start,double end,Project& output){
    if(!std::isfinite(start)||!std::isfinite(end)||start<0||end>4096||end-start<.25)return Result::fail("A Playlist loop must span at least a quarter beat within beats 0-4096.");
    auto valid=validateSongMarkers(source);if(valid.failed())return valid;auto next=source;
    valid=storeSongMarker(next,{playlistLoopMarkerId,"Playlist loop",start,end,4});if(valid.failed())return valid;
    next.loopSection=playlistLoopMarkerId;next.songMode=true;next.loop=true;output=std::move(next);return Result::ok();
}
Result clearPlaylistLoopRange(const Project& source,Project& output){
    auto valid=validateSongMarkers(source);if(valid.failed())return valid;auto next=source;next.loopSection.clear();next.loop=false;
    if(findSongMarker(next,playlistLoopMarkerId))removeSongMarker(next,playlistLoopMarkerId);output=std::move(next);return Result::ok();
}
Result crossfadeSelectedAudio(const Project& source,const std::set<int>& selection,bool clear,Project& output,AudioCrossfadeStats* stats,FadeCurve curve){
    if(!validFadeCurve(curve))return Result::fail("Choose a valid fade curve.");
    PlaylistGroupStats scope;auto valid=inspectPlaylistGroup(source,selection,scope);if(valid.failed())return valid;
    for(int i:selection)if(!source.clips[(size_t)i].isAudio())return Result::fail("Select only audio clips. Pattern and automation clips cannot be crossfaded.");
    auto next=source;AudioCrossfadeStats result;
    if(clear){for(int i:selection){auto& c=next.clips[(size_t)i];if(c.fadeIn!=0||c.fadeOut!=0||c.fadeInCurve!=FadeCurve::linear||c.fadeOutCurve!=FadeCurve::linear){c.fadeIn=c.fadeOut=0;c.fadeInCurve=c.fadeOutCurve=FadeCurve::linear;result.changed.insert(i);}}}
    else{
        if(selection.size()<2)return Result::fail("Select at least two overlapping audio clips on the same Playlist lane.");
        std::vector<int> ordered(selection.begin(),selection.end());std::sort(ordered.begin(),ordered.end(),[&](int a,int b){const auto& x=source.clips[(size_t)a];const auto& y=source.clips[(size_t)b];return x.track!=y.track?x.track<y.track:x.beat!=y.beat?x.beat<y.beat:a<b;});
        for(size_t i=1;i<ordered.size();++i){int first=ordered[i-1],second=ordered[i];const auto& a=source.clips[(size_t)first];const auto& b=source.clips[(size_t)second];if(a.track!=b.track||b.beat>=a.beat+a.length)continue;
            double end=a.beat+a.length;if(a.beat>=b.beat||end>=b.beat+b.length)return Result::fail("Nested clips, identical starts or identical ends make a crossfade ambiguous. Trim or move the clips first.");
            for(int other=0;other<(int)source.clips.size();++other){const auto& c=source.clips[(size_t)other];if(other!=first&&other!=second&&c.isAudio()&&c.track==a.track&&overlaps(b.beat,end,c.beat,c.beat+c.length))return Result::fail("A third audio clip intersects this overlap. Use an isolated two-clip transition first; nothing changed.");}
            double length=end-b.beat;auto& outgoing=next.clips[(size_t)first];auto& incoming=next.clips[(size_t)second];if(outgoing.fadeOut!=length||outgoing.fadeOutCurve!=curve){outgoing.fadeOut=length;outgoing.fadeOutCurve=curve;result.changed.insert(first);}if(incoming.fadeIn!=length||incoming.fadeInCurve!=curve){incoming.fadeIn=length;incoming.fadeInCurve=curve;result.changed.insert(second);}result.pairs.push_back({first,second,b.beat,end});
        }
        if(result.pairs.empty())return Result::fail("No selected audio clips overlap on the same lane. Move the start of the incoming clip before the outgoing clip ends, then select both.");
        for(const auto& pair:result.pairs)for(int i:{pair.outgoing,pair.incoming}){const auto& c=next.clips[(size_t)i];if(c.fadeIn+c.fadeOut>c.length+1e-9)return Result::fail("An existing fade interferes with a transition. Shorten it in Audio clip settings or clear selected fades, then retry.");}
    }
    output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
}
