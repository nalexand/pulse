// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include "Wavetable.h"
#include "Additive.h"
#include "PhysicalModel.h"
namespace pulse {
struct SynthPatch {
    PhysicalSettings physical;
    HarmonicLevels harmonics=defaultHarmonics;
    int wave1=2,wave2=3,coarse=0,table=0;
    float tablePosition=0,tableEnv=0,tableDecay=1,tableLfo=0;
    float mix=.35f,detune=7,pulse=.5f,sub=.12f,noise=0;
    float decay=.25f,sustain=.7f,cutoff=8000,resonance=.1f,filterEnv=0,filterDecay=.35f;
    float fmRatio=2,fmIndex=2,fmDecay=1,lfoRate=5,lfoPitch=0,lfoFilter=0;
    bool operator==(const SynthPatch&)const=default;
};
juce::Result validateSynthPatch(const SynthPatch&);
juce::var synthPatchToJson(const SynthPatch&);
juce::Result synthPatchFromJson(const juce::var&,SynthPatch&,bool requireWavetable=false,bool requireAdditive=false,bool requirePhysical=false);
const char* synthWaveName(int);
// Fixed storage, no allocation/locks/file access in the voice render path.
class SynthVoice {
public:
    void start(int pitch,double rate,uint32_t seed,double tuningCents=0);
    float process(const SynthPatch&,int engine,bool gate,float attack,float release);
    bool active()const{return alive;}
private:
    double sampleRate=48000,frequency=440,phase1=0,phase2=.23,phaseSub=0,phaseLfo=0;
    double env=0,releaseLevel=0,seconds=0,releasedSeconds=0,decaySeconds=0;
    double s1=0,s2=0,g=0,r2=2,h=1,dt1=0,dt2=0,mix=.35,duty=.5,sub=.12,noise=0,index=2;
    double smoothCut=8000,smoothRes=.1,smoothSustain=.7,lfo=0,last=0,anchor=0;
    int stage=0,control=0,transition=0,lastWave1=-1,lastWave2=-1,lastTable=-1;
    const WavetableBank* tables=nullptr;
    double tablePosition=0,tableTarget=0,tableSmoothing=0;
    bool lastWavetable=false;
    bool lastAdditive=false;
    PhysicalVoice physical;
    HarmonicLevels harmonics=defaultHarmonics;
    double harmonicGain=1;
    bool alive=false,wasGate=true,lastFm=false;uint32_t random=12345;
};
}
