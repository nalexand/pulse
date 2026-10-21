// SPDX-License-Identifier: AGPL-3.0-only
#include "EditHistory.h"
#include "EffectPresets.h"
#include <tuple>
#include <algorithm>
namespace pulse {
using namespace juce;
namespace {
bool mixerEqual(const Track::Mixer& a,const Track::Mixer& b){
    return a.dynamics==b.dynamics&&std::tie(a.output,a.sidechain,a.eq,a.compressor,a.reverb,a.order,a.chorus,a.stereo,a.flanger,a.phaser,a.low,a.mid,a.high,a.threshold,a.ratio,a.compAttack,a.compRelease,a.makeup,a.reverbMix,a.room,a.damping)
        ==std::tie(b.output,b.sidechain,b.eq,b.compressor,b.reverb,b.order,b.chorus,b.stereo,b.flanger,b.phaser,b.low,b.mid,b.high,b.threshold,b.ratio,b.compAttack,b.compRelease,b.makeup,b.reverbMix,b.room,b.damping);
}
bool clipEqual(const Clip& a,const Clip& b){return std::tie(a.track,a.pattern,a.beat,a.length,a.audioPath,a.sourceOffset,a.gain,a.fadeIn,a.fadeOut,a.automationTarget,a.automationTrack,a.points,a.fadeInCurve,a.fadeOutCurve)==std::tie(b.track,b.pattern,b.beat,b.length,b.audioPath,b.sourceOffset,b.gain,b.fadeIn,b.fadeOut,b.automationTarget,b.automationTrack,b.points,b.fadeInCurve,b.fadeOutCurve);}
String clipName(const Project& p,const Clip& c){
    String name=c.isAudio()?"Audio: "+File(c.audioPath).getFileName():c.isAutomation()?"Automation: "+automationName((AutomationTarget)c.automationTarget):"Pattern: "+(c.pattern>=0&&c.pattern<(int)p.patterns.size()?p.patterns[(size_t)c.pattern].name:String(c.pattern+1));
    if(c.track>=0&&c.track<(int)p.tracks.size())name+=" / "+p.tracks[(size_t)c.track].name;
    return name+" / beat "+String(c.beat,3)+" / length "+String(c.length,3);
}
template<class T> bool reordered(const std::vector<T>& a,const std::vector<T>& b){
    if(a.size()!=b.size())return false;
    bool different=false;for(size_t i=0;i<a.size();++i){different|=a[i].editorId!=b[i].editorId;if(std::none_of(b.begin(),b.end(),[&](const T& v){return v.editorId==a[i].editorId;}))return false;}return different;
}
template<class T> int previousIndex(const std::vector<T>& before,const T& after,int){
    for(int i=0;i<(int)before.size();++i)if(before[(size_t)i].editorId==after.editorId)return i;
    return -1;
}
}
HistoryDescription describeHistoryEdit(const Project& a,const Project& b){
    StringArray titles,details;
    auto add=[&](const String& title,const String& detail){titles.addIfNotAlreadyThere(title);details.add(detail);};
    if(a.title!=b.title)add("Project name",a.title+" -> "+b.title);
    if(a.bpm!=b.bpm)add("Tempo",String(a.bpm,3)+" -> "+String(b.bpm,3)+" BPM");
    if(a.swing!=b.swing)add("Swing",String(a.swing*100,1)+" -> "+String(b.swing*100,1)+"%");
    if(a.master!=b.master||a.delayFeedback!=b.delayFeedback||a.delayReturn!=b.delayReturn||a.masterSoftClip!=b.masterSoftClip)add("Master mix","Master volume, soft saturation or shared delay settings changed.");
    if(juce::JSON::toString(masterEffectsToJson(a.masterMixer))!=juce::JSON::toString(masterEffectsToJson(b.masterMixer)))add("Master effects","Master insert chain, stereo settings or bypass states changed.");
    if(a.songMode!=b.songMode)add("Playback mode",b.songMode?"Song mode":"Pattern mode");
    if(a.loop!=b.loop||a.loopSection!=b.loopSection)add("Loop range","Loop "+String(b.loop?"on":"off")+(b.loopSection.isEmpty()?String():" / "+b.loopSection));
    if(a.metronome!=b.metronome)add("Metronome",b.metronome?"Click enabled":"Click disabled");
    if(a.notes!=b.notes)add("Project notes","Project notes text changed.");
    if(std::tie(a.recording.countInBars,a.recording.preRollBars,a.recording.punch,a.recording.punchStart,a.recording.punchEnd)!=std::tie(b.recording.countInBars,b.recording.preRollBars,b.recording.punch,b.recording.punchStart,b.recording.punchEnd))add("Recording settings","Count-in, pre-roll or punch settings changed.");
    if(a.tracks.size()!=b.tracks.size())add("Channels",String((int)a.tracks.size())+" -> "+String((int)b.tracks.size())+" channels");
    if(reordered(a.tracks,b.tracks))add("Channel order","Channels reordered.");
    for(int i=0;i<(int)b.tracks.size();++i){const auto& y=b.tracks[(size_t)i];int old=previousIndex(a.tracks,y,i);if(old<0){add("Channels","Added "+y.name+" / "+instrumentName(y.instrument));continue;}const auto& x=a.tracks[(size_t)old];
        if(x.tuning!=y.tuning)add("Tuning: "+y.name,y.name+" / "+y.tuning.name+" / A4 "+String(y.tuning.referenceHz,3)+" Hz / "+(y.tuning.enabled?"enabled":"bypassed"));
        if(x.name!=y.name)add("Channel name",x.name+" -> "+y.name);
        if(x.instrument!=y.instrument||x.synth!=y.synth||x.soundName!=y.soundName||std::tie(x.cutoff,x.drive,x.attack,x.release,x.isBus)!=std::tie(y.cutoff,y.drive,y.attack,y.release,y.isBus))add("Sound: "+y.name,y.name+" / "+instrumentName(x.instrument)+" -> "+instrumentName(y.instrument)+(y.soundName.isEmpty()?String():" / "+y.soundName)+" / patch or tone settings");
        if(std::tie(x.samplePath,x.sampleStart,x.sampleEnd,x.sampleLoop,x.sampleRoot,x.sampleTune)!=std::tie(y.samplePath,y.sampleStart,y.sampleEnd,y.sampleLoop,y.sampleRoot,y.sampleTune))add("Sampler: "+y.name,y.name+" / source, range, loop or tuning changed / "+y.samplePath);
        if(!mixerEqual(x.mixer,y.mixer)||std::tie(x.volume,x.pan,x.send,x.mute,x.solo)!=std::tie(y.volume,y.pan,y.send,y.mute,y.solo))add("Mixer: "+y.name,y.name+" / levels, mute/solo, effects or routing changed");
    }
    if(a.patterns.size()!=b.patterns.size())add("Patterns",String((int)a.patterns.size())+" -> "+String((int)b.patterns.size())+" patterns");
    if(reordered(a.patterns,b.patterns))add("Pattern order","Pattern bank reordered.");
    for(int i=0;i<(int)b.patterns.size();++i){const auto& y=b.patterns[(size_t)i];int old=previousIndex(a.patterns,y,i);if(old<0){add("Patterns","Added "+y.name);continue;}const auto& x=a.patterns[(size_t)old];
        if(x.name!=y.name||x.length!=y.length||x.colour!=y.colour)add("Pattern: "+y.name,x.name+" -> "+y.name+" / length "+String(x.length,3)+" -> "+String(y.length,3)+" beats / colour "+String(x.colour)+" -> "+String(y.colour));
        for(int t=0;t<(int)y.lanes.size();++t){int previous=t<(int)b.tracks.size()?previousIndex(a.tracks,b.tracks[(size_t)t],t):t;static const std::vector<Note> empty;const auto& source=previous>=0&&previous<(int)x.lanes.size()?x.lanes[(size_t)previous]:empty;
            if(source!=y.lanes[(size_t)t]){auto channel=t<(int)b.tracks.size()?b.tracks[(size_t)t].name:"Channel "+String(t+1);add("Notes: "+y.name+" / "+channel,y.name+" / "+channel+" / "+String((int)source.size())+" -> "+String((int)y.lanes[(size_t)t].size())+" notes (pitch, timing, gate or dynamics)");}}
    }
    if(a.clips.size()!=b.clips.size())add("Playlist",String((int)a.clips.size())+" -> "+String((int)b.clips.size())+" clips");
    for(int i=0;i<(int)b.clips.size();++i){const auto& y=b.clips[(size_t)i];int old=previousIndex(a.clips,y,i);if(old<0)add(y.isAutomation()?"Automation":"Playlist","Added "+clipName(b,y));else if(!clipEqual(a.clips[(size_t)old],y))add(y.isAutomation()?"Automation":"Playlist",clipName(a,a.clips[(size_t)old])+" -> "+clipName(b,y)+(y.isAutomation()?" / points or shape changed":" / placement, trim, gain or fade changed"));}
    // Deleted names remain searchable even when the object is absent in the result.
    auto removed=[&](const auto& before,const auto& after,const String& category,auto name){for(const auto& x:before)if(std::none_of(after.begin(),after.end(),[&](const auto& y){return x.editorId==y.editorId;}))add(category,"Removed or replaced: "+name(x));};
    removed(a.tracks,b.tracks,"Channels",[](const Track& t){return t.name;});
    removed(a.patterns,b.patterns,"Patterns",[](const Pattern& p){return p.name;});
    removed(a.clips,b.clips,"Playlist",[&](const Clip& c){return clipName(a,c);});
    for(const auto& m:b.markers){auto* old=findSongMarker(a,m.id);if(!old||std::tie(old->name,old->beat,old->endBeat,old->colour)!=std::tie(m.name,m.beat,m.endBeat,m.colour))add("Song markers",(old?old->name+" -> ":"Added ")+m.name+" / beat "+String(m.beat,3));}
    for(const auto& m:a.markers)if(!findSongMarker(b,m.id))add("Song markers","Removed "+m.name);
    if(titles.isEmpty())return {"Project checkpoint","No musical difference; selection or an unchanged edit checkpoint."};
    auto summary=titles[0];if(titles.size()>1)summary+=" + "+String(titles.size()-1)+" other "+String(titles.size()==2?"area":"areas");
    return {summary,details.joinIntoString("\n")};
}
std::vector<HistoryRow> projectHistoryRows(const Project& current,const std::deque<HistorySnapshot>& undo,const std::deque<HistorySnapshot>& redo){
    std::vector<const Project*> states;std::vector<int64> stamps;
    for(const auto& s:undo){states.push_back(&s);stamps.push_back(s.editTime);}states.push_back(&current);
    for(auto s=redo.rbegin();s!=redo.rend();++s){states.push_back(&*s);stamps.push_back(s->editTime);}
    std::vector<HistoryRow> rows;rows.reserve(states.size());rows.push_back({0,0,"Oldest retained state",states[0]->title+" / before the first retained edit"});
    for(int i=1;i<(int)states.size();++i){auto d=describeHistoryEdit(*states[(size_t)i-1],*states[(size_t)i]);rows.push_back({i,stamps[(size_t)i-1],d.summary,d.detail});}return rows;
}
Result moveProjectHistory(Project& project,std::deque<HistorySnapshot>& undo,std::deque<HistorySnapshot>& redo,int state){
    if(state<0||state>(int)(undo.size()+redo.size()))return Result::fail("History state is no longer available.");
    while((int)undo.size()>state){auto stamp=undo.back().editTime;redo.emplace_back(std::move(project),stamp);project=std::move(static_cast<Project&>(undo.back()));undo.pop_back();}
    while((int)undo.size()<state){auto stamp=redo.back().editTime;undo.emplace_back(std::move(project),stamp);project=std::move(static_cast<Project&>(redo.back()));redo.pop_back();}return Result::ok();
}
}
