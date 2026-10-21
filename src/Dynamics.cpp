// SPDX-License-Identifier: AGPL-3.0-only
#include "Dynamics.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
namespace pulse {
using namespace juce;
String dynamicsName(DynamicsMode mode){switch(mode){case DynamicsMode::compressor:return "Compressor";case DynamicsMode::limiter:return "Peak limiter";case DynamicsMode::gate:return "Noise gate";case DynamicsMode::expander:return "Downward expander";}return "Unknown dynamics";}
Result validateDynamics(const DynamicsOptions& d){if((int)d.mode<0||(int)d.mode>3||!std::isfinite(d.range)||d.range<0||d.range>96||!std::isfinite(d.hysteresis)||d.hysteresis<0||d.hysteresis>24||!std::isfinite(d.hold)||d.hold<0||d.hold>1000||!std::isfinite(d.knee)||d.knee<0||d.knee>24)return Result::fail("Invalid dynamics mode, range, hysteresis, hold or knee.");return Result::ok();}
var dynamicsToJson(const DynamicsOptions& d){auto* o=new DynamicObject();o->setProperty("mode",(int)d.mode);o->setProperty("range",d.range);o->setProperty("hysteresis",d.hysteresis);o->setProperty("hold",d.hold);o->setProperty("knee",d.knee);return var(o);}
Result dynamicsFromJson(const var& v,DynamicsOptions& out){if(!v.isObject())return Result::fail("Missing dynamics options.");for(const auto& p:v.getDynamicObject()->getProperties())if(p.name.toString()!="mode"&&p.name.toString()!="range"&&p.name.toString()!="hysteresis"&&p.name.toString()!="hold"&&p.name.toString()!="knee")return Result::fail("Unknown dynamics option.");auto mode=v["mode"];if(!(mode.isInt()||mode.isInt64())||(int64)mode<0||(int64)mode>3)return Result::fail("Invalid dynamics mode.");DynamicsOptions d;d.mode=(DynamicsMode)(int)mode;for(auto pair:{std::pair{"range",&d.range},{"hysteresis",&d.hysteresis},{"hold",&d.hold},{"knee",&d.knee}}){const auto& x=v[pair.first];if(!(x.isInt()||x.isInt64()||x.isDouble())||!std::isfinite((double)x))return Result::fail("Missing or nonnumeric dynamics value.");*pair.second=(float)(double)x;}auto valid=validateDynamics(d);if(valid.wasOk())out=d;return valid;}
void PeakDynamics::reset(){gain=(settings.mode==DynamicsMode::gate||(settings.mode==DynamicsMode::expander&&ratio>1))?floorGain:1;open=false;remaining=0;}
void PeakDynamics::configure(const DynamicsOptions& next,float th,float rat,float attackMs,float releaseMs,float makeupDb){bool changed=settings.mode!=next.mode;settings=next;thresholdDb=th;ratio=rat;threshold=Decibels::decibelsToGain(th);closeThreshold=Decibels::decibelsToGain(th-settings.hysteresis);ceiling=threshold;floorGain=Decibels::decibelsToGain(-settings.range);inputGain=Decibels::decibelsToGain(makeupDb);attack=(float)std::exp(-1/(sampleRate*attackMs*.001));release=(float)std::exp(-1/(sampleRate*releaseMs*.001));holdSamples=(uint64_t)std::llround(sampleRate*settings.hold*.001);if(changed)reset();remaining=jmin(remaining,holdSamples);gain=jlimit(settings.mode==DynamicsMode::limiter?0.f:floorGain,1.f,gain);}
float PeakDynamics::process(float detector,float& left,float& right){
    if(settings.mode==DynamicsMode::limiter){left*=inputGain;right*=inputGain;float peak=jmax(std::abs(left),std::abs(right)),required=peak>ceiling?ceiling/peak:1.f;gain=required<gain?required:1+(gain-1)*release;gain=jmin(gain,required);left*=gain;right*=gain;}
    else{float target=1;detector=jmax(0.f,detector);if(settings.mode==DynamicsMode::gate){if(!open&&detector>=threshold){open=true;remaining=holdSamples;}if(open){if(detector>=closeThreshold)remaining=holdSamples;else if(remaining>0)--remaining;else open=false;}target=open?1:floorGain;}
        else if(settings.mode==DynamicsMode::expander){float db=20*(float)std::log10(jmax(1.e-8f,detector));float reduction=jlimit(0.f,settings.range,(thresholdDb-db)*(ratio-1));target=Decibels::decibelsToGain(-reduction);}
        float coefficient=target>gain?attack:release;gain=target+(gain-target)*coefficient;gain=jlimit(floorGain,1.f,gain);left*=gain*inputGain;right*=gain*inputGain;}
    return -20*(float)std::log10(jmax(1.e-8f,gain));
}
}
