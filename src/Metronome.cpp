// SPDX-License-Identifier: AGPL-3.0-only
#include "Metronome.h"
#include <cmath>
namespace pulse {
using namespace juce;
String metronomeSoundName(int sound){static const char* names[]={"Classic tone","Wood block","Hi-hat","Rim click"};return sound>=0&&sound<metronomeSoundCount?names[sound]:"Unknown";}
Result validateMetronome(const MetronomeSettings& s){return s.sound>=0&&s.sound<metronomeSoundCount&&s.level>=0&&s.level<=200?Result::ok():Result::fail("Choose a metronome sound and a whole-number level from 0 to 200%.");}
uint32_t packMetronome(const MetronomeSettings& s){return (uint32_t)jlimit(0,3,s.sound)|((uint32_t)jlimit(0,200,s.level)<<2)|(s.accent?1024u:0u);}
MetronomeSettings unpackMetronome(uint32_t bits){return {(int)(bits&3),jmin(200,(int)((bits>>2)&255)),(bits&1024)!=0};}
Result loadMetronome(const File& file,MetronomeSettings& output){
    if(!file.existsAsFile())return Result::ok();if(file.getSize()>4096)return Result::fail("Metronome settings file is too large.");
    auto v=JSON::parse(file.loadFileAsString());if(!v.isObject()||!v["schema"].isInt()||(int)v["schema"]!=1||!v["sound"].isInt()||!v["level"].isInt()||!v["accent"].isBool())return Result::fail("Invalid local metronome settings.");
    MetronomeSettings next{(int)v["sound"],(int)v["level"],(bool)v["accent"]};auto valid=validateMetronome(next);if(valid.wasOk())output=next;return valid;
}
Result saveMetronome(const File& file,const MetronomeSettings& s){
    auto valid=validateMetronome(s);if(valid.failed())return valid;auto made=file.getParentDirectory().createDirectory();if(made.failed())return made;
    auto* v=new DynamicObject();v->setProperty("schema",1);v->setProperty("sound",s.sound);v->setProperty("level",s.level);v->setProperty("accent",s.accent);
    TemporaryFile temp(file);if(!temp.getFile().replaceWithText(JSON::toString(var(v),false))||!temp.overwriteTargetFileWithTemporary())return Result::fail("Could not save local metronome settings.");return Result::ok();
}
void MetronomeClick::start(double sampleRate,bool barStart,const MetronomeSettings& s){
    rate=sampleRate;total=remaining=jmax(1,(int)(rate*.024));sound=jlimit(0,3,s.sound);bool accented=barStart&&s.accent;frequency=accented?2200.f:1500.f;level=(accented?.13f:.08f)*jlimit(0,200,s.level)/100.f;noise=accented?0x5f3759dfu:0x1234567u;previousNoise=0;
}
float MetronomeClick::next(){
    if(remaining<=0)return 0;constexpr double pi=3.14159265358979323846;float out=0;
    // Retain the original tone expression exactly at the default settings.
    if(sound==0)out=(float)(std::sin(remaining*2*pi*frequency/rate)*level*remaining/(rate*.024));
    else{double elapsed=(total-remaining)/rate,envelope=(double)remaining/total;noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;float white=(float)(noise&0xffffff)/8388607.5f-1;double phase=2*pi*frequency*elapsed;
        if(sound==1)out=(float)(level*(std::sin(phase*.65)+.45*std::sin(phase*1.71))*std::exp(-elapsed*190)*envelope);
        else if(sound==2)out=(float)(level*(white-previousNoise)*.72*envelope*envelope);
        else out=(float)(level*(.8*std::sin(phase*1.15)+.35*white)*std::exp(-elapsed*310)*envelope);
        previousNoise=white;
    }
    --remaining;return out;
}
}
