// SPDX-License-Identifier: AGPL-3.0-only
#include "SpectralEditing.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
bool stopped(const std::atomic<bool>* c){return c&&c->load();}
constexpr double pi=MathConstants<double>::pi;
float windowAt(int i,int n){return (float)(.5-.5*std::cos(2*pi*i/n));}
double maskAt(double hz,double bin,double nyquist,const SpectralOptions& o){
    if(hz<o.lowHz||hz>o.highHz)return 0;
    double skirt=jmin(2*bin,(o.highHz-o.lowHz)*.5),weight=1;
    if(o.lowHz>0&&hz<o.lowHz+skirt)weight*=.5-.5*std::cos(pi*(hz-o.lowHz)/skirt);
    if(o.highHz<nyquist&&hz>o.highHz-skirt)weight*=.5-.5*std::cos(pi*(o.highHz-hz)/skirt);
    return weight;
}
}
Result validateSpectral(const SpectralOptions& o){
    if(!std::isfinite(o.lowHz)||!std::isfinite(o.highHz)||!std::isfinite(o.gainDb)
       ||o.lowHz<0||o.highHz<=o.lowHz||o.highHz>96000||o.gainDb< -96||o.gainDb>0
       ||o.fftOrder<10||o.fftOrder>13)
        return Result::fail("Use an ordered 0-96000 Hz band, attenuation -96 to 0 dB and FFT size 1024-8192.");
    return Result::ok();
}
Result editSpectral(const AudioBuffer<float>& input,double rate,int first,int end,
                    const SpectralOptions& o,double strength,AudioBuffer<float>& output,
                    std::atomic<bool>* cancel,std::atomic<float>* progress){
    if(progress)progress->store(0);
    if(stopped(cancel))return Result::fail("Spectral edit cancelled.");
    auto valid=validateSpectral(o);if(valid.failed())return valid;
    valid=validateWaveAudio(input,rate);if(valid.failed())return valid;
    const int n=1<<o.fftOrder,hop=n/4,length=end-first;
    if(first<0||end>input.getNumSamples()||length<n||length>rate*30
       ||o.highHz>rate*.5||!std::isfinite(strength)||strength<0||strength>1)
        return Result::fail("Select one FFT window to 30 seconds, a band below Nyquist and strength 0-100%.");
    AudioBuffer<float> next;next.makeCopyOf(input);
    if(o.gainDb==0||strength==0){output=std::move(next);if(progress)progress->store(1);return Result::ok();}
    dsp::FFT fft(o.fftOrder);
    std::vector<float> window((size_t)n),work((size_t)n*2),gain((size_t)n/2+1);
    std::vector<double> delta((size_t)length),weights((size_t)length);
    for(int i=0;i<n;++i)window[(size_t)i]=windowAt(i,n);
    double wetGain=std::pow(10.,o.gainDb/20)-1;
    for(int k=0;k<=n/2;++k)gain[(size_t)k]=(float)(wetGain*maskAt(k*rate/n,rate/n,rate*.5,o));
    const int blocks=(length+n-hop+hop-1)/hop;
    for(int ch=0;ch<2;++ch){
        std::fill(delta.begin(),delta.end(),0);std::fill(weights.begin(),weights.end(),0);
        int block=0;
        for(int start=first-n+hop;start<end;start+=hop,++block){
            if(stopped(cancel))return Result::fail("Spectral edit cancelled.");
            std::fill(work.begin(),work.end(),0.f);
            for(int i=0;i<n;++i){int source=start+i;if(source>=0&&source<input.getNumSamples())work[(size_t)i]=input.getSample(ch,source)*window[(size_t)i];}
            fft.performRealOnlyForwardTransform(work.data(),true);
            for(int k=0;k<=n/2;++k){work[(size_t)k*2]*=gain[(size_t)k];work[(size_t)k*2+1]*=gain[(size_t)k];}
            fft.performRealOnlyInverseTransform(work.data());
            for(int i=jmax(0,first-start);i<jmin(n,end-start);++i){int target=start+i-first;double w=window[(size_t)i];delta[(size_t)target]+=work[(size_t)i]*w;weights[(size_t)target]+=w*w;}
            if(progress)progress->store((ch+block/(float)blocks)*.5f);
        }
        int fade=jmax(1,(int)std::llround(rate*.005));
        for(int i=0;i<length;++i){
            if((i&4095)==0&&stopped(cancel))return Result::fail("Spectral edit cancelled.");
            double wet=strength;
            if(first>0)wet*=jlimit(0.,1.,i/(double)fade);
            if(end<input.getNumSamples())wet*=jlimit(0.,1.,(length-1-i)/(double)fade);
            float value=(float)(input.getSample(ch,first+i)+wet*delta[(size_t)i]/weights[(size_t)i]);
            if(!std::isfinite(value))return Result::fail("Spectral edit exceeds finite sample range. Source retained.");
            next.setSample(ch,first+i,value);
        }
    }
    if(stopped(cancel))return Result::fail("Spectral edit cancelled.");
    output=std::move(next);if(progress)progress->store(1);return Result::ok();
}
Result spectralImages(const AudioBuffer<float>& original,const AudioBuffer<float>& edited,
                      double rate,int first,int end,int order,SpectralImages& output,std::atomic<bool>* cancel){
    auto valid=validateWaveAudio(original,rate);if(valid.failed())return valid;
    valid=validateWaveAudio(edited,rate);if(valid.failed())return valid;
    if(first<0||end>original.getNumSamples()||end<=first||edited.getNumSamples()!=original.getNumSamples()||order<10||order>13)
        return Result::fail("Invalid spectrogram source or selection.");
    SpectralImages next;next.sampleRate=rate;next.firstSeconds=first/rate;next.endSeconds=end/rate;
    next.original.resize(SpectralImages::columns*SpectralImages::rows);next.edited.resize(next.original.size());
    int n=1<<order;dsp::FFT fft(order);std::vector<float> work((size_t)n*2),window((size_t)n);
    std::vector<double> power((size_t)n/2+1);double sum=0;
    for(int i=0;i<n;++i){window[(size_t)i]=windowAt(i,n);sum+=window[(size_t)i];}
    for(int version=0;version<2;++version){const auto& audio=version?edited:original;auto& pixels=version?next.edited:next.original;
        for(int x=0;x<SpectralImages::columns;++x){
            if(stopped(cancel))return Result::fail("Spectral edit cancelled.");
            std::fill(power.begin(),power.end(),0);
            int centre=first+(int)std::llround((end-first-1)*x/(double)(SpectralImages::columns-1));
            for(int ch=0;ch<2;++ch){
                std::fill(work.begin(),work.end(),0.f);
                for(int i=0;i<n;++i){int pos=centre+i-n/2;if(pos>=0&&pos<audio.getNumSamples())work[(size_t)i]=audio.getSample(ch,pos)*window[(size_t)i];}
                fft.performRealOnlyForwardTransform(work.data(),true);
                for(int k=0;k<=n/2;++k){double re=work[(size_t)k*2],im=work[(size_t)k*2+1],scale=(k==0||k==n/2?1.:2.)/sum;power[(size_t)k]+=(re*re+im*im)*scale*scale*.5;}
            }
            for(int y=0;y<SpectralImages::rows;++y){
                double low=20*std::pow(rate/40.,y/(double)SpectralImages::rows),high=20*std::pow(rate/40.,(y+1)/(double)SpectralImages::rows);
                int a=jlimit(0,n/2,(int)std::floor(low*n/rate)),b=jlimit(a,n/2,(int)std::ceil(high*n/rate));double peak=0;
                for(int k=a;k<=b;++k)peak=jmax(peak,power[(size_t)k]);
                pixels[(size_t)y*SpectralImages::columns+x]=(float)jlimit(-100.,12.,10*std::log10(jmax(1e-10,peak)));
            }
        }
    }
    if(stopped(cancel))return Result::fail("Spectral edit cancelled.");output=std::move(next);return Result::ok();
}
}
