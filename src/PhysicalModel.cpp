// SPDX-License-Identifier: AGPL-3.0-only
#include "PhysicalModel.h"
#include <algorithm>
#include <cmath>
namespace pulse {
namespace {constexpr double pi=3.14159265358979323846;}
void PhysicalVoice::start(double sampleRate){
    *this=PhysicalVoice{};rate=std::clamp(sampleRate,8000.,192000.);smoothing=1-std::exp(-1/(rate*.005));
    constexpr double frequencies[]={180,430,970};
    for(int i=0;i<3;++i){auto& m=body[(size_t)i];double r=std::exp(-1/(rate*.035)),w=2*pi*std::min(rate*.4,frequencies[i])/rate;m.real=m.imag=0;m.c=r*std::cos(w);m.s=r*std::sin(w);m.gain=1-r;}
}
double PhysicalVoice::process(const PhysicalSettings& p,double frequency){
    if(frame++%32==0&&(lastFrequency!=frequency||cached!=p)){
        cached=p;lastFrequency=frequency;
        double sum=0;
        // Free-bar modal ratios (the approximation continues the measured roots).
        constexpr double bars[]={1,2.75654,5.40392,8.93295,13.3443,18.6389};
        for(int i=0;i<16;++i){auto& m=modes[(size_t)i];double n=i+1,ratio=p.mode==0?n*std::sqrt((1+p.stiffness*.002*n*n)/(1+p.stiffness*.002)):(i<6?bars[i]:std::pow((2*n+1)/3.0112,2));
            double hz=frequency*ratio,window=std::clamp((rate*.45-hz)/(rate*.05),0.,1.);
            double excitation=std::pow(std::sin(pi*n*p.position),2);
            m.target=window*excitation*std::exp(-(1-p.brightness)*n*.6)/(p.mode==0?n:std::sqrt(n));sum+=m.target;
            double t60=p.decay/(1+p.damping*(n*n-1)*.12),r=std::exp(-std::log(1000.)/(rate*t60)),w=2*pi*std::min(hz,rate*.45)/rate;
            m.c=r*std::cos(w);m.s=r*std::sin(w);
        }
        double normal=1/std::max(1e-12,sum);for(auto& m:modes){m.target*=normal;if(frame==1)m.gain=m.target;}
        if(frame==1)bodyMix=p.body;
    }
    double value=0;for(auto& m:modes){double real=m.real*m.c-m.imag*m.s;m.imag=m.real*m.s+m.imag*m.c;m.real=real;m.gain+=(m.target-m.gain)*smoothing;value+=m.real*m.gain;}
    double resonance=0;for(auto& m:body){double real=m.real*m.c-m.imag*m.s+value*m.gain;m.imag=m.real*m.s+m.imag*m.c;m.real=real;resonance+=real/3;}
    bodyMix+=(p.body-bodyMix)*smoothing;double result=value*(1-.5*bodyMix)+.5*bodyMix*resonance;
    return std::isfinite(result)?result:0;
}
}
