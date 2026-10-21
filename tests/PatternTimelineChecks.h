// SPDX-License-Identifier: AGPL-3.0-only
#include "PatternTimeline.h"
static void testPatternTimeline(){
    auto p=emptyProject();p.patterns[0].length=8;p.songMode=true;p.clips={{7,0,13,20}};
    auto at=[&](int track,double beat){return patternPlaybackPositions(p,track,beat);};
    expect(at(7,0).empty()&&at(7,12.999).empty(),"Song editor shows no local playhead before its pattern clip");
    expect(at(7,13)==std::vector<double>{0}&&at(7,15.25)==std::vector<double>{2.25},"Song playhead subtracts an off-grid clip start instead of wrapping absolute Song time");
    expect(at(7,21)==std::vector<double>{0}&&at(7,23.25)==std::vector<double>{2.25}&&at(7,32.5)==std::vector<double>{3.5},"repeated and final partial clip passes use source-pattern length");
    expect(at(7,33).empty()&&at(7,100).empty(),"clip end is exclusive and later silence hides local playhead");
    expect(at(5,15.25).empty(),"a pattern placed on another source channel does not animate this Piano roll");
    Pattern second=p.patterns[0];second.editorId=Uuid().toString();p.patterns.push_back(second);p.activePattern=1;p.clips.push_back({7,1,48,8});expect(at(7,15.25).empty()&&at(7,48)==std::vector<double>{0},"editing a later pattern shows no playhead for a currently playing different pattern");
    p.activePattern=0;p.clips.push_back({7,0,15,8});expect(at(7,16)==std::vector<double>({1,3}),"overlapping instances display each distinct source phase");p.clips.push_back({7,0,5,30});expect(at(7,16)==std::vector<double>({1,3}),"coincident repeat phases are drawn only once");
    p.clips.clear();Clip audio;audio.track=7;audio.pattern=0;audio.audioPath="source.wav";audio.length=20;p.clips.push_back(audio);Clip automation;automation.track=7;automation.pattern=0;automation.automationTarget=0;automation.length=20;p.clips.push_back(automation);expect(at(7,4).empty(),"audio and automation clips do not impersonate pattern playback");
    p.songMode=false;expect(at(7,0)==std::vector<double>{0}&&at(7,3.25)==std::vector<double>{3.25}&&at(7,11.25)==std::vector<double>{3.25},"PAT mode keeps its own source-pattern clock");p.patterns[0].length=16;expect(at(7,11.25)==std::vector<double>{11.25},"64-step PAT phase uses sixteen beats");p.patterns[0].length=8;expect(at(7,11.25)==std::vector<double>{3.25},"32-step phase uses eight beats without time scaling");
    expect(at(7,-1).empty()&&at(7,std::nan("")).empty()&&at(7,std::numeric_limits<double>::infinity()).empty(),"invalid transport coordinates produce no editor playhead");expect(at(-1,0).empty()&&at(16,0).empty(),"invalid editor channels produce no playhead");p.tracks[7].isBus=true;expect(at(7,0).empty(),"mixer buses have no pattern playback position");p.tracks[7].isBus=false;p.activePattern=99;expect(at(7,0).empty(),"invalid active pattern is handled safely");p.activePattern=0;p.patterns[0].length=0;expect(at(7,0).empty(),"zero pattern length cannot enter modulo arithmetic");
}
