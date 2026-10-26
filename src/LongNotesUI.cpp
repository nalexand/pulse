// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
bool Studio::startLongNotes(){
    if(recording||recorder.isActive()||!audioReady())return false;
    int slots=jmin(maxPatterns-(int)project.patterns.size(),maxClips-(int)project.clips.size());if(slots<1){status="Long NOTES needs a free pattern and Playlist slot.";return false;}
    auto plan=recordingPlan(false);if(!project.recording.punch)plan.startBeat=plan.gateBeat=project.songMode?recordingPosition():editCursorBeat;
    double length=jmin(slots*64.,audioTakeSeconds*project.bpm/60,4096-plan.gateBeat);if(plan.endBeat>=0)length=jmin(length,plan.endBeat-plan.gateBeat);if(!std::isfinite(length)||length<.25||plan.gateBeat<0){status="Choose an earlier Song cursor or a valid punch range for Long NOTES.";return false;}plan.endBeat=plan.gateBeat+length;
    releaseKeys();engine->stop();checkpoint();takeOldLoop=project.loop;takeOldMode=project.songMode;takeRestoreLoop=takeRestoreMode=true;takeTrack=selectedTrack;takeBeat=plan.gateBeat;takeGateEnd=plan.endBeat;longNotesActive=true;longNoteParts.clear();recordedKeys.clear();project.songMode=true;project.loop=false;recording=true;takeScheduled=true;changed();
    if(!engine->beginTake(plan)){longNotesActive=false;recording=false;restoreTakeTransport();status="Recording command queue busy; try REC again.";return false;}
    showPane(playlistPane);status="Long NOTES armed for up to "+String(length*60/project.bpm/60,2)+" min, bounded by available pattern slots/timeline. Play normally; REC/Stop finishes one Undo take.";return true;
}
int Studio::ensureLongNotePart(int part){
    if(auto it=longNoteParts.find(part);it!=longNoteParts.end())return it->second;
    double available=takeGateEnd-takeBeat-part*64.;if(part<0||available<=0||project.patterns.size()>=maxPatterns||project.clips.size()>=maxClips)return -1;
    Pattern pattern;pattern.name="Keyboard take "+String((int)project.patterns.size()+1)+" - part "+String(part+1);pattern.length=jmax(1.,jmin(64.,available));pattern.lanes.resize(project.tracks.size());int index=(int)project.patterns.size();project.patterns.push_back(std::move(pattern));
    Clip clip;clip.track=takeTrack;clip.pattern=index;clip.beat=takeBeat+part*64.;clip.length=jmin(64.,available);project.clips.push_back(clip);longNoteParts[part]=index;return index;
}
void Studio::recordLongNoteOn(int pitch){
    if(!recording||audioRecordMode||!engine->captureReady())return;double global=recordingPosition();if(global<takeBeat||global>=takeGateEnd)return;
    double local=std::floor((global-takeBeat)*4)/4;int part=(int)std::floor(local/64),index=ensureLongNotePart(part);if(index<0){status="Long NOTES bank/timeline limit reached.";return;}auto& notes=project.patterns[(size_t)index].lanes[(size_t)takeTrack];
    if(notes.size()>=maxNotesPerLane){finishLongNotes();status="Long NOTES stopped: 512 notes in one part. Recorded notes are retained.";return;}
    notes.push_back({local-part*64,jmin(.25,takeGateEnd-global),pitch,typingVelocity/127.f});recordedKeys[pitch]={index*maxNotesPerLane+(int)notes.size()-1,global};project.activePattern=index;selectedTrack=takeTrack;selectedNote=-1;selectedNotes.clear();pianoStart=pianoNoteCursor=0;changed();
}
void Studio::recordLongNoteOff(int pitch){
    auto it=recordedKeys.find(pitch);if(it==recordedKeys.end())return;int pattern=it->second.first/maxNotesPerLane,index=it->second.first%maxNotesPerLane;double started=it->second.second;recordedKeys.erase(it);
    if(pattern<0||pattern>=(int)project.patterns.size())return;auto& notes=project.patterns[(size_t)pattern].lanes[(size_t)takeTrack];if(index<0||index>=(int)notes.size())return;
    double duration=jmax(.01,jmin(takeGateEnd,recordingPosition())-started);auto original=notes[(size_t)index];notes[(size_t)index].length=jmin(64.,duration);
    // Project note gates are bounded to 64 beats. Very long holds preserve their
    // total duration as adjacent rearticulated notes in subsequent parts.
    double at=takeBeat;for(const auto& [part,p]:longNoteParts)if(p==pattern){at+=part*64+original.beat;break;}
    for(double offset=64;offset<duration-1e-7;offset+=64){double local=at+offset-takeBeat;int part=(int)std::floor(local/64),target=ensureLongNotePart(part);if(target<0)break;auto& lane=project.patterns[(size_t)target].lanes[(size_t)takeTrack];if(lane.size()>=maxNotesPerLane){status="Held-note tail exceeds the part's note capacity.";break;}auto n=original;n.beat=local-part*64;n.length=jmin(64.,duration-offset);lane.push_back(n);}
    changed();
}
void Studio::finishLongNotes(){
    if(!longNotesActive)return;double ended=jlimit(takeBeat,takeGateEnd,recordingPosition());releaseKeys();while(!recordedKeys.empty())recordLongNoteOff(recordedKeys.begin()->first);engine->stop();
    for(const auto& [part,index]:longNoteParts){auto& pattern=project.patterns[(size_t)index];double used=jmax(1.,jmin(64.,ended-takeBeat-part*64));for(const auto& n:pattern.lanes[(size_t)takeTrack])used=jmax(used,jmin(64.,n.beat+n.length));pattern.length=jmin(64.,std::ceil(used*4)/4);for(auto& clip:project.clips)if(!clip.isAudio()&&!clip.isAutomation()&&clip.pattern==index&&clip.track==takeTrack)clip.length=jmin(pattern.length,takeGateEnd-clip.beat);}
    int parts=(int)longNoteParts.size();longNotesActive=false;recording=false;takeRestoreMode=parts==0;recordedKeys.clear();longNoteParts.clear();restoreTakeTransport();changed();status=parts>0?"Long keyboard take saved in "+String(parts)+" consecutive pattern parts. One Undo/Redo restores the complete take.":"Long keyboard take finished without notes; no pattern or Playlist clip added.";
}
}
