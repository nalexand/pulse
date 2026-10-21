// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
namespace pulse {
enum class FadeCurve { linear, equalPower, sCurve };
constexpr bool validFadeCurve(FadeCurve curve){return (int)curve>=0&&(int)curve<=2;}
inline double audioFadeGain(double progress,FadeCurve curve){
    if(!std::isfinite(progress)||progress<=0)return 0;
    if(progress>=1)return 1;
    if(curve==FadeCurve::equalPower)return std::sin(progress*1.57079632679489661923);
    if(curve==FadeCurve::sCurve)return progress*progress*(3-2*progress);
    return progress;
}
}
