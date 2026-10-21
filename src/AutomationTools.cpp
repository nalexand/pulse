// SPDX-License-Identifier: AGPL-3.0-only
#include "AutomationTools.h"
#include <cmath>
namespace pulse {
using namespace juce;
const char* automationWaveName(AutomationWave w){static constexpr const char* names[]={"Sine LFO","Triangle LFO","Square LFO","Step sequence","Seeded random hold","Seeded random smooth"};return (int)w>=0&&(int)w<6?names[(int)w]:"Invalid";}
Result validateAutomationPoints(const std::vector<AutomationPoint>& points,double length,bool endpoints){
    if(!std::isfinite(length)||length<=0||length>600||points.size()<2||points.size()>maxAutomationPoints)return Result::fail("Automation needs 2-256 points in a clip up to 600 beats.");
    double previous=-1;for(const auto& p:points){if(!std::isfinite(p.beat)||p.beat<0||p.beat>600||p.beat<=previous||!std::isfinite(p.value)||p.value<0||p.value>1||!validAutomationCurve(p.curve)||!std::isfinite(p.tension)||p.tension< -1||p.tension>1)return Result::fail("Automation points need increasing beats, values 0-1 and valid curve/tension settings.");previous=p.beat;}
    if(endpoints&&(points.front().beat!=0||points.back().beat!=length))return Result::fail("Automation must include both clip endpoints.");return Result::ok();
}
namespace {
double randomValue(uint32_t seed,uint32_t cell){uint32_t x=seed^(cell+1)*0x9e3779b9u;x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return (double)(x>>8)/16777215.;}
}
Result generateAutomation(double length,const AutomationGenerator& s,std::vector<AutomationPoint>& output){
    auto finite=[](double x,double lo,double hi){return std::isfinite(x)&&x>=lo&&x<=hi;};
    if(!finite(length,.0625,600)||(int)s.wave<0||(int)s.wave>5||!finite(s.period,.0625,600)||!finite(s.phase,0,1)||!finite(s.low,0,1)||!finite(s.high,0,1)||s.low>s.high||s.resolution<4||s.resolution>64||s.resolution%2||s.steps.size()<2||s.steps.size()>32)return Result::fail("Use period 0.0625-600 beats, phase 0-1, low <= high in 0-1, an even resolution 4-64 and 2-32 step values.");
    for(float v:s.steps)if(!finite(v,0,1))return Result::fail("Step values must be finite numbers from 0 to 1.");
    int cells=s.wave==AutomationWave::triangle||s.wave==AutomationWave::square?2:s.wave==AutomationWave::steps?(int)s.steps.size():s.resolution;
    double span=s.period/cells,phase=s.phase==1?0:s.phase;
    // Check capacity before allocating or iterating even for extreme valid periods.
    double start=phase*cells,finish=start+length/span;
    int first=(int)std::floor(start)+1,last=(int)std::ceil(finish)-1;
    if(last-first+3>maxAutomationPoints)return Result::fail("Generator exceeds 256 points. Increase period or reduce resolution/step count.");
    auto raw=[&](double cell){double p=cell/cells;double f=p-std::floor(p);uint32_t index=(uint32_t)std::floor(cell+1e-10);
        switch(s.wave){case AutomationWave::sine:return .5-.5*std::cos(6.28318530717958647692*f);case AutomationWave::triangle:return 1-std::abs(2*f-1);case AutomationWave::square:return f<.5?0.:1.;case AutomationWave::steps:return (double)s.steps[(size_t)(index%(uint32_t)cells)];default:return randomValue(s.seed,index);}};
    bool hold=s.wave==AutomationWave::square||s.wave==AutomationWave::steps||s.wave==AutomationWave::randomHold;
    auto value=[&](double cell){double v;if(hold)v=raw(std::floor(cell+1e-10));else{double a=std::floor(cell),t=cell-a;double fraction=s.wave==AutomationWave::randomSmooth?automationShape(t,AutomationCurve::smooth):t;v=raw(a)+(raw(a+1)-raw(a))*fraction;}return(float)jlimit(s.low,s.high,s.low+(s.high-s.low)*v);};
    std::vector<AutomationPoint> next;auto add=[&](double beat,double cell){AutomationPoint p{beat,value(cell),hold};if(s.wave==AutomationWave::randomSmooth)p.curve=AutomationCurve::smooth;next.push_back(p);};
    add(0,start);for(int k=first;k<=last;++k){double beat=(k-start)*span;if(beat>1e-10&&beat<length-1e-10)add(beat,k);}add(length,finish);
    auto valid=validateAutomationPoints(next,length);if(valid.failed())return valid;output=std::move(next);return Result::ok();
}
Result replaceAutomationPoints(Project& project,int clip,const std::vector<AutomationPoint>& points,bool endpoints){
    if(clip<0||clip>=(int)project.clips.size()||!project.clips[(size_t)clip].isAutomation())return Result::fail("Choose an existing automation clip.");
    auto valid=validateAutomationPoints(points,project.clips[(size_t)clip].length,endpoints);if(valid.failed())return valid;
    size_t total=points.size();for(int i=0;i<(int)project.clips.size();++i)if(i!=clip&&project.clips[(size_t)i].isAutomation())total+=project.clips[(size_t)i].points.size();if(total>maxTotalAutomationPoints)return Result::fail("The project supports 4096 automation points.");
    project.clips[(size_t)clip].points=points;return Result::ok();
}
}
