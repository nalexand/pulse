// SPDX-License-Identifier: AGPL-3.0-only
#include "Modulation.h"
#include <cmath>
namespace pulse {
using namespace juce;
const char* modulationName(ModulationKind kind){return kind==ModulationKind::flanger?"Flanger":"Phaser";}
ModulationSettings defaultModulation(ModulationKind kind){ModulationSettings p;if(kind==ModulationKind::phaser){p.rate=.4f;p.depth=.7f;p.centre=800;p.feedback=.3f;}return p;}
const std::array<ChorusParameterSpec,6>& modulationSpecs(ModulationKind kind){
    static const std::array<ChorusParameterSpec,6> flanger{{{"Wet mix",0,1,.5,false,""},{"Rate",.02,5,.25,true," Hz"},{"Depth",0,1,.6,false,""},{"Centre delay",.2,8,2,true," ms"},{"Feedback",-.85,.85,.5,false,""},{"Stereo width",0,1,1,false,""}}};
    static const std::array<ChorusParameterSpec,6> phaser{{{"Wet mix",0,1,.5,false,""},{"Rate",.02,5,.4,true," Hz"},{"Depth",0,1,.7,false,""},{"Centre frequency",100,4000,800,true," Hz"},{"Feedback",-.8,.8,.3,false,""},{"Stereo width",0,1,1,false,""}}};
    return kind==ModulationKind::flanger?flanger:phaser;
}
double modulationValue(const ModulationSettings& p,int i){switch(i){case 0:return p.mix;case 1:return p.rate;case 2:return p.depth;case 3:return p.centre;case 4:return p.feedback;case 5:return p.width;default:return 0;}}
double modulationNormal(ModulationKind kind,int i,double v){if(i<0||i>=6||!std::isfinite(v))return 0;auto s=modulationSpecs(kind)[(size_t)i];v=jlimit(s.low,s.high,v);return jlimit(0.,1.,s.logarithmic?std::log(v/s.low)/std::log(s.high/s.low):(v-s.low)/(s.high-s.low));}
double modulationFromNormal(ModulationKind kind,int i,double n){if(i<0||i>=6||!std::isfinite(n))return 0;auto s=modulationSpecs(kind)[(size_t)i];n=jlimit(0.,1.,n);return s.logarithmic?s.low*std::pow(s.high/s.low,n):s.low+n*(s.high-s.low);}
Result validateModulation(const ModulationSettings& p,ModulationKind kind){if(kind!=ModulationKind::flanger&&kind!=ModulationKind::phaser)return Result::fail("Unknown modulation effect.");for(int i=0;i<6;++i){auto v=modulationValue(p,i);auto s=modulationSpecs(kind)[(size_t)i];if(!std::isfinite(v)||v<(double)(float)s.low||v>(double)(float)s.high)return Result::fail(String(modulationName(kind))+" contains an invalid or out-of-range value.");}return Result::ok();}
Result setModulationValue(ModulationSettings& p,ModulationKind kind,int i,double v){if(i<0||i>=6||!std::isfinite(v))return Result::fail("Invalid modulation parameter.");auto spec=modulationSpecs(kind)[(size_t)i];if(v<spec.low||v>spec.high)return Result::fail("Value is outside the allowed modulation range.");auto next=p;switch(i){case 0:next.mix=(float)v;break;case 1:next.rate=(float)v;break;case 2:next.depth=(float)v;break;case 3:next.centre=(float)v;break;case 4:next.feedback=(float)v;break;case 5:next.width=(float)v;break;}auto valid=validateModulation(next,kind);if(valid.wasOk())p=next;return valid;}
var modulationToJson(const ModulationSettings& p){auto* o=new DynamicObject();o->setProperty("enabled",p.enabled);
#define WRITE(name) o->setProperty(#name,p.name);
    WRITE(mix) WRITE(rate) WRITE(depth) WRITE(centre) WRITE(feedback) WRITE(width)
#undef WRITE
    return var(o);
}
Result modulationFromJson(const var& json,ModulationKind kind,ModulationSettings& out){if(!json.isObject()||!json["enabled"].isBool())return Result::fail("Invalid modulation settings object.");auto p=defaultModulation(kind);p.enabled=(bool)json["enabled"];
#define READ(name) {const auto& v=json[#name];if(!(v.isInt()||v.isInt64()||v.isDouble())||!std::isfinite((double)v))return Result::fail("Missing or invalid modulation setting: " #name);p.name=(float)(double)v;}
    READ(mix) READ(rate) READ(depth) READ(centre) READ(feedback) READ(width)
#undef READ
    auto result=validateModulation(p,kind);if(result.wasOk())out=p;return result;
}
void StereoFlanger::prepare(double sampleRate){rate=jlimit(8000.,192000.,sampleRate);for(auto& ch:history)ch.assign((size_t)std::ceil(rate*.017)+4,0);coefficient=1-std::exp(-1/(rate*.02));fadeCoefficient=1-std::exp(-1/(rate*.01));reset();}
void StereoFlanger::reset(){write=filled=0;phase=wetGain=0;running=false;for(int i=0;i<6;++i)smooth[(size_t)i]=modulationValue(target,i);}
void StereoFlanger::setSettings(const ModulationSettings& p){if(validateModulation(p,ModulationKind::flanger).wasOk())target=p;}
float StereoFlanger::read(int ch,double samples)const noexcept{if(std::ceil(samples)>filled)return 0;const auto& buffer=history[(size_t)ch];double position=(double)write-samples;if(position<0)position+=buffer.size();auto a=(size_t)position,b=(a+1)%buffer.size();return(float)(buffer[a]+(buffer[b]-buffer[a])*(position-a));}
void StereoFlanger::process(float& left,float& right)noexcept{
    if(history[0].empty()||(!target.enabled&&!running))return;if(!running){for(int i=0;i<6;++i)smooth[(size_t)i]=modulationValue(target,i);running=true;}
    for(int i=0;i<6;++i)smooth[(size_t)i]+=(modulationValue(target,i)-smooth[(size_t)i])*coefficient;
    wetGain+=((target.enabled?1.:0.)-wetGain)*fadeCoefficient;if(!target.enabled&&wetGain<.000001){reset();return;}if(target.enabled&&wetGain>.999999)wetGain=1;
    float in[2]={left,right},wet[2];for(int ch=0;ch<2;++ch){double lfo=std::sin(2*MathConstants<double>::pi*(phase+ch*smooth[5]*.25));double delay=jmax(.1,smooth[3]*(1+.95*smooth[2]*lfo));wet[ch]=read(ch,jmax(1.,delay*rate*.001));history[(size_t)ch][write]=in[ch]+wet[ch]*(float)smooth[4];}
    double mix=smooth[0]*wetGain;if(mix<1e-9)mix=0;left=(float)(in[0]*(1-mix)+wet[0]*mix);right=(float)(in[1]*(1-mix)+wet[1]*mix);
    write=(write+1)%history[0].size();filled=jmin(filled+1,history[0].size());phase+=smooth[1]/rate;if(phase>=1)phase-=1;
}
void StereoPhaser::prepare(double sampleRate){rate=jlimit(8000.,192000.,sampleRate);coefficient=1-std::exp(-1/(rate*.02));fadeCoefficient=1-std::exp(-1/(rate*.01));reset();}
void StereoPhaser::reset(){memory={};last={};phase=wetGain=0;running=false;for(int i=0;i<6;++i)smooth[(size_t)i]=modulationValue(target,i);}
void StereoPhaser::setSettings(const ModulationSettings& p){if(validateModulation(p,ModulationKind::phaser).wasOk())target=p;}
void StereoPhaser::process(float& left,float& right)noexcept{
    if(!target.enabled&&!running)return;if(!running){for(int i=0;i<6;++i)smooth[(size_t)i]=modulationValue(target,i);running=true;}
    for(int i=0;i<6;++i)smooth[(size_t)i]+=(modulationValue(target,i)-smooth[(size_t)i])*coefficient;
    wetGain+=((target.enabled?1.:0.)-wetGain)*fadeCoefficient;if(!target.enabled&&wetGain<.000001){reset();return;}if(target.enabled&&wetGain>.999999)wetGain=1;
    float in[2]={left,right};double wet[2];for(int ch=0;ch<2;++ch){double lfo=std::sin(2*MathConstants<double>::pi*(phase+ch*smooth[5]*.25));double f=jlimit(20.,rate*.4,smooth[3]*std::exp2(2.5*smooth[2]*lfo));double g=std::tan(MathConstants<double>::pi*f/rate),a=(g-1)/(g+1);double x=in[ch]+smooth[4]*std::tanh(last[(size_t)ch]);
        for(auto& z:memory[(size_t)ch]){double y=a*x+z;z=x-a*y;x=y;if(std::abs(z)<1e-25)z=0;}wet[ch]=x;last[(size_t)ch]=x;}
    double mix=smooth[0]*wetGain;if(mix<1e-9)mix=0;left=(float)(in[0]*(1-mix)+wet[0]*mix);right=(float)(in[1]*(1-mix)+wet[1]*mix);phase+=smooth[1]/rate;if(phase>=1)phase-=1;
}
}
