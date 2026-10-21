// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
namespace pulse {
enum class AutomationCurve { linear, smooth, easeIn, easeOut };
inline bool validAutomationCurve(AutomationCurve c){return (int)c>=0&&(int)c<=3;}
inline const char* automationCurveName(AutomationCurve c){static constexpr const char* names[]={"Linear","Smooth","Ease in","Ease out"};return validAutomationCurve(c)?names[(int)c]:"Invalid";}
// Curve belongs to the outgoing segment. Tension warps time, never the value range.
inline double automationShape(double t,AutomationCurve curve,double tension=0) noexcept {
    if(!std::isfinite(t)||!std::isfinite(tension)||!validAutomationCurve(curve))return 0;
    t=std::clamp(t,0.,1.);if(t==0||t==1)return t;
    if(tension!=0)t=std::pow(t,std::exp2(std::clamp(tension,-1.,1.)*4));
    switch(curve){case AutomationCurve::smooth:return t*t*(3-2*t);case AutomationCurve::easeIn:return t*t;case AutomationCurve::easeOut:return 1-(1-t)*(1-t);default:return t;}
}
}
