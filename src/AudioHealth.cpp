// SPDX-License-Identifier: AGPL-3.0-only
#include "AudioHealth.h"
#include <cmath>
#include <algorithm>
namespace pulse {
AudioHealth::AudioHealth(double ticks):frequency(std::isfinite(ticks)&&ticks>0?ticks:1.){}
void AudioHealth::clearMeasurement(){values.fill(0);havePrevious=false;previousFrames=0;previousStart=0;++measurement;applied=requested.load(std::memory_order_acquire);}
void AudioHealth::publish(){
    sequence.fetch_add(1,std::memory_order_acq_rel);
    for(size_t i=0;i<values.size();++i)published[i].store(values[i],std::memory_order_relaxed);
    publishedRate.store(rate,std::memory_order_relaxed);publishedBlock.store(nominalBlock,std::memory_order_relaxed);publishedMeasurement.store(measurement,std::memory_order_relaxed);publishedReset.store(applied,std::memory_order_relaxed);
    sequence.fetch_add(1,std::memory_order_release);
}
void AudioHealth::prepare(double sampleRate,int block){
    rate=std::isfinite(sampleRate)&&sampleRate>0&&sampleRate<=768000?sampleRate:0;nominalBlock=block>0?block:0;clearMeasurement();
    bool valid=rate>0&&nominalBlock>0;active.store(valid,std::memory_order_release);if(valid)starts.fetch_add(1);publish();
}
void AudioHealth::stopped(){if(active.exchange(false))stops.fetch_add(1);havePrevious=false;publish();}
AudioHealth::Token AudioHealth::begin(int count,int64_t started){
    if(requested.load(std::memory_order_acquire)!=applied)clearMeasurement();
    if(!active.load(std::memory_order_relaxed))return {};
    if(count<=0||count>1048576){++values[invalid];havePrevious=false;publish();return {};}
    if(havePrevious){
        if(started<previousStart){++values[clockErrors];}
        else{auto gap=(uint64_t)(started-previousStart);values[maxGap]=std::max(values[maxGap],gap);double period=previousFrames*frequency/rate;
            if(period>0&&gap>period*1.5){++values[lateStarts];values[missing]+=(uint64_t)std::min(1000000.,std::max(0.,std::floor(gap/period)-1));}}
    }
    previousStart=started;previousFrames=count;havePrevious=true;return {started,count,true};
}
void AudioHealth::end(Token token,int64_t ended,bool outputAvailable){
    if(!token.valid)return;
    uint64_t elapsed=0;if(ended<token.started)++values[clockErrors];else elapsed=(uint64_t)(ended-token.started);
    ++values[callbacks];values[frames]+=(uint64_t)token.frames;values[lastTicks]=elapsed;values[maxTicks]=std::max(values[maxTicks],elapsed);values[totalTicks]+=elapsed;if(!outputAvailable)++values[silentBlocks];
    double budget=token.frames*frequency/rate;if(elapsed>budget)++values[deadlineMisses];
    if(budget>0)values[worstUtilization]=std::max(values[worstUtilization],(uint64_t)std::llround(std::min(1.e12,elapsed/budget*100)*1000));publish();
}
AudioHealth::Snapshot AudioHealth::snapshot()const{
    Snapshot s;s.running=active.load(std::memory_order_acquire);s.deviceStarts=starts.load();s.deviceStops=stops.load();
    for(int attempt=0;attempt<4;++attempt){
        auto before=sequence.load(std::memory_order_acquire);if(before&1)continue;
        std::array<uint64_t,fieldCount> v;for(size_t i=0;i<v.size();++i)v[i]=published[i].load(std::memory_order_relaxed);
        auto r=publishedRate.load(std::memory_order_relaxed);auto block=publishedBlock.load(std::memory_order_relaxed);auto epoch=publishedMeasurement.load(std::memory_order_relaxed);auto reset=publishedReset.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);if(sequence.load(std::memory_order_relaxed)!=before)continue;
        s.consistent=true;s.rate=r;s.nominalBlock=block;s.measurement=epoch;s.resetPending=reset!=requested.load(std::memory_order_acquire);
        s.callbacks=v[callbacks];s.frames=v[frames];s.deadlineMisses=v[deadlineMisses];s.lateStarts=v[lateStarts];s.estimatedMissingBlocks=v[missing];s.invalidBlocks=v[invalid];s.clockErrors=v[clockErrors];s.silentBlocks=v[silentBlocks];
        s.lastMs=v[lastTicks]*1000./frequency;s.maxMs=v[maxTicks]*1000./frequency;s.meanMs=s.callbacks?v[totalTicks]*1000./frequency/s.callbacks:0;s.maxStartGapMs=v[maxGap]*1000./frequency;
        s.worstUtilization=v[worstUtilization]/1000.;s.meanUtilization=s.frames&&r>0?v[totalTicks]*100./(s.frames*frequency/r):0;return s;
    }s.resetPending=requested.load()!=publishedReset.load();return s;
}
}
