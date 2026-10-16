// SPDX-License-Identifier: AGPL-3.0-only
#include "Engine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
namespace pulse {
void Engine::configureEffects(){
    using Coeff=juce::dsp::IIR::ArrayCoefficients<float>;
    for(int t=0;t<state->trackCount;++t){auto& fx=effects[(size_t)t];const auto& m=state->tracks[(size_t)t].mixer;
        fx.eq[0]=Coeff::makeLowShelf(sampleRate,120.f,.707f,juce::Decibels::decibelsToGain(m.low));
        fx.eq[1]=Coeff::makePeakFilter(sampleRate,juce::jmin(1000.f,(float)sampleRate*.4f),.8f,juce::Decibels::decibelsToGain(m.mid));
        fx.eq[2]=Coeff::makeHighShelf(sampleRate,juce::jmin(5000.f,(float)sampleRate*.4f),.707f,juce::Decibels::decibelsToGain(m.high));
        for(auto& c:fx.eq){float a0=c[3];for(auto& value:c)value/=a0;}
        fx.attack=(float)std::exp(-1/(sampleRate*m.compAttack*.001));fx.release=(float)std::exp(-1/(sampleRate*m.compRelease*.001));
        juce::Reverb::Parameters r;r.roomSize=m.room;r.damping=m.damping;r.wetLevel=m.reverbMix/3;r.dryLevel=(1-m.reverbMix)/2;r.width=1;r.freezeMode=0;fx.reverb.setParameters(r);
        fx.stereo.setSettings(m.stereo);fx.chorus.setSettings(m.chorus);fx.flanger.setSettings(m.flanger);fx.phaser.setSettings(m.phaser);
        if(!m.eq){fx.zL={};fx.zR={};}if(!m.compressor)fx.envelope=0;if(!m.reverb)fx.reverb.reset();
    }
}
void Engine::processEffects(int track,const TrackSound& sound,float& left,float& right){
    auto& fx=effects[(size_t)track];const auto& m=sound.mixer;
    fx.reduction=0;
    for(int effect:m.order)switch(effect){
    case 0:if(m.eq)for(int band=0;band<3;++band){const auto& c=fx.eq[(size_t)band];
        auto filter=[&](float x,std::array<float,2>& z){float y=c[0]*x+z[0];z[0]=c[1]*x-c[4]*y+z[1];z[1]=c[2]*x-c[5]*y;return y;};
        left=filter(left,fx.zL[(size_t)band]);right=filter(right,fx.zR[(size_t)band]);}break;
    case 1:if(m.compressor){float level=m.sidechain>=0?detectorLast[(size_t)m.sidechain]:juce::jmax(std::abs(left),std::abs(right));
        float coefficient=level>fx.envelope?fx.attack:fx.release;fx.envelope=level+(fx.envelope-level)*coefficient;
        float db=20*(float)std::log10(juce::jmax(.00000001f,fx.envelope));fx.reduction=juce::jmax(0.f,db-m.threshold)*(1-1/m.ratio);
        float gain=juce::Decibels::decibelsToGain(m.makeup-fx.reduction);left*=gain;right*=gain;}break;
    case 2:if(m.reverb)fx.reverb.processStereo(&left,&right,1);break;
    case 3:fx.chorus.process(left,right);break;
    case 4:fx.flanger.process(left,right);break;
    case 5:fx.phaser.process(left,right);break;
    }
    fx.stereo.process(left,right);
}
}
