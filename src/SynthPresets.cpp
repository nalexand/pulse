// SPDX-License-Identifier: AGPL-3.0-only
#include "SynthPresets.h"
#include <cmath>
namespace pulse {
using namespace juce;
const std::vector<SynthPreset>& factorySynthPresets(){
    static const auto presets=[] {std::vector<SynthPreset> rows;
        auto analog=[&](const char* name,const char* category,int w1,int w2,float mix,float detune,int coarse,float cutoff,float resonance,float envelope,float decay,float sustain,float attack,float release,float sub=0,float noise=0,float vibrato=0,float rate=5,float sweep=0){SynthPreset p;p.name=name;p.category=category;p.patch.wave1=w1;p.patch.wave2=w2;p.patch.mix=mix;p.patch.detune=detune;p.patch.coarse=coarse;p.patch.cutoff=cutoff;p.patch.resonance=resonance;p.patch.filterEnv=envelope;p.patch.filterDecay=decay;p.patch.decay=decay;p.patch.sustain=sustain;p.patch.sub=sub;p.patch.noise=noise;p.patch.lfoPitch=vibrato;p.patch.lfoRate=rate;p.patch.lfoFilter=sweep;p.attack=attack;p.release=release;rows.push_back(p);};
        analog("Round sub","Bass",0,1,.15,0,0,850,.05,0,.2,.9,.005,.12,.5);
        analog("Rubber pulse","Bass",3,2,.2,-5,0,420,.5,3,.16,.35,.002,.12,.3);
        analog("Acid steps","Bass",2,3,.1,0,0,280,.8,4.5,.12,.2,.001,.08,.15);
        analog("Detuned saw bass","Bass",2,2,.5,14,0,1500,.18,1,.3,.7,.004,.15,.25);
        analog("Hollow octave","Bass",3,1,.3,-4,-12,1200,.25,1,.2,.5,.003,.12,.1);
        analog("Soft triangle","Bass",1,0,.4,2,0,2000,.08,0,.5,.8,.01,.22,.2);
        analog("Bright saw lead","Lead",2,2,.45,9,0,13000,.12,0,.2,.8,.006,.22);
        analog("Singing pulse","Lead",3,1,.2,4,0,4200,.25,1,.4,.65,.015,.35,0,0,12,5.5);
        analog("Octave stack","Lead",2,3,.4,-7,12,11000,.2,0,.3,.7,.005,.2);
        analog("Soft whistle","Lead",0,1,.08,0,0,5000,.04,0,.25,.8,.025,.35,0,0,8,5);
        analog("Narrow reed","Lead",3,3,.32,6,0,3500,.35,1,.2,.55,.008,.2);rows.back().patch.pulse=.22f;
        analog("Noise edge","Lead",2,3,.2,11,0,9000,.15,0,.3,.65,.003,.25,0,.08);
        analog("Slow sunrise","Pad",2,2,.5,12,0,2800,.15,1.5,2,.8,.8,2.4,0,0,5,.3,.7);
        analog("Velvet triangles","Pad",1,1,.5,-9,0,6500,.05,0,1,.75,.6,2,0,0,6,.4);
        analog("Air and glass","Pad",0,2,.15,5,12,5800,.12,1,1.5,.65,.5,2.6,0,.06,4,.2,.5);
        analog("Deep dusk","Pad",3,2,.4,-11,-12,900,.2,2,2.5,.8,1,2.8,.1,0,3,.15,.8);
        analog("Orbit sweep","Pad",2,3,.5,16,7,2200,.5,1,1.5,.7,.35,2.2,0,0,4,.35,1.5);
        analog("Warm fifths","Pad",1,2,.25,3,7,4000,.12,0,1,.7,.4,1.8);
        analog("Wood pluck","Pluck",1,0,.2,0,12,2600,.12,2,.18,0,.001,.12);
        analog("Pulse mallet","Pluck",3,1,.25,-3,0,1600,.3,3,.12,.05,.001,.18);
        analog("Short saw keys","Keys",2,3,.4,5,0,3200,.2,2,.35,.25,.003,.3);
        analog("Soft electric keys","Keys",1,0,.35,2,12,6500,.05,0,.65,.2,.008,.5);
        analog("Wind rise","FX",2,3,.2,22,7,400,.6,5,3,.4,1.2,2,0,.3,15,.15,1);
        analog("Clockwork sweep","FX",3,2,.5,40,-7,1700,.7,-2,1.2,.6,.02,1,0,.1,40,12,2);
        auto fm=[&](const char* name,const char* category,float ratio,float index,float fmDecay,float decay,float sustain,float cutoff,float attack,float release,float mix=0,float detune=0,float vibrato=0){SynthPreset p;p.name=name;p.category=category;p.instrument=Instrument::fm;p.patch.wave1=0;p.patch.wave2=0;p.patch.mix=mix;p.patch.detune=detune;p.patch.sub=0;p.patch.cutoff=cutoff;p.patch.fmRatio=ratio;p.patch.fmIndex=index;p.patch.fmDecay=fmDecay;p.patch.decay=decay;p.patch.sustain=sustain;p.patch.lfoPitch=vibrato;p.attack=attack;p.release=release;rows.push_back(p);};
        fm("Tine electric piano","Keys",1,3,.35,1.2,.18,12000,.003,.8);
        fm("Mellow electric piano","Keys",1,1.2,.5,1.5,.15,7000,.005,.9);
        fm("Bright digital keys","Keys",2,4,.25,.8,.25,16000,.002,.6);
        fm("Harmonic organ","Keys",2,1.1,4,.1,1,14000,.008,.12,.15);
        fm("Glass bell","Bell",3.5,5,1.5,2,0,16000,.001,1.4);
        fm("Tubular bell","Bell",1.4142f,6,2.5,3,0,15000,.001,2);
        fm("Music box","Bell",2,2,.7,1.3,0,12000,.001,.8);
        fm("Small metal mallet","Bell",7,3,.4,.7,0,15000,.001,.5);
        fm("Digital round bass","Bass",1,1.8,.2,.3,.55,3000,.002,.15);
        fm("Punch FM bass","Bass",2,6,.09,.18,.35,6000,.001,.12);
        fm("Rubbery FM bass","Bass",.5,3,.25,.4,.6,3500,.003,.2);
        fm("Clear FM lead","Lead",2,2,4,.3,.75,16000,.01,.3,0,2,7);
        fm("Brass bite","Lead",1,5,.55,.5,.5,10000,.025,.4);
        fm("Shimmer field","Pad",3,2.2,4,2,.75,9000,.7,2.5,.1,4,6);
        fm("Inharmonic cloud","Pad",1.618f,4,4,3,.6,11000,1,2.8,.15,-6,10);
        fm("Metallic transmission","FX",11.3f,10,3,2,.4,18000,.01,1.5,.25,20,45);
        auto table=[&](const char* name,const char* category,int bank,float position,float envelope,float decay,float depth,float rate,float cutoff,float sustain,float attack,float release,float detune=7,float mix=.35f){SynthPreset p;p.name=name;p.category=category;p.instrument=Instrument::wavetable;p.patch.table=bank;p.patch.tablePosition=position;p.patch.tableEnv=envelope;p.patch.tableDecay=decay;p.patch.tableLfo=depth;p.patch.lfoRate=rate;p.patch.cutoff=cutoff;p.patch.sustain=sustain;p.patch.decay=.5f;p.patch.detune=detune;p.patch.mix=mix;p.patch.sub=.05f;p.attack=attack;p.release=release;rows.push_back(p);};
        table("Morphing saw lead","Lead",0,.12,.65,.6,.08,1.2,12000,.8,.006,.25);
        table("Shifting pulse bass","Bass",0,.38,-.35,.18,.03,.8,1800,.55,.002,.12,0,.1);
        table("Hollow drift","Pad",1,.15,.5,3,.2,.3,6500,.8,.5,2);
        table("Reed steps","Pluck",1,.72,-.5,.28,0,2,5000,.12,.002,.2,3,.2);
        table("Vowel sweep","Lead",2,.2,.65,.9,.15,.7,10000,.7,.02,.4);
        table("Choir haze","Pad",2,.45,.3,4,.3,.22,4800,.75,.8,2.8,12,.45);
        table("Metal bloom","Pad",3,.05,.9,2,.1,.35,9500,.6,.35,2.2,9,.3);
        table("Wire pluck","Pluck",3,.8,-.7,.16,0,3,14000,.05,.001,.18,0,.1);
        table("Drawbar morph","Keys",4,.1,.6,.8,.06,3,12000,.7,.006,.25,0,.25);
        table("Rotating organ","Keys",4,.5,0,1,.4,4.2,9500,.9,.01,.22,4,.4);
        table("Glass current","Pad",5,.6,-.45,2.5,.18,.4,15000,.7,.6,2.6,6,.2);
        table("Prism keys","Keys",5,.2,.7,.5,.02,2,17000,.18,.003,.6,0,.1);
        table("Pixel wander","FX",6,.15,.65,3,.35,1.1,11000,.6,.05,1.2,17,.5);
        table("Digital edge","Lead",6,.75,-.5,.35,.12,2.5,16000,.75,.004,.2,5,.3);
        table("Soft horizon","Pad",7,.3,.5,3,.2,.18,6000,.8,1,2.5,5,.4);
        table("Breathing sine","Bass",7,.1,.3,.5,.08,1,1600,.8,.008,.22,0,.1);
        auto additive=[&](const char* name,const char* category,HarmonicLevels levels,float cutoff,float sustain,float attack,float release,float decay,float detune,float mix){SynthPreset p;p.name=name;p.category=category;p.instrument=Instrument::additive;p.patch.harmonics=levels;p.patch.cutoff=cutoff;p.patch.sustain=sustain;p.patch.decay=decay;p.patch.detune=detune;p.patch.mix=mix;p.patch.sub=0;p.patch.noise=0;p.attack=attack;p.release=release;rows.push_back(p);};
        additive("Drawbar foundation","Organ",{1,.5f,.25f,0,.18f,0,.1f,0,.06f},10000,.9f,.008f,.2f,.2f,0,0);
        additive("Odd velvet","Keys",{1,0,.3f,0,.12f,0,.06f,0,.03f},6500,.6f,.02f,.5f,.5f,6,.3f);
        additive("Bright additive brass","Brass",{1,.7f,.5f,.35f,.25f,.18f,.12f,.1f,.08f,.06f},8000,.7f,.04f,.3f,.25f,9,.4f);rows.back().patch.filterEnv=1.5f;
        additive("Glass partial keys","Keys",{1,0,.65f,0,.3f,0,0,.45f,0,0,0,.2f,0,0,0,.12f},15000,.12f,.002f,.9f,.65f,4,.35f);
        additive("Hollow harmonic bass","Bass",{1,0,.5f,0,.2f},1600,.8f,.004f,.16f,.2f,0,0);rows.back().patch.sub=.2f;
        additive("Air choir","Pad",{.7f,0,.9f,.2f,.5f,0,0,.2f},6000,.8f,.4f,1.6f,1.2f,12,.45f);
        additive("Sixteenth shimmer","Pad",{.7f,0,0,0,0,0,0,.6f,0,0,0,.4f,0,0,0,.3f},13000,.7f,.1f,1.2f,.7f,8,.4f);
        additive("Pure additive pluck","Pluck",{1,.15f,.1f,0,.04f},9000,0,.003f,.25f,.18f,3,.2f);
        auto physical=[&](const char* name,const char* category,int mode,float decay,float damping,float bright,float position,float body,float stiffness,float release){SynthPreset p;p.name=name;p.category=category;p.instrument=Instrument::physical;p.patch.physical={mode,decay,damping,bright,position,body,stiffness};p.patch.sub=p.patch.noise=0;p.patch.cutoff=16000;p.patch.sustain=1;p.patch.decay=.1f;p.attack=.001f;p.release=release;rows.push_back(p);};
        physical("Cedar string","Pluck",0,4,.3f,.8f,.3f,.45f,.02f,1.2f);
        physical("Muted wire","Pluck",0,.65f,.8f,.55f,.18f,.2f,.1f,.2f);
        physical("Glass harp","Pluck",0,8,.1f,1,.41f,.1f,.35f,2.5f);
        physical("Round string bass","Bass",0,3,.6f,.3f,.45f,.6f,0,.6f);
        physical("Copper tine","Keys",1,4,.25f,.7f,.23f,.2f,0,1.5f);
        physical("Wooden keys","Keys",1,.8f,.7f,.45f,.37f,.7f,0,.4f);
        physical("Long bronze","Bell",1,12,.08f,1,.17f,.15f,0,3);
        physical("Soft mallet","Keys",1,2,.6f,.2f,.42f,.55f,0,.8f);
        return rows;}();return presets;
}
Result validateSynthPreset(const SynthPreset& p){auto valid=validateSynthPatch(p.patch);if(valid.failed())return valid;if(!isSynth(p.instrument)||p.name.trim().isEmpty()||p.name.length()>80||p.category.length()>40||!std::isfinite(p.attack)||p.attack<.001f||p.attack>2||!std::isfinite(p.release)||p.release<.02f||p.release>3)return Result::fail("Invalid synth sound name, engine or envelope.");return Result::ok();}
var synthPresetToJson(const SynthPreset& p){auto* o=new DynamicObject();o->setProperty("schema",p.instrument==Instrument::physical||p.patch.physical!=PhysicalSettings{}?4:p.instrument==Instrument::additive||p.patch.harmonics!=defaultHarmonics?3:2);o->setProperty("application","Pulse Studio sound");o->setProperty("name",p.name);o->setProperty("category",p.category);o->setProperty("instrument",(int)p.instrument);o->setProperty("attack",p.attack);o->setProperty("release",p.release);o->setProperty("patch",synthPatchToJson(p.patch));return var(o);}
Result synthPresetFromJson(const var& value,SynthPreset& out){if(!value.isObject()||!value["schema"].isInt()||((int)value["schema"]<1||(int)value["schema"]>4)||!value["name"].isString()||!value["category"].isString()||!value["instrument"].isInt())return Result::fail("Invalid or unsupported Pulse sound file.");if((int)value["schema"]<4&&(int)value["instrument"]==(int)Instrument::physical)return Result::fail("Physical sounds require schema 4.");SynthPreset p;p.name=value["name"].toString();p.category=value["category"].toString();p.instrument=(Instrument)(int)value["instrument"];if((int)value["schema"]==1&&p.instrument==Instrument::wavetable)return Result::fail("Wavetable sounds require sound-file schema 2.");if((int)value["schema"]<3&&p.instrument==Instrument::additive)return Result::fail("Additive sounds require sound-file schema 3.");for(auto key:{"attack","release"}){auto v=value[key];if(!(v.isInt()||v.isInt64()||v.isDouble())||!std::isfinite((double)v))return Result::fail("Invalid sound envelope.");}p.attack=(float)(double)value["attack"];p.release=(float)(double)value["release"];auto valid=synthPatchFromJson(value["patch"],p.patch,(int)value["schema"]>=2,(int)value["schema"]>=3,(int)value["schema"]>=4);if(valid.failed())return valid;if((int)value["schema"]<4&&p.patch.physical!=PhysicalSettings{})return Result::fail("Physical settings require sound schema 4.");if((int)value["schema"]<3&&p.patch.harmonics!=defaultHarmonics)return Result::fail("Custom harmonics require sound-file schema 3.");valid=validateSynthPreset(p);if(valid.failed())return valid;out=p;return Result::ok();}
Result saveSynthPreset(const SynthPreset& p,const File& file){auto valid=validateSynthPreset(p);if(valid.failed())return valid;valid=file.getParentDirectory().createDirectory();if(valid.failed())return valid;TemporaryFile temp(file);if(!temp.getFile().replaceWithText(JSON::toString(synthPresetToJson(p),false))||!temp.overwriteTargetFileWithTemporary())return Result::fail("Could not write sound file.");return Result::ok();}
Result loadSynthPreset(const File& file,SynthPreset& p){if(!file.existsAsFile()||file.getSize()>65536)return Result::fail("Sound file is missing or exceeds 64 KiB.");return synthPresetFromJson(JSON::parse(file.loadFileAsString()),p);}
const std::array<SynthParameterSpec,synthParameterCount>& synthParameterSpecs(){static const std::array<SynthParameterSpec,synthParameterCount> specs={{{"Oscillator mix",0,1,false,""},{"Detune",-50,50,false," cents"},{"Oscillator 2 tune",-24,24,false," st"},{"Pulse width",.05f,.95f,false,""},{"Sub level",0,1,false,""},{"Noise level",0,.5,false,""},{"Filter cutoff",40,20000,true," Hz"},{"Resonance",0,1,false,""},{"Filter envelope",-6,6,false," oct"},{"FM ratio",.25,16,true,""},{"FM index",0,12,false,""},{"FM decay",.01f,4,true," s"},{"Amp attack",.001f,2,true," s"},{"Amp decay",.01f,4,true," s"},{"Amp sustain",0,1,false,""},{"Amp release",.02f,3,true," s"},{"Filter decay",.01f,4,true," s"},{"LFO rate",.1f,20,true," Hz"},{"Vibrato depth",0,100,false," cents"},{"Filter LFO depth",0,4,false," oct"},{"Table position",0,1,false,""},{"Table envelope",-1,1,false,""},{"Table decay",.01f,8,true," s"},{"Table LFO depth",0,1,false,""},{"Table family",0,7,false,""},{"Harmonic 1",0,1,false,""},{"Harmonic 2",0,1,false,""},{"Harmonic 3",0,1,false,""},{"Harmonic 4",0,1,false,""},{"Harmonic 5",0,1,false,""},{"Harmonic 6",0,1,false,""},{"Harmonic 7",0,1,false,""},{"Harmonic 8",0,1,false,""},{"Harmonic 9",0,1,false,""},{"Harmonic 10",0,1,false,""},{"Harmonic 11",0,1,false,""},{"Harmonic 12",0,1,false,""},{"Harmonic 13",0,1,false,""},{"Harmonic 14",0,1,false,""},{"Harmonic 15",0,1,false,""},{"Harmonic 16",0,1,false,""},{"Model: 0 string / 1 bar",0,1,false,""},{"Resonance decay",.1f,20,true," s T60"},{"High-mode damping",0,1,false,""},{"Excitation brightness",.05f,1,false,""},{"Excitation position",.05f,.95f,false,""},{"Body resonance",0,1,false,""},{"String stiffness",0,1,false,""}}};return specs;}
double synthParameterValue(const Track& t,int index){const auto& p=t.synth;switch(index){case 0:return p.mix;case 1:return p.detune;case 2:return p.coarse;case 3:return p.pulse;case 4:return p.sub;case 5:return p.noise;case 6:return p.cutoff;case 7:return p.resonance;case 8:return p.filterEnv;case 9:return p.fmRatio;case 10:return p.fmIndex;case 11:return p.fmDecay;case 12:return t.attack;case 13:return p.decay;case 14:return p.sustain;case 15:return t.release;case 16:return p.filterDecay;case 17:return p.lfoRate;case 18:return p.lfoPitch;case 19:return p.lfoFilter;case 20:return p.tablePosition;case 21:return p.tableEnv;case 22:return p.tableDecay;case 23:return p.tableLfo;case 24:return p.table;case 41:return p.physical.mode;case 42:return p.physical.decay;case 43:return p.physical.damping;case 44:return p.physical.brightness;case 45:return p.physical.position;case 46:return p.physical.body;case 47:return p.physical.stiffness;default:return index>=25&&index<41?p.harmonics[(size_t)(index-25)]:0;}}
double synthParameterNormal(int index,double value){if(index<0||index>=synthParameterCount)return 0;auto s=synthParameterSpecs()[(size_t)index];value=jlimit(s.low,s.high,value);return s.logarithmic?std::log(value/s.low)/std::log(s.high/s.low):(value-s.low)/(s.high-s.low);}
double synthParameterFromNormal(int index,double normal){if(index<0||index>=synthParameterCount||!std::isfinite(normal))return 0;auto s=synthParameterSpecs()[(size_t)index];normal=jlimit(0.,1.,normal);return s.logarithmic?s.low*std::pow(s.high/s.low,normal):s.low+(s.high-s.low)*normal;}
Result setSynthParameter(Track& track,int index,double value){if(index<0||index>=synthParameterCount||!std::isfinite(value))return Result::fail("Invalid synth parameter.");auto spec=synthParameterSpecs()[(size_t)index];if(value<spec.low-1e-7||value>spec.high+1e-7)return Result::fail("Synth value is outside its allowed range.");auto t=track;auto& p=t.synth;float v=(float)jlimit(spec.low,spec.high,value);switch(index){case 0:p.mix=v;break;case 1:p.detune=v;break;case 2:p.coarse=(int)std::round(v);break;case 3:p.pulse=v;break;case 4:p.sub=v;break;case 5:p.noise=v;break;case 6:p.cutoff=v;break;case 7:p.resonance=v;break;case 8:p.filterEnv=v;break;case 9:p.fmRatio=v;break;case 10:p.fmIndex=v;break;case 11:p.fmDecay=v;break;case 12:t.attack=v;break;case 13:p.decay=v;break;case 14:p.sustain=v;break;case 15:t.release=v;break;case 16:p.filterDecay=v;break;case 17:p.lfoRate=v;break;case 18:p.lfoPitch=v;break;case 19:p.lfoFilter=v;break;case 20:p.tablePosition=v;break;case 21:p.tableEnv=v;break;case 22:p.tableDecay=v;break;case 23:p.tableLfo=v;break;case 24:p.table=(int)std::round(v);break;case 41:p.physical.mode=(int)std::round(v);break;case 42:p.physical.decay=v;break;case 43:p.physical.damping=v;break;case 44:p.physical.brightness=v;break;case 45:p.physical.position=v;break;case 46:p.physical.body=v;break;case 47:p.physical.stiffness=v;break;default:if(index>=25&&index<41)p.harmonics[(size_t)(index-25)]=v;break;}auto valid=validateSynthPatch(p);if(valid.failed())return valid;t.soundName.clear();track=std::move(t);return Result::ok();}

}
