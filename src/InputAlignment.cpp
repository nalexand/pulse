// SPDX-License-Identifier: AGPL-3.0-only
#include "InputAlignment.h"
#include <cmath>
namespace pulse {
using namespace juce;
String inputAlignmentName(int mode){return StringArray{"Off","Manual","Input + manual","Input/output + manual"}[jlimit(0,3,mode)];}
Result validateInputAlignment(const InputAlignmentSettings& s){return s.mode>=0&&s.mode<=3&&std::isfinite(s.manualMs)&&s.manualMs>=-1000&&s.manualMs<=1000?Result::ok():Result::fail("Choose Off, Manual, Input or Input/output; manual correction must be -1000 to +1000 ms.");}
var inputAlignmentToJson(const InputAlignmentSettings& s){auto* d=new DynamicObject();d->setProperty("schema",1);d->setProperty("mode",s.mode);d->setProperty("manualMs",s.manualMs);return var(d);}
Result inputAlignmentFromJson(const var& v,InputAlignmentSettings& out){
    if(!v.isObject()||!v["schema"].isInt()||(int)v["schema"]!=1||!v["mode"].isInt()||!(v["manualMs"].isInt()||v["manualMs"].isInt64()||v["manualMs"].isDouble()))return Result::fail("Invalid input alignment preferences.");
    for(const auto& field:v.getDynamicObject()->getProperties())if(field.name.toString()!="schema"&&field.name.toString()!="mode"&&field.name.toString()!="manualMs")return Result::fail("Unknown input alignment preference.");
    InputAlignmentSettings next{(int)v["mode"],(double)v["manualMs"]};auto valid=validateInputAlignment(next);if(valid.wasOk())out=next;return valid;
}
Result resolveInputAlignment(const InputAlignmentSettings& s,double rate,int input,int output,int64_t& frames){
    auto valid=validateInputAlignment(s);if(valid.failed())return valid;
    if(!std::isfinite(rate)||rate<8000||rate>192000)return Result::fail("Input alignment needs a current 8-192 kHz audio device.");
    double value=0;if(s.mode){value=s.manualMs*rate/1000;if(s.mode>=2){if(input<0)return Result::fail("The driver did not report a valid input latency.");value+=input;}if(s.mode==3){if(output<0)return Result::fail("The driver did not report a valid output latency.");value+=output;}}
    if(!std::isfinite(value)||std::abs(value)>rate*2)return Result::fail("Combined input alignment exceeds two seconds. Check the driver report/manual correction.");
    frames=(int64_t)std::llround(value);return Result::ok();
}
Result alignInputWindow(double start,double gate,double end,double bpm,double rate,int64_t offset,AlignedInputWindow& out){
    if(!std::isfinite(start)||!std::isfinite(gate)||!std::isfinite(end)||!std::isfinite(bpm)||!std::isfinite(rate)||start<0||gate<start||end<=gate||end>4096||bpm<30||bpm>300||rate<8000||rate>192000||offset< -rate*2||offset>rate*2)return Result::fail("Invalid bounded input recording window.");
    double framesPerBeat=60*rate/bpm,shift=offset/framesPerBeat;AlignedInputWindow next;
    next.gateBeat=jmax(0.,gate+shift);next.startBeat=jmin(start,next.gateBeat);next.endBeat=jmin(4096.,end+shift);next.clipBeat=next.gateBeat-shift;next.limited=gate+shift<0||end+shift>4096;
    double count=std::floor((next.endBeat-next.gateBeat)*framesPerBeat+1e-7);
    if(next.gateBeat>4096||next.clipBeat<0||next.clipBeat>=4096||!std::isfinite(count)||count<1)return Result::fail("No aligned audio fits before the timeline boundary. Move the cursor/range or reduce the correction.");
    next.frames=(uint64_t)count;next.endBeat=next.gateBeat+next.frames/framesPerBeat;
    if(next.clipBeat+next.frames/framesPerBeat>4096+1e-8)return Result::fail("Aligned take exceeds the musical timeline.");
    out=next;return Result::ok();
}
}
