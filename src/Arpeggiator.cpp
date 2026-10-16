// SPDX-License-Identifier: AGPL-3.0-only
#include "Arpeggiator.h"
#include "NoteTools.h"
#include <cmath>
namespace pulse {
using namespace juce;
Result arpeggiateNotes(const std::vector<Note>& source,double patternLength,const std::set<int>& selection,const ArpOptions& o,std::vector<Note>& output,ArpStats* statistics){
    auto within=[](double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    if((int)o.order<0||(int)o.order>(int)ArpOrder::random||!within(o.rate,1./16,2)||!within(o.gate,.05,1)||o.octaves<1||o.octaves>4||o.transpose< -24||o.transpose>24||!within(o.velocity,.1f,1.5f))return Result::fail("Invalid arpeggiator order, rate, pitch range, gate or velocity.");
    NoteToolOptions identity;identity.positionStrength=0;std::vector<Note> validated;auto valid=processNotes(source,patternLength,selection,identity,validated);if(valid.failed())return valid;
    std::vector<Note> next;std::vector<int> target;ArpStats stats;double first=patternLength,last=0;
    for(int i=0;i<(int)source.size();++i){if(selection.empty()||selection.count(i)){target.push_back(i);first=jmin(first,source[(size_t)i].beat);last=jmax(last,jmin(patternLength,source[(size_t)i].beat+source[(size_t)i].length));}else next.push_back(source[(size_t)i]);}
    stats.inputNotes=(int)target.size();std::vector<int> previous;int phase=0;
    struct Candidate { int pitch,source; };
    for(int step=0;!target.empty()&&step<1025;++step){double beat=first+step*o.rate;if(beat>=last-1e-10)break;
        std::vector<int> active;std::vector<Candidate> candidates;std::array<bool,128> seen{};
        for(int index:target){const auto& n=source[(size_t)index];if(n.beat<=beat+1e-10&&n.beat+n.length>beat+1e-10)active.push_back(index);}
        for(int octave=0;octave<o.octaves;++octave)for(int index:active){int pitch=source[(size_t)index].pitch+o.transpose+12*octave;if(pitch<0||pitch>127||seen[(size_t)pitch])continue;seen[(size_t)pitch]=true;candidates.push_back({pitch,index});}
        if(o.restart&&active!=previous)phase=0;previous=active;
        if(candidates.empty()){++stats.restSteps;++phase;continue;}
        if(o.order!=ArpOrder::source)std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){return a.pitch<b.pitch;});
        int count=(int)candidates.size(),chosen=phase%count;
        if(o.order==ArpOrder::down)chosen=count-1-chosen;
        else if(o.order==ArpOrder::upDown||o.order==ArpOrder::downUp){int period=jmax(1,2*count-2),position=phase%period;chosen=position<count?position:period-position;if(o.order==ArpOrder::downUp)chosen=count-1-chosen;}
        else if(o.order==ArpOrder::random){uint32_t value=o.seed^((uint32_t)step*0x9e3779b9u)^0x41525053u;value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;chosen=(int)(value%(uint32_t)count);}
        const auto& selected=candidates[(size_t)chosen];auto note=source[(size_t)selected.source];double available=jmin(patternLength,note.beat+note.length)-beat;++phase;
        if(available<.01-1e-9){++stats.shortSteps;continue;}
        if(next.size()>=maxNotesPerLane)return Result::fail("Arpeggio would exceed 512 notes in this lane. Use a slower rate or fewer source notes.");
        note.beat=beat;note.length=jmax(.01,jmin(available,o.rate*o.gate));note.pitch=selected.pitch;note.velocity=jlimit(0.f,1.f,note.velocity*o.velocity);next.push_back(note);++stats.generated;
    }
    if(!target.empty()&&stats.generated==0)return Result::fail("No playable arpeggio notes. Reduce transposition or move the source away from the pattern end.");
    output=std::move(next);if(statistics)*statistics=stats;return Result::ok();
}
}
