// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
namespace pulse {
void Engine::configureEffects(){
    for(int index=0;index<=state->trackCount;++index){int t=index==state->trackCount?maxTracks:index;configureEffect(t,t==maxTracks?state->masterMixer:state->tracks[(size_t)t].mixer,true);}
}
namespace {
bool effectEnabled(const Track::Mixer& m,int effect){switch(effect){case 0:return m.eq;case 1:return m.compressor;case 2:return m.reverb;case 3:return m.chorus.enabled;case 4:return m.flanger.enabled;case 5:return m.phaser.enabled;case 6:return m.delay.enabled;case 7:return m.distortion.enabled;default:return false;}}
void enableEffect(Track::Mixer& m,int effect){switch(effect){case 0:m.eq=true;break;case 1:m.compressor=true;break;case 2:m.reverb=true;break;case 3:m.chorus.enabled=true;break;case 4:m.flanger.enabled=true;break;case 5:m.phaser.enabled=true;break;case 6:m.delay.enabled=true;break;case 7:m.distortion.enabled=true;break;default:break;}}
}
void Engine::configureEffect(int t,const Track::Mixer& requested,bool force){
    // Automated bypass fades the effect output over 5 ms. Keep its internal
    // processor warm while bypassed, including delay/reverb tails. Unautomated
    // chains retain their exact legacy path and reset rules.
    auto m=requested;auto mask=state->effectBypassMask[(size_t)t];
    auto& fade=effects[(size_t)t];
    if(force){fade.bypassStep=(float)(1/(sampleRate*.005));for(int i=0;i<builtinEffectCount;++i)fade.bypassGain[(size_t)i]=effectEnabled(requested,i)?1.f:0.f;}
    for(int i=0;i<builtinEffectCount;++i)if(mask&(1u<<i))enableEffect(m,i);
    using Coeff=juce::dsp::IIR::ArrayCoefficients<float>;auto& fx=effects[(size_t)t];const auto& old=fx.lastSettings;if(!force&&old==m)return;
    if(force||old.low!=m.low||old.mid!=m.mid||old.high!=m.high){
        fx.eq[0]=Coeff::makeLowShelf(sampleRate,120.f,.707f,juce::Decibels::decibelsToGain(m.low));
        fx.eq[1]=Coeff::makePeakFilter(sampleRate,juce::jmin(1000.f,(float)sampleRate*.4f),.8f,juce::Decibels::decibelsToGain(m.mid));
        fx.eq[2]=Coeff::makeHighShelf(sampleRate,juce::jmin(5000.f,(float)sampleRate*.4f),.707f,juce::Decibels::decibelsToGain(m.high));
        for(auto& c:fx.eq){float a0=c[3];for(auto& value:c)value/=a0;}
    }
    if(force||old.compAttack!=m.compAttack)fx.attack=(float)std::exp(-1/(sampleRate*m.compAttack*.001));
    if(force||old.compRelease!=m.compRelease)fx.release=(float)std::exp(-1/(sampleRate*m.compRelease*.001));
    if(force||old.room!=m.room||old.damping!=m.damping||old.reverbMix!=m.reverbMix){juce::Reverb::Parameters r;r.roomSize=m.room;r.damping=m.damping;r.wetLevel=m.reverbMix/3;r.dryLevel=(1-m.reverbMix)/2;r.width=1;r.freezeMode=0;fx.reverb.setParameters(r);}
    if(force||old.dynamics!=m.dynamics||old.threshold!=m.threshold||old.ratio!=m.ratio||old.compAttack!=m.compAttack||old.compRelease!=m.compRelease||old.makeup!=m.makeup)fx.dynamics.configure(m.dynamics,m.threshold,m.ratio,m.compAttack,m.compRelease,m.makeup);
    if(!m.compressor)fx.dynamics.reset();
    if(force||old.stereo!=m.stereo)fx.stereo.setSettings(m.stereo);
    if(force||old.chorus!=m.chorus)fx.chorus.setSettings(m.chorus);
    if(force||old.flanger!=m.flanger)fx.flanger.setSettings(m.flanger);
    if(force||old.phaser!=m.phaser)fx.phaser.setSettings(m.phaser);
    if(force||old.delay!=m.delay)fx.delay.setSettings(m.delay,!force);
    if(force||old.distortion!=m.distortion)fx.distortion.setSettings(m.distortion);
    if(!m.eq){fx.zL={};fx.zR={};}if(!m.compressor)fx.envelope=0;if(!m.reverb)fx.reverb.reset();fx.lastSettings=m;
}
void Engine::processEffects(int track,const Track::Mixer& m,float& left,float& right){
    auto& fx=effects[(size_t)track];
    fx.reduction=0;
    for(int effect:m.order){
        bool automated=(state->effectBypassMask[(size_t)track]&(1u<<effect))!=0;
        float dryL=left,dryR=right;
        if(automated){auto& gain=fx.bypassGain[(size_t)effect];float target=effectEnabled(m,effect)?1.f:0.f;gain+=juce::jlimit(-fx.bypassStep,fx.bypassStep,target-gain);}
        switch(effect){
    case 0:if(m.eq||automated)for(int band=0;band<3;++band){const auto& c=fx.eq[(size_t)band];
        auto filter=[&](float x,std::array<float,2>& z){float y=c[0]*x+z[0];z[0]=c[1]*x-c[4]*y+z[1];z[1]=c[2]*x-c[5]*y;return y;};
        left=filter(left,fx.zL[(size_t)band]);right=filter(right,fx.zR[(size_t)band]);}break;
    case 1:if(m.compressor||automated){float level=m.sidechain>=0?detectorLast[(size_t)m.sidechain]:juce::jmax(std::abs(left),std::abs(right));
        if(m.dynamics.mode!=DynamicsMode::compressor){fx.envelope=0;fx.reduction=fx.dynamics.process(level,left,right);break;}float coefficient=level>fx.envelope?fx.attack:fx.release;fx.envelope=level+(fx.envelope-level)*coefficient;
        float db=20*(float)std::log10(juce::jmax(.00000001f,fx.envelope));fx.reduction=juce::jmax(0.f,db-m.threshold)*(1-1/m.ratio);
        if(m.dynamics.knee>0){float distance=db-m.threshold,half=m.dynamics.knee*.5f;if(distance> -half&&distance<half)fx.reduction=(1-1/m.ratio)*(distance+half)*(distance+half)/(2*m.dynamics.knee);}
        float gain=juce::Decibels::decibelsToGain(m.makeup-fx.reduction);left*=gain;right*=gain;}break;
    case 2:if(m.reverb||automated)fx.reverb.processStereo(&left,&right,1);break;
    case 3:fx.chorus.process(left,right);break;
    case 4:fx.flanger.process(left,right);break;
    case 5:fx.phaser.process(left,right);break;
    case 6:fx.delay.process(left,right);break;
    case 7:fx.distortion.process(left,right);break;
    }
        if(automated){float gain=fx.bypassGain[(size_t)effect];if(gain==0){left=dryL;right=dryR;}else if(gain<1){left=dryL+(left-dryL)*gain;right=dryR+(right-dryR)*gain;}}
    }
    fx.stereo.process(left,right);
}
}
