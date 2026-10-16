// SPDX-License-Identifier: AGPL-3.0-only
#include "PatternTools.h"
#include "NoteTools.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
Result validate(const Project& p,int pattern){
    if(p.tracks.empty()||p.tracks.size()>maxTracks||p.patterns.empty()||p.patterns.size()>maxPatterns||pattern<0||pattern>=(int)p.patterns.size()||p.activePattern<0||p.activePattern>=(int)p.patterns.size()||p.clips.size()>maxClips)return Result::fail("Choose a valid pattern in a project within its limits.");
    const auto& source=p.patterns[(size_t)pattern];if(source.lanes.size()!=p.tracks.size())return Result::fail("Pattern channel lanes do not match the project.");
    NoteToolOptions identity;identity.positionStrength=0;std::vector<Note> checked;
    for(const auto& lane:source.lanes){auto r=processNotes(lane,source.length,{},identity,checked);if(r.failed())return r;}
    for(const auto& clip:p.clips)if(clip.track<0||clip.track>=(int)p.tracks.size()||!std::isfinite(clip.beat)||clip.beat<0||clip.beat>4096||!std::isfinite(clip.length)||clip.length<=0||clip.length>600||(!clip.isAudio()&&!clip.isAutomation()&&(clip.pattern<0||clip.pattern>=(int)p.patterns.size())))return Result::fail("Invalid Playlist reference; pattern edit cancelled.");
    return Result::ok();
}
String uniqueName(const Project& p,const String& requested){
    auto base=requested.trim();if(base.isEmpty())base="Pattern";base=base.substring(0,80);
    for(int suffix=1;suffix<=maxPatterns+1;++suffix){auto end=suffix==1?String():" ("+String(suffix)+")";auto candidate=base.substring(0,80-end.length())+end;bool used=false;for(const auto& pat:p.patterns)used|=pat.name.equalsIgnoreCase(candidate);if(!used)return candidate;}
    return base;
}
}
Result makeClipPatternUnique(const Project& source,int clip,Project& output,PatternEditStats* stats){
    if(clip<0||clip>=(int)source.clips.size())return Result::fail("Select a pattern clip in the Playlist first.");const auto& instance=source.clips[(size_t)clip];if(instance.isAudio()||instance.isAutomation())return Result::fail("Make unique applies to pattern clips. Audio and automation have independent editing tools.");
    auto valid=validate(source,instance.pattern);if(valid.failed())return valid;if(source.patterns.size()>=maxPatterns)return Result::fail("Make unique needs one free pattern slot (32 maximum).");
    auto next=source;auto pattern=source.patterns[(size_t)instance.pattern];pattern.editorId=Uuid().toString();pattern.name=uniqueName(next,pattern.name+" - unique");int index=(int)next.patterns.size();next.patterns.push_back(std::move(pattern));next.clips[(size_t)clip].pattern=index;next.activePattern=index;PatternEditStats result;result.created={index};result.relinked=1;output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
Result splitPatternChannels(const Project& source,int pattern,Project& output,PatternEditStats* stats){
    auto valid=validate(source,pattern);if(valid.failed())return valid;const auto& original=source.patterns[(size_t)pattern];int count=0;for(auto& lane:original.lanes)count+=!lane.empty();if(count<2)return Result::fail("Split by channel needs notes on at least two channels. This pattern is already separated or empty.");
    if(source.patterns.size()+count>maxPatterns)return Result::fail("Split needs "+String(count)+" free pattern slots (32 maximum). Original pattern is retained.");
    auto next=source;PatternEditStats result;std::array<int,maxTracks> mapping;mapping.fill(-1);
    for(int track=0;track<(int)source.tracks.size();++track){if(original.lanes[(size_t)track].empty())continue;Pattern part;part.name=uniqueName(next,original.name+" - "+source.tracks[(size_t)track].name);part.length=original.length;part.colour=original.colour;part.lanes.resize(source.tracks.size());part.lanes[(size_t)track]=original.lanes[(size_t)track];int index=(int)next.patterns.size();mapping[(size_t)track]=index;result.created.push_back(index);next.patterns.push_back(std::move(part));}
    for(auto& clip:next.clips)if(!clip.isAudio()&&!clip.isAutomation()&&clip.pattern==pattern&&mapping[(size_t)clip.track]>=0){clip.pattern=mapping[(size_t)clip.track];++result.relinked;}
    next.activePattern=result.created.front();output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
String patternMergePolicyName(PatternMergePolicy policy){switch(policy){case PatternMergePolicy::keepAll:return "Keep every note";case PatternMergePolicy::skipExact:return "Remove exact duplicates";case PatternMergePolicy::preferFirst:return "Prefer current pattern at same onset/pitch";case PatternMergePolicy::preferSecond:return "Prefer incoming pattern at same onset/pitch";}return "Unknown conflict policy";}
Result mergePatternLayers(const Project& source,int first,int second,Project& output,PatternEditStats* stats,PatternMergePolicy policy){
    if(policy<PatternMergePolicy::keepAll||policy>PatternMergePolicy::preferSecond)return Result::fail("Choose a valid merge conflict policy.");
    auto valid=validate(source,first);if(valid.failed())return valid;valid=validate(source,second);if(valid.failed())return valid;if(first==second)return Result::fail("Choose two different patterns to merge.");if(source.patterns.size()>=maxPatterns)return Result::fail("Merge needs one free pattern slot (32 maximum).");
    const auto& a=source.patterns[(size_t)first];const auto& b=source.patterns[(size_t)second];Pattern merged;merged.colour=a.colour;merged.length=jmax(a.length,b.length);merged.lanes.resize(a.lanes.size());PatternEditStats result;
    auto clashes=[](const Note& x,const Note& y){return x.pitch==y.pitch&&x.beat==y.beat;};
    for(size_t track=0;track<a.lanes.size();++track){const auto& left=a.lanes[track];const auto& right=b.lanes[track];auto& lane=merged.lanes[track];
        for(const auto& n:left){if(policy==PatternMergePolicy::skipExact&&std::find(lane.begin(),lane.end(),n)!=lane.end())continue;if(policy==PatternMergePolicy::preferSecond&&std::any_of(right.begin(),right.end(),[&](const Note& incoming){return clashes(n,incoming);}))continue;lane.push_back(n);}
        for(const auto& n:right){if(policy==PatternMergePolicy::skipExact&&std::find(lane.begin(),lane.end(),n)!=lane.end())continue;if(policy==PatternMergePolicy::preferFirst&&std::any_of(left.begin(),left.end(),[&](const Note& current){return clashes(n,current);}))continue;lane.push_back(n);}
        if(lane.size()>maxNotesPerLane)return Result::fail("Merged channel exceeds 512 notes after conflict handling. Originals are unchanged.");result.skippedNotes+=(int)(left.size()+right.size()-lane.size());
    }
    auto next=source;merged.name=uniqueName(next,a.name+" + "+b.name);int index=(int)next.patterns.size();next.patterns.push_back(std::move(merged));next.activePattern=index;result.created={index};output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
Result extractPatternNotes(const Project& source,int pattern,int track,const std::set<int>& selection,bool move,bool alignStart,Project& output,PatternEditStats* stats){
    auto valid=validate(source,pattern);if(valid.failed())return valid;
    if(track<0||track>=(int)source.tracks.size()||source.tracks[(size_t)track].isBus)return Result::fail("Choose an instrument channel for selected-note extraction.");
    if(source.patterns.size()>=maxPatterns)return Result::fail("Selected-note extraction needs one free pattern slot (32 maximum).");
    const auto& original=source.patterns[(size_t)pattern];const auto& lane=original.lanes[(size_t)track];if(selection.empty())return Result::fail("Select notes in the Piano roll first. An empty selection never means all notes.");
    for(int i:selection)if(i<0||i>=(int)lane.size())return Result::fail("The selected notes changed. Select the intended notes again.");
    double offset=0;if(alignStart){offset=original.length;for(int i:selection)offset=jmin(offset,lane[(size_t)i].beat);}
    Pattern extracted;extracted.length=original.length;extracted.colour=original.colour;extracted.lanes.resize(source.tracks.size());for(int i:selection){auto n=lane[(size_t)i];n.beat-=offset;extracted.lanes[(size_t)track].push_back(n);}
    auto next=source;extracted.name=uniqueName(next,original.name+" - selection");int index=(int)next.patterns.size();next.patterns.push_back(std::move(extracted));next.activePattern=index;
    if(move){auto& remaining=next.patterns[(size_t)pattern].lanes[(size_t)track];for(auto i=selection.rbegin();i!=selection.rend();++i)remaining.erase(remaining.begin()+*i);}
    PatternEditStats result;result.created={index};result.extractedNotes=(int)selection.size();result.movedNotes=move?result.extractedNotes:0;result.sourceTrack=track;result.shiftBeats=offset;output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
Result mergePatternSequence(const Project& source,int first,int second,Project& output,PatternEditStats* stats){
    auto valid=validate(source,first);if(valid.failed())return valid;valid=validate(source,second);if(valid.failed())return valid;
    if(first==second)return Result::fail("Choose two different patterns to sequence.");if(source.patterns.size()>=maxPatterns)return Result::fail("Sequential merge needs one free pattern slot (32 maximum).");
    const auto& a=source.patterns[(size_t)first];const auto& b=source.patterns[(size_t)second];
    if(a.length+b.length>64)return Result::fail("Sequential merge exceeds 64 beats. Shorten the source patterns first; originals are unchanged.");
    Pattern merged;merged.colour=a.colour;merged.length=a.length+b.length;merged.lanes=a.lanes;
    for(size_t track=0;track<a.lanes.size();++track){if(a.lanes[track].size()+b.lanes[track].size()>maxNotesPerLane)return Result::fail("Sequenced channel exceeds 512 notes. Originals are unchanged.");for(auto note:b.lanes[track]){note.beat+=a.length;merged.lanes[track].push_back(note);}}
    auto next=source;merged.name=uniqueName(next,a.name+" then "+b.name);int index=(int)next.patterns.size();next.patterns.push_back(std::move(merged));next.activePattern=index;
    PatternEditStats result;result.created={index};output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
int patternUsage(const Project& p,int pattern){int count=0;for(const auto& c:p.clips)count+=!c.isAudio()&&!c.isAutomation()&&c.pattern==pattern;return count;}
Result movePattern(const Project& source,int from,int to,Project& output,PatternEditStats* stats){
    auto valid=validate(source,from);if(valid.failed())return valid;if(to<0||to>=(int)source.patterns.size()||from==to)return Result::fail("Choose a different position inside the pattern bank.");
    auto remap=[&](int index){if(index==from)return to;if(from<to&&index>from&&index<=to)return index-1;if(from>to&&index>=to&&index<from)return index+1;return index;};
    auto next=source;auto value=std::move(next.patterns[(size_t)from]);next.patterns.erase(next.patterns.begin()+from);next.patterns.insert(next.patterns.begin()+to,std::move(value));next.activePattern=remap(source.activePattern);PatternEditStats result;result.movedFrom=from;result.movedTo=to;
    for(auto& c:next.clips)if(!c.isAudio()&&!c.isAutomation()){int mapped=remap(c.pattern);result.relinked+=mapped!=c.pattern;c.pattern=mapped;}
    output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
Result deleteUnusedPattern(const Project& source,int pattern,Project& output,PatternEditStats* stats){
    auto valid=validate(source,pattern);if(valid.failed())return valid;if(source.patterns.size()==1)return Result::fail("Keep at least one pattern in the project.");int usage=patternUsage(source,pattern);if(usage)return Result::fail("This pattern is used by "+String(usage)+" Playlist clips. Remove or relink those clips first.");
    return deletePatternWithClips(source,pattern,output,stats);
}
Result deletePatternWithClips(const Project& source,int pattern,Project& output,PatternEditStats* stats){
    auto valid=validate(source,pattern);if(valid.failed())return valid;if(source.patterns.size()==1)return Result::fail("Keep at least one pattern in the project.");
    auto next=source;next.patterns.erase(next.patterns.begin()+pattern);next.activePattern=source.activePattern==pattern?jmin(pattern,(int)next.patterns.size()-1):source.activePattern-(source.activePattern>pattern?1:0);PatternEditStats result;result.removed=pattern;
    for(int i=0;i<(int)source.clips.size();++i){const auto& c=source.clips[(size_t)i];if(!c.isAudio()&&!c.isAutomation()&&c.pattern==pattern)result.removedClips.push_back(i);}
    std::erase_if(next.clips,[&](const Clip& c){return !c.isAudio()&&!c.isAutomation()&&c.pattern==pattern;});
    for(auto& c:next.clips)if(!c.isAudio()&&!c.isAutomation()&&c.pattern>pattern){--c.pattern;++result.relinked;}
    output=std::move(next);if(stats)*stats=std::move(result);return Result::ok();
}
}
