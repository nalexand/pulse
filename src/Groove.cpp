// SPDX-License-Identifier: AGPL-3.0-only
#include "Groove.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
bool within(double x,double low,double high){return std::isfinite(x)&&x>=low&&x<=high;}
int nearestCell(double position){double lower=std::floor(position);return (int)(position-lower<=.5?lower:lower+1);}
Result sourceValid(const std::vector<Note>& source,double length,const std::set<int>& selected){
    if(!within(length,1,64)||source.size()>maxNotesPerLane)return Result::fail("Invalid groove source length or note count.");
    for(int index:selected)if(index<0||index>=(int)source.size())return Result::fail("Groove selection changed. Use current selection again.");
    for(const auto& note:source)if(!within(note.beat,0,length)||note.beat>=length||!within(note.length,.01,64)||!within(note.velocity,0,1)||!within(note.probability,0,1)||note.pitch<0||note.pitch>127||note.ratchets<1||note.ratchets>8||!validNoteCondition(note))return Result::fail("Invalid note in the groove source.");
    return Result::ok();
}
}
Result validateGroove(const GrooveSettings& settings){
    const auto& p=settings.pattern;if(!within(p.grid,.0625,4)||p.steps.size()<2||p.steps.size()>32||p.grid*p.steps.size()>64||!within(settings.timing,0,1)||!within(settings.velocity,0,1)||!within(settings.window,0,.5))return Result::fail("Use a groove of 2-32 cells, grid 0.0625-4 beats, cycle up to 64 beats, strengths 0-100% and window 0-50%.");
    for(const auto& step:p.steps)if(!within(step.offset,-.49,.49)||!within(step.velocity,0,1))return Result::fail("Groove cell offsets must be -49 to +49% of a grid cell, and velocities 0-100%.");return Result::ok();
}
Result applyGroove(const std::vector<Note>& source,double length,const std::set<int>& selected,const GrooveSettings& settings,std::vector<Note>& output,GrooveStats* stats){
    auto valid=sourceValid(source,length,selected);if(valid.failed())return valid;valid=validateGroove(settings);if(valid.failed())return valid;auto next=source;GrooveStats report;
    for(size_t i=0;i<source.size();++i){if(!selected.empty()&&!selected.count((int)i))continue;++report.notes;const auto& original=source[i];int cell=nearestCell(original.beat/settings.pattern.grid);const auto& step=settings.pattern.steps[(size_t)cell%settings.pattern.steps.size()];if(!step.active)continue;double straight=cell*settings.pattern.grid;if(settings.useWindow&&std::abs(original.beat-straight)>settings.window*settings.pattern.grid+1e-10)continue;
        auto& note=next[i];double target=straight+step.offset*settings.pattern.grid;if(settings.timing>0)note.beat=original.beat+(target-original.beat)*settings.timing;
        if(!std::isfinite(note.beat)||note.beat<0||note.beat>=length)return Result::fail("Groove would move a note outside this pattern. Reduce timing strength, edit the boundary cell, or disable that cell.");
        if(settings.velocity>0)note.velocity=(float)(original.velocity+(step.velocity-original.velocity)*settings.velocity);report.changed+=note!=original;
    }
    for(const auto& step:settings.pattern.steps)report.cells+=step.active;output=std::move(next);if(stats)*stats=report;return Result::ok();
}
Result extractGroove(const std::vector<Note>& source,double length,const std::set<int>& selected,double start,double grid,int steps,GroovePattern& output,GrooveStats* stats){
    auto valid=sourceValid(source,length,selected);if(valid.failed())return valid;GrooveSettings checked;checked.pattern.grid=grid;if(steps<2||steps>32)return Result::fail("Capture 2-32 groove cells.");checked.pattern.steps.resize((size_t)steps);valid=validateGroove(checked);if(valid.failed())return valid;if(!within(start,0,length)||start+grid*steps>length+1e-9)return Result::fail("The complete capture cycle must fit inside this pattern.");
    std::array<double,32> offsets{},velocities{};std::array<int,32> counts{};GrooveStats report;
    for(size_t i=0;i<source.size();++i){if(!selected.empty()&&!selected.count((int)i))continue;const auto& note=source[i];if(note.beat<start||note.beat>=start+grid*steps)continue;double position=(note.beat-start)/grid;int cell=nearestCell(position);double offset=position-cell;if(!within(offset,-.49,.49))return Result::fail("An onset lies halfway between groove cells. Choose a finer grid or a different capture range.");cell%=steps;offsets[(size_t)cell]+=offset;velocities[(size_t)cell]+=note.velocity;++counts[(size_t)cell];++report.notes;}
    if(!report.notes)return Result::fail("No selected source notes fall inside this capture cycle.");GroovePattern next;next.grid=grid;next.steps.resize((size_t)steps);for(int i=0;i<steps;++i){auto& step=next.steps[(size_t)i];step.active=counts[(size_t)i]>0;if(step.active){step.offset=jlimit(-.49,.49,offsets[(size_t)i]/counts[(size_t)i]);step.velocity=(float)(velocities[(size_t)i]/counts[(size_t)i]);++report.cells;}}
    output=std::move(next);if(stats)*stats=report;return Result::ok();
}
}
