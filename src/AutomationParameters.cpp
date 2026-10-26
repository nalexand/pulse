// SPDX-License-Identifier: AGPL-3.0-only
#include "AutomationParameters.h"
#include <cmath>
namespace pulse {
using namespace juce;
bool automationIsMaster(int target,int track){return target==5||track<0;}
bool validAutomationLink(int target,int track,int tracks){return target>=0&&target<automationTargetCount&&(target==5?(track>=-1&&track<tracks):isMasterProcessingAutomation(target)?track==-1:track>=0?track<tracks:(track==-1&&target>=6&&target<121&&!isSynthAutomation(target)));}
const std::array<AutomationParameter,automationTargetCount>& automationParameters(){
    static const auto rows=[] {std::array<AutomationParameter,automationTargetCount> r;auto add=[&](int i,String key,String name,String group,double low,double high,bool log=false,bool discrete=false,bool master=false){r[(size_t)i]={key,name,group,low,high,log,discrete,master};};
        add(0,"channel.volume","Volume","Channel",0,1.5);add(1,"channel.pan","Pan","Channel",-1,1);add(2,"channel.cutoff","Tone filter","Channel",60,20000,true);add(3,"channel.drive","Saturation","Channel",0,1);add(4,"channel.send","Delay send","Channel",0,.8);add(5,"master.volume","Master volume","Master",0,1,false,false,true);
        for(int i=0;i<4;++i){auto s=echoSpecs()[(size_t)i];add(6+i,String("delay.")+s.key,s.name,"Delay",s.low,s.high,s.logarithmic,s.integer,true);}
        for(int i=0;i<8;++i){auto s=distortionSpecs()[(size_t)i];add(10+i,String("distortion.")+s.key,s.name,"Distortion",s.low,s.high,s.logarithmic,s.integer,true);}
        const char* keys[]={"eq.low","eq.mid","eq.high","compressor.threshold","compressor.ratio","compressor.attack","compressor.release","compressor.makeup","reverb.mix","reverb.room","reverb.damping"};const char* names[]={"Low shelf","Mid gain","High shelf","Threshold","Ratio","Attack","Release","Makeup","Wet mix","Room size","Damping"};const double lo[]={-18,-18,-18,-60,1,.1,5,-12,0,0,0},hi[]={18,18,18,0,20,100,1000,24,1,1,1};for(int i=0;i<11;++i)add(18+i,keys[i],names[i],i<3?"EQ":i<8?"Dynamics":"Reverb",lo[i],hi[i],false,false,true);
        const char* suffix[]={"mix","rate","depth","centre","feedback","width"};for(int family=0;family<3;++family)for(int i=0;i<6;++i){auto s=family==0?chorusParameterSpecs()[(size_t)i]:modulationSpecs(family==1?ModulationKind::flanger:ModulationKind::phaser)[(size_t)i];String group=family==0?"Chorus":family==1?"Flanger":"Phaser";String key=group.toLowerCase()+"."+(family==0&&i==3?"delay":suffix[i]);add(29+family*6+i,key,s.name,group,s.low,s.high,s.logarithmic,false,true);}
        add(47,"stereo.width","Width","Stereo",0,2,false,false,true);add(48,"stereo.trim","Output trim","Stereo",-24,6,false,false,true);const char* dynKeys[]={"range","hysteresis","hold","knee"};const char* dynNames[]={"Range","Hysteresis","Hold","Knee"};const double dynMax[]={96,24,1000,24};for(int i=0;i<4;++i)add(49+i,String("dynamics.")+dynKeys[i],dynNames[i],"Dynamics",0,dynMax[i],false,false,true);
        const char* synthKeys[]={"mix","detune","coarse","pulse","sub","noise","cutoff","resonance","filterEnv","fmRatio","fmIndex","fmDecay","attack","decay","sustain","release","filterDecay","lfoRate","lfoPitch","lfoFilter","tablePosition","tableEnv","tableDecay","tableLfo","table"};for(int i=0;i<synthParameterCount;++i){const auto& s=synthParameterSpecs()[(size_t)i];String key=i<25?String("synth.")+synthKeys[i]:i<41?"synth.harmonic"+String(i-24):String("synth.physical.")+StringArray{"mode","decay","damping","brightness","position","body","stiffness"}[i-41];add(automationSynthFirst+i,key,s.name,"Synth",s.low,s.high,s.logarithmic,i==2||i==24||i==41);}
        const char* enableKeys[]={"eq.enabled","compressor.enabled","reverb.enabled","chorus.enabled","flanger.enabled","phaser.enabled","delay.enabled","distortion.enabled"};
        const char* enableGroups[]={"EQ","Dynamics","Reverb","Chorus","Flanger","Phaser","Delay","Distortion"};
        for(int i=0;i<8;++i)add(101+i,enableKeys[i],"Enabled",enableGroups[i],0,1,false,true,true);
        const char* stereoKeys[]={"enabled","mono","swap","invertLeft","invertRight"};
        const char* stereoNames[]={"Enabled","Mono","Swap channels","Invert left","Invert right"};
        for(int i=0;i<5;++i)add(109+i,String("stereo.")+stereoKeys[i],stereoNames[i],"Stereo",0,1,false,true,true);
        add(114,"dynamics.mode","Mode","Dynamics",0,3,false,true,true);
        add(115,"distortion.mode","Mode","Distortion",0,2,false,true,true);
        add(116,"synth.wave1","Oscillator 1 waveform","Synth",0,3,false,true);
        add(117,"synth.wave2","Oscillator 2 waveform","Synth",0,3,false,true);
        add(118,"master.delayReturn","Shared delay return","Master",0,1,false,false,true);
        add(119,"master.delayFeedback","Shared delay feedback","Master",0,.8,false,false,true);
        add(120,"master.softClip","Soft saturation","Master",0,1,false,true,true);
        add(121,"channel.mute","Mute","Channel",0,1,false,true);
        for(int slot=0;slot<auxSendCount;++slot)add(automationAuxFirst+slot,"aux.send"+String(slot+1)+".level","Send "+String(slot+1)+" level","Auxiliary sends",0,1.5);
        return r;
    }();return rows;
}
int automationParameterId(const String& key){const auto& p=automationParameters();for(int i=0;i<automationTargetCount;++i)if(p[(size_t)i].key==key)return i;return -1;}
StringArray automationParameterChoices(int id){
    if((id>=101&&id<=113)||id==120||id==121)return {"Off","On"};
    if(id==114)return {"Compressor","Limiter","Gate","Expander"};
    if(id==115)return {"Soft / tube","Hard clip","Fuzz"};
    if(id==116||id==117)return {"Sine","Triangle","Saw","Pulse"};
    if(id==automationSynthFirst+41)return {"Plucked string","Struck bar"};
    if(id==automationSynthFirst+24){StringArray choices;for(int i=0;i<wavetableCount;++i)choices.add(wavetableName(i));return choices;}
    return {};
}
String automationParameterText(int id,float normal){
    if(id<0||id>=automationTargetCount)return {};
    double value=automationParameterValue(id,normal);auto choices=automationParameterChoices(id);
    if(!choices.isEmpty())return choices[jlimit(0,choices.size()-1,(int)value)];
    return String(value,automationParameters()[(size_t)id].discrete?0:3);
}
String automationClipValueText(const Clip& c,float normal){return c.automationTrim?String((normal-.5f)*200.f,2)+"% range":automationParameterText(c.automationTarget,normal);}
var automationParametersToJson(){
    Array<var> rows;
    for(int id=0;id<automationTargetCount;++id){
        const auto& p=automationParameters()[(size_t)id];auto* o=new DynamicObject();
        o->setProperty("id",id);o->setProperty("key",p.key);o->setProperty("name",p.name);o->setProperty("group",p.group);
        o->setProperty("minimum",p.low);o->setProperty("maximum",p.high);o->setProperty("logarithmic",p.logarithmic);o->setProperty("discrete",p.discrete);o->setProperty("master",p.master);o->setProperty("channel",!isMasterOnlyAutomation(id));o->setProperty("trim",!p.discrete);
        String unit=id>=automationSynthFirst&&id<automationSynthEnd?String(synthParameterSpecs()[(size_t)(id-automationSynthFirst)].unit).trim():id==7||id==23||id==24||id==32||id==38||id==51?"ms":id==2||id==11||id==12||id==30||id==36||id==42||id==44?"Hz":id==10||id==13||id==17||(id>=18&&id<=21)||id==25||id==48||id==49||id==50||id==52?"dB":id==6?"repeats":"";
        o->setProperty("unit",unit);auto choices=automationParameterChoices(id);if(!choices.isEmpty()){Array<var> labels;for(const auto& label:choices)labels.add(label);o->setProperty("choices",labels);}
        if(isSynthAutomation(id)){
            int index=id-automationSynthFirst;Array<var> instruments;
            for(int instrument=5;instrument<instrumentCount;++instrument){
                bool amp=index==12||index==15,shared=index>=12&&index<=19,tone=index>=6&&index<=8,osc=index<=2||index==4||index==5;
                bool supported=id>=automationWaveFirst?instrument==9:amp||(instrument==9&&(index<=8||shared))||(instrument==10&&(osc||tone||(index>=9&&index<=19)))||(instrument==11&&(osc||tone||shared||(index>=20&&index<=24)))||(instrument==12&&(osc||tone||shared||(index>=25&&index<41)))||(instrument==13&&(index==2||tone||shared||index>=41));
                if(supported)instruments.add(instrument);
            }
            o->setProperty("instruments",instruments);
        }
        rows.add(var(o));
    }
    return var(rows);
}
double automationParameterValue(int id,double normal)noexcept{if(id<0||id>=automationTargetCount||!std::isfinite(normal))return 0;const auto& p=automationParameters()[(size_t)id];normal=jlimit(0.,1.,normal);double v=p.logarithmic?p.low*std::pow(p.high/p.low,normal):p.low+(p.high-p.low)*normal;return p.discrete?std::round(v):v;}
float automationParameterNormal(int id,double v)noexcept{if(id<0||id>=automationTargetCount||!std::isfinite(v))return 0;const auto& p=automationParameters()[(size_t)id];v=jlimit(p.low,p.high,v);return(float)jlimit(0.,1.,p.logarithmic?std::log(v/p.low)/std::log(p.high/p.low):(v-p.low)/(p.high-p.low));}
namespace {
const std::array<float Track::Mixer::*,11> fields{&Track::Mixer::low,&Track::Mixer::mid,&Track::Mixer::high,&Track::Mixer::threshold,&Track::Mixer::ratio,&Track::Mixer::compAttack,&Track::Mixer::compRelease,&Track::Mixer::makeup,&Track::Mixer::reverbMix,&Track::Mixer::room,&Track::Mixer::damping};
const std::array<float ChorusSettings::*,6> chorus{&ChorusSettings::mix,&ChorusSettings::rate,&ChorusSettings::depth,&ChorusSettings::delay,&ChorusSettings::feedback,&ChorusSettings::width};
const std::array<float ModulationSettings::*,6> modulation{&ModulationSettings::mix,&ModulationSettings::rate,&ModulationSettings::depth,&ModulationSettings::centre,&ModulationSettings::feedback,&ModulationSettings::width};
const std::array<float DistortionSettings::*,8> distortion{&DistortionSettings::drive,&DistortionSettings::lowCut,&DistortionSettings::tone,&DistortionSettings::presence,&DistortionSettings::asymmetry,&DistortionSettings::cabinet,&DistortionSettings::mix,&DistortionSettings::output};
const std::array<float DynamicsOptions::*,4> dynamics{&DynamicsOptions::range,&DynamicsOptions::hysteresis,&DynamicsOptions::hold,&DynamicsOptions::knee};
}
float mixerAutomationNormal(const Track::Mixer& m,int id){double v=0;if(id>=6&&id<10)v=echoValue(m.delay,id-6);else if(id>=10&&id<18)v=distortionValue(m.distortion,id-10);else if(id>=18&&id<29)v=m.*fields[(size_t)(id-18)];else if(id>=29&&id<35)v=chorusValue(m.chorus,id-29);else if(id>=35&&id<47)v=modulationValue(id<41?m.flanger:m.phaser,(id-35)%6);else if(id==47)v=m.stereo.width;else if(id==48)v=m.stereo.trim;else if(id>=49&&id<53)v=m.dynamics.*dynamics[(size_t)(id-49)];if(id>=101&&id<=108){const std::array<bool,8> enabled{m.eq,m.compressor,m.reverb,m.chorus.enabled,m.flanger.enabled,m.phaser.enabled,m.delay.enabled,m.distortion.enabled};v=enabled[(size_t)(id-101)]?1:0;}
    else if(id>=109&&id<=113){const std::array<bool,5> enabled{m.stereo.enabled,m.stereo.mono,m.stereo.swap,m.stereo.invertLeft,m.stereo.invertRight};v=enabled[(size_t)(id-109)]?1:0;}
    else if(id==114)v=(int)m.dynamics.mode;else if(id==115)v=m.distortion.mode;
    return automationParameterNormal(id,v);}
float projectAutomationNormal(const Project& p,int id){switch(id){case 5:return p.master;case 118:return p.delayReturn;case 119:return p.delayFeedback/.8f;case 120:return p.masterSoftClip?1.f:0.f;default:return 0;}}
float trackAutomationNormal(const Track& t,int id){if(isAuxAutomation(id))return t.auxSends[(size_t)(id-automationAuxFirst)].level/1.5f;if(id==121)return t.mute?1.f:0.f;if(id==116||id==117)return automationParameterNormal(id,id==116?t.synth.wave1:t.synth.wave2);if(id>=automationSynthFirst&&id<automationSynthEnd)return automationParameterNormal(id,synthParameterValue(t,id-automationSynthFirst));switch(id){case 0:return t.volume/1.5f;case 1:return(t.pan+1)/2;case 2:return automationParameterNormal(id,t.cutoff);case 3:return t.drive;case 4:return t.send/.8f;default:return mixerAutomationNormal(t.mixer,id);}}
void applyMixerAutomation(Track::Mixer& m,int id,float normal)noexcept{float v=(float)automationParameterValue(id,normal);if(id==6)m.delay.repeats=(int)v;else if(id==7)m.delay.interval=v;else if(id==8)m.delay.fade=v;else if(id==9)m.delay.mix=v;else if(id>=10&&id<18)m.distortion.*distortion[(size_t)(id-10)]=v;else if(id>=18&&id<29)m.*fields[(size_t)(id-18)]=v;else if(id>=29&&id<35)m.chorus.*chorus[(size_t)(id-29)]=v;else if(id>=35&&id<47)(id<41?m.flanger:m.phaser).*modulation[(size_t)((id-35)%6)]=v;else if(id==47)m.stereo.width=v;else if(id==48)m.stereo.trim=v;else if(id>=49&&id<53)m.dynamics.*dynamics[(size_t)(id-49)]=v;
    else switch(id){case 101:m.eq=v!=0;break;case 102:m.compressor=v!=0;break;case 103:m.reverb=v!=0;break;case 104:m.chorus.enabled=v!=0;break;case 105:m.flanger.enabled=v!=0;break;case 106:m.phaser.enabled=v!=0;break;case 107:m.delay.enabled=v!=0;break;case 108:m.distortion.enabled=v!=0;break;case 109:m.stereo.enabled=v!=0;break;case 110:m.stereo.mono=v!=0;break;case 111:m.stereo.swap=v!=0;break;case 112:m.stereo.invertLeft=v!=0;break;case 113:m.stereo.invertRight=v!=0;break;case 114:m.dynamics.mode=(DynamicsMode)(int)v;break;case 115:m.distortion.mode=(int)v;break;default:break;}}
void applySynthAutomation(SynthPatch& p,float& attack,float& release,int id,float normal)noexcept{if(id==116)p.wave1=(int)automationParameterValue(id,normal);else if(id==117)p.wave2=(int)automationParameterValue(id,normal);else if(id>=automationSynthFirst&&id<automationSynthEnd)assignSynthParameter(p,attack,release,id-automationSynthFirst,automationParameterValue(id,normal));}
}
