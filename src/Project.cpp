// SPDX-License-Identifier: AGPL-3.0-only
#include "Project.h"
#include "EffectPresets.h"
#include "SamplerTuning.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <set>

namespace pulse {
using namespace juce;
String instrumentName(Instrument i) {
    static const char* names[] = {"Analog kick", "Noise snare", "Closed hat", "Clap", "Open hat", "Mono bass", "Warm pad", "Pluck lead", "Sampler", "Pulse Analog", "Pulse FM", "Pulse Wavetable", "Pulse Additive", "Pulse Resonator"};
    return names[jlimit(0, instrumentCount-1, (int)i)];
}
String effectName(int i,bool compact){static const char* names[]={"Equalizer","Compressor","Stereo reverb","Stereo chorus","Stereo flanger","Stereo phaser"};static const char* shortNames[]={"EQ","Comp","Reverb","Chorus","Flanger","Phaser"};return i>=0&&i<builtinEffectCount?String(compact?shortNames[i]:names[i]):String("Unknown effect");}
Result validateEffectOrder(const EffectOrder& order){std::array<bool,builtinEffectCount> seen{};for(int i:order){if(i<0||i>=builtinEffectCount||seen[(size_t)i])return Result::fail("Effect chain must contain each of the six built-in effects exactly once.");seen[(size_t)i]=true;}return Result::ok();}
var effectOrderToJson(const EffectOrder& order){Array<var> array;for(int i:order)array.add(i);return var(array);}
Result effectOrderFromJson(const var& value,EffectOrder& out){auto* array=value.getArray();if(!array||array->size()!=builtinEffectCount)return Result::fail("Invalid effect chain order.");EffectOrder order;for(int i=0;i<builtinEffectCount;++i){const auto& v=(*array)[i];double n=(double)v;if(!(v.isInt()||v.isInt64()||v.isDouble())||!std::isfinite(n)||n<0||n>=builtinEffectCount||n!=std::floor(n))return Result::fail("Invalid effect identity in chain.");order[(size_t)i]=(int)n;}auto valid=validateEffectOrder(order);if(valid.wasOk())out=order;return valid;}
bool isSynth(Instrument i){return i==Instrument::analog||i==Instrument::fm||i==Instrument::wavetable||i==Instrument::additive||i==Instrument::physical;}
bool isDrum(Instrument i) { return (int)i <= 4; }
String patternColourName(int index){static const char* names[]={"Automatic","Green","Sage","Teal","Sky","Blue","Violet","Lavender","Rose","Coral","Orange","Sand","Grey"};return index>=0&&index<patternColourCount?names[index]:"Unknown";}
uint32_t patternColourARGB(int index){static constexpr uint32_t colours[]={0,0xffb2c98d,0xffa3bd9b,0xff83c0b7,0xff8abed4,0xff8ea9d2,0xffa69ad0,0xffc2a8cc,0xffd2a0b6,0xffd69e93,0xffd5b084,0xffc9bf9b,0xffadb9bf};return index>=0&&index<patternColourCount?colours[index]:0;}
uint32_t automaticPatternColourARGB(int index){
    static constexpr uint32_t colours[]={0xffb2c98d,0xffe082d2,0xff6accbe,0xffe0bd90,0xff8e76cc,0xff77e075,0xffcc8399,0xff82bbe0,0xffc2cc6a,0xffd190e0,0xff76cca3,0xffe08d75,0xff8387cc,0xffa3e082,0xffcc6aa9,0xff90dbe0,0xffccb876,0xffa875e0,0xff83cc90,0xffe0828c,0xff6a91cc,0xffc7e090,0xffcc76ca,0xff75e0c3,0xffcca383,0xff9082e0,0xff78cc6a,0xffe090b3,0xff76b5cc,0xffe0de75,0xffb583cc,0xff82e0a7};
    return colours[jmax(0,index)%32];
}
void ensurePatternEditorIds(Project& p){std::set<String> seen;for(auto& pattern:p.patterns){while(pattern.editorId.isEmpty()||!seen.insert(pattern.editorId).second)pattern.editorId=Uuid().toString();}}
void ensurePlaylistEditorIds(Project& p){std::set<String> tracks,clips;for(auto& track:p.tracks)while(track.editorId.isEmpty()||!tracks.insert(track.editorId).second)track.editorId=Uuid().toString();for(auto& clip:p.clips)while(clip.editorId.isEmpty()||!clips.insert(clip.editorId).second)clip.editorId=Uuid().toString();}
int patternIndexForEditorId(const Project& p,const String& id){if(id.isEmpty())return -1;for(int i=0;i<(int)p.patterns.size();++i)if(p.patterns[(size_t)i].editorId==id)return i;return -1;}
double Project::songLength() const {
    double end = 8;
    for (const auto& c : clips) end = jmax(end, c.beat + c.length);
    for(const auto& m:markers)end=jmax(end,m.isSection()?m.endBeat:m.beat+.25);
    return end;
}
const SongMarker* findSongMarker(const Project& p,const String& id){for(const auto& marker:p.markers)if(marker.id==id)return &marker;return nullptr;}
Result validateSongMarkers(const Project& p){
    if(p.markers.size()>maxSongMarkers)return Result::fail("A song supports up to 128 markers and sections.");std::set<String> ids;
    for(const auto& m:p.markers){if(m.id.isEmpty()||m.id.length()>80||!m.id.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")||!ids.insert(m.id).second||m.name.trim().isEmpty()||m.name.length()>80||!std::isfinite(m.beat)||!std::isfinite(m.endBeat)||m.beat<0||m.beat>4096||(m.endBeat!=-1&&(m.endBeat>4096||m.endBeat-m.beat<.25))||m.colour<0||m.colour>7)return Result::fail("Invalid or duplicate song marker, range, name or colour.");}
    if(p.loopSection.isNotEmpty()){auto* m=findSongMarker(p,p.loopSection);if(!m||!m->isSection())return Result::fail("The loop section must reference an existing section range.");}return Result::ok();
}
Result storeSongMarker(Project& p,const SongMarker& marker){
    Project next;next.markers=p.markers;next.loopSection=p.loopSection;auto found=std::find_if(next.markers.begin(),next.markers.end(),[&](const SongMarker& m){return m.id==marker.id;});if(found==next.markers.end())next.markers.push_back(marker);else *found=marker;
    if(next.loopSection==marker.id&&!marker.isSection())next.loopSection.clear();auto result=validateSongMarkers(next);if(result.failed())return result;
    std::sort(next.markers.begin(),next.markers.end(),[](const SongMarker& a,const SongMarker& b){return a.beat==b.beat?a.id<b.id:a.beat<b.beat;});p.markers=std::move(next.markers);p.loopSection=next.loopSection;return Result::ok();
}
Result removeSongMarker(Project& p,const String& id){auto found=std::find_if(p.markers.begin(),p.markers.end(),[&](const SongMarker& m){return m.id==id;});if(found==p.markers.end())return Result::fail("Song marker no longer exists.");p.markers.erase(found);if(p.loopSection==id)p.loopSection.clear();return Result::ok();}
const SongMarker* adjacentSongMarker(const Project& p,double beat,bool next){const SongMarker* found=nullptr;for(const auto& m:p.markers)if(next?m.beat>beat+1e-6:m.beat<beat-1e-6)if(!found||(next?m.beat<found->beat:m.beat>found->beat))found=&m;return found;}
Project emptyProject() {
    Project p;
    const char* names[] = {"Kick", "Snare", "Closed hat", "Clap", "Open hat", "Bass", "Chords", "Lead"};
    for (int i = 0; i < 8; ++i) {
        Track t;
        t.name = names[i]; t.instrument = (Instrument)i;
        t.volume = i == 2 ? 0.32f : i >= 6 ? 0.28f : 0.65f;
        t.cutoff = i == 5 ? 1400.f : 16000.f;
        t.send = i == 6 ? 0.35f : i == 7 ? 0.18f : 0.f;
        t.pan = i == 2 ? 0.18f : i == 4 ? -0.2f : 0.f;
        p.tracks.push_back(t);
    }
    Pattern a; a.name = "Pattern 01"; a.lanes.resize(p.tracks.size()); p.patterns.push_back(a);
    return p;
}
Project demoProject() {
    Project p = emptyProject(); p.title = "Midnight motion";
    auto& a = p.patterns[0]; a.name = "Main groove";
    auto add = [&](int track, double beat, int pitch, double length, float vel) {
        a.lanes[(size_t)track].push_back({beat, length, pitch, vel});
    };
    for (int s = 0; s < 32; ++s) {
        if (s % 4 == 0) add(0, s * .25, 36, .2, .94f);
        if (s % 8 == 4) { add(1, s * .25, 38, .2, .78f); add(3, s * .25, 39, .2, .6f); }
        if (s % 2 == 0) add(2, s * .25, 42, .12, s % 4 == 0 ? .58f : .78f);
        if (s % 8 == 6) add(4, s * .25, 46, .22, .58f);
    }
    const int bass[] = {45,45,48,45,41,41,43,40};
    for (int i = 0; i < 8; ++i) { add(5, i + .5, bass[i], .42, .85f); add(5, i + .875, bass[i], .11, .55f); }
    const int chords[4][3] = {{57,60,64},{57,60,64},{53,57,60},{55,59,62}};
    for (int i = 0; i < 4; ++i) for (int n : chords[i]) add(6, i * 2, n, 1.75, .65f);
    const int lead[] = {76,72,69,72,77,76,74,71};
    for (int i = 0; i < 8; ++i) add(7, i + .25, lead[i], .28, .68f);
    Pattern intro = a; intro.name = "Intro";
    intro.lanes[1].clear(); intro.lanes[3].clear(); intro.lanes[5].clear(); intro.lanes[7].clear();
    Pattern variation = a; variation.name = "Variation";
    variation.lanes[0].push_back({7.5,.15,36,.65f});
    variation.lanes[1].push_back({7.75,.15,38,.5f});
    for (auto& n : variation.lanes[7]) n.pitch += n.beat >= 4 ? 5 : 0;
    Pattern breakPattern = a; breakPattern.name = "Breakdown";
    breakPattern.lanes[0].clear(); breakPattern.lanes[1].clear(); breakPattern.lanes[3].clear();
    p.patterns.push_back(intro); p.patterns.push_back(variation); p.patterns.push_back(breakPattern);
    for (int section = 0; section < 4; ++section) {
        int pat = section == 0 ? 1 : section == 2 ? 3 : section == 3 ? 2 : 0;
        for (int t = 0; t < (int)p.tracks.size(); ++t)
            if (!p.patterns[(size_t)pat].lanes[(size_t)t].empty()) p.clips.push_back({t,pat,section * 8.0,8});
    }
    return p;
}
Result validateRouting(const Project& p,std::array<int,maxTracks>* result){
    const int count=(int)p.tracks.size();if(count<1||count>maxTracks)return Result::fail("Invalid mixer track count.");
    std::array<int,maxTracks> incoming{},order{};std::array<bool,maxTracks> used{};
    for(int i=0;i<count;++i){auto& m=p.tracks[(size_t)i].mixer;
        if(m.output<-1||m.output>=count||m.output==i||m.sidechain<-1||m.sidechain>=count||m.sidechain==i)return Result::fail("Invalid mixer output or sidechain source.");
        if(m.output>=0)++incoming[(size_t)m.output];}
    for(int n=0;n<count;++n){int found=-1;for(int i=0;i<count;++i)if(!used[(size_t)i]&&incoming[(size_t)i]==0){found=i;break;}
        if(found<0)return Result::fail("Mixer routing cannot contain feedback cycles.");
        order[(size_t)n]=found;used[(size_t)found]=true;int output=p.tracks[(size_t)found].mixer.output;if(output>=0)--incoming[(size_t)output];}
    if(result)*result=order;return Result::ok();
}
String automationName(AutomationTarget target){
    const char* names[]={"Volume","Pan","Tone filter","Saturation","Delay send","Master volume"};
    return names[jlimit(0,5,(int)target)];
}
static var object() { return var(new DynamicObject()); }
static void put(var& o, const char* name, const var& value) { o.getDynamicObject()->setProperty(name, value); }
var projectToJson(const Project& p) {
    var root = object(); put(root,"schema",projectSchema);var rec=object();put(rec,"countInBars",p.recording.countInBars);put(rec,"preRollBars",p.recording.preRollBars);put(rec,"punch",p.recording.punch);put(rec,"punchStart",p.recording.punchStart);put(rec,"punchEnd",p.recording.punchEnd);put(root,"recording",rec); put(root,"application","Pulse Studio"); put(root,"title",p.title);
    Array<var> markers;for(const auto& m:p.markers){auto row=object();put(row,"id",m.id);put(row,"name",m.name);put(row,"beat",m.beat);put(row,"endBeat",m.endBeat);put(row,"colour",m.colour);markers.add(row);}put(root,"markers",markers);put(root,"loopSection",p.loopSection);
    put(root,"bpm",p.bpm); put(root,"swing",p.swing); put(root,"master",p.master); put(root,"delayFeedback",p.delayFeedback); put(root,"delayReturn",p.delayReturn); put(root,"masterSoftClip",p.masterSoftClip); put(root,"masterEffects",masterEffectsToJson(p.masterMixer));
    put(root,"songMode",p.songMode); put(root,"loop",p.loop); put(root,"metronome",p.metronome); put(root,"activePattern",p.activePattern); put(root,"notes",p.notes);
    Array<var> tracks;
    for (const auto& t : p.tracks) {
        var v = object(); put(v,"name",t.name); put(v,"instrument",(int)t.instrument); put(v,"volume",t.volume); put(v,"pan",t.pan);
        put(v,"cutoff",t.cutoff); put(v,"drive",t.drive); put(v,"send",t.send); put(v,"attack",t.attack); put(v,"release",t.release);
        put(v,"mute",t.mute); put(v,"solo",t.solo); put(v,"samplePath",t.samplePath);put(v,"sampleStart",t.sampleStart);put(v,"sampleEnd",t.sampleEnd);put(v,"sampleLoop",t.sampleLoop);put(v,"sampleRoot",t.sampleRoot);put(v,"sampleTune",t.sampleTune);put(v,"tuning",microTuningToJson(t.tuning));put(v,"isBus",t.isBus);var m=object();const auto& fx=t.mixer;
        put(m,"dynamics",dynamicsToJson(fx.dynamics));put(m,"stereo",stereoToJson(fx.stereo));put(m,"order",effectOrderToJson(fx.order));put(m,"flanger",modulationToJson(fx.flanger));put(m,"phaser",modulationToJson(fx.phaser));put(m,"chorus",chorusToJson(fx.chorus));put(m,"output",fx.output);put(m,"sidechain",fx.sidechain);put(m,"eq",fx.eq);put(m,"compressor",fx.compressor);put(m,"reverb",fx.reverb);
        put(m,"low",fx.low);put(m,"mid",fx.mid);put(m,"high",fx.high);put(m,"threshold",fx.threshold);put(m,"ratio",fx.ratio);put(m,"attack",fx.compAttack);put(m,"release",fx.compRelease);put(m,"makeup",fx.makeup);put(m,"mix",fx.reverbMix);put(m,"room",fx.room);put(m,"damping",fx.damping);put(v,"mixer",m);put(v,"synth",synthPatchToJson(t.synth));put(v,"soundName",t.soundName);tracks.add(v);
    } put(root,"tracks",tracks);
    Array<var> patterns;
    for (const auto& pat : p.patterns) {
        var v = object(); put(v,"name",pat.name); put(v,"length",pat.length); put(v,"colour",pat.colour); Array<var> lanes;
        for (const auto& lane : pat.lanes) {
            Array<var> notes;
            for (const auto& n : lane) {
                var x = object(); put(x,"beat",n.beat); put(x,"length",n.length); put(x,"pitch",n.pitch);
                put(x,"velocity",n.velocity); put(x,"probability",n.probability); put(x,"ratchets",n.ratchets); put(x,"conditionEvery",n.conditionEvery); put(x,"conditionStep",n.conditionStep); notes.add(x);
            } lanes.add(var(notes));
        } put(v,"lanes",lanes); patterns.add(v);
    } put(root,"patterns",patterns);
    Array<var> clips;
    for (const auto& c : p.clips) { var v = object(); put(v,"track",c.track); put(v,"pattern",c.pattern); put(v,"beat",c.beat); put(v,"length",c.length); put(v,"audioPath",c.audioPath); put(v,"sourceOffset",c.sourceOffset); put(v,"gain",c.gain); put(v,"fadeIn",c.fadeIn); put(v,"fadeOut",c.fadeOut); put(v,"fadeInCurve",(int)c.fadeInCurve); put(v,"fadeOutCurve",(int)c.fadeOutCurve); put(v,"automationTarget",c.automationTarget); put(v,"automationTrack",c.automationTrack); Array<var> points; for(const auto& point:c.points){var x=object();put(x,"beat",point.beat);put(x,"value",point.value);put(x,"hold",point.hold);put(x,"curve",(int)point.curve);put(x,"tension",point.tension);points.add(x);} put(v,"points",points); clips.add(v); }
    put(root,"clips",clips); return root;
}
static double value(const var& v, const char* key, double fallback) {
    if (!v.hasProperty(key)) return fallback;
    const auto& x = v[key];
    if (!(x.isInt() || x.isInt64() || x.isDouble())) return fallback;
    double d = (double)x; return std::isfinite(d) ? d : fallback;
}
Result projectFromJson(const var& root, Project& out) {
    if (!root.isObject() || ((int)root["schema"] < 1 || (int)root["schema"] > projectSchema)) return Result::fail("Unsupported or invalid project schema.");
    auto* tracks = root["tracks"].getArray(); auto* pats = root["patterns"].getArray();
    if (!tracks || tracks->isEmpty() || tracks->size() > maxTracks || !pats || pats->isEmpty() || pats->size() > maxPatterns)
        return Result::fail("Project must have 1-16 tracks and 1-32 patterns.");
    Project p; p.title = root["title"].toString().substring(0,200);
    p.bpm = jlimit(30.,300.,value(root,"bpm",118)); p.swing = (float)jlimit(0.,.6,value(root,"swing",0));
    p.master = (float)jlimit(0.,1.,value(root,"master",.8)); p.delayFeedback = (float)jlimit(0.,.8,value(root,"delayFeedback",.3));
    // Before schema 19 the master always used tanh saturation and a fixed 50% delay return.
    // Preserve that exact signal path when an older song is opened.
    if((int)root["schema"]<19){p.masterSoftClip=true;p.delayReturn=.5f;}
    else{const auto& wet=root["delayReturn"];if(!(wet.isInt()||wet.isInt64()||wet.isDouble())||!std::isfinite((double)wet)||(double)wet<0||(double)wet>1||!root["masterSoftClip"].isBool())return Result::fail("Invalid master processing settings.");p.delayReturn=(float)(double)wet;p.masterSoftClip=(bool)root["masterSoftClip"];}
    if((int)root["schema"]>=20){auto valid=masterEffectsFromJson(root["masterEffects"],p.masterMixer);if(valid.failed())return valid;if((int)root["schema"]<22&&p.masterMixer.dynamics!=DynamicsOptions{})return Result::fail("Extended Master dynamics require schema 22.");if((int)root["schema"]>=22&&(int)root["masterEffects"]["chain"]["schema"]<2)return Result::fail("Missing complete Master dynamics state.");}
    p.songMode = (bool)root["songMode"]; p.loop = root.hasProperty("loop") ? (bool)root["loop"] : true;
    p.metronome = (bool)root["metronome"]; p.notes = root["notes"].toString();
    if(root.hasProperty("recording")){auto r=root["recording"];if(!r.isObject())return Result::fail("Invalid recording settings.");p.recording.countInBars=jlimit(0,2,(int)value(r,"countInBars",0));p.recording.preRollBars=jlimit(0,2,(int)value(r,"preRollBars",1));p.recording.punch=(bool)r["punch"];p.recording.punchStart=value(r,"punchStart",0);p.recording.punchEnd=value(r,"punchEnd",8);if(p.recording.punchStart<0||p.recording.punchEnd>4096||p.recording.punchEnd-p.recording.punchStart<.25)return Result::fail("Punch range must span at least 1/4 beat inside 0-4096 beats.");}
    p.activePattern = jlimit(0,pats->size()-1,(int)root["activePattern"]);
    p.loopSection=root["loopSection"].toString();if(root.hasProperty("markers")){auto* markers=root["markers"].getArray();if(!markers||markers->size()>maxSongMarkers)return Result::fail("Invalid song marker list.");for(const auto& row:*markers){if(!row.isObject())return Result::fail("Invalid song marker.");SongMarker m;m.id=row["id"].toString();m.name=row["name"].toString();m.beat=value(row,"beat",-1);m.colour=(int)value(row,"colour",0);if(row.hasProperty("endBeat")){const auto& end=row["endBeat"];if(!(end.isInt()||end.isInt64()||end.isDouble()))return Result::fail("Invalid section end.");m.endBeat=(double)end;}p.markers.push_back(m);}}
    auto markersValid=validateSongMarkers(p);if(markersValid.failed())return markersValid;std::sort(p.markers.begin(),p.markers.end(),[](const SongMarker& a,const SongMarker& b){return a.beat==b.beat?a.id<b.id:a.beat<b.beat;});
    for (const auto& v : *tracks) {
        if (!v.isObject()) return Result::fail("Invalid track object.");
        if((int)root["schema"]>=17&&(!(v["instrument"].isInt()||v["instrument"].isInt64())||(int64)v["instrument"]<0||(int64)v["instrument"]>=instrumentCount))return Result::fail("Invalid instrument identity.");
        if((int)root["schema"]<17&&(int)v["instrument"]==(int)Instrument::additive)return Result::fail("Additive instruments require project schema 17.");
        if((int)root["schema"]<13&&(int)v["instrument"]==(int)Instrument::wavetable)return Result::fail("Wavetable instruments require project schema 13.");
        if((int)root["schema"]<24&&(int)v["instrument"]==(int)Instrument::physical)return Result::fail("Physical instruments require project schema 24.");
        Track t;
        if(v.hasProperty("tuning")){auto valid=microTuningFromJson(v["tuning"],t.tuning);if(valid.failed())return valid;if((int)root["schema"]<23&&t.tuning!=MicroTuning{})return Result::fail("Microtonal tuning requires project schema 23.");}
        else if((int)root["schema"]>=23)return Result::fail("Channel is missing its complete tuning table.");
        t.name = v["name"].toString().substring(0,100); t.instrument = (Instrument)jlimit(0,(int)root["schema"]<8?8:(int)root["schema"]<13?10:(int)root["schema"]<17?11:instrumentCount-1,(int)v["instrument"]);
        t.volume = (float)jlimit(0.,1.5,value(v,"volume",.7)); t.pan = (float)jlimit(-1.,1.,value(v,"pan",0));
        t.cutoff = (float)jlimit(60.,20000.,value(v,"cutoff",16000)); t.drive = (float)jlimit(0.,1.,value(v,"drive",0));
        t.send = (float)jlimit(0.,.8,value(v,"send",0)); t.attack = (float)jlimit(.001,2.,value(v,"attack",.005));
        t.release = (float)jlimit(.02,3.,value(v,"release",.18)); t.mute = (bool)v["mute"]; t.solo = (bool)v["solo"];
        t.samplePath = v["samplePath"].toString();t.sampleStart=jlimit(0.,(int)root["schema"]>=25?maxAudioSeconds:120.,value(v,"sampleStart",0));t.sampleEnd=jlimit(0.,(int)root["schema"]>=25?maxAudioSeconds:120.,value(v,"sampleEnd",0));t.sampleLoop=(bool)v["sampleLoop"];if(t.sampleEnd>0&&t.sampleEnd<=t.sampleStart)return Result::fail("Invalid sampler selection.");t.isBus=(bool)v["isBus"];
        if((int)root["schema"]>=15){auto note=v["sampleRoot"],tune=v["sampleTune"];auto numeric=[](const var& x){return x.isInt()||x.isInt64()||x.isDouble();};if(!numeric(note)||!numeric(tune)||!std::isfinite((double)note)||(double)note!=std::floor((double)note)||(double)note<0||(double)note>127)return Result::fail("Invalid sampler root or fine tune.");t.sampleRoot=(int)note;t.sampleTune=(double)tune;auto valid=validateSamplerTuning(t.sampleRoot,t.sampleTune);if(valid.failed())return valid;}
        if(v.hasProperty("mixer")){auto m=v["mixer"];if(!m.isObject())return Result::fail("Invalid mixer settings.");auto& fx=t.mixer;
            if(m.hasProperty("dynamics")){auto result=dynamicsFromJson(m["dynamics"],fx.dynamics);if(result.failed())return result;if((int)root["schema"]<22&&fx.dynamics!=DynamicsOptions{})return Result::fail("Extended dynamics require schema 22.");}else if((int)root["schema"]>=22)return Result::fail("Missing dynamics options.");
            if(m.hasProperty("stereo")){auto result=stereoFromJson(m["stereo"],fx.stereo);if(result.failed())return result;}else if((int)root["schema"]>=12)return Result::fail("Mixer is missing its stereo settings.");
            if(m.hasProperty("order")){auto result=effectOrderFromJson(m["order"],fx.order);if(result.failed())return result;}else if((int)root["schema"]>=11)return Result::fail("Mixer is missing its effect chain order.");
            if(m.hasProperty("chorus")){auto result=chorusFromJson(m["chorus"],fx.chorus);if(result.failed())return result;}else if((int)root["schema"]>=9)return Result::fail("Mixer is missing its chorus settings.");
            for(auto kind:{ModulationKind::flanger,ModulationKind::phaser}){auto key=kind==ModulationKind::flanger?"flanger":"phaser";auto& effect=kind==ModulationKind::flanger?fx.flanger:fx.phaser;if(m.hasProperty(key)){auto result=modulationFromJson(m[key],kind,effect);if(result.failed())return result;}else if((int)root["schema"]>=10)return Result::fail("Mixer is missing its modulation settings.");}
            fx.output=(int)value(m,"output",-1);fx.sidechain=(int)value(m,"sidechain",-1);fx.eq=(bool)m["eq"];fx.compressor=(bool)m["compressor"];fx.reverb=(bool)m["reverb"];
            fx.low=(float)jlimit(-18.,18.,value(m,"low",0));fx.mid=(float)jlimit(-18.,18.,value(m,"mid",0));fx.high=(float)jlimit(-18.,18.,value(m,"high",0));
            fx.threshold=(float)jlimit(-60.,0.,value(m,"threshold",-18));fx.ratio=(float)jlimit(1.,20.,value(m,"ratio",4));fx.compAttack=(float)jlimit(.1,100.,value(m,"attack",10));fx.compRelease=(float)jlimit(5.,1000.,value(m,"release",120));fx.makeup=(float)jlimit(-12.,24.,value(m,"makeup",0));
            fx.reverbMix=(float)jlimit(0.,1.,value(m,"mix",.15));fx.room=(float)jlimit(0.,1.,value(m,"room",.5));fx.damping=(float)jlimit(0.,1.,value(m,"damping",.5));}
        else if((int)root["schema"]>=10)return Result::fail("Track is missing mixer settings.");
        if(v.hasProperty("synth")){auto result=synthPatchFromJson(v["synth"],t.synth,(int)root["schema"]>=13,(int)root["schema"]>=17,(int)root["schema"]>=24);if(result.failed())return result;if((int)root["schema"]<24&&t.synth.physical!=PhysicalSettings{})return Result::fail("Physical settings require project schema 24.");if((int)root["schema"]<17&&t.synth.harmonics!=defaultHarmonics)return Result::fail("Custom harmonics require project schema 17.");}
        else if((int)root["schema"]>=8&&isSynth(t.instrument))return Result::fail("Synth channel is missing its patch state.");
        t.soundName=v["soundName"].toString().substring(0,80);p.tracks.push_back(t);
    }
    for (const auto& v : *pats) {
        Pattern pat; pat.name = v["name"].toString().substring(0,100); pat.length = jlimit(1.,64.,value(v,"length",8));
        if(v.hasProperty("colour")){if(!v["colour"].isInt()||(int)v["colour"]<0||(int)v["colour"]>=patternColourCount)return Result::fail("Invalid pattern colour.");pat.colour=(int)v["colour"];}
        else if((int)root["schema"]>=14)return Result::fail("Pattern is missing its colour setting.");
        auto* lanes = v["lanes"].getArray(); if (!lanes || lanes->size() != tracks->size()) return Result::fail("Pattern lanes do not match tracks.");
        for (const auto& lane : *lanes) {
            auto* notes = lane.getArray(); if (!notes || notes->size() > maxNotesPerLane) return Result::fail("Invalid or oversized note lane.");
            std::vector<Note> ns;
            for (const auto& x : *notes) {
                if (!x.isObject()) return Result::fail("Invalid note object.");
                Note n; n.beat = jlimit(0.,pat.length-.000001,value(x,"beat",0)); n.length = jlimit(.01,64.,value(x,"length",.25));
                n.pitch = jlimit(0,127,(int)x["pitch"]); n.velocity = (float)jlimit(0.,1.,value(x,"velocity",.8));
                n.probability = (float)jlimit(0.,1.,value(x,"probability",1)); n.ratchets = jlimit(1,8,(int)value(x,"ratchets",1));
                for(auto field:{"conditionEvery","conditionStep"}){if(!x.hasProperty(field)){if((int)root["schema"]>=21)return Result::fail("Note is missing its repeat condition.");continue;}const auto& c=x[field];if(!(c.isInt()||c.isInt64()||c.isDouble())||!std::isfinite((double)c)||(double)c!=std::floor((double)c)||(double)c<1||(double)c>16)return Result::fail("Invalid note repeat condition.");if(String(field)=="conditionEvery")n.conditionEvery=(int)c;else n.conditionStep=(int)c;}
                if(!validNoteCondition(n))return Result::fail("Note condition pass exceeds its cycle length.");
                if((int)root["schema"]<21&&(n.conditionEvery!=1||n.conditionStep!=1))return Result::fail("Conditional notes require project schema 21.");ns.push_back(n);
            } pat.lanes.push_back(std::move(ns));
        } p.patterns.push_back(std::move(pat));
    }
    if(root.hasProperty("clips")&&!root["clips"].isArray())return Result::fail("Invalid clip array.");
    int automationCount=0,totalPoints=0;
    if (auto* clips = root["clips"].getArray()) {
        if (clips->size() > maxClips) return Result::fail("Too many arrangement clips.");
        for (const auto& v : *clips) {
            if(!v.isObject())return Result::fail("Invalid arrangement clip object.");
            Clip c; c.track = (int)v["track"]; c.pattern = (int)v["pattern"]; c.beat = value(v,"beat",0); c.length = value(v,"length",8);
            c.audioPath=v["audioPath"].toString(); c.sourceOffset=value(v,"sourceOffset",0); c.gain=(float)jlimit(0.,2.,value(v,"gain",1)); c.fadeIn=jlimit(0.,c.length,value(v,"fadeIn",0)); c.fadeOut=jlimit(0.,c.length,value(v,"fadeOut",0));
            for(auto field:{"fadeInCurve","fadeOutCurve"}){auto x=v[field];if(!v.hasProperty(field)){if((int)root["schema"]>=16)return Result::fail("Audio fade curve fields are missing.");continue;}if(!(x.isInt()||x.isInt64())||(int64)x<0||(int64)x>2)return Result::fail("Invalid audio fade curve (0 Linear, 1 Equal power, 2 S-curve).");if((int)root["schema"]<16&&(int)x!=0)return Result::fail("Nonlinear audio fades require project schema 16.");if(String(field)=="fadeInCurve")c.fadeInCurve=(FadeCurve)(int)x;else c.fadeOutCurve=(FadeCurve)(int)x;}
            if(!c.isAudio()&&(c.fadeInCurve!=FadeCurve::linear||c.fadeOutCurve!=FadeCurve::linear))return Result::fail("Nonlinear fades require an audio clip.");
            c.automationTarget=(int)value(v,"automationTarget",-1);c.automationTrack=(int)value(v,"automationTrack",-1);
            if(c.automationTarget<-1||c.automationTarget>5||c.length<=0||c.length>((int)root["schema"]>=25?maxClipLength(c):600.))return Result::fail("Invalid automation target or clip length.");
            if(c.isAutomation()){
                auto* points=v["points"].getArray();
                if((c.automationTarget!=5&&(c.automationTrack<0||c.automationTrack>=tracks->size()))||c.isAudio()||!points||points->size()<2||points->size()>maxAutomationPoints||++automationCount>maxAutomationClips||(totalPoints+=points->size())>maxTotalAutomationPoints)return Result::fail("Invalid or oversized automation clip.");
                double previous=-1;
                for(const auto& x:*points){double beat=value(x,"beat",-1),val=value(x,"value",-1);
                    if(!x.isObject()||beat<0||beat>600||beat<=previous||val<0||val>1)return Result::fail("Automation points must have increasing beats and values from 0 to 1.");
                    AutomationPoint point{beat,(float)val,(bool)x["hold"]};
                    bool modern=(int)root["schema"]>=18;
                    if(modern||x.hasProperty("curve")||x.hasProperty("tension")){
                        auto curve=x["curve"],tension=x["tension"];
                        if(!(curve.isInt()||curve.isInt64())||(int64)curve<0||(int64)curve>3||!(tension.isInt()||tension.isInt64()||tension.isDouble())||!std::isfinite((double)tension)||(double)tension< -1||(double)tension>1||!x["hold"].isBool())return Result::fail("Invalid automation curve or tension.");
                        point.curve=(AutomationCurve)(int)curve;point.tension=(float)(double)tension;
                        if(!modern&&(point.curve!=AutomationCurve::linear||point.tension!=0))return Result::fail("Shaped automation requires project schema 18.");
                    }
                    c.points.push_back(point);previous=beat;}
            }
            if(c.sourceOffset<0||c.sourceOffset>((int)root["schema"]>=25?maxAudioSeconds:120.))return Result::fail("Invalid audio source offset.");
            if (c.track < 0 || c.track >= tracks->size() || (!c.isAudio() && !c.isAutomation() && (c.pattern < 0 || c.pattern >= pats->size())) || c.beat < 0 || c.beat > 4096 || c.length <= 0 || c.length > ((int)root["schema"]>=25?maxClipLength(c):600.))
                return Result::fail("Invalid arrangement clip.");
            p.clips.push_back(c);
        }
    }
    auto routing=validateRouting(p);if(routing.failed())return routing;
    out = std::move(p); return Result::ok();
}
Project absoluteAssets(const Project& p,const File& base) {
    auto copy=p;
    for(auto& t:copy.tracks)if(t.samplePath.isNotEmpty()&&!File::isAbsolutePath(t.samplePath))t.samplePath=base.getChildFile(t.samplePath).getFullPathName();
    for(auto& c:copy.clips)if(c.isAudio()&&!File::isAbsolutePath(c.audioPath))c.audioPath=base.getChildFile(c.audioPath).getFullPathName();
    return copy;
}
Result collectAssets(const Project& p,const File& base,const File& target,Project& collected) {
    auto copy=absoluteAssets(p,base);
    auto folder=target.getParentDirectory().getChildFile("Assets");
    std::vector<String*> paths;for(auto& t:copy.tracks)if(t.samplePath.isNotEmpty())paths.push_back(&t.samplePath);for(auto& c:copy.clips)if(c.isAudio())paths.push_back(&c.audioPath);
    for(auto* path:paths) {
        File source(*path);if(!source.existsAsFile())return Result::fail("Missing sample: "+source.getFileName());
        auto result=folder.createDirectory();if(result.failed())return result;
        auto hash=SHA256(source).toHexString();auto destination=folder.getChildFile(hash+source.getFileExtension().toLowerCase());
        if(!destination.existsAsFile()) {
            TemporaryFile staging(destination);
            if(!source.copyFileTo(staging.getFile())||!staging.overwriteTargetFileWithTemporary())return Result::fail("Could not collect sample: "+source.getFileName());
        }
        if(SHA256(destination).toHexString()!=hash)return Result::fail("Collected sample checksum does not match.");
        *path=destination.getRelativePathFrom(target.getParentDirectory());
    }
    collected=std::move(copy);return Result::ok();
}
Result writeMidi(const Project& p, const File& file) {
    for(const auto& pat:p.patterns)for(const auto& lane:pat.lanes)for(const auto& n:lane)if(!validNoteCondition(n))return Result::fail("Invalid note repeat condition.");
    MidiFile midi; midi.setTicksPerQuarterNote(960);
    MidiMessageSequence meta;
    auto tempo = MidiMessage::tempoMetaEvent((int)(60000000.0/p.bpm)); tempo.setTimeStamp(0); meta.addEvent(tempo);
    auto meter = MidiMessage::timeSignatureMetaEvent(4,4); meta.addEvent(meter); midi.addTrack(meta);
    for (size_t t = 0; t < p.tracks.size(); ++t) {
        MidiMessageSequence seq; auto title = MidiMessage::textMetaEvent(3,p.tracks[t].name); seq.addEvent(title);
        auto emit = [&](const Note& n, double offset) {
            int channel = isDrum(p.tracks[t].instrument) ? 10 : (int)t + 1;
            auto on = MidiMessage::noteOn(channel,n.pitch,n.velocity); on.setTimeStamp((offset+n.beat)*960); seq.addEvent(on);
            auto off = MidiMessage::noteOff(channel,n.pitch); off.setTimeStamp((offset+n.beat+n.length)*960); seq.addEvent(off);
        };
        if (!p.songMode) { for (const auto& n : p.patterns[(size_t)p.activePattern].lanes[t]) if(noteConditionMatches(n.conditionEvery,n.conditionStep,0))emit(n,0); }
        else for (const auto& c : p.clips) if (c.track == (int)t && !c.isAudio() && !c.isAutomation()) {
            const auto& pat = p.patterns[(size_t)c.pattern];
            for (double off = 0; off < c.length; off += pat.length) for (const auto& n : pat.lanes[t]) if (off+n.beat < c.length&&noteConditionMatches(n.conditionEvery,n.conditionStep,(uint64_t)std::floor((c.beat+off)/pat.length+1.e-9))) emit(n,c.beat+off);
        }
        seq.updateMatchedPairs(); midi.addTrack(seq);
    }
    TemporaryFile temp(file); auto stream = temp.getFile().createOutputStream();
    if (!stream || !midi.writeTo(*stream)) return Result::fail("Could not write MIDI.");
    stream.reset(); return temp.overwriteTargetFileWithTemporary() ? Result::ok() : Result::fail("Could not commit MIDI.");
}
Result importMidi(const File& file, Project& p) {
    if (file.getSize() > 16 * 1024 * 1024) return Result::fail("MIDI file too large.");
    auto input = file.createInputStream(); MidiFile midi;
    if (!input || !midi.readFrom(*input)) return Result::fail("Invalid MIDI file.");
    if (midi.getTimeFormat() <= 0) return Result::fail("Time-based MIDI is not supported by this build; use PPQ MIDI.");
    Project next = emptyProject(); next.title = file.getFileNameWithoutExtension(); next.tracks.clear(); next.patterns.clear(); next.clips.clear();
    Pattern pat; pat.name = "Imported MIDI"; double end = 8;
    for (int i = 0; i < midi.getNumTracks(); ++i) {
        auto seq = *midi.getTrack(i); seq.updateMatchedPairs(); std::vector<Note> notes; String name = "MIDI " + String(i+1); bool drums = false;
        for (int e = 0; e < seq.getNumEvents(); ++e) {
            auto* ev = seq.getEventPointer(e); const auto& msg = ev->message;
            if (msg.isTempoMetaEvent()) next.bpm = jlimit(30.,300.,60.0/msg.getTempoSecondsPerQuarterNote());
            if (msg.isTrackNameEvent()) name = msg.getTextFromTextMetaEvent();
            if (msg.isNoteOn() && ev->noteOffObject != nullptr) {
                double start = msg.getTimeStamp()/midi.getTimeFormat();
                double len = (ev->noteOffObject->message.getTimeStamp()-msg.getTimeStamp())/midi.getTimeFormat();
                if (notes.size() >= maxNotesPerLane || start >= 64) return Result::fail("MIDI exceeds this build's 64-beat / 512-note-per-track import limit.");
                notes.push_back({start,jmax(.01,len),msg.getNoteNumber(),msg.getFloatVelocity()}); end = jmax(end,start+len); drums |= msg.getChannel()==10;
            }
        }
        if (!notes.empty()) {
            if (next.tracks.size() >= maxTracks) return Result::fail("MIDI exceeds 16 tracks.");
            Track t;
        t.name = name; t.instrument = drums ? Instrument::sample : Instrument::lead; t.volume = .5f;
            if (drums) t.instrument = Instrument::kick;
            next.tracks.push_back(t); pat.lanes.push_back(std::move(notes));
        }
    }
    if (next.tracks.empty()) return Result::fail("MIDI contains no paired notes.");
    pat.length = jlimit(8.,64.,std::ceil(end/4)*4); next.patterns.push_back(std::move(pat)); p = std::move(next); return Result::ok();
}
}
