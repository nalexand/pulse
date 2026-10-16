// SPDX-License-Identifier: AGPL-3.0-only
#include "Synth.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
constexpr double pi=3.14159265358979323846;
bool bounded(double x,double lo,double hi){return std::isfinite(x)&&x>=lo&&x<=hi;}
double blep(double p,double dt){if(p<dt){double x=p/dt;return x+x-x*x-1;}if(p>1-dt){double x=(p-1)/dt;return x*x+x+x+1;}return 0;}
double wave(int type,double p,double dt,double duty){if(type==0)return std::sin(2*pi*p);if(type==1)return 1-4*std::abs(p-.5);if(type==2)return 2*p-1-blep(p,dt);double shifted=p-duty;if(shifted<0)shifted+=1;return(p<duty?1.:-1.)+blep(p,dt)-blep(shifted,dt);}
void advance(double& phase,double dt){phase+=dt;phase-=std::floor(phase);}
}
const char* synthWaveName(int wave){static const char* names[]={"Sine","Triangle","Saw","Pulse"};return names[jlimit(0,3,wave)];}
Result validateSynthPatch(const SynthPatch& p){
    if(p.table<0||p.table>=wavetableCount||!bounded(p.tablePosition,0,1)||!bounded(p.tableEnv,-1,1)||!bounded(p.tableDecay,.01f,8)||!bounded(p.tableLfo,0,1)||p.wave1<0||p.wave1>3||p.wave2<0||p.wave2>3||p.coarse<-24||p.coarse>24||!bounded(p.mix,0,1)||!bounded(p.detune,-50,50)||!bounded(p.pulse,.05f,.95f)||!bounded(p.sub,0,1)||!bounded(p.noise,0,.5)||!bounded(p.decay,.01f,4)||!bounded(p.sustain,0,1)||!bounded(p.cutoff,40,20000)||!bounded(p.resonance,0,1)||!bounded(p.filterEnv,-6,6)||!bounded(p.filterDecay,.01f,4)||!bounded(p.fmRatio,.25,16)||!bounded(p.fmIndex,0,12)||!bounded(p.fmDecay,.01f,4)||!bounded(p.lfoRate,.1f,20)||!bounded(p.lfoPitch,0,100)||!bounded(p.lfoFilter,0,4))return Result::fail("Synth settings contain an invalid or out-of-range value.");return Result::ok();
}
var synthPatchToJson(const SynthPatch& p){auto* o=new DynamicObject();
#define WRITE(name) o->setProperty(#name,p.name);
    WRITE(wave1) WRITE(wave2) WRITE(coarse) WRITE(mix) WRITE(detune) WRITE(pulse) WRITE(sub) WRITE(noise) WRITE(decay) WRITE(sustain) WRITE(cutoff) WRITE(resonance) WRITE(filterEnv) WRITE(filterDecay) WRITE(fmRatio) WRITE(fmIndex) WRITE(fmDecay) WRITE(lfoRate) WRITE(lfoPitch) WRITE(lfoFilter) WRITE(table) WRITE(tablePosition) WRITE(tableEnv) WRITE(tableDecay) WRITE(tableLfo)
#undef WRITE
    return var(o);
}
Result synthPatchFromJson(const var& value,SynthPatch& out,bool requireWavetable){if(!value.isObject())return Result::fail("Invalid synth patch object.");SynthPatch p;
#define READ(name) {const auto& v=value[#name];if(!(v.isInt()||v.isInt64()||v.isDouble())||!std::isfinite((double)v))return Result::fail("Missing or invalid synth setting: " #name);p.name=static_cast<decltype(p.name)>((double)v);}
    READ(mix) READ(detune) READ(pulse) READ(sub) READ(noise) READ(decay) READ(sustain) READ(cutoff) READ(resonance) READ(filterEnv) READ(filterDecay) READ(fmRatio) READ(fmIndex) READ(fmDecay) READ(lfoRate) READ(lfoPitch) READ(lfoFilter)
#undef READ
    for(const auto* name:{"wave1","wave2","coarse"}){const auto& v=value[name];if(!(v.isInt()||v.isInt64()||v.isDouble())||!std::isfinite((double)v)||std::abs((double)v)>100||(double)v!=std::floor((double)v))return Result::fail("Invalid synth oscillator selection.");}
    bool tableFields=requireWavetable;for(auto key:{"table","tablePosition","tableEnv","tableDecay","tableLfo"})tableFields|=value.hasProperty(key);
    if(tableFields){for(auto key:{"table","tablePosition","tableEnv","tableDecay","tableLfo"}){const auto& v=value[key];if(!(v.isInt()||v.isInt64()||v.isDouble())||!std::isfinite((double)v))return Result::fail("Missing or invalid wavetable setting.");}double table=(double)value["table"];if(table!=std::floor(table)||table<0||table>=wavetableCount)return Result::fail("Invalid wavetable family.");p.table=(int)table;
        for(auto key:{"tablePosition","tableEnv","tableDecay","tableLfo"}){double v=(double)value[key];double low=String(key)=="tableEnv"?-1:String(key)=="tableDecay"?.01f:0,high=String(key)=="tableDecay"?8:1;if(v<low||v>high)return Result::fail("Wavetable value is outside its allowed range.");}p.tablePosition=(float)(double)value["tablePosition"];p.tableEnv=(float)(double)value["tableEnv"];p.tableDecay=(float)(double)value["tableDecay"];p.tableLfo=(float)(double)value["tableLfo"];
    }
    p.wave1=(int)value["wave1"];p.wave2=(int)value["wave2"];p.coarse=(int)value["coarse"];auto valid=validateSynthPatch(p);if(valid.failed())return valid;out=p;return Result::ok();
}
void SynthVoice::start(int pitch,double rate,uint32_t seed){*this=SynthVoice{};sampleRate=jlimit(8000.,192000.,rate);frequency=jmin(sampleRate*.45,440.*std::exp2((jlimit(0,127,pitch)-69)/12.));random=seed?seed:12345;tables=preparedWavetables();tableSmoothing=1-std::exp(-1/(sampleRate*.01));alive=true;}
float SynthVoice::process(const SynthPatch& p,int engine,bool gate,float attack,float release){
    bool fm=engine==1,wavetable=engine==2;
    if(!alive)return 0;double step=1/sampleRate;
    if(!gate&&wasGate){stage=3;releaseLevel=env;releasedSeconds=0;}wasGate=gate;
    if(stage==0){env=jmin(1.,seconds/jmax(.001,(double)attack));if(env>=1){stage=1;decaySeconds=0;}}
    else if(stage==1){decaySeconds+=step;double t=jmin(1.,decaySeconds/p.decay);env=1+(smoothSustain-1)*t;if(t>=1)stage=2;}
    else if(stage==2)env=smoothSustain;
    else{releasedSeconds+=step;env=releaseLevel*jmax(0.,1-releasedSeconds/jmax(.02,(double)release));if(env<=.000001){alive=false;return 0;}}
    if(control++%16==0){double smooth=1-std::exp(-16/(sampleRate*.01));
        if(lastWave1<0){mix=p.mix;duty=p.pulse;sub=p.sub;noise=p.noise;smoothCut=p.cutoff;smoothRes=p.resonance;smoothSustain=p.sustain;index=p.fmIndex;lastFm=fm;lastWavetable=wavetable;tablePosition=jlimit(0.,1.,(double)p.tablePosition+p.tableEnv);lastTable=p.table;}
        else if(lastWave1!=p.wave1||lastWave2!=p.wave2||lastFm!=fm||lastWavetable!=wavetable||(wavetable&&lastTable!=p.table)){transition=(int)(sampleRate*.005);anchor=last;}
        lastWave1=p.wave1;lastWave2=p.wave2;lastFm=fm;lastWavetable=wavetable;lastTable=p.table;
        mix+=(p.mix-mix)*smooth;duty+=(p.pulse-duty)*smooth;sub+=(p.sub-sub)*smooth;noise+=(p.noise-noise)*smooth;index+=(p.fmIndex-index)*smooth;smoothCut+=(p.cutoff-smoothCut)*smooth;smoothRes+=(p.resonance-smoothRes)*smooth;smoothSustain+=(p.sustain-smoothSustain)*smooth;
        lfo=std::sin(2*pi*phaseLfo);if(wavetable)tableTarget=jlimit(0.,1.,p.tablePosition+p.tableEnv*std::exp(-seconds/p.tableDecay)+lfo*p.tableLfo);double vibrato=std::exp2(lfo*p.lfoPitch/1200.);dt1=jmin(.45,frequency*vibrato/sampleRate);dt2=jmin(.45,dt1*std::exp2((p.coarse+p.detune/100.)/12.)*(fm?p.fmRatio:1));
        double cut=jlimit(20.,sampleRate*.45,smoothCut*std::exp2(p.filterEnv*std::exp(-seconds/p.filterDecay)+lfo*p.lfoFilter));g=std::tan(pi*cut/sampleRate);r2=1/(.5+smoothRes*7.5);h=1/(1+r2*g+g*g);
    }
    random^=random<<13;random^=random>>17;random^=random<<5;double white=(double)(random&0xffff)/32768.-1;
    double signal;if(wavetable){tablePosition+=(tableTarget-tablePosition)*tableSmoothing;signal=(1-mix)*readWavetable(tables,p.table,tablePosition,phase1,dt1)+mix*readWavetable(tables,p.table,tablePosition,phase2,dt2);}
    else if(lastFm){double amount=jmin(index*std::exp(-seconds/p.fmDecay),jmax(0.,(.45-dt1)/jmax(.000001,dt2)));signal=(1-mix)*std::sin(2*pi*phase1+amount*std::sin(2*pi*phase2))+mix*.5*std::sin(2*pi*phase2);}
    else signal=(1-mix)*wave(lastWave1,phase1,dt1,duty)+mix*wave(lastWave2,phase2,dt2,duty);
    signal=(signal+sub*std::sin(2*pi*phaseSub)+noise*white)/(1+sub+noise);
    double hp=h*(signal-s1*(g+r2)-s2),bp=hp*g+s1,lp=bp*g+s2;s1=std::tanh(hp*g+bp);s2=std::tanh(bp*g+lp);
    double value=std::tanh(lp)*env*.55;if(transition>0){double t=transition/(sampleRate*.005);value=anchor*t+value*(1-t);--transition;}
    if(!std::isfinite(value)){value=0;s1=s2=0;alive=false;}last=value;
    advance(phase1,dt1);advance(phase2,dt2);advance(phaseSub,dt1*.5);advance(phaseLfo,p.lfoRate/sampleRate);seconds+=step;return(float)value;
}
}
