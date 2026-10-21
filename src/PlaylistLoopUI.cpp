// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
void Studio::cancelPlaylistLoopGesture(){if(dragAction==playlistLoopDrag)dragAction=0;playlistLoopMoved=false;playlistLoopMenu=false;repaint();}
bool Studio::playlistLoopMouseDown(const Hit& hit,const MouseEvent& event){
    if(hit.action!=seekRuler||hit.data!=playlistPane||(!event.mods.isRightButtonDown()&&!event.mods.isShiftDown()))return false;
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before choosing a Playlist loop.";return true;}
    cancelPlaylistGesture();playlistLoopAnchor=jlimit(0.,4096.,snapped(playlistStart+(event.position.x-arrangeRect.getX())/arrangeRect.getWidth()*playlistSpan,event.mods.isAltDown()));
    playlistLoopRevision=revision;playlistLoopMenu=event.mods.isRightButtonDown();playlistLoopStart=playlistLoopEnd=playlistLoopAnchor;dragAction=playlistLoopDrag;return true;
}
bool Studio::playlistLoopMouseDrag(const MouseEvent& event){
    if(dragAction!=playlistLoopDrag)return false;
    if(revision!=playlistLoopRevision||recording||recorder.isActive()||pendingNoteTool()){cancelPlaylistLoopGesture();status="The project changed; choose the loop range again.";return true;}
    if(std::abs(event.position.x-dragOrigin.x)<3)return true;
    double at=jlimit(0.,4096.,snapped(playlistStart+(event.position.x-arrangeRect.getX())/arrangeRect.getWidth()*playlistSpan,event.mods.isAltDown()));
    playlistLoopStart=jmin(playlistLoopAnchor,at);playlistLoopEnd=jmax(playlistLoopAnchor,at);playlistLoopMoved=playlistLoopEnd-playlistLoopStart>=.25;
    status="Song loop preview: bars "+String(playlistLoopStart/4+1,3)+" to "+String(playlistLoopEnd/4+1,3)+". Release sets it; Space plays; Escape cancels.";repaint();return true;
}
bool Studio::playlistLoopMouseUp(const MouseEvent& event){
    if(dragAction!=playlistLoopDrag)return false;playlistLoopMouseDrag(event);if(dragAction!=playlistLoopDrag)return true;
    bool moved=playlistLoopMoved,menu=playlistLoopMenu;double start=playlistLoopStart,end=playlistLoopEnd,anchor=playlistLoopAnchor;auto rev=playlistLoopRevision;cancelPlaylistLoopGesture();
    if(rev!=revision){status="The project changed; choose the loop range again.";return true;}
    if(moved)applyPlaylistLoop(start,end,rev);else if(std::abs(event.position.x-dragOrigin.x)<3){editCursorBeat=anchor;if(menu)showMarkerMenu(anchor);else engine->seekRequested.store(anchor);}else status="Choose a loop spanning at least a quarter beat. Nothing changed.";return true;
}
bool Studio::applyPlaylistLoop(double start,double end,uint64_t expected){
    if(expected!=revision||recording||recorder.isActive()||pendingNoteTool()){status="Finish the take/preview or choose the loop again after project edits.";return false;}
    Project next;auto valid=setPlaylistLoopRange(project,start,end,next);if(valid.failed()){status=valid.getErrorMessage();return false;}
    auto selection=playlistSelection();if(auto* range=findSongMarker(project,playlistLoopMarkerId);range&&range->beat==start&&range->endBeat==end&&project.loop&&project.songMode&&project.loopSection==playlistLoopMarkerId){engine->seekRequested.store(start);status="Song loop is already set. Space plays; LOOP toggles repetition.";return true;}
    int editTrack=selectedTrack,editNote=selectedNote,editPoint=selectedPoint,editLow=pianoLow;auto editNotes=selectedNotes;
    if(!commitPlaylistGroup(std::move(next),std::vector<int>(selection.begin(),selection.end()),selectedClip,"Song loop: bars "+String(start/4+1,3)+" to "+String(end/4+1,3)+". All tracks play; Space starts; LOOP toggles repetition."))return false;
    selectedTrack=editTrack;selectedNote=editNote;selectedPoint=editPoint;pianoLow=editLow;selectedNotes=std::move(editNotes);editCursorBeat=start;engine->seekRequested.store(start);repaint();return true;
}
}
