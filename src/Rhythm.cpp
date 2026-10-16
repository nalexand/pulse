// SPDX-License-Identifier: AGPL-3.0-only
#include "Rhythm.h"
#include "NoteTools.h"
#include <cmath>
namespace pulse {
using namespace juce;
Result generateEuclidean(const std::vector<Note>& source,double patternLength,const RhythmOptions& o,std::vector<Note>& output,RhythmStats* statistics){
    auto valid=[](double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    if(!valid(patternLength,1,64)||!valid(o.start,0,patternLength)||!valid(o.end,0,patternLength)||o.end-o.start<.01||o.steps<1||o.steps>64||o.pulses<0||o.pulses>o.steps||o.rotation<0||o.rotation>=o.steps||o.cycles<1||o.cycles>16||o.pitch<0||o.pitch>127||!valid(o.gate,.05,1)||!valid(o.velocity,.01f,1)||!valid(o.variation,0,.5)||(o.write!=RhythmWriteMode::replacePitch&&o.write!=RhythmWriteMode::fill))return Result::fail("Invalid rhythm settings or beat range.");
    double cell=(o.end-o.start)/(o.steps*o.cycles);if(cell<.01-1e-12)return Result::fail("Range is too short: each step needs at least 0.01 beats. Reduce steps/repeats or widen the range.");
    NoteToolOptions identity;identity.positionStrength=0;std::vector<Note> next;auto result=processNotes(source,patternLength,{},identity,next);if(result.failed())return result;
    RhythmStats stats;
    if(o.write==RhythmWriteMode::replacePitch){auto end=std::remove_if(next.begin(),next.end(),[&](const Note& n){return n.pitch==o.pitch&&n.beat>=o.start&&n.beat<o.end;});stats.removed=(int)(next.end()-end);next.erase(end,next.end());}
    for(int cycle=0;cycle<o.cycles;++cycle)for(int step=0;step<o.steps;++step){int phase=(step-o.rotation+o.steps)%o.steps;if(o.pulses==0||(phase*o.pulses)%o.steps>=o.pulses)continue;int index=cycle*o.steps+step;double beat=o.start+index*cell;
        if(o.write==RhythmWriteMode::fill&&std::any_of(next.begin(),next.end(),[&](const Note& n){return n.pitch==o.pitch&&std::abs(n.beat-beat)<1e-6;})){++stats.skipped;continue;}
        if(next.size()>=maxNotesPerLane)return Result::fail("Rhythm would exceed 512 notes in this channel. Reduce repeats/pulses or use a smaller range.");
        uint32_t value=o.seed^((uint32_t)index*0x9e3779b9u)^0x52485954u;value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;
        float variation=(float)(value>>8)/8388608.f-1;float velocity=jlimit(.01f,1.f,o.velocity+variation*o.variation);double length=jmax(.01,cell*o.gate);
        next.push_back({beat,length,o.pitch,velocity,1,1});++stats.added;
    }
    output=std::move(next);if(statistics)*statistics=stats;return Result::ok();
}
}
