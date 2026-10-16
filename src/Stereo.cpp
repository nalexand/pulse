// SPDX-License-Identifier: AGPL-3.0-only
#include "Stereo.h"
#include <cmath>
namespace pulse {
using namespace juce;
Result validateStereo(const StereoSettings& s){if(!std::isfinite(s.width)||s.width<0||s.width>2||!std::isfinite(s.trim)||s.trim< -24||s.trim>6)return Result::fail("Stereo width must be 0–200% and trim -24 to +6 dB.");return Result::ok();}
var stereoToJson(const StereoSettings& s){auto* v=new DynamicObject();v->setProperty("enabled",s.enabled);v->setProperty("mono",s.mono);v->setProperty("swap",s.swap);v->setProperty("invertLeft",s.invertLeft);v->setProperty("invertRight",s.invertRight);v->setProperty("width",s.width);v->setProperty("trim",s.trim);return var(v);}
Result stereoFromJson(const var& json,StereoSettings& out){if(!json.isObject())return Result::fail("Invalid stereo utility settings.");for(auto key:{"enabled","mono","swap","invertLeft","invertRight"})if(!json[key].isBool())return Result::fail("Invalid stereo switch.");for(auto key:{"width","trim"}){const auto& v=json[key];if(!(v.isDouble()||v.isInt()||v.isInt64())||!std::isfinite((double)v))return Result::fail("Invalid stereo numeric value.");}
    StereoSettings s;s.enabled=(bool)json["enabled"];s.mono=(bool)json["mono"];s.swap=(bool)json["swap"];s.invertLeft=(bool)json["invertLeft"];s.invertRight=(bool)json["invertRight"];double w=(double)json["width"],t=(double)json["trim"];if(w<0||w>2||t< -24||t>6)return Result::fail("Stereo values are outside their allowed range.");s.width=(float)w;s.trim=(float)t;auto result=validateStereo(s);if(result.wasOk())out=s;return result;}
void StereoUtility::prepare(double rate){coefficient=1-std::exp(-1/(juce::jlimit(1000.,768000.,std::isfinite(rate)?rate:48000.)*.02));configured=false;target=current={1,0,0,1};}
void StereoUtility::reset(){current=target;}
void StereoUtility::setSettings(const StereoSettings& s){
    target={1,0,0,1};if(s.enabled){double width=s.mono?0:s.width,a=(1+width)*.5,b=(1-width)*.5,gain=std::pow(10.,s.trim/20.);target={a,b,b,a};if(s.swap){std::swap(target[0],target[1]);std::swap(target[2],target[3]);}if(s.invertLeft){target[0]=-target[0];target[2]=-target[2];}if(s.invertRight){target[1]=-target[1];target[3]=-target[3];}for(auto& v:target)v*=gain;}
    if(!configured){current=target;configured=true;}
}
void StereoUtility::process(float& left,float& right)noexcept{
    for(size_t i=0;i<4;++i){current[i]+=(target[i]-current[i])*coefficient;if(std::abs(target[i]-current[i])<1e-9)current[i]=target[i];}
    if(current==std::array<double,4>{1,0,0,1})return;
    double l=left,r=right;left=(float)(current[0]*l+current[1]*r);right=(float)(current[2]*l+current[3]*r);
}
}
