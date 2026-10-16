// SPDX-License-Identifier: AGPL-3.0-only
#include "Chorus.h"
#include <cmath>
namespace pulse {
using namespace juce;
const std::array<ChorusParameterSpec,6>& chorusParameterSpecs(){
    static const std::array<ChorusParameterSpec,6> specs{{
        {"Wet mix",0,1,.35,false,""},{"Rate",.05,8,.8,true," Hz"},
        {"Depth",0,1,.45,false,""},{"Centre delay",5,35,15,false," ms"},
        {"Feedback",-.8,.8,.1,false,""},{"Stereo width",0,1,1,false,""}}};
    return specs;
}
double chorusValue(const ChorusSettings& p,int i){switch(i){case 0:return p.mix;case 1:return p.rate;case 2:return p.depth;case 3:return p.delay;case 4:return p.feedback;case 5:return p.width;default:return 0;}}
double chorusNormal(int i,double value){if(i<0||i>=6||!std::isfinite(value))return 0;const auto& s=chorusParameterSpecs()[(size_t)i];return jlimit(0.,1.,s.logarithmic?std::log(value/s.low)/std::log(s.high/s.low):(value-s.low)/(s.high-s.low));}
double chorusFromNormal(int i,double normal){if(i<0||i>=6||!std::isfinite(normal))return 0;const auto& s=chorusParameterSpecs()[(size_t)i];normal=jlimit(0.,1.,normal);return s.logarithmic?s.low*std::pow(s.high/s.low,normal):s.low+normal*(s.high-s.low);}
Result validateChorus(const ChorusSettings& p){for(int i=0;i<6;++i){auto v=chorusValue(p,i);const auto& s=chorusParameterSpecs()[(size_t)i];if(!std::isfinite(v)||v<(double)(float)s.low||v>(double)(float)s.high)return Result::fail("Chorus contains an invalid or out-of-range parameter.");}return Result::ok();}
Result setChorusValue(ChorusSettings& p,int i,double value){if(i<0||i>=6||!std::isfinite(value))return Result::fail("Invalid chorus parameter.");const auto& s=chorusParameterSpecs()[(size_t)i];if(value<s.low||value>s.high)return Result::fail("Chorus value is outside its allowed range.");auto next=p;switch(i){case 0:next.mix=(float)value;break;case 1:next.rate=(float)value;break;case 2:next.depth=(float)value;break;case 3:next.delay=(float)value;break;case 4:next.feedback=(float)value;break;case 5:next.width=(float)value;break;}auto valid=validateChorus(next);if(valid.wasOk())p=next;return valid;}
var chorusToJson(const ChorusSettings& p){auto* o=new DynamicObject();o->setProperty("enabled",p.enabled);
#define WRITE(name) o->setProperty(#name,p.name);
    WRITE(mix) WRITE(rate) WRITE(depth) WRITE(delay) WRITE(feedback) WRITE(width)
#undef WRITE
    return var(o);
}
Result chorusFromJson(const var& value,ChorusSettings& out){if(!value.isObject()||!value["enabled"].isBool())return Result::fail("Invalid chorus settings object.");ChorusSettings p;p.enabled=(bool)value["enabled"];
#define READ(name) {const auto& v=value[#name];if(!(v.isInt()||v.isInt64()||v.isDouble())||!std::isfinite((double)v))return Result::fail("Missing or invalid chorus setting: " #name);p.name=(float)(double)v;}
    READ(mix) READ(rate) READ(depth) READ(delay) READ(feedback) READ(width)
#undef READ
    auto valid=validateChorus(p);if(valid.failed())return valid;out=p;return Result::ok();
}
void StereoChorus::prepare(double sampleRate){rate=jlimit(8000.,192000.,sampleRate);for(auto& channel:history)channel.assign((size_t)std::ceil(rate*.046)+4,0);coefficient=1-std::exp(-1/(rate*.02));fadeCoefficient=1-std::exp(-1/(rate*.01));reset();}
void StereoChorus::reset(){write=filled=0;phase=wetGain=0;running=false;for(int i=0;i<6;++i)smooth[(size_t)i]=chorusValue(target,i);}
void StereoChorus::setSettings(const ChorusSettings& p){if(validateChorus(p).wasOk())target=p;}
float StereoChorus::read(int channel,double samples)const noexcept{
    if(std::ceil(samples)>filled)return 0;double position=(double)write-samples;const auto& buffer=history[(size_t)channel];if(position<0)position+=buffer.size();auto a=(size_t)position,b=(a+1)%buffer.size();double f=position-a;return(float)(buffer[a]+(buffer[b]-buffer[a])*f);
}
void StereoChorus::process(float& left,float& right)noexcept{
    if(history[0].empty()||(!target.enabled&&!running))return;
    if(!running){for(int i=0;i<6;++i)smooth[(size_t)i]=chorusValue(target,i);running=true;}
    for(int i=0;i<6;++i)smooth[(size_t)i]+=(chorusValue(target,i)-smooth[(size_t)i])*coefficient;
    wetGain+=((target.enabled?1.:0.)-wetGain)*fadeCoefficient;
    if(!target.enabled&&wetGain<.000001){reset();return;}
    if(target.enabled&&wetGain> .999999)wetGain=1;
    const double excursion=jmin(10.,smooth[3]-1)*smooth[2];
    float input[2]={left,right},wet[2];
    for(int ch=0;ch<2;++ch){double lfo=std::sin(2*MathConstants<double>::pi*(phase+ch*smooth[5]*.25));double delaySamples=(smooth[3]+excursion*lfo)*rate*.001;wet[ch]=read(ch,delaySamples);history[(size_t)ch][write]=input[ch]+wet[ch]*(float)smooth[4];}
    double mix=smooth[0]*wetGain;if(mix<1e-9)mix=0;
    left=(float)(input[0]*(1-mix)+wet[0]*mix);right=(float)(input[1]*(1-mix)+wet[1]*mix);
    write=(write+1)%history[0].size();filled=jmin(filled+1,history[0].size());phase+=smooth[1]/rate;if(phase>=1)phase-=1;
}
}
