// SPDX-License-Identifier: AGPL-3.0-only
#include "Wavetable.h"
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
namespace pulse {
namespace {
constexpr int levels=8,stride=wavetableSize+1;
constexpr std::array<int,levels> limits{128,64,32,16,8,4,2,1};
std::atomic<const WavetableBank*> ready{nullptr};
double harmonic(int bank,int frame,int h){
    if(h==1)return 1;
    double n=h,f=frame;
    switch(bank){
    case 0:return frame==0?1/n:frame==1?(h%2?1/n:0):frame==2?(h%2?((h%4==1?1.:-1.)/(n*n)):0):0;
    case 1:return (h%2?1.:.08)*(.3+.7*std::exp(-std::pow((n-(3+f*3))/(2+f),2)))/n;
    case 2:return (.55*std::exp(-std::pow((n-(3+f))/(1.2+f*.2),2))+.4*std::exp(-std::pow((n-(7+f*2))/2,2)))/std::sqrt(n);
    case 3:return ((h+frame)%3==0?.65:.08)*std::exp(-n/(14+f*12))/std::sqrt(n)*(h%4==0?-1:1);
    case 4:return h==2?.15+f*.13:h==3?.45-f*.08:h==4?.1+f*.12:h==6?.08+f*.09:h==8?.05+f*.08:0;
    case 5:return (h==3||h==7||h==11||h==15||h==23)?(.2+f*.15)*std::exp(-n/(8+f*10)):0;
    case 6:return h%(frame+2)==1?.65/std::sqrt(n)*(h%3==0?-1:1):.04/n;
    default:return frame==0?0:frame==1?(h%2?.4/(n*n):0):frame==2?.35/(n*n):.55*std::exp(-std::pow((n-4)/2,2))/n;
    }
}
}
class WavetableBank {
public:
    std::array<float,wavetableCount*wavetableFrames*levels*stride> samples{};
    const float* row(int bank,int frame,int level)const noexcept{return samples.data()+((bank*wavetableFrames+frame)*levels+level)*stride;}
    WavetableBank(){
        std::vector<double> basis(129*wavetableSize);for(int h=1;h<=128;++h)for(int i=0;i<wavetableSize;++i)basis[(size_t)h*wavetableSize+i]=std::sin(2*juce::MathConstants<double>::pi*h*i/wavetableSize);
        for(int bank=0;bank<wavetableCount;++bank){double peak=0;for(int frame=0;frame<wavetableFrames;++frame)for(int level=0;level<levels;++level){auto* data=samples.data()+((bank*wavetableFrames+frame)*levels+level)*stride;for(int h=1;h<=limits[(size_t)level];++h){double amplitude=harmonic(bank,frame,h);if(amplitude==0)continue;for(int i=0;i<wavetableSize;++i)data[i]+=(float)(amplitude*basis[(size_t)h*wavetableSize+i]);}for(int i=0;i<wavetableSize;++i)peak=juce::jmax(peak,(double)std::abs(data[i]));data[wavetableSize]=data[0];}
            float gain=(float)(1/juce::jmax(1.,peak));auto start=(size_t)bank*wavetableFrames*levels*stride;for(size_t i=start;i<start+wavetableFrames*levels*stride;++i)samples[i]*=gain;
        }
    }
};
void prepareWavetables(){static const WavetableBank bank;ready.store(&bank,std::memory_order_release);}
const WavetableBank* preparedWavetables()noexcept{return ready.load(std::memory_order_acquire);}
const char* wavetableName(int index){static const char* names[]{"Classic","Hollow","Vocal","Metal","Organ","Glass","Digital","Soft"};return names[juce::jlimit(0,wavetableCount-1,index)];}
float readWavetable(const WavetableBank* bank,int table,double position,double phase,double step)noexcept{
    if(!bank||table<0||table>=wavetableCount||!std::isfinite(position)||!std::isfinite(phase)||!std::isfinite(step)||step<=0||step>.5)return 0;
    double available=.45/step;int level=0;while(level<levels-1&&limits[(size_t)level]>available)++level;
    double blend=level==levels-1?1:juce::jlimit(0.,1.,available/limits[(size_t)level]-1);
    double frame=juce::jlimit(0.,1.,position)*(wavetableFrames-1),sample=(phase-std::floor(phase))*wavetableSize;int first=juce::jmin(wavetableFrames-2,(int)frame),at=juce::jmin(wavetableSize-1,(int)sample);double morph=frame-first,fraction=sample-at;
    auto read=[&](int f,int m){const auto* row=bank->row(table,f,m);return row[at]+fraction*(row[at+1]-row[at]);};
    auto mix=[&](int m){double a=read(first,m),b=read(first+1,m);return a+(b-a)*morph;};
    double value=mix(level);if(blend<1){double lower=mix(level+1);value=lower+(value-lower)*blend;}return(float)value;
}
}
