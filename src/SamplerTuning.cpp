// SPDX-License-Identifier: AGPL-3.0-only
#include "SamplerTuning.h"
#include "WaveTools.h"
#include <algorithm>
#include <vector>
namespace pulse {
using namespace juce;
String samplerNoteName(int pitch){static const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};return pitch>=0&&pitch<=127?String(names[pitch%12])+String(pitch/12-1):"?";}
Result validateSamplerTuning(int root,double cents){return root<0||root>127||!std::isfinite(cents)||cents< -100||cents>100?Result::fail("Sampler root must be MIDI 0-127 and fine tune -100 to +100 cents."):Result::ok();}
Result detectSamplePitch(const AudioBuffer<float>& source,double rate,int first,int end,SamplePitch& output,const std::function<bool()>& cancelled){
    auto stop=[&]{return cancelled&&cancelled();};if(stop())return Result::fail("Sample pitch detection cancelled.");
    if(!std::isfinite(rate)||rate<8000||rate>192000||source.getNumChannels()<1||source.getNumChannels()>2||first<0||end>source.getNumSamples()||end<=first)return Result::fail("Choose a valid mono/stereo sample range.");
    end=jmin(end,first+(int)(rate*6));if(end-first<rate*.12)return Result::fail("Pitch detection needs at least 120 ms of a sustained note.");
    // Preserve anti-phase stereo: analyse the channel with most AC energy, rather than summing L/R.
    int channel=0;double bestEnergy=0;for(int ch=0;ch<source.getNumChannels();++ch){double sum=0,square=0;for(int i=first;i<end;++i){if((i&4095)==0&&stop())return Result::fail("Sample pitch detection cancelled.");double v=source.getSample(ch,i);if(!std::isfinite(v))return Result::fail("Sample contains non-finite audio.");sum+=v;square+=v*v;}double energy=square/(end-first)-std::pow(sum/(end-first),2);if(energy>bestEnergy){bestEnergy=energy;channel=ch;}}
    if(bestEnergy<1.e-8)return Result::fail("No stable pitch: the range is silent or contains only DC.");
    AudioBuffer<float> selected(2,end-first);for(int ch=0;ch<2;++ch)selected.copyFrom(ch,0,source,channel,first,end-first);
    AudioBuffer<float> audio;auto converted=resampleWave(selected,rate,24000,audio,cancelled);if(converted.failed())return converted;
    int frames=audio.getNumSamples(),maxLag=jmin(750,frames/3),n=jmin(4096,frames-maxLag),window=n+maxLag;if(n<1024)return Result::fail("Sample range is too short for stable pitch detection.");
    std::vector<double> signal((size_t)frames);double mean=0;for(int i=0;i<frames;++i)mean+=audio.getSample(0,i);mean/=frames;for(int i=0;i<frames;++i)signal[(size_t)i]=audio.getSample(0,i)-mean;
    struct Window{int start;double energy;};std::vector<Window> windows;for(int start=0;;start+=jmax(512,n/2)){start=jmin(start,frames-window);double energy=0;for(int i=0;i<window;++i)energy+=signal[(size_t)(start+i)]*signal[(size_t)(start+i)];windows.push_back({start,energy/window});if(start==frames-window)break;}
    std::stable_sort(windows.begin(),windows.end(),[](auto a,auto b){return a.energy>b.energy;});std::vector<Window> chosen;for(auto w:windows){bool distinct=true;for(auto prior:chosen)distinct&=std::abs(w.start-prior.start)>=n/2;if(distinct&&w.energy>=bestEnergy*.05)chosen.push_back(w);if(chosen.size()==3)break;}
    struct Candidate{double midi,quality;};std::vector<Candidate> candidates;
    for(auto w:chosen){std::vector<double> difference((size_t)maxLag+1,0),normal((size_t)maxLag+1,1);double cumulative=0;for(int lag=1;lag<=maxLag;++lag){if((lag&31)==0&&stop())return Result::fail("Sample pitch detection cancelled.");double sum=0;for(int i=0;i<n;++i){double d=signal[(size_t)(w.start+i)]-signal[(size_t)(w.start+i+lag)];sum+=d*d;}difference[(size_t)lag]=sum;cumulative+=sum;normal[(size_t)lag]=cumulative>1.e-20?sum*lag/cumulative:1;}
        int lag=0;for(int tau=2;tau<maxLag;++tau)if(normal[(size_t)tau]<.14){while(tau+1<maxLag&&normal[(size_t)tau+1]<normal[(size_t)tau])++tau;lag=tau;break;}if(!lag)continue;
        double a=difference[(size_t)lag-1],b=difference[(size_t)lag],c=difference[(size_t)lag+1],denominator=a-2*b+c;double adjusted=lag+(std::abs(denominator)>1.e-20?jlimit(-.5,.5,.5*(a-c)/denominator):0);double frequency=24000/adjusted;
        if(frequency<32||frequency>2000)continue;candidates.push_back({69+12*std::log2(frequency/440),1-normal[(size_t)lag]});
    }
    if(stop())return Result::fail("Sample pitch detection cancelled.");if(candidates.empty())return Result::fail("No stable single-note pitch found. Use manual root and fine tune.");
    std::vector<Candidate> consensus;for(auto centre:candidates){std::vector<Candidate> group;for(auto row:candidates)if(std::abs(row.midi-centre.midi)<.35)group.push_back(row);if(group.size()>consensus.size())consensus=std::move(group);}
    if(chosen.size()>1&&consensus.size()<2)return Result::fail("Pitch varies across the range. Select a steadier single note or set the root manually.");
    double midi=0,quality=0;for(auto row:consensus){midi+=row.midi;quality+=row.quality;}midi/=consensus.size();quality/=consensus.size();int root=(int)std::round(midi);if(root<0||root>127)return Result::fail("Detected pitch lies outside MIDI note range.");
    // Fine tune is a playback correction: a sharp recording receives a negative correction.
    SamplePitch result{root,std::round((root-midi)*1000)*.1,440*std::pow(2.,(midi-69)/12.),quality,(int)consensus.size()};output=result;return Result::ok();
}
}
