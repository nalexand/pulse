// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
namespace pulse {
using namespace juce;
std::set<int> Studio::playlistSelection()const{
    std::set<int> result;if(selectedClip<0||selectedClip>=(int)project.clips.size())return result;
    if(selectedPlaylistIds.empty()||playlistPrimaryId!=project.clips[(size_t)selectedClip].editorId){result.insert(selectedClip);return result;}
    for(int i=0;i<(int)project.clips.size();++i)if(selectedPlaylistIds.count(project.clips[(size_t)i].editorId))result.insert(i);return result;
}
void Studio::setPlaylistSelection(const std::set<int>& indices,int primary){
    ensurePlaylistEditorIds(project);selectedPlaylistIds.clear();std::set<int> valid;for(int i:indices)if(i>=0&&i<(int)project.clips.size()){valid.insert(i);selectedPlaylistIds.insert(project.clips[(size_t)i].editorId);}
    selectedClip=valid.count(primary)?primary:valid.empty()?-1:*valid.begin();if(selectedClip<0||selectedClip>=(int)project.clips.size()){selectedClip=-1;playlistPrimaryId.clear();return;}
    playlistPrimaryId=project.clips[(size_t)selectedClip].editorId;if(selectedTrack!=project.clips[(size_t)selectedClip].track)releaseKeys();selectedTrack=project.clips[(size_t)selectedClip].track;repaint();
}
void Studio::reconcilePlaylistSelection(){
    if(selectedClip<0||selectedClip>=(int)project.clips.size()||(!selectedPlaylistIds.empty()&&playlistPrimaryId!=project.clips[(size_t)selectedClip].editorId)){selectedPlaylistIds.clear();playlistPrimaryId.clear();return;}
    std::set<String> retained;for(const auto& clip:project.clips)if(selectedPlaylistIds.count(clip.editorId))retained.insert(clip.editorId);selectedPlaylistIds=std::move(retained);
}
Rectangle<float> Studio::playlistClipBounds(const Clip& clip)const{return {arrangeRect.getX()+(float)((clip.beat-playlistStart)/playlistSpan)*arrangeRect.getWidth()+1,arrangeRect.getY()+clip.track*playlistLaneHeight+2,jmax(3.f,(float)(clip.length/playlistSpan)*arrangeRect.getWidth()-2),playlistLaneHeight-4};}
void Studio::cancelPlaylistGesture(){cancelPlaylistLoopGesture();playlistDragSource.reset();playlistDragCandidate.reset();playlistDragIndices.clear();playlistDragStats={};playlistLassoBase.clear();if(dragAction==playlistGroupDrag||dragAction==playlistLasso)dragAction=0;selectionBox={};repaint();}
bool Studio::playlistMouseDown(const Hit& hit,const MouseEvent& event){
    if(hit.action!=clipHit&&hit.action!=arrangeGrid)return false;
    if(event.mods.isRightButtonDown()||editorTool==2)return false;
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before arranging clips.";return true;}
    cancelPlaylistGesture();
    if(hit.action==arrangeGrid){
        if(editorTool!=3&&!event.mods.isCtrlDown()){setPlaylistSelection({});return false;}
        playlistLassoAdd=event.mods.isShiftDown();if(playlistLassoAdd)for(int i:playlistSelection())playlistLassoBase.insert(project.clips[(size_t)i].editorId);else setPlaylistSelection({});
        dragAction=playlistLasso;playlistDragRevision=revision;selectionBox={event.position.x,event.position.y,0,0};status="Drag across clips to select; Ctrl+click toggles clips. Shift adds to a lasso.";return true;
    }
    if(hit.data<0||hit.data>=(int)project.clips.size())return true;auto selected=playlistSelection();
    if(event.mods.isCtrlDown()){if(selected.count(hit.data))selected.erase(hit.data);else selected.insert(hit.data);setPlaylistSelection(selected,hit.data);status=String((int)selected.size())+" Playlist clips selected.";return true;}
    if(!selected.count(hit.data)){selected={hit.data};setPlaylistSelection(selected,hit.data);}else setPlaylistSelection(selected,hit.data);
    if(selected.size()==1&&!event.mods.isShiftDown()){const auto& clip=project.clips[(size_t)hit.data];if(event.position.x>=hit.rect.getRight()-6||(clip.isAudio()&&event.position.x<=hit.rect.getX()+6))return false;}
    playlistDragSource=std::make_unique<Project>(project);playlistDragIndices=selected;playlistDragPrimary=hit.data;playlistDragRevision=revision;playlistDragCopy=event.mods.isShiftDown();playlistDragDelta=0;playlistDragTrackDelta=0;dragAction=playlistGroupDrag;status=String((int)selected.size())+(playlistDragCopy?" clips: drag to copy.":" clips: drag to move.");return true;
}
bool Studio::playlistMouseDrag(const MouseEvent& event){
    if(dragAction!=playlistGroupDrag&&dragAction!=playlistLasso)return false;
    if(revision!=playlistDragRevision||recording||recorder.isActive()||pendingNoteTool()){cancelPlaylistGesture();status="The project changed. Start the Playlist gesture again.";return true;}
    if(dragAction==playlistLasso){selectionBox=Rectangle<float>::leftTopRightBottom(jmin(event.position.x,dragOrigin.x),jmin(event.position.y,dragOrigin.y),jmax(event.position.x,dragOrigin.x),jmax(event.position.y,dragOrigin.y)).getIntersection(arrangeRect);std::set<int> next;for(int i=0;i<(int)project.clips.size();++i)if((playlistLassoAdd&&playlistLassoBase.count(project.clips[(size_t)i].editorId))||playlistClipBounds(project.clips[(size_t)i]).intersects(selectionBox))next.insert(i);setPlaylistSelection(next);return true;}
    if(!playlistDragSource||event.position.getDistanceFrom(dragOrigin)<3)return true;
    PlaylistGroupStats bounds;auto valid=inspectPlaylistGroup(*playlistDragSource,playlistDragIndices,bounds);if(valid.failed()){status=valid.getErrorMessage();return true;}
    double delta=snapped((event.position.x-dragOrigin.x)/arrangeRect.getWidth()*playlistSpan,event.mods.isAltDown());double lo=-bounds.start,hi=4096-bounds.end;if(lo>hi){status="The whole selection exceeds the timeline span.";return true;}delta=jlimit(lo,hi,delta);
    // Group arrangements retain their existing lanes; a single clip keeps the familiar lane drag.
    int tracks=playlistDragIndices.size()==1?(int)std::round((event.position.y-dragOrigin.y)/playlistLaneHeight):0;tracks=jlimit(-bounds.lowTrack,(int)project.tracks.size()-1-bounds.highTrack,tracks);
    if(!playlistDragCopy&&delta==0&&tracks==0){playlistDragCandidate.reset();playlistDragStats={};repaint();return true;}if(playlistDragCandidate&&delta==playlistDragDelta&&tracks==playlistDragTrackDelta)return true;
    auto candidate=std::make_unique<Project>();PlaylistGroupStats stats;valid=transformPlaylistGroup(*playlistDragSource,playlistDragIndices,delta,tracks,playlistDragCopy,true,*candidate,&stats);if(valid.failed()){playlistDragCandidate.reset();status=valid.getErrorMessage();repaint();return true;}
    playlistDragDelta=delta;playlistDragTrackDelta=tracks;playlistDragStats=std::move(stats);playlistDragCandidate=std::move(candidate);status=String((int)playlistDragIndices.size())+(playlistDragCopy?" clips: copy preview at bar ":" clips: move preview at bar ")+String(playlistDragStats.start/4+1,3)+". Release applies; Escape cancels.";repaint();return true;
}
bool Studio::playlistMouseUp(const MouseEvent&){
    if(dragAction==playlistLasso){dragAction=0;selectionBox={};playlistLassoBase.clear();status=String((int)playlistSelection().size())+" Playlist clips selected. Drag a selected block to move the group; Shift+drag copies.";repaint();return true;}
    if(dragAction!=playlistGroupDrag)return false;
    auto candidate=std::move(playlistDragCandidate);auto stats=playlistDragStats;int primary=playlistDragPrimary;bool copy=playlistDragCopy;bool current=revision==playlistDragRevision&&playlistSelection()==playlistDragIndices;cancelPlaylistGesture();
    if(!current){status="The project or selection changed. Start the Playlist drag again.";return true;}if(!candidate)return true;
    int resultPrimary=stats.resultIndices.front();for(size_t i=0;i<stats.sourceIndices.size();++i)if(stats.sourceIndices[i]==primary)resultPrimary=stats.resultIndices[i];commitPlaylistGroup(std::move(*candidate),stats.resultIndices,resultPrimary,String((int)stats.resultIndices.size())+(copy?" Playlist clips copied.":" Playlist clips moved."));return true;
}
bool Studio::commitPlaylistGroup(Project next,const std::vector<int>& indices,int primary,const String& message){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before editing the Playlist.";return false;}
    auto playback=std::make_unique<Playback>();auto song=next;song.songMode=true;String warnings;auto base=currentFile==File()?root:currentFile.getParentDirectory();auto valid=compilePlayback(song,assets,base,*playback,warnings);if(valid.wasOk()&&!next.songMode)valid=compilePlayback(next,assets,base,*playback,warnings);if(valid.failed()){status="Playlist edit cancelled: "+valid.getErrorMessage();return false;}
    releaseKeys();checkpoint();project=std::move(next);setPlaylistSelection(std::set<int>(indices.begin(),indices.end()),primary);selectedPoint=-1;changed();if(project.songMode)engine->seekRequested.store(jmin(engine->position.load(),project.songLength()));status=message+" One Undo restores the edit.";return true;
}
void Studio::deletePlaylistSelection(){auto selected=playlistSelection();if(selected.empty()){status="Select Playlist clips to delete.";return;}Project next;auto valid=deletePlaylistGroup(project,selected,next);if(valid.failed()){status=valid.getErrorMessage();return;}commitPlaylistGroup(std::move(next),{},-1,String((int)selected.size())+" Playlist clips deleted.");}
bool Studio::editPlaylistGroupClipboard(int action){
    auto selected=playlistSelection();bool copy=action==copySelection,cut=action==cutSelection,duplicate=action==duplicateSelection;
    if(copy||cut||duplicate){if(selected.size()<=1){if(!duplicate)playlistClipboard.clear();return false;}PlaylistGroupStats group;auto valid=inspectPlaylistGroup(project,selected,group);if(valid.failed()){status=valid.getErrorMessage();return true;}
        if(duplicate){Project next;PlaylistGroupStats stats;valid=transformPlaylistGroup(project,selected,group.end-group.start,0,true,true,next,&stats);if(valid.failed())status=valid.getErrorMessage();else commitPlaylistGroup(std::move(next),stats.resultIndices,stats.resultIndices.front(),String((int)stats.resultIndices.size())+" clips duplicated to the right.");return true;}
        ensurePlaylistEditorIds(project);playlistClipboard.clear();auto base=currentFile==File()?root:currentFile.getParentDirectory();for(int index:selected){auto clip=project.clips[(size_t)index];if(clip.isAudio()&&!File::isAbsolutePath(clip.audioPath))clip.audioPath=base.getChildFile(clip.audioPath).getFullPathName();playlistClipboard.push_back({clip,!clip.isAudio()&&!clip.isAutomation()?project.patterns[(size_t)clip.pattern].editorId:String(),project.tracks[(size_t)clip.track].editorId,clip.isAutomation()&&clip.automationTarget!=5?project.tracks[(size_t)clip.automationTrack].editorId:String()});}
        clipClipboard=playlistClipboard.front().clip;clipboardPatternId=playlistClipboard.front().patternId;clipboardHasClip=true;if(cut){deletePlaylistSelection();status=String((int)playlistClipboard.size())+" Playlist clips cut to the group clipboard.";}else status=String((int)playlistClipboard.size())+" Playlist clips copied. Seek/click the ruler at a destination, then Paste; lanes and relative timing are retained.";return true;
    }
    if(action!=pasteSelection||!clipboardHasClip||playlistClipboard.size()<=1)return false;
    std::vector<Clip> clips;double start=4096;auto trackIndex=[&](const String& id){for(int i=0;i<(int)project.tracks.size();++i)if(project.tracks[(size_t)i].editorId==id)return i;return -1;};
    for(const auto& entry:playlistClipboard){auto clip=entry.clip;clip.track=trackIndex(entry.trackId);if(clip.track<0){status="A copied channel is no longer present. Undo its deletion or copy again.";return true;}if(clip.isAutomation()&&clip.automationTarget!=5){clip.automationTrack=trackIndex(entry.targetId);if(clip.automationTrack<0){status="A copied automation target is no longer present. Undo its deletion or copy again.";return true;}}
        if(!clip.isAudio()&&!clip.isAutomation()){clip.pattern=patternIndexForEditorId(project,entry.patternId);if(clip.pattern<0){status="A copied pattern is no longer present. Undo its deletion or copy again.";return true;}}start=jmin(start,clip.beat);clips.push_back(std::move(clip));}
    Project next;PlaylistGroupStats stats;double destination=jmax(0.,snapped(editCursorBeat));auto valid=insertPlaylistGroup(project,clips,destination-start,0,true,next,&stats);if(valid.failed()){status=valid.getErrorMessage();return true;}if(commitPlaylistGroup(std::move(next),stats.resultIndices,stats.resultIndices.front(),String((int)clips.size())+" Playlist clips pasted.")){editCursorBeat=destination;if(destination<playlistStart||destination>=playlistStart+playlistSpan)playlistStart=jmax(0.,std::floor(destination/4)*4);showPane(playlistPane);}return true;
}
void Studio::showPlaylistGroupDialog(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before arranging clips.";return;}auto selection=playlistSelection();PlaylistGroupStats scope;auto valid=inspectPlaylistGroup(project,selection,scope);if(valid.failed()){status=valid.getErrorMessage();return;}
    setPlaylistSelection(selection,selectedClip);auto ids=selectedPlaylistIds;auto rev=revision;auto* dialog=new AlertWindow("Move or copy Playlist selection",String((int)selection.size())+" whole clips, from bar "+String(scope.start/4+1,3)+" to "+String(scope.end/4+1,3)+". Relative timing, existing lanes, audio offsets/fades and automation links stay intact. Markers stay at their current beats.",MessageBoxIconType::NoIcon,this);
    dialog->addComboBox("operation",{"Move selected clips","Copy selected clips"},"Operation");dialog->addTextEditor("bar",String(scope.start/4+1,5),"Destination first bar (1-based; fractions allowed)");dialog->addComboBox("overlap",{"Reject overlapping destination clips","Allow layering with destination clips"},"Destination");dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,ids,rev](int result){if(!safe||result!=1)return;auto text=dialog->getTextEditorContents("bar").trim();auto* begin=text.toRawUTF8();char* end=nullptr;double bar=std::strtod(begin,&end);if(end==begin||*end!=0||!std::isfinite(bar)){safe->status="Enter a numeric destination bar. Nothing changed.";return;}safe->applyPlaylistGroup(ids,rev,(bar-1)*4,dialog->getComboBoxComponent("operation")->getSelectedItemIndex()==1,dialog->getComboBoxComponent("overlap")->getSelectedItemIndex()==1);}),true);
}
bool Studio::applyPlaylistGroup(const std::set<String>& ids,uint64_t rev,double destination,bool copy,bool allowOverlap){
    if(rev!=revision||ids!=selectedPlaylistIds||ids.empty()){status="The project or clip selection changed. Reopen Move or copy selection.";return false;}auto selected=playlistSelection();PlaylistGroupStats scope;auto valid=inspectPlaylistGroup(project,selected,scope);if(valid.failed()){status=valid.getErrorMessage();return false;}double delta=destination-scope.start;if(!copy&&delta==0){status="Selection is already at that bar.";return true;}Project next;PlaylistGroupStats stats;valid=transformPlaylistGroup(project,selected,delta,0,copy,allowOverlap,next,&stats);if(valid.failed()){status=valid.getErrorMessage();return false;}if(!commitPlaylistGroup(std::move(next),stats.resultIndices,stats.resultIndices.front(),String((int)selected.size())+(copy?" Playlist clips copied":" Playlist clips moved")+" to bar "+String(destination/4+1,3)+"."))return false;editCursorBeat=destination;if(destination<playlistStart||destination>=playlistStart+playlistSpan)playlistStart=jmax(0.,std::floor(destination/4)*4);showPane(playlistPane);return true;
}
}
