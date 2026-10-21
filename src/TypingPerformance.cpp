// SPDX-License-Identifier: AGPL-3.0-only
#include "TypingPerformance.h"
#include <algorithm>
#include <cmath>

namespace pulse {
using namespace juce;
int typingKeyOffset(int keyCode,juce_wchar textCharacter){
    auto key=CharacterFunctions::toLowerCase((juce_wchar)keyCode);
    if(keyCode<32||keyCode>126)key=CharacterFunctions::toLowerCase(textCharacter);
    int offset=String("zsxdcvgbhnjm,l.;/").indexOfChar(key);if(offset>=0)return offset;
    offset=String("q2w3er5t6y7ui9o0p[=]").indexOfChar(key);return offset>=0?offset+12:-1;
}
Result loadTypingSettings(const File& file,TypingSettings& output){
    if(!file.existsAsFile())return Result::ok();
    if(file.getSize()>4096)return Result::fail("Typing preferences are too large.");
    auto value=JSON::parse(file.loadFileAsString());
    auto integer=[](const var& v,int low,int high){return(v.isInt()||v.isInt64())&&(int64)v>=low&&(int64)v<=high;};
    if(!value.isObject()||!integer(value["schema"],1,1)||!integer(value["velocity"],1,127)||!integer(value["octave"],1,7)||!value["enabled"].isBool())return Result::fail("Invalid typing preferences; defaults retained.");
    TypingSettings next;next.velocity=(int)value["velocity"];next.octave=(int)value["octave"];next.enabled=(bool)value["enabled"];
    if(value.hasProperty("transpose")){if(!integer(value["transpose"],-12,12))return Result::fail("Invalid typing transposition.");next.transpose=(int)value["transpose"];}
    if(value.hasProperty("capture")){if(!value["capture"].isBool())return Result::fail("Invalid typing capture preference.");next.capture=(bool)value["capture"];}
    if(value.hasProperty("captureSeconds")){if(!integer(value["captureSeconds"],15,60)||((int)value["captureSeconds"]!=15&&(int)value["captureSeconds"]!=30&&(int)value["captureSeconds"]!=60))return Result::fail("Invalid typing capture window.");next.captureSeconds=(int)value["captureSeconds"];}
    output=next;return Result::ok();
}
Result saveTypingSettings(const File& file,const TypingSettings& settings){
    if(settings.velocity<1||settings.velocity>127||settings.octave<1||settings.octave>7||settings.transpose< -12||settings.transpose>12||(settings.captureSeconds!=15&&settings.captureSeconds!=30&&settings.captureSeconds!=60))return Result::fail("Invalid typing preferences.");
    auto value=var(new DynamicObject());auto* obj=value.getDynamicObject();obj->setProperty("schema",1);obj->setProperty("velocity",settings.velocity);obj->setProperty("octave",settings.octave);obj->setProperty("enabled",settings.enabled);
    obj->setProperty("transpose",settings.transpose);obj->setProperty("capture",settings.capture);obj->setProperty("captureSeconds",settings.captureSeconds);
    if(file.getParentDirectory().createDirectory().failed())return Result::fail("Cannot create typing preferences folder.");
    TemporaryFile temp(file);if(!temp.getFile().replaceWithText(JSON::toString(value,false))||!temp.overwriteTargetFileWithTemporary())return Result::fail("Cannot save typing preferences locally.");return Result::ok();
}
void TypingCapture::clear(){complete.clear();active.clear();}
void TypingCapture::prune(double now){
    complete.erase(std::remove_if(complete.begin(),complete.end(),[&](const Played& n){return n.end<=now-historyMs;}),complete.end());
    while(complete.size()+active.size()>capacity&&!complete.empty())complete.pop_front();
}
void TypingCapture::finish(Played note,double now){note.end=std::max(note.start,now);complete.push_back(note);prune(now);}
void TypingCapture::noteOn(int pitch,float velocity,double now){
    if(pitch<0||pitch>127||!std::isfinite(velocity)||velocity<=0||velocity>1||!std::isfinite(now))return;
    noteOff(pitch,now);prune(now);
    while(complete.size()+active.size()>=capacity&&!complete.empty())complete.pop_front();
    active[pitch]={pitch,velocity,now,now};
}
void TypingCapture::noteOff(int pitch,double now){
    if(!std::isfinite(now))return;auto found=active.find(pitch);if(found==active.end())return;auto note=found->second;active.erase(found);finish(note,now);
}
void TypingCapture::allOff(double now){if(!std::isfinite(now))return;while(!active.empty())noteOff(active.begin()->first,now);prune(now);}
Result TypingCapture::phrase(double now,double bpm,std::vector<Note>& output,double& length)const{
    if(!std::isfinite(now)||!std::isfinite(bpm)||bpm<30||bpm>300)return Result::fail("Invalid capture time or tempo.");
    const double perBeat=60000/bpm,begin=now-std::min(historyMs,64*perBeat);
    std::vector<Played> kept;auto collect=[&](Played note){note.end=std::min(now,note.end);note.start=std::max(begin,note.start);if(note.end>note.start&&note.start<=now)kept.push_back(note);};
    for(auto note:complete)collect(note);for(auto [pitch,note]:active){note.end=now;collect(note);}
    if(kept.empty())return Result::fail("No recent typing performance on this channel. Play a phrase, then recall it within the selected history window.");
    std::stable_sort(kept.begin(),kept.end(),[](const Played& a,const Played& b){return a.start==b.start?a.pitch<b.pitch:a.start<b.start;});
    if(kept.size()>maxNotesPerLane)return Result::fail("The captured phrase exceeds the pattern note limit.");
    std::vector<Note> notes;double end=0,origin=kept.front().start;
    for(const auto& n:kept){double start=(n.start-origin)/perBeat;if(start>=64-.01)continue;double gate=jlimit(.01,64-start,(n.end-n.start)/perBeat);notes.push_back({start,gate,n.pitch,n.velocity});end=jmax(end,start+gate);}
    if(notes.empty())return Result::fail("The captured phrase has no usable notes.");
    length=jlimit(1.,64.,std::ceil((end-1e-9)/4)*4);output=std::move(notes);return Result::ok();
}
}
