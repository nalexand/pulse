// SPDX-License-Identifier: AGPL-3.0-only
#include "TrackBounce.h"
#include <cmath>
#include <juce_cryptography/juce_cryptography.h>
namespace pulse {
using namespace juce;
Result planTrackBounce(const Project& p,int track,double tail,uint64_t available,BouncePlan& out){
    if(!p.songMode)return Result::fail("Switch to SONG to bounce the selected channel's arrangement. Pattern notes stay editable.");
    if(track<0||track>=(int)p.tracks.size()||p.tracks[(size_t)track].isBus)return Result::fail("Choose an instrument/audio source channel, not a bus.");
    if(!std::isfinite(tail)||tail<0||tail>10)return Result::fail("Choose a release tail from 0 to 10 seconds.");
    for(const auto& c:p.clips)if(!c.isAudio()&&!c.isAutomation()){
        if(c.pattern<0||c.pattern>=(int)p.patterns.size()||c.track<0||c.track>=(int)p.tracks.size()||c.track>=(int)p.patterns[(size_t)c.pattern].lanes.size())return Result::fail("Invalid Song source block.");
        if(c.track!=track)for(const auto& n:p.patterns[(size_t)c.pattern].lanes[(size_t)c.track])if(n.probability<1)return Result::fail("Another arranged channel has probability notes. Source bounce currently needs 100% note probability on other channels so removing source events cannot change their random choices.");
    }
    if(p.tracks[(size_t)track].chokeGroup>0)for(int i=0;i<(int)p.tracks.size();++i)if(i!=track&&!p.tracks[(size_t)i].isBus&&p.tracks[(size_t)i].chokeGroup==p.tracks[(size_t)track].chokeGroup)return Result::fail("This channel shares a choke group. Replacing its notes with audio would remove hits that close other channels. Export the full Song/stems, or separate the group before source bounce.");
    BouncePlan next;next.track=track;next.source=p;double end=0;
    for(const auto& c:p.clips)if(c.track==track&&!c.isAutomation())++next.replaced;
    if(!next.replaced)return Result::fail("The selected channel has no Song blocks to bounce.");
    // Retain the full Song timeline/events so held gates, deterministic
    // probability indices and condition passes match its original first pass.
    // Other tracks are muted; the tap captures only the selected raw source.
    end=p.songLength();
    for(int i=0;i<(int)next.source.tracks.size();++i){auto& t=next.source.tracks[(size_t)i];t.mute=i!=track;t.solo=i==track;t.mixer={};t.auxSends={};t.send=0;}
    next.source.master=1;next.source.masterMixer={};next.source.masterSoftClip=false;next.source.delayReturn=0;next.source.loop=false;next.source.loopSection.clear();
    next.settings.source=RenderSource::song;next.settings.bits=32;next.settings.tailSeconds=tail;next.settings.endBeat=end;next.settings.rawTrack=track;
    RenderPlan render;auto valid=planRender(next.source,next.settings,render);if(valid.failed())return valid;
    next.length=render.outputFrames/(double)next.settings.sampleRate*p.bpm/60;
    if(next.length>4096||render.outputFrames>(int64)(maxAudioSeconds*next.settings.sampleRate)|| (uint64_t)render.outputFrames*8>available)return Result::fail("Bounce exceeds the 4096-beat timeline, 30-minute source limit or remaining decoded-audio memory. Shorten the Song/channel or save and reopen first.");
    out=std::move(next);return Result::ok();
}
Result applyTrackBounce(const Project& p,int track,const File& audio,double length,Project& out){
    if(!p.songMode||track<0||track>=(int)p.tracks.size()||p.tracks[(size_t)track].isBus||audio==File()||!audio.existsAsFile()||!std::isfinite(length)||length<=0||length>4096)return Result::fail("Invalid or unavailable channel bounce.");
    Project next=p;auto count=next.clips.size();next.clips.erase(std::remove_if(next.clips.begin(),next.clips.end(),[&](const Clip& c){return c.track==track&&!c.isAutomation();}),next.clips.end());
    if(count==next.clips.size()||next.clips.size()>=maxClips)return Result::fail("The original channel blocks are unavailable or the clip bank is full.");
    Clip clip;clip.track=track;clip.beat=0;clip.length=length;clip.audioPath=audio.getFullPathName();next.clips.push_back(clip);out=std::move(next);return Result::ok();
}
namespace {
Result fingerprint(const File& f,BounceDependency& d,std::atomic<bool>& cancel){
    if(cancel.load())return Result::fail("Bounce cancelled.");
    auto bytes=f.getSize(),modified=f.getLastModificationTime().toMilliseconds();
    if(!f.existsAsFile()||bytes<=0||bytes>512ll*1024*1024)return Result::fail("Bounce source is missing or exceeds the 512 MiB file limit: "+f.getFileName());
    FileInputStream stream(f);if(!stream.openedOk())return Result::fail("Cannot read bounce source: "+f.getFileName());
    auto hash=SHA256(stream).toHexString();if(cancel.load())return Result::fail("Bounce cancelled.");
    if(stream.getStatus().failed()||stream.getPosition()!=bytes||f.getSize()!=bytes||f.getLastModificationTime().toMilliseconds()!=modified)return Result::fail("Bounce source changed while reading: "+f.getFileName());
    d={f,bytes,modified,hash};return Result::ok();
}
}
void BounceTask::run(){
    auto work=[&]()->Result{
        if(directory.exists())return Result::fail("Bounce requires a new private output folder.");
        if(directory.createDirectory().failed())return Result::fail("Cannot create local bounce folder.");
        ownsDirectory=true;
        uint64_t bytes=0;for(const auto& asset:projectAssets(plan.source,File())){if(asset.file==File())continue;if(asset.file.getSize()>0&&(uint64_t)asset.file.getSize()>512ull*1024*1024-bytes)return Result::fail("Bounce source files exceed the 512 MiB read budget.");BounceDependency dep;auto r=fingerprint(asset.file,dep,cancel);if(r.failed())return r;bytes+=(uint64_t)dep.bytes;if(bytes>512ull*1024*1024)return Result::fail("Bounce source files exceed the 512 MiB read budget.");dependencies.push_back(std::move(dep));}
        audioFile=directory.getChildFile("source.wav");Assets assets;auto r=renderAudio(plan.source,assets,File(),audioFile,plan.settings,-1,&cancel,&progress);if(r.failed())return r;
        for(const auto& dep:dependencies){BounceDependency after;r=fingerprint(dep.file,after,cancel);if(r.failed())return r;if(after.hash!=dep.hash||after.bytes!=dep.bytes||after.modified!=dep.modified)return Result::fail("Source audio changed during the bounce. Render again.");}
        BounceDependency output;r=fingerprint(audioFile,output,cancel);if(r.failed())return r;audioHash=output.hash;audioBytes=output.bytes;audioModified=output.modified;
        AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(audioFile));RenderPlan expected;r=planRender(plan.source,plan.settings,expected);
        if(r.failed()||!reader||!reader->usesFloatingPointData||reader->bitsPerSample!=32||reader->numChannels!=2||reader->sampleRate!=48000||reader->lengthInSamples!=expected.outputFrames)return Result::fail("Bounce output failed its format/length read-back check.");
        auto* report=new DynamicObject();report->setProperty("schema",1);report->setProperty("kind","Pulse Studio channel source bounce");report->setProperty("track",plan.track);report->setProperty("channel",plan.source.tracks[(size_t)plan.track].name);report->setProperty("lengthBeats",plan.length);report->setProperty("tailSeconds",plan.settings.tailSeconds);report->setProperty("sha256",audioHash);Array<var> sources;
        for(const auto& dep:dependencies){auto* d=new DynamicObject();d->setProperty("path",dep.file.getFullPathName());d->setProperty("bytes",dep.bytes);d->setProperty("modified",dep.modified);d->setProperty("sha256",dep.hash);sources.add(var(d));}report->setProperty("sources",sources);
        auto json=JSON::toString(var(report),true);auto file=directory.getChildFile("bounce.json");if(!file.replaceWithText(json)||file.loadFileAsString()!=json)return Result::fail("Cannot verify local bounce metadata.");return Result::ok();
    };
    try{result=work();}catch(const std::exception& e){result=Result::fail(String("Bounce failed: ")+e.what());}catch(...){result=Result::fail("Bounce failed; no song changes were made.");}
    if(ownsDirectory&&(result.failed()||cancel.load())){if(audioFile!=File())audioFile.deleteFile();directory.getChildFile("bounce.json").deleteFile();directory.deleteFile();}
    done.store(true,std::memory_order_release);
}
}
