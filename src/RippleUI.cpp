// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <cstdlib>
namespace pulse {
using namespace juce;
namespace {
bool readNumber(const String& text,double& value){auto utf=text.trim();auto* first=utf.toRawUTF8();char* last=nullptr;value=std::strtod(first,&last);return last!=first&&*last==0&&std::isfinite(value);}
}
void Studio::showPlaylistRipple(){
    if(recording||recorder.isActive()||pendingNoteTool()||automationWriteContext||exporting.load()){status="Finish the take, automation pass, note preview or export before ripple editing.";return;}
    double start=editCursorBeat,length=4;if(auto* section=findSongMarker(project,selectedMarker);section&&section->isSection()){start=section->beat;length=section->endBeat-section->beat;}
    auto* dialog=new AlertWindow("Playlist ripple time","Apply across every Playlist lane. Insert moves later clips right. Delete removes complete clips in the range and closes the gap. Clips crossing a boundary must be split first. Markers, loop and punch ranges follow the edit. Review the affected counts before applying.",MessageBoxIconType::NoIcon,this);
    dialog->addComboBox("operation",{"Insert empty time","Delete time and close gap"},"Operation");dialog->addTextEditor("bar",String(start/4+1,6),"Start bar (1-based; fractions allowed)");dialog->addTextEditor("bars",String(length/4,6),"Length in bars (4 beats per bar)");
    dialog->addButton("Review...",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto expected=revision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,expected](int result){if(!safe||result!=1)return;double bar=0,bars=0;if(!readNumber(dialog->getTextEditorContents("bar"),bar)||!readNumber(dialog->getTextEditorContents("bars"),bars)){safe->status="Enter numeric start and length bars. Nothing was changed.";return;}bool remove=dialog->getComboBoxComponent("operation")->getSelectedItemIndex()==1;MessageManager::callAsync([safe,expected,start=(bar-1)*4,length=bars*4,remove]{if(safe)safe->reviewPlaylistRipple(expected,start,length,remove);});}),true);
}
void Studio::reviewPlaylistRipple(uint64_t expected,double start,double length,bool remove){
    if(expected!=revision||recording||recorder.isActive()||pendingNoteTool()||automationWriteContext||exporting.load()){status="The project changed or is busy. Reopen Playlist ripple time.";return;}
    Project next;RippleStats stats;auto valid=ripplePlaylist(project,start,length,remove,next,&stats);if(valid.failed()){status=valid.getErrorMessage();return;}if(!stats.changed){status="No Playlist objects or ranges would change.";return;}
    String details=String(remove?"Delete":"Insert")+" "+String(length/4,3)+" bars at bar "+String(start/4+1,3)+" across ALL lanes.\n\n"+String(stats.movedClips)+" clips move; "+String(stats.removedClips)+" clips are removed.\n"+String(stats.movedMarkers)+" markers/sections move or resize; "+String(stats.removedMarkers)+" are removed.\nLoop and punch boundaries follow the edit.";
    if(project.loopSection.isNotEmpty()&&next.loopSection.isEmpty())details+="\nThe selected loop section is removed; looping switches off.";
    if(stats.punchReset)details+="\nThe punch range collapses; punch switches off and resets to bars 1-3.";
    details+="\n\nSource audio files and pattern notes stay intact. One Undo restores this entire edit.";
    auto* dialog=new AlertWindow("Review Playlist ripple",details,MessageBoxIconType::NoIcon,this);dialog->addButton(remove?"Delete time":"Insert time",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,expected,start,length,remove](int result){if(safe&&result==1)safe->acceptPlaylistRipple(expected,start,length,remove);}),true);
}
bool Studio::acceptPlaylistRipple(uint64_t expected,double start,double length,bool remove){
    if(expected!=revision||recording||recorder.isActive()||pendingNoteTool()||automationWriteContext||exporting.load()){status="The project changed or is busy. Reopen Playlist ripple time.";return false;}
    Project next;RippleStats stats;auto valid=ripplePlaylist(project,start,length,remove,next,&stats);if(valid.failed()){status=valid.getErrorMessage();return false;}if(!stats.changed){status="No Playlist objects or ranges would change.";return true;}
    auto playback=std::make_unique<Playback>();auto song=next;song.songMode=true;String warnings;auto base=currentFile==File()?root:currentFile.getParentDirectory();valid=compilePlayback(song,assets,base,*playback,warnings);if(valid.wasOk()&&!next.songMode)valid=compilePlayback(next,assets,base,*playback,warnings);if(valid.failed()||warnings.isNotEmpty()){status="Ripple cancelled: "+(valid.failed()?valid.getErrorMessage():warnings);return false;}
    auto ids=selectedPlaylistIds;String primary=selectedClip>=0&&selectedClip<(int)project.clips.size()?project.clips[(size_t)selectedClip].editorId:String();for(int i:playlistSelection())ids.insert(project.clips[(size_t)i].editorId);
    double at=engine->seekRequested.load();if(at<0)at=engine->position.load();if(project.songMode)releaseKeys();cancelPlaylistGesture();checkpoint();project=std::move(next);
    std::set<int> selection;int primaryIndex=-1;for(int i=0;i<(int)project.clips.size();++i){auto id=project.clips[(size_t)i].editorId;if(ids.count(id))selection.insert(i);if(id==primary)primaryIndex=i;}setPlaylistSelection(selection,primaryIndex);selectedPoint=-1;
    if(!findSongMarker(project,selectedMarker))selectedMarker.clear();editCursorBeat=start;playlistStart=jlimit(0.,jmax(0.,4096-playlistSpan),std::floor(start/4)*4);
    changed();if(project.songMode)engine->seekRequested.store(jlimit(0.,project.songLength(),rippleBeat(at,start,length,remove)));showPane(playlistPane);
    status=String(remove?"Deleted":"Inserted")+" "+String(length/4,3)+" bars across all lanes; "+String(stats.movedClips)+" clips moved, "+String(stats.removedClips)+" removed. One Undo restores the edit.";return true;
}
}
