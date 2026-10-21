// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include "EffectPresets.h"
#include "SamplerTuning.h"
#include "AutomationTools.h"
#include <cmath>
#include <algorithm>

namespace pulse {
using namespace juce;
static constexpr double pi = 3.14159265358979323846;
static bool eventAccepted(const Event& e,int index,int cycle){
    if(!noteConditionMatches(e.conditionEvery,e.conditionStep,(uint64_t)(e.conditionPass<0?cycle:e.conditionPass)))return false;
    uint32_t hash=(uint32_t)(index+1)*2654435761u ^ (uint32_t)(cycle+1)*2246822519u;
    return e.probability>=1.f || (float)(hash&0xffff)/65535.f<e.probability;
}
Assets::Assets() { formats.registerBasicFormats(); }
const Sample* Assets::load(const File& file, String& error) {
    auto key = file.getFullPathName()+"|"+String(file.getSize())+"|"+String(file.getLastModificationTime().toMilliseconds());
    if (auto it = samples.find(key); it != samples.end()) return it->second.get();
    std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) { error = "Cannot decode sample: " + file.getFileName(); return nullptr; }
    if (reader->sampleRate < 8000 || reader->sampleRate > 192000 || reader->lengthInSamples <= 0 || reader->lengthInSamples > reader->sampleRate * maxAudioSeconds) {
        error = "Audio must be nonempty and at most 30 minutes at 8-192 kHz; decoded audio must fit the shared 512 MiB cache."; return nullptr;
    }
    auto sample = std::make_unique<Sample>(); sample->sampleRate = reader->sampleRate;
    auto bytes=(uint64_t)reader->lengthInSamples*2*sizeof(float);
    if(bytes>availableBytes()){error="Sample cache limit (512 MB) reached. Save and reopen the application to release unused samples.";return nullptr;}
    try{sample->audio.setSize(2,(int)reader->lengthInSamples);}catch(const std::exception&){error="Not enough memory to decode this audio file. Use a shorter/lower-rate file or reopen to release cached sources.";return nullptr;}
    if (!reader->read(&sample->audio,0,sample->audio.getNumSamples(),0,true,true)) { error = "Could not read sample data."; return nullptr; }
    if (reader->numChannels == 1) sample->audio.copyFrom(1,0,sample->audio,0,0,sample->audio.getNumSamples());
    auto* ptr = sample.get(); samples.emplace(key,std::move(sample));cachedBytes+=bytes;return ptr;
}
float automationValue(const AutomationPoint* points,int count,double local){
    if(count<=0)return .5f;
    if(local<=points[0].beat)return points[0].value;
    if(local>=points[count-1].beat)return points[count-1].value;
    auto* right=std::upper_bound(points,points+count,local,[](double beat,const AutomationPoint& point){return beat<point.beat;});
    const auto& left=*(right-1);if(left.hold)return left.value;
    return left.value+(right->value-left.value)*(float)automationShape((local-left.beat)/(right->beat-left.beat),left.curve,left.tension);
}
Result compilePlayback(const Project& p, Assets& assets, const File& base, Playback& s, String& warnings) {
    auto masterValid=validateMasterEffects(p.masterMixer);if(masterValid.failed())return masterValid;
    if(!std::isfinite(p.delayReturn)||p.delayReturn<0||p.delayReturn>1)return Result::fail("Invalid master delay return.");
    s.songMode=p.songMode;s.revisionTag=0;s.eventCount = 0; s.audioCount = 0; s.automationCount=0; s.pointCount=0; s.trackCount = (int)p.tracks.size(); s.bpm = p.bpm; s.master = p.master;
    s.length = p.songMode ? p.songLength() : p.patterns[(size_t)p.activePattern].length;
    s.loop = p.loop; s.feedback = p.delayFeedback; s.delayReturn=p.delayReturn; s.masterSoftClip=p.masterSoftClip; s.masterMixer=p.masterMixer; s.metronome = p.metronome;
    auto markerValidation=validateSongMarkers(p);if(markerValidation.failed())return markerValidation;s.loopStart=0;s.loopEnd=-1;
    if(p.songMode&&p.loop&&p.loopSection.isNotEmpty()){auto* section=findSongMarker(p,p.loopSection);s.loopStart=section->beat;s.loopEnd=section->endBeat;}
    auto routing=validateRouting(p,&s.routingOrder);if(routing.failed())return routing;
    bool solo = false; for (const auto& t : p.tracks) solo |= t.solo;
    for (int i = 0; i < s.trackCount; ++i) {
        const auto& t = p.tracks[(size_t)i]; auto& x = s.tracks[(size_t)i];
        auto microValid=validateMicroTuning(t.tuning);if(microValid.failed())return microValid;
        x = {t.instrument,t.volume,t.pan,t.cutoff,t.drive,t.send,t.attack,t.release,t.mute,nullptr};
        auto dynamicsValid=validateDynamics(t.mixer.dynamics);if(dynamicsValid.failed())return dynamicsValid;auto stereoValid=validateStereo(t.mixer.stereo);if(stereoValid.failed())return stereoValid;auto orderValid=validateEffectOrder(t.mixer.order);if(orderValid.failed())return orderValid;auto modulationValid=validateModulation(t.mixer.flanger,ModulationKind::flanger);if(modulationValid.failed())return modulationValid;modulationValid=validateModulation(t.mixer.phaser,ModulationKind::phaser);if(modulationValid.failed())return modulationValid;auto chorusValid=validateChorus(t.mixer.chorus);if(chorusValid.failed())return chorusValid;x.tuning=t.tuning;x.synth=t.synth;if(isSynth(t.instrument)){auto valid=validateSynthPatch(t.synth);if(valid.failed())return valid;if(!std::isfinite(t.attack)||t.attack<.001f||t.attack>2||!std::isfinite(t.release)||t.release<.02f||t.release>3)return Result::fail("Invalid synth amplitude envelope.");}auto tuningValid=validateSamplerTuning(t.sampleRoot,t.sampleTune);if(tuningValid.failed())return tuningValid;x.sampleRoot=t.sampleRoot;x.sampleTune=t.sampleTune;x.sampleStart=t.sampleStart;x.sampleEnd=t.sampleEnd;x.sampleLoop=t.sampleLoop;x.mixer=t.mixer;x.isBus=t.isBus;bool audible=!solo;int at=i;while(at>=0){audible|=p.tracks[(size_t)at].solo;at=p.tracks[(size_t)at].mixer.output;}x.inputMuted=t.mute||!audible||t.isBus;
        if (t.instrument == Instrument::sample && t.samplePath.isNotEmpty()) {
            String error; auto path = File::isAbsolutePath(t.samplePath) ? File(t.samplePath) : base.getChildFile(t.samplePath);
            x.sample = assets.load(path,error); if(x.sample){double duration=x.sample->audio.getNumSamples()/x.sample->sampleRate;x.sampleStart=jlimit(0.,jmax(0.,duration-1/x.sample->sampleRate),t.sampleStart);x.sampleEnd=t.sampleEnd>0?jlimit(x.sampleStart+1/x.sample->sampleRate,duration,t.sampleEnd):duration;} if (error.isNotEmpty()) warnings += error + "\n";
        }
    }
    for(const auto& pat:p.patterns)for(const auto& lane:pat.lanes)for(const auto& n:lane)if(!validNoteCondition(n))return Result::fail("Note condition must choose pass 1-N in a cycle of 1-16 repeats.");
    bool overflow = false;
    auto addLane = [&](int track,const Pattern& pat,double offset,double available) {
        for (const auto& n : pat.lanes[(size_t)track]) for (int rat = 0; rat < n.ratchets; ++rat) {
            double local = n.beat + rat * n.length / n.ratchets;
            if (local >= available) continue;
            double start = offset + local;
            if (((int)std::floor(local * 4 + .00001) & 1) != 0) start += p.swing * .125;
            if (start >= s.length) continue;
            if (s.eventCount >= maxEvents) { overflow = true; continue; }
            s.events[(size_t)s.eventCount++] = {start,n.length/n.ratchets,track,n.pitch,n.velocity,n.probability,n.conditionEvery,n.conditionStep,p.songMode?(int)std::floor(offset/pat.length+1.e-9):-1};
        }
    };
    if (!p.songMode) {
        const auto& pat = p.patterns[(size_t)p.activePattern];
        for (int i = 0; i < s.trackCount; ++i) addLane(i,pat,0,pat.length);
    } else for (const auto& c : p.clips) {
        if(!validFadeCurve(c.fadeInCurve)||!validFadeCurve(c.fadeOutCurve))return Result::fail("Invalid audio fade curve.");
        if(c.isAutomation()){
            if(s.automationCount>=maxAutomationClips||s.pointCount+(int)c.points.size()>maxTotalAutomationPoints||c.points.size()<2||c.points.size()>maxAutomationPoints||c.automationTarget>5||(c.automationTarget!=5&&(c.automationTrack<0||c.automationTrack>=s.trackCount)))return Result::fail("Automation exceeds 64 clips / 4096 total points, or contains invalid data.");
            s.automation[(size_t)s.automationCount++]={c.automationTarget==5?0:c.automationTrack,c.automationTarget,s.pointCount,(int)c.points.size(),c.beat,c.length};
            auto validPoints=validateAutomationPoints(c.points,c.length,false);if(validPoints.failed())return validPoints;for(const auto& point:c.points)s.points[(size_t)s.pointCount++]=point;
            continue;
        }
        if (c.isAudio()) {
            String error; auto path=File::isAbsolutePath(c.audioPath)?File(c.audioPath):base.getChildFile(c.audioPath);
            auto* sample=assets.load(path,error);
            if (!sample) { warnings+=error+"\n"; continue; }
            if(s.audioCount>=maxClips)return Result::fail("Too many audio clips.");
            s.audio[(size_t)s.audioCount++]={sample,c.track,c.beat,c.length,c.sourceOffset,c.fadeIn,c.fadeOut,c.gain,c.fadeInCurve,c.fadeOutCurve};
            continue;
        }
        const auto& pat = p.patterns[(size_t)c.pattern];
        for (double off = 0; off < c.length; off += pat.length) addLane(c.track,pat,c.beat+off,jmin(pat.length,c.length-off));
    }
    if(s.loopEnd>=0)for(int i=0;i<s.eventCount;++i){auto& e=s.events[(size_t)i];if(e.beat>=s.loopStart&&e.beat<s.loopEnd)e.length=jmin(e.length,s.loopEnd-e.beat);}
    std::stable_sort(s.events.begin(),s.events.begin()+s.eventCount,[](const Event& a,const Event& b) { return a.beat < b.beat; });
    return overflow ? Result::fail("Playback exceeds 32768 scheduled events. Shorten the arrangement or reduce ratchets.") : Result::ok();
}
Engine::Engine() { for (auto& m : meters) m.store(0);for(auto& m:gainReduction)m.store(0);for(auto& row:automationReadout)for(auto& value:row)value.store(-1); }
void Engine::prepare(double rate) {
    monitorGain=0;monitorTrack=-1;
    prepareWavetables();sampleRate = rate; preparedChange=true;for(auto& fx:effects){fx.reverb.setSampleRate(rate);fx.chorus.prepare(rate);fx.stereo.prepare(rate);fx.dynamics.prepare(rate);fx.flanger.prepare(rate);fx.phaser.prepare(rate);} delayL.assign((size_t)(rate*2.0)+1,0); delayR.assign(delayL.size(),0); clear();
}
bool Engine::beginTake(const TakePlan& plan){
    if(!std::isfinite(plan.startBeat)||!std::isfinite(plan.gateBeat)||!std::isfinite(plan.endBeat)||plan.startBeat<0||plan.gateBeat<plan.startBeat||plan.gateBeat>4096||(plan.endBeat>=0&&(plan.endBeat<=plan.gateBeat||plan.endBeat>4096))||plan.countInBeats<0||plan.countInBeats>8)return false;
    auto w=takeWrite.load(std::memory_order_relaxed),r=takeRead.load(std::memory_order_acquire);if(w-r>=takeCommands.size())return false;
    cancelPreview();auto generation=takeGeneration.fetch_add(1)+1;takeCommands[w%takeCommands.size()]={plan,generation};takeGateOpen.store(false);takeFinished.store(false);countInRemaining.store(plan.countInBeats);resumeRequested.store(false);takeWrite.store(w+1,std::memory_order_release);playing.store(true);return true;
}
void Engine::clear() {
    chasePending=false;
    takeRunning=false;takeGateOpen.store(false);countInRemaining.store(0);takeFinished.store(false);
    for (auto& v : voices) v.active = false;
    detectorLast.fill(0);for(auto& fx:effects){fx.zL={};fx.zR={};fx.envelope=0;fx.reduction=0;fx.reverb.reset();fx.chorus.reset();fx.stereo.reset();fx.dynamics.reset();fx.flanger.reset();fx.phaser.reset();}
    filterL.fill(0); filterR.fill(0); smoothGain.fill(0); beat = 0; nextEvent = 0; loopCycle = 0; click.reset();
    std::fill(delayL.begin(),delayL.end(),0.f); std::fill(delayR.begin(),delayR.end(),0.f); delayPos = 0;
}
void Engine::stop() { cancelPreview();takeGeneration.fetch_add(1);takeGateOpen.store(false);countInRemaining.store(0);playing.store(false);resumeRequested.store(false);seekRequested.store(-1);resetRequested.store(true); }
bool Engine::preview(const Sample* source,double start,double end,float gain,double sourceBpm,bool repeat,bool align){
    if(!source){cancelPreview();return true;}
    if(!std::isfinite(gain)||gain<=0||gain>1||!std::isfinite(sourceBpm)||(sourceBpm!=0&&(sourceBpm<20||sourceBpm>400))
       ||!std::isfinite(start)||!std::isfinite(end)||start<0||end<0||!std::isfinite(source->sampleRate)||source->sampleRate<8000||source->sampleRate>192000
       ||source->audio.getNumChannels()<2||source->audio.getNumSamples()<1||start>=source->audio.getNumSamples()/source->sampleRate
       ||(end>0&&(end<=start||end>source->audio.getNumSamples()/source->sampleRate+1e-9)))return false;
    unsigned w=previewWrite.load(std::memory_order_relaxed),r=previewRead.load(std::memory_order_acquire);if(w-r>=previewCommands.size())return false;
    previewCommands[w%previewCommands.size()]={source,start,end,previewGeneration.load(std::memory_order_acquire),gain,sourceBpm,repeat,align};previewWrite.store(w+1,std::memory_order_release);return true;
}
void Engine::start() { cancelPreview();takeGeneration.fetch_add(1);takeGateOpen.store(false);countInRemaining.store(0);resumeRequested.store(true);playing.store(true); }
bool Engine::audition(int track,int pitch,bool on,float velocity,float seconds) {
    unsigned w = inputWrite.load(std::memory_order_relaxed), r = inputRead.load(std::memory_order_acquire);
    if (w-r >= input.size()) return false;
    if(on)cancelPreview();
    input[(size_t)(w%input.size())] = {track,pitch,on,velocity,jlimit(.05f,60.f,seconds)}; inputWrite.store(w+1,std::memory_order_release); return true;
}
Engine::Voice& Engine::trigger(const Event& ev,double gateSamples,int eventIndex) {
    Voice* target = &voices[0];
    for (auto& v : voices) { if (!v.active) { target=&v; break; } if (v.age > target->age) target=&v; }
    *target = Voice{}; target->active = true; target->track = ev.track; target->pitch=ev.pitch; target->velocity=ev.velocity;
    target->tuningCents=state?microTuningCents(state->tracks[(size_t)ev.track].tuning,ev.pitch):0;
    target->scheduledEvent=eventIndex;target->sourceBeat=ev.beat;
    if(state&&state->tracks[(size_t)ev.track].sample)target->samplePosition=state->tracks[(size_t)ev.track].sampleStart*state->tracks[(size_t)ev.track].sample->sampleRate;
    target->gate=gateSamples; target->random = 12345u + (uint32_t)(ev.pitch*313 + ev.track*719 + loopCycle*991);
    if(state&&isSynth(state->tracks[(size_t)ev.track].instrument))target->synth.start(ev.pitch,sampleRate,target->random,target->tuningCents);
    return *target;
}
void Engine::chaseNotes(double at){
    if(at<=0||!state)return;
    // Select the latest held gates, bounded by the existing 128-voice budget.
    // No allocation or historical DSP rendering is performed in the callback.
    std::array<int,128> candidates{};int count=0;
    auto end=std::lower_bound(state->events.begin(),state->events.begin()+state->eventCount,at-.000001,[](const Event& e,double value){return e.beat<value;});
    for(int index=(int)(end-state->events.begin())-1;index>=0&&count<(int)candidates.size();--index){const auto& e=state->events[(size_t)index];const auto& track=state->tracks[(size_t)e.track];
        if(e.beat+e.length<=at+1e-9||e.velocity<=0||track.inputMuted||isDrum(track.instrument)||!eventAccepted(e,index,loopCycle))continue;
        if(track.instrument==Instrument::sample){if(!track.sample)continue;double begin=track.sampleStart*track.sample->sampleRate,last=track.sampleEnd*track.sample->sampleRate;double position=begin+(at-e.beat)*60/state->bpm*track.sample->sampleRate*samplerRatio(e.pitch,track.sampleRoot,track.sampleTune)*(track.tuning.enabled?std::exp2(microTuningCents(track.tuning,e.pitch)/1200):1);if(last<=begin||(!track.sampleLoop&&position>=last))continue;}
        candidates[(size_t)count++]=index;
    }
    for(int selected=count-1;selected>=0;--selected){int index=candidates[(size_t)selected];const auto& e=state->events[(size_t)index];const auto& track=state->tracks[(size_t)e.track];double remaining=e.beat+e.length-at;
        if(state->loop&&state->loopEnd>=0)remaining=jmin(remaining,state->loopEnd-at);
        if(remaining<=0)continue;
        Voice* voice=nullptr;for(auto& existing:voices)if(existing.active&&!existing.released&&existing.scheduledEvent==index&&existing.sourceBeat==e.beat&&existing.track==e.track&&existing.pitch==e.pitch){voice=&existing;break;}
        if(!voice)voice=&trigger(e,remaining*60/state->bpm*sampleRate,index);
        else voice->gate=voice->age+remaining*60/state->bpm*sampleRate;
        voice->velocity=e.velocity;
        if(track.instrument==Instrument::sample){double begin=track.sampleStart*track.sample->sampleRate,last=track.sampleEnd*track.sample->sampleRate;double position=begin+(at-e.beat)*60/state->bpm*track.sample->sampleRate*samplerRatio(e.pitch,track.sampleRoot,track.sampleTune)*(track.tuning.enabled?std::exp2(microTuningCents(track.tuning,e.pitch)/1200):1);if(track.sampleLoop&&position>=last)position=begin+std::fmod(position-begin,last-begin);voice->samplePosition=position;}
        chasedNotes.fetch_add(1,std::memory_order_relaxed);
    }
}
static double polyBlep(double phase,double dt) {
    if (phase < dt) { double x=phase/dt; return x+x-x*x-1; }
    if (phase > 1-dt) { double x=(phase-1)/dt; return x*x+x+x+1; }
    return 0;
}
float Engine::voiceSample(Voice& v,const TrackSound& t) {
    if(isSynth(t.instrument)){if(!v.released&&v.age>=v.gate){v.released=true;v.releasedAt=v.age;}auto value=v.synth.process(t.synth,t.instrument==Instrument::physical?4:t.instrument==Instrument::additive?3:t.instrument==Instrument::wavetable?2:t.instrument==Instrument::fm?1:0,!v.released,t.attack,t.release);v.active=v.synth.active();++v.age;v.right=value*v.velocity;return v.right;}
    double seconds = v.age/sampleRate; double drumTime=seconds*.18/jmax(.02,(double)t.release);
    double frequency=440.0*std::pow(2.0,(v.pitch-69)/12.0);if(v.tuningCents!=0)frequency*=std::exp2(v.tuningCents/1200);
    double dt=jmin(.45,frequency/sampleRate); double signal=0, env=1;
    v.random ^= v.random << 13; v.random ^= v.random >> 17; v.random ^= v.random << 5;
    double noise = ((double)(v.random & 0xffff)/32768.0)-1;
    if (t.instrument == Instrument::kick) {
        double f = 48 + 140 * std::exp(-seconds*42); v.phase += f/sampleRate;
        signal=std::sin(2*pi*v.phase)*std::exp(-drumTime*8) + noise*std::exp(-seconds*300)*.15;
        if (drumTime > .8) v.active=false;
    } else if (t.instrument == Instrument::snare || t.instrument == Instrument::clap) {
        double high = noise-v.lastNoise; v.lastNoise=(float)noise;
        double pulse = t.instrument==Instrument::clap ? (seconds < .032 ? .4+.6*std::sin(seconds*600)*std::sin(seconds*600) : .6) : .8;
        signal=(high*.45*pulse + std::sin(2*pi*185*seconds)*.28)*std::exp(-drumTime*17);
        if (drumTime > .5) v.active=false;
    } else if (t.instrument==Instrument::hat || t.instrument==Instrument::openHat) {
        double high=noise-v.lastNoise; v.lastNoise=(float)noise;
        double decay = t.instrument==Instrument::hat ? 72 : 13;
        signal=high*.3*std::exp(-drumTime*decay);
        if (drumTime > (t.instrument==Instrument::hat ? .15 : .65)) v.active=false;
    } else if (t.instrument==Instrument::sample) {
        if (!t.sample) { v.active=false; v.right=0; return 0; }
        const auto& audio=t.sample->audio;
        double begin=t.sampleStart*t.sample->sampleRate,end=t.sampleEnd>0?jmin((double)audio.getNumSamples(),t.sampleEnd*t.sample->sampleRate):audio.getNumSamples();
        if(t.sampleLoop&&!v.released&&v.age<v.gate&&v.samplePosition>=end&&end>begin)v.samplePosition=begin+std::fmod(v.samplePosition-begin,end-begin);
        // Extreme microtonal mappings can exhaust a one-shot in a single frame.
        // Check the double before converting it to an integer sample index.
        if (!std::isfinite(v.samplePosition)||v.samplePosition<0||v.samplePosition>=end||v.samplePosition>=audio.getNumSamples()) { v.active=false; v.right=0; return 0; }
        int index=(int)v.samplePosition;
        float fraction=(float)(v.samplePosition-index);
        signal=audio.getSample(0,index)*(1-fraction)+audio.getSample(0,jmin(index+1,audio.getNumSamples()-1))*fraction;
        double rightSignal=audio.getSample(1,index)*(1-fraction)+audio.getSample(1,jmin(index+1,audio.getNumSamples()-1))*fraction;
        v.samplePosition += t.sample->sampleRate/sampleRate*samplerRatio(v.pitch,t.sampleRoot,t.sampleTune)*(v.tuningCents!=0?std::exp2(v.tuningCents/1200):1);
        if(!v.released && v.age>=v.gate){v.released=true;v.releasedAt=v.age;}
        double envelope=jmin(1.,seconds/jmax(.001,(double)t.attack));
        if(v.released)envelope*=std::exp(-(v.age-v.releasedAt)/(sampleRate*jmax(.02,(double)t.release)));
        if(v.released&&envelope<.0005)v.active=false;
        v.right=(float)(rightSignal*envelope*v.velocity);++v.age;return(float)(signal*envelope*v.velocity);
    } else {
        if (!v.released && v.age >= v.gate) { v.released=true; v.releasedAt=v.age; }
        double attack=jmax(.001,(double)t.attack);
        env=jmin(1.,seconds/attack);
        if (v.released) env *= std::exp(-(v.age-v.releasedAt)/(sampleRate*jmax(.02,(double)t.release)));
        if (v.released && env < .0005) v.active=false;
        double saw=2*v.phase-1-polyBlep(v.phase,dt);
        if (t.instrument==Instrument::bass) signal=.55*saw+.35*std::sin(2*pi*v.phase);
        else if (t.instrument==Instrument::pad) signal=.25*saw+.3*std::sin(2*pi*v.phase2)+.2*std::sin(2*pi*v.phase);
        else signal=(.65*std::sin(2*pi*v.phase)+.2*std::sin(4*pi*v.phase))*std::exp(-seconds*3);
        v.phase+=dt; v.phase-=std::floor(v.phase); v.phase2+=dt*1.003; v.phase2-=std::floor(v.phase2);
        signal *= env*.5;
    }
    if(isDrum(t.instrument))signal*=jmin(1.,seconds/jmax(.001,(double)t.attack));
    ++v.age;v.right=(float)(signal*v.velocity);return v.right;
}
void Engine::render(float* left,float* right,int frames){render(left,right,frames,MonitorInput{});}
void Engine::render(float* left,float* right,int frames,const MonitorInput& monitor) {
    auto started=Time::getHighResolutionTicks();
    bool analyze=analyzer.enabled.load(std::memory_order_relaxed);auto analysisToken=analyzer.token();int analysisSource=AnalyzerCapture::source(analysisToken);
    bool changed=false; state=&exchange.read(changed);changed=changed||preparedChange;preparedChange=false;
    if (resetRequested.exchange(false)) clear();
    bool resumed=resumeRequested.exchange(false);bool discontinuity=resumed;if(resumed&&beat>=state->length-.000001){clear();changed=true;}
    double seek=seekRequested.exchange(-1);
    if(seek>=0){clear();beat=jlimit(0.,jmax(0.,state->length-.000001),seek);changed=true;discontinuity=true;}
    captureOffset=0;captureCount=0;
    unsigned tr=takeRead.load(std::memory_order_relaxed),tw=takeWrite.load(std::memory_order_acquire);
    while(tr<tw){auto command=takeCommands[tr%takeCommands.size()];if(command.generation==takeGeneration.load()&&playing.load()){
        clear();takePlan=command.plan;beat=takePlan.startBeat;takeRunning=true;takeReadGeneration.store(command.generation);takeFramesPerBeat=60/state->bpm*sampleRate;
        takeCountFrames=(uint64_t)std::llround(takePlan.countInBeats*takeFramesPerBeat);takeCountElapsed=0;takeTimelineFrames=0;takeClickIndex=0;
        takeGateStart=(uint64_t)std::ceil((takePlan.gateBeat-takePlan.startBeat)*takeFramesPerBeat-1e-7);takeGateEnd=takePlan.endBeat>=0?(uint64_t)std::ceil((takePlan.endBeat-takePlan.startBeat)*takeFramesPerBeat-1e-7):UINT64_MAX;changed=true;discontinuity=true;
    }++tr;}takeRead.store(tr,std::memory_order_release);
    if(takeRunning&&takeReadGeneration.load()!=takeGeneration.load()){takeRunning=false;takeGateOpen.store(false);countInRemaining.store(0);}
    if((changed||resumed)&&playing.load()&&!takeRunning&&state->loop&&state->loopEnd>=0&&(beat<state->loopStart||beat>=state->loopEnd)){clear();beat=state->loopStart;changed=true;discontinuity=true;}
    if(discontinuity)chasePending=true;
    if (changed) {
        configureEffects();
        if (beat >= state->length && state->loop) beat = std::fmod(beat,state->length);
        nextEvent=0; while (nextEvent<state->eventCount && state->events[(size_t)nextEvent].beat < beat-.000001) ++nextEvent;
        for (int t=0;t<state->trackCount;++t) {
            filterCoeff[(size_t)t]=state->tracks[(size_t)t].isBus&&state->tracks[(size_t)t].cutoff>=20000?1.f:(float)(1-std::exp(-2*pi*jmin((double)state->tracks[(size_t)t].cutoff,sampleRate*.45)/sampleRate));
            lastCutoff[(size_t)t]=state->tracks[(size_t)t].cutoff;
            double pan=state->tracks[(size_t)t].pan;lastPan[(size_t)t]=(float)pan;
            panL[(size_t)t]=state->tracks[(size_t)t].isBus?(pan>0?(float)std::cos(pan*pi/2):1.f):(float)std::cos((pan+1)*pi/4); panR[(size_t)t]=state->tracks[(size_t)t].isBus?(pan<0?(float)std::cos(-pan*pi/2):1.f):(float)std::sin((pan+1)*pi/4);
        }
    }
    unsigned r=inputRead.load(std::memory_order_relaxed), w=inputWrite.load(std::memory_order_acquire);
    while (r<w) {
        const auto& ev=input[(size_t)(r%input.size())];
        if (ev.track>=0 && ev.track<state->trackCount) {
            if (ev.on&&!state->tracks[(size_t)ev.track].inputMuted) trigger({0,0,ev.track,ev.pitch,ev.velocity},sampleRate*ev.seconds);
            else if(!ev.on)for (auto& v : voices) if (v.active && v.track==ev.track && v.pitch==ev.pitch) { v.released=true; v.releasedAt=v.age; }
        } ++r;
    } inputRead.store(r,std::memory_order_release);
    if(auditionPanicRequested.exchange(false))for(auto& voice:voices)if(voice.active){voice.released=true;voice.releasedAt=voice.age;}
    unsigned pr=previewRead.load(std::memory_order_relaxed),pw=previewWrite.load(std::memory_order_acquire);
    while(pr<pw){auto command=previewCommands[pr%previewCommands.size()];++pr;if(command.generation!=previewGeneration.load(std::memory_order_acquire))continue;previewSourceGeneration=command.generation;previewSource=command.sample;previewGain=command.gain;previewBpm=command.sourceBpm;previewRepeat=command.repeat;previewWait=command.align&&playing.load()?std::ceil(beat-1e-9)-beat:0;previewWait=jmax(0.,previewWait);if(previewSource){double count=previewSource->audio.getNumSamples();previewFrame=jlimit(0.,jmax(0.,count-1),command.start*previewSource->sampleRate);previewStart=previewFrame;previewEnd=command.end>0?jlimit(previewFrame+1,count,command.end*previewSource->sampleRate):count;}}
    if(previewSourceGeneration!=previewGeneration.load(std::memory_order_acquire))previewSource=nullptr;
    previewRead.store(pr,std::memory_order_release);
    bool running=playing.load(); double beatIncrement=state->bpm/(60*sampleRate);
    std::array<float,maxTracks> peaks{};
    std::array<TrackSound,maxTracks> effective;
    float effectiveMaster=state->master,masterReadout=-1;
    std::array<std::array<float,5>,maxTracks> readout;for(auto& row:readout)row.fill(-1);
    bool monitorValid=monitor.track>=0&&monitor.track<state->trackCount&&monitor.left&&std::isfinite(monitor.gain)&&!state->tracks[(size_t)monitor.track].inputMuted;
    int destination=monitorValid?monitor.track:-1;
    if(destination!=monitorTrack){monitorGain=0;monitorTrack=destination;}
    float targetMonitorGain=monitorValid?jlimit(0.f,1.f,monitor.gain):0;
    float monitorSmoothing=(float)(1-std::exp(-1/(sampleRate*.005)));
    const auto clickSettings=unpackMetronome(metronomeConfig.load(std::memory_order_acquire));
    float peak=0;
    historySpan={beat,state->bpm,sampleRate,frames,state->revisionTag,false};
    bool historyRunning=running,historyTaking=takeRunning,historyPreviewing=previewSource!=nullptr;int historyLoop=loopCycle;
    for (int i=0;i<frames;++i) {
        bool counting=running&&takeRunning&&takeCountFrames>0;bool run=running&&!counting;
        if(counting&&takeCountElapsed==(uint64_t)std::llround(takeClickIndex*takeFramesPerBeat)){click.start(sampleRate,takeClickIndex%4==0,clickSettings);++takeClickIndex;}
        bool capturing=run&&takeRunning&&takeTimelineFrames>=takeGateStart&&takeTimelineFrames<takeGateEnd;
        if(capturing){if(captureCount==0)captureOffset=i;++captureCount;}
        effectiveMaster=state->master;
        if(state->automationCount>0){
            effective=state->tracks;
            double automationBeat=run?beat:jmax(0.,beat-beatIncrement);
            if(run||(preserveAutomationInTails&&beat>0))for(int a=0;a<state->automationCount;++a){const auto& c=state->automation[(size_t)a];
                if(automationBeat<c.beat||automationBeat>=c.beat+c.length)continue;
                float v=automationValue(state->points.data()+c.firstPoint,c.pointCount,automationBeat-c.beat);auto& tr=effective[(size_t)c.track];if(i==frames-1){if(c.target==5)masterReadout=v;else readout[(size_t)c.track][(size_t)c.target]=v;}
                switch((AutomationTarget)c.target){case AutomationTarget::volume:tr.volume=v*1.5f;break;case AutomationTarget::pan:tr.pan=v*2-1;break;case AutomationTarget::cutoff:tr.cutoff=(float)(60*std::pow(20000./60,v));break;case AutomationTarget::drive:tr.drive=v;break;case AutomationTarget::send:tr.send=v*.8f;break;case AutomationTarget::master:effectiveMaster=v;break;}
            }
        }
        if (run) {
            if(chasePending){chasePending=false;chaseNotes(beat);}
            while (nextEvent<state->eventCount && state->events[(size_t)nextEvent].beat <= beat+.000000001) {
                const auto& e=state->events[(size_t)nextEvent];
                if (eventAccepted(e,nextEvent,loopCycle) && !state->tracks[(size_t)e.track].inputMuted)
                    trigger(e,e.length*60/state->bpm*sampleRate,nextEvent);
                ++nextEvent;
            }
            if (state->metronome && std::floor(beat+1e-8)!=std::floor(beat-beatIncrement+1e-8)){click.start(sampleRate,((int)std::floor(beat+1e-8)%4)==0,clickSettings);}
        }
        std::array<float,maxTracks> sum{},sumRight{};
        monitorGain+=(targetMonitorGain-monitorGain)*monitorSmoothing;
        if(monitorValid){float ml=monitor.left[i],mr=monitor.right?monitor.right[i]:ml;
            sum[(size_t)destination]+=std::isfinite(ml)?jlimit(-4.f,4.f,ml)*monitorGain:0;
            sumRight[(size_t)destination]+=std::isfinite(mr)?jlimit(-4.f,4.f,mr)*monitorGain:0;}
        if(run) for(int a=0;a<state->audioCount;++a) {
            const auto& c=state->audio[(size_t)a];
            if(beat<c.beat||beat>=c.beat+c.length||state->tracks[(size_t)c.track].inputMuted)continue;
            double local=beat-c.beat, pos=(c.offset+local*60/state->bpm)*c.sample->sampleRate;
            int index=(int)pos;int count=c.sample->audio.getNumSamples();
            if(index<0||index>=count)continue;
            float alpha=(float)(pos-index),gain=c.gain;
            if(c.fadeIn>0)gain*=(float)audioFadeGain(local/c.fadeIn,c.fadeInCurve);
            if(c.fadeOut>0)gain*=(float)audioFadeGain((c.length-local)/c.fadeOut,c.fadeOutCurve);
            int next=jmin(index+1,count-1);
            for(int channel=0;channel<2;++channel){auto* data=c.sample->audio.getReadPointer(channel);float value=(data[index]+alpha*(data[next]-data[index]))*gain;
                (channel==0?sum:sumRight)[(size_t)c.track]+=value;}
        }
        for (auto& v : voices) if (v.active) {
            if (v.track >= state->trackCount) { v.active=false; continue; }
            float value=voiceSample(v,state->tracks[(size_t)v.track]);if(!state->tracks[(size_t)v.track].inputMuted){sum[(size_t)v.track]+=value;sumRight[(size_t)v.track]+=v.right;}
        }
        float l=0,rr=0,sendL=0,sendR=0,analysisLeft=0,analysisRight=0;
        std::array<float,maxTracks> detectorNext{};
        for (int order=0;order<state->trackCount;++order) {
            int t=state->routingOrder[(size_t)order];
            const auto& tr=state->automationCount>0?effective[(size_t)t]:state->tracks[(size_t)t];
            if(state->automationCount>0){if(tr.cutoff!=lastCutoff[(size_t)t]){filterCoeff[(size_t)t]=tr.isBus&&tr.cutoff>=20000?1.f:(float)(1-std::exp(-2*pi*jmin((double)tr.cutoff,sampleRate*.45)/sampleRate));lastCutoff[(size_t)t]=tr.cutoff;}if(tr.pan!=lastPan[(size_t)t]){panL[(size_t)t]=tr.isBus?(tr.pan>0?(float)std::cos(tr.pan*pi/2):1.f):(float)std::cos((tr.pan+1)*pi/4);panR[(size_t)t]=tr.isBus?(tr.pan<0?(float)std::cos(-tr.pan*pi/2):1.f):(float)std::sin((tr.pan+1)*pi/4);lastPan[(size_t)t]=tr.pan;}}
            float target=tr.muted?0:tr.volume;
            smoothGain[(size_t)t]+=(target-smoothGain[(size_t)t])*.002f;
            float signal=sum[(size_t)t],rs=sumRight[(size_t)t];
            if (tr.drive > .001){signal=std::tanh(signal*(1+tr.drive*8))/(1+tr.drive*2);rs=std::tanh(rs*(1+tr.drive*8))/(1+tr.drive*2);}
            filterL[(size_t)t]+=(signal-filterL[(size_t)t])*filterCoeff[(size_t)t];
            filterR[(size_t)t]+=(rs-filterR[(size_t)t])*filterCoeff[(size_t)t];
            float processedL=filterL[(size_t)t],processedR=filterR[(size_t)t];processEffects(t,tr.mixer,processedL,processedR);
            float tl=processedL*smoothGain[(size_t)t]*panL[(size_t)t],rt=processedR*smoothGain[(size_t)t]*panR[(size_t)t];
            if(analyze&&analysisSource==t){analysisLeft=tl;analysisRight=rt;}
            detectorNext[(size_t)t]=jmax(std::abs(tl),std::abs(rt));
            if(tr.mixer.output<0){l+=tl;rr+=rt;}else{sum[(size_t)tr.mixer.output]+=tl;sumRight[(size_t)tr.mixer.output]+=rt;}sendL+=tl*tr.send;sendR+=rt*tr.send;peaks[(size_t)t]=jmax(peaks[(size_t)t],std::abs(tl),std::abs(rt));
        }
        detectorLast=detectorNext;
        if (!delayL.empty()) {
            size_t delaySamples=jmin(delayL.size()-1,(size_t)(60/state->bpm*.75*sampleRate));
            size_t read=(delayPos+delayL.size()-delaySamples)%delayL.size();
            float dl=delayL[read], dr=delayR[read];
            delayL[delayPos]=sendL+dr*state->feedback; delayR[delayPos]=sendR+dl*state->feedback;
            l+=dl*state->delayReturn;rr+=dr*state->delayReturn; delayPos=(delayPos+1)%delayL.size();
        }
        {float value=click.next();l+=value;rr+=value;}
        processEffects(maxTracks,state->masterMixer,l,rr);
        l*=effectiveMaster;rr*=effectiveMaster;
        if(state->masterSoftClip){l=std::tanh(l);rr=std::tanh(rr);}
        if(previewSource){
            if(previewWait>1e-9){if(run)previewWait=jmax(0.,previewWait-beatIncrement);}
            else{
                if(previewFrame>=previewEnd&&previewRepeat)previewFrame=previewStart+std::fmod(previewFrame-previewStart,previewEnd-previewStart);
                int index=(int)previewFrame,count=previewSource->audio.getNumSamples();
                if(previewFrame>=previewEnd||index>=count)previewSource=nullptr;
                else{
                    float f=(float)(previewFrame-index);int next=jmin(index+1,count-1);
                    // Repeated trimmed ranges never interpolate into excluded audio.
                    if(previewRepeat)next=jmin(next,(int)std::ceil(previewEnd)-1);
                    auto& a=previewSource->audio;
                    l=jlimit(-1.f,1.f,l+previewGain*(a.getSample(0,index)+f*(a.getSample(0,next)-a.getSample(0,index))));
                    rr=jlimit(-1.f,1.f,rr+previewGain*(a.getSample(1,index)+f*(a.getSample(1,next)-a.getSample(1,index))));
                    previewFrame+=previewSource->sampleRate/sampleRate*(previewBpm>0?state->bpm/previewBpm:1.);
                }
            }
        }
        if(analyze)analyzer.push(analysisSource<0?l:analysisLeft,analysisSource<0?rr:analysisRight,(float)sampleRate,analysisToken);
        if (left) left[i]=l; if (right) right[i]=rr; peak=jmax(peak,std::abs(l),std::abs(rr));
        if(counting){--takeCountFrames;++takeCountElapsed;}
        if (run) {
            beat+=beatIncrement;
            if(takeRunning){++takeTimelineFrames;if(takeTimelineFrames>=takeGateEnd){beat=takePlan.endBeat;running=false;playing.store(false);takeFinished.store(true);}}
            double end=state->loop&&state->loopEnd>=0?state->loopEnd:state->length;
            if (beat>=end-1e-9 && running) {
                if (state->loop) { double start=state->loopEnd>=0?state->loopStart:0;beat=start+jmax(0.,beat-end);nextEvent=(int)(std::lower_bound(state->events.begin(),state->events.begin()+state->eventCount,start-1e-9,[](const Event& e,double at){return e.beat<at;})-state->events.begin());++loopCycle;chasePending=true; }
                else { running=false; playing.store(false); beat=state->length; }
            }
        }
    }
    if(takeRunning){countInRemaining.store(takeCountFrames>0?(int)std::ceil(takeCountFrames/takeFramesPerBeat):0);takeGateOpen.store(running&&takeCountFrames==0&&takeTimelineFrames>=takeGateStart&&takeTimelineFrames<takeGateEnd);}
    previewPlaying.store(previewSource!=nullptr);if(previewSource){
        double cursor=previewFrame;if(previewRepeat&&cursor>=previewEnd)cursor=previewStart+std::fmod(cursor-previewStart,previewEnd-previewStart);
        previewPosition.store(cursor/previewSource->sampleRate);
    }
    historySpan.valid=frames>0&&state->songMode&&historyRunning&&!historyTaking&&!historyPreviewing&&running&&loopCycle==historyLoop&&std::abs(beat-historySpan.firstBeat-frames*beatIncrement)<1e-7;
    position.store(beat,std::memory_order_relaxed);
    completedLoops.store((uint64_t)loopCycle,std::memory_order_relaxed);
    masterAutomation.store(masterReadout);for(int t=0;t<maxTracks;++t)for(int target=0;target<5;++target)automationReadout[(size_t)t][(size_t)target].store(readout[(size_t)t][(size_t)target],std::memory_order_relaxed);
    for (int t=0;t<maxTracks;++t) meters[(size_t)t].store(jmax(peaks[(size_t)t],meters[(size_t)t].load()*.88f),std::memory_order_relaxed);
    for(int t=0;t<maxTracks;++t)gainReduction[(size_t)t].store(effects[(size_t)t].reduction);
    masterGainReduction.store(effects[maxTracks].reduction);
    masterMeter.store(jmax(peak,masterMeter.load()*.9f));
    observedPeak.store(jmax(peak,observedPeak.load()));processedFrames.fetch_add((uint64_t)frames,std::memory_order_relaxed);
    double elapsed=Time::highResolutionTicksToSeconds(Time::getHighResolutionTicks()-started);
    cpuPercent.store((float)(elapsed*sampleRate/jmax(1,frames)*100));
}
}
