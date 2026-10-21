// SPDX-License-Identifier: AGPL-3.0-only
#include "NoteTools.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
bool within(double value,double lo,double hi){return std::isfinite(value)&&value>=lo&&value<=hi;}
double jitter(uint32_t seed,uint32_t key,uint32_t salt){uint32_t value=seed^(key*0x9e3779b9u)^salt;value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;return(double)(value>>8)/8388608.-1;}
double gridTarget(double beat,double length,const NoteToolOptions& options){
    int centre=(int)std::floor(beat/options.grid);double target=0,best=std::abs(beat);
    for(int step=jmax(0,centre-2);step<=centre+2;++step){double at=step*options.grid+(step%2?options.swing*options.grid:0);if(at>=length)continue;double distance=std::abs(at-beat);if(distance<best-1e-12||(std::abs(distance-best)<1e-12&&at<target)){target=at;best=distance;}}
    return target;
}
}
Result processNotes(const std::vector<Note>& source,double patternLength,const std::set<int>& selection,const NoteToolOptions& options,std::vector<Note>& output){
    if(!within(patternLength,1,64)||source.size()>maxNotesPerLane)return Result::fail("Invalid note-tool source length or note count.");
    if((options.mode!=NoteToolMode::quantize&&options.mode!=NoteToolMode::humanize)||!within(options.grid,1./64,4)||!within(options.positionStrength,0,1)||!within(options.lengthStrength,0,1)||!within(options.swing,0,.5)||!within(options.window,0,4)||!within(options.timing,0,.25)||!within(options.length,0,.25)||!within(options.velocity,0,.5))return Result::fail("Invalid quantize or humanize settings.");
    for(int index:selection)if(index<0||index>=(int)source.size())return Result::fail("Note selection is no longer valid.");
    auto next=source;
    for(int index=0;index<(int)source.size();++index){const auto& original=source[(size_t)index];if(!within(original.beat,0,patternLength)||original.beat>=patternLength||!within(original.length,.01,64)||!within(original.velocity,0,1)||!within(original.probability,0,1)||original.pitch<0||original.pitch>127||original.ratchets<1||original.ratchets>8||!validNoteCondition(original))return Result::fail("Invalid note in the source lane.");if(!selection.empty()&&!selection.count(index))continue;auto& note=next[(size_t)index];
        if(options.mode==NoteToolMode::quantize){double target=gridTarget(original.beat,patternLength,options);if(options.positionStrength>0&&(!options.useWindow||std::abs(target-original.beat)<=options.window+1e-12))note.beat=original.beat+(target-original.beat)*options.positionStrength;
            if(options.lengthStrength>0){double length=jlimit(.01,64.,jmax(options.grid,std::round(original.length/options.grid)*options.grid));note.length=original.length+(length-original.length)*options.lengthStrength;}
        }else{uint32_t onsetKey=options.keepChords?(uint32_t)std::llround(original.beat*1000000):(uint32_t)index;
            if(options.timing>0)note.beat=jlimit(0.,patternLength-.000001,original.beat+jitter(options.seed,onsetKey,0x54494d45)*options.timing);
            if(options.length>0)note.length=jlimit(.01,64.,original.length+jitter(options.seed,(uint32_t)index,0x4c454e47)*options.length);
            if(options.velocity>0)note.velocity=(float)jlimit(0.,1.,original.velocity+jitter(options.seed,(uint32_t)index,0x56454c4f)*options.velocity);
        }
    }output=std::move(next);return Result::ok();
}
}
