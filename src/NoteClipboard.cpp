// SPDX-License-Identifier: AGPL-3.0-only
#include "PatternTools.h"
#include "NoteTools.h"
#include <cmath>
namespace pulse {
using namespace juce;
Result pasteLaneNotes(const std::vector<Note>& copied,const std::vector<Note>& destination,double length,NotePasteMode mode,double cursor,bool replace,std::vector<Note>& output,double& outputLength){
    if(mode!=NotePasteMode::atCursor&&mode!=NotePasteMode::originalPositions&&mode!=NotePasteMode::atStart)return Result::fail("Invalid note paste placement.");
    NoteToolOptions identity;identity.positionStrength=0;std::vector<Note> validated;auto valid=processNotes(copied,64,{},identity,validated);if(valid.failed())return valid;
    valid=processNotes(destination,length,{},identity,validated);if(valid.failed())return valid;
    if(copied.empty())return Result::fail("No notes copied. Open the source Piano roll, select notes and press Ctrl+C.");
    if((replace?0:destination.size())+copied.size()>maxNotesPerLane)return Result::fail("Paste exceeds 512 notes on the destination channel.");
    if(mode==NotePasteMode::atCursor&&(!std::isfinite(cursor)||cursor<0||cursor>=64))return Result::fail("Paste cursor is outside 0-64 beats. Use Paste at start or Paste at original positions.");
    double first=64;for(const auto& note:copied)first=jmin(first,note.beat);
    double shift=mode==NotePasteMode::originalPositions?0:(mode==NotePasteMode::atStart?0:cursor)-first,last=0;
    for(const auto& note:copied){double at=note.beat+shift;if(at<0||at>=64||at+note.length>64+1.e-9)return Result::fail("Pasted notes would exceed 64 beats. Use Paste at start or select a shorter phrase.");last=jmax(last,at+note.length);}
    std::vector<Note> next=replace?std::vector<Note>{}:destination;for(auto note:copied){note.beat+=shift;next.push_back(note);}
    double nextLength=jmax(length,jmin(64.,std::ceil(last/4)*4));output=std::move(next);outputLength=nextLength;return Result::ok();
}
Result copyNotesToPattern(const Project& source,int pattern,int track,const std::set<int>& selection,int target,int targetTrack,bool replace,bool alignStart,Project& output){
    if(source.tracks.empty()||source.tracks.size()>maxTracks||source.patterns.empty()||source.patterns.size()>maxPatterns||pattern<0||target<0||pattern>=(int)source.patterns.size()||target>=(int)source.patterns.size()||track<0||targetTrack<0||track>=(int)source.tracks.size()||targetTrack>=(int)source.tracks.size()||source.patterns[(size_t)pattern].lanes.size()!=source.tracks.size()||source.patterns[(size_t)target].lanes.size()!=source.tracks.size())return Result::fail("Choose valid source and destination pattern channels.");
    if(source.tracks[(size_t)track].isBus||source.tracks[(size_t)targetTrack].isBus)return Result::fail("Copy notes between instrument channels, not mixer buses.");
    if(pattern==target&&track==targetTrack)return Result::fail("Choose a different destination pattern or channel.");
    const auto& lane=source.patterns[(size_t)pattern].lanes[(size_t)track];NoteToolOptions identity;identity.positionStrength=0;std::vector<Note> checked;auto valid=processNotes(lane,source.patterns[(size_t)pattern].length,selection,identity,checked);if(valid.failed())return valid;
    std::vector<Note> copied;for(int i=0;i<(int)lane.size();++i)if(selection.empty()||selection.count(i))copied.push_back(lane[(size_t)i]);
    std::vector<Note> notes;double length=0;valid=pasteLaneNotes(copied,source.patterns[(size_t)target].lanes[(size_t)targetTrack],source.patterns[(size_t)target].length,alignStart?NotePasteMode::atStart:NotePasteMode::originalPositions,0,replace,notes,length);if(valid.failed())return valid;
    Project next=source;next.patterns[(size_t)target].lanes[(size_t)targetTrack]=std::move(notes);next.patterns[(size_t)target].length=length;next.activePattern=target;output=std::move(next);return Result::ok();
}
}
