// SPDX-License-Identifier: AGPL-3.0-only
#include "Analyzer.h"
#include <cmath>
namespace pulse {
using namespace juce;
void AnalyzerCapture::push(float l,float r,float rate,uint32_t token)noexcept{unsigned w=write.load(std::memory_order_relaxed),rd=read.load(std::memory_order_acquire);if(w-rd>=capacity){lost.fetch_add(1,std::memory_order_relaxed);return;}frames[w%capacity]={l,r,rate,token};write.store(w+1,std::memory_order_release);}
bool AnalyzerCapture::pop(Frame& frame)noexcept{unsigned rd=read.load(std::memory_order_relaxed),w=write.load(std::memory_order_acquire);if(rd==w)return false;frame=frames[rd%capacity];read.store(rd+1,std::memory_order_release);return true;}
AnalyzerModel::AnalyzerModel(){for(int i=0;i<size;++i){window[(size_t)i]=(float)(.5-.5*std::cos(2*MathConstants<double>::pi*i/size));windowSum+=window[(size_t)i];}reset();}
void AnalyzerModel::reset(double sampleRate){rate=std::isfinite(sampleRate)&&sampleRate>=1000&&sampleRate<=768000?sampleRate:48000;coefficient=1-std::exp(-1/(rate*.3));ring={};work={};amplitudes={};power={};displayPeak={};hold={};batchPeak={};clipped={};cross=0;count=invalid=batchFrames=0;position=0;}
void AnalyzerModel::beginBatch(){batchPeak={};batchFrames=0;}
void AnalyzerModel::feed(float l,float r,double sampleRate){if(!std::isfinite(sampleRate)||sampleRate<1000||sampleRate>768000){++invalid;return;}if(sampleRate!=rate)reset(sampleRate);float values[2]={l,r};for(int ch=0;ch<2;++ch){float& v=values[ch];if(!std::isfinite(v)||std::abs(v)>1e12f){v=0;++invalid;}double square=(double)v*v;power[(size_t)ch]+=(square-power[(size_t)ch])*coefficient;float peak=std::abs(v);batchPeak[(size_t)ch]=jmax(batchPeak[(size_t)ch],peak);hold[(size_t)ch]=jmax(hold[(size_t)ch],peak);if(peak>=1)++clipped[(size_t)ch];ring[(size_t)ch][(size_t)position]=v;}cross+=((double)values[0]*values[1]-cross)*coefficient;position=(position+1)%size;++count;++batchFrames;}
void AnalyzerModel::finishBatch(){if(batchFrames==0)return;float decay=(float)std::exp(-(double)batchFrames/(rate*.6));for(int ch=0;ch<2;++ch)displayPeak[(size_t)ch]=jmax(batchPeak[(size_t)ch],displayPeak[(size_t)ch]*decay);if(!spectrumReady())return;for(int ch=0;ch<2;++ch){auto& data=work[(size_t)ch];data.fill(0);for(int i=0;i<size;++i)data[(size_t)i]=ring[(size_t)ch][(size_t)((position+i)%size)]*window[(size_t)i];fft.performFrequencyOnlyForwardTransform(data.data(),true);}for(int bin=0;bin<bins;++bin){double a=work[0][(size_t)bin],b=work[1][(size_t)bin],scale=(bin==0||bin==size/2?1.:2.)/windowSum;amplitudes[(size_t)bin]=(float)(std::sqrt((a*a+b*b)*.5)*scale);}}
}
