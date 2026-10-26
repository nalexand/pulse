// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "AutomationParameters.h"
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>

namespace pulse {
// One control-thread producer, one audio-thread consumer. No callback allocation,
// locks, project mutation or file IO. Ready publishes an immutable review result.
class AutomationWrite {
public:
    enum class Mode { touch, latch, write, trim };
    enum class Phase { idle, armed, running, ready };
    enum class End { complete, stopped, cancelled, discontinuity, changed, capacity };
    struct Plan { int target=0,track=0; double start=0,end=0; uint64_t revision=0; Mode mode=Mode::touch; float initial=0; };
    struct Point { double beat=0; float value=0; };
    struct Segment { double start=0,end=0; int first=0,count=0; };
    struct Take {
        Plan plan;
        std::array<Point,256> points;
        std::array<Segment,16> segments;
        int pointCount=0,segmentCount=0;
        End end=End::complete;
    };
    Phase phase()const noexcept{return state.load(std::memory_order_acquire);}
    // Snapshot at render entry: a callback already in flight must not consume a
    // pass published after its transport/project commands were read.
    uint64_t entryToken()const noexcept{return startEpoch.load(std::memory_order_acquire);}
    // Control thread only; a ready result can be replaced after review/discard.
    bool begin(const Plan& p,bool deferred=false) noexcept {
        auto current=phase();
        if((current!=Phase::idle&&current!=Phase::ready)||!validAutomationLink(p.target,p.track,maxTracks)||!std::isfinite(p.start)||!std::isfinite(p.end)||p.start<0||p.end<=p.start||p.end-p.start>600||p.end>4096||!std::isfinite(p.initial)||p.initial<0||p.initial>1||(p.mode!=Mode::touch&&p.mode!=Mode::latch&&p.mode!=Mode::write&&p.mode!=Mode::trim)||(p.mode==Mode::trim&&automationParameters()[(size_t)p.target].discrete))return false;
        take={};take.plan=p;open=-1;writing=false;lastBeat=p.start;deferredStart=deferred;input.store(std::bit_cast<uint32_t>(p.initial),std::memory_order_relaxed);cancelled.store(false,std::memory_order_relaxed);startReady.store(!deferred,std::memory_order_relaxed);startEpoch.fetch_add(1,std::memory_order_release);state.store(Phase::armed,std::memory_order_release);return true;
    }
    // Publish only after stop/seek/start have all been submitted. No audio-thread
    // wait: an older callback skips this pass and the next callback starts it.
    void activate() noexcept {
        if(phase()!=Phase::armed||startReady.load(std::memory_order_acquire))return;
        startEpoch.fetch_add(1,std::memory_order_release);startReady.store(true,std::memory_order_release);
    }
    bool transportStart(uint64_t entry,uint64_t revision,double& start)const noexcept {
        if(phase()!=Phase::armed||!startReady.load(std::memory_order_acquire)||entry!=entryToken()||!deferredStart||cancelled.load(std::memory_order_acquire)||take.plan.revision!=revision)return false;
        start=take.plan.start;return true;
    }
    void control(float value,bool touched) noexcept {
        if(!std::isfinite(value)||value<0||value>1)return;
        input.store((uint64_t)std::bit_cast<uint32_t>(value)|(touched?uint64_t{1}<<32:0),std::memory_order_release);
    }
    void cancel() noexcept {cancelled.store(true,std::memory_order_release);}
    const Take* result()const noexcept{return phase()==Phase::ready?&take:nullptr;}
    bool overriding()const noexcept{return phase()==Phase::running&&writing; } // Audio thread only.
    float value()const noexcept{return held; } // Audio thread only.
    const Plan& plan()const noexcept{return take.plan; } // Audio thread / immutable Ready result only.
    // Called once before a reset/seek, while the old timeline position is intact.
    void interrupt(double beat,bool stopped) noexcept {
        if(phase()==Phase::running)finish(beat,stopped?End::stopped:End::discontinuity);
    }
    // Called after transport commands at the first sample of each callback.
    void block(double beat,bool playing,uint64_t revision,uint64_t entry=UINT64_MAX) noexcept {
        auto s=phase();if(s!=Phase::armed&&s!=Phase::running)return;
        if(!startReady.load(std::memory_order_acquire)||(entry!=UINT64_MAX&&entry!=entryToken()))return;
        if(cancelled.load(std::memory_order_acquire)){finish(beat,End::cancelled);return;}
        if(revision!=take.plan.revision){finish(beat,End::changed);return;}
        if(!playing){finish(beat,End::stopped);return;}
        if(beat<take.plan.start-1e-7||beat>take.plan.end+1e-7){finish(beat,End::discontinuity);return;}
        if(s==Phase::armed)state.store(Phase::running,std::memory_order_release);
        auto control=input.load(std::memory_order_acquire);bool touched=take.plan.mode==Mode::write||(control&(uint64_t{1}<<32))!=0;float v=std::bit_cast<float>((uint32_t)control);
        if(touched){
            if(!writing){
                if(take.segmentCount==(int)take.segments.size()){finish(beat,End::capacity);return;}
                open=take.segmentCount++;auto& segment=take.segments[(size_t)open];segment={beat,beat,take.pointCount,0};writing=true;held=v;
                append(beat,v);
            }else if(v!=held){held=v;append(beat,v);}
        }else if(writing&&(take.plan.mode==Mode::touch||take.plan.mode==Mode::trim))close(beat);
    }
    // The boundary check is per sample, independent of device callback size.
    void tick(double beat,bool playing) noexcept {
        if(phase()!=Phase::running)return;
        if(beat+1e-9<lastBeat){finish(lastBeat,End::discontinuity);return;}
        if(!playing){finish(lastBeat,End::stopped);return;}
        if(beat>=take.plan.end-1e-9){finish(take.plan.end,End::complete);return;}
        lastBeat=beat;
    }
private:
    void append(double beat,float v) noexcept {
        if(open<0)return;auto& s=take.segments[(size_t)open];
        if(s.count&&beat<=take.points[(size_t)(s.first+s.count-1)].beat+1e-10){take.points[(size_t)(s.first+s.count-1)].value=v;return;}
        // Reserve one endpoint so every successful segment is serializable.
        if(take.pointCount>=(int)take.points.size()-1){finish(beat,End::capacity);return;}
        take.points[(size_t)take.pointCount++]={beat,v};++s.count;
    }
    void close(double beat) noexcept {
        if(open<0)return;auto& s=take.segments[(size_t)open];double end=std::fmin(take.plan.end,std::fmax(s.start,beat));
        if(s.count==0||end<=s.start+1e-8){take.pointCount=s.first;--take.segmentCount;}
        else {
            auto& last=take.points[(size_t)(s.first+s.count-1)];
            if(end<=last.beat+1e-10){last.beat=end;if(s.count==1){take.pointCount=s.first;--take.segmentCount;}}
            else if(take.pointCount<(int)take.points.size()){take.points[(size_t)take.pointCount++]={end,held};++s.count;}
            else take.end=End::capacity;
            s.end=end;
        }
        open=-1;writing=false;
    }
    void finish(double beat,End why) noexcept {
        close(beat);if(take.end!=End::capacity)take.end=why;state.store(Phase::ready,std::memory_order_release);
    }
    std::atomic<Phase> state{Phase::idle};
    std::atomic<uint64_t> input{0};
    std::atomic<bool> cancelled{false};
    std::atomic<bool> startReady{false};
    std::atomic<uint64_t> startEpoch{0};
    Take take;
    int open=-1;bool writing=false,deferredStart=false;float held=0;double lastBeat=0;
};
}
