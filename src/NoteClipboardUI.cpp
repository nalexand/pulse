// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
bool Studio::commitCopiedNotes(Project next,int track,int first,int count){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before pasting notes.";return false;}
    auto playback=std::make_unique<Playback>();String warnings;auto base=currentFile==File()?root:currentFile.getParentDirectory();auto result=compilePlayback(next,assets,base,*playback,warnings);
    if(result.wasOk()){auto other=next;other.songMode=!next.songMode;result=compilePlayback(other,assets,base,*playback,warnings);}if(result.failed()){status="Note paste cancelled: "+result.getErrorMessage();return false;}
    bool switched=project.patterns[(size_t)project.activePattern].editorId!=next.patterns[(size_t)next.activePattern].editorId;
    releaseKeys();checkpoint();project=std::move(next);selectedTrack=track;selectedNotes.clear();for(int i=0;i<count;++i)selectedNotes.insert(first+i);selectedNote=first;selectedClip=-1;pianoLow=-1;
    const auto& notes=lane();double start=64,end=0;int low=127,high=0;for(int i:selectedNotes){start=jmin(start,notes[(size_t)i].beat);end=jmax(end,notes[(size_t)i].beat+notes[(size_t)i].length);low=jmin(low,notes[(size_t)i].pitch);high=jmax(high,notes[(size_t)i].pitch);}
    int centre=high-low<pianoRows?(low+high)/2:notes[(size_t)first].pitch;pianoLow=jlimit(0,128-pianoRows,centre-pianoRows/2);
    pianoStart=jmax(0.,start-.25);pianoSpan=jlimit(1.,64.,jmax(8.,end-start+.5));pianoStart=jmin(pianoStart,jmax(0.,project.patterns[(size_t)project.activePattern].length-pianoSpan));pianoNoteCursor=start;changed();showPane(pianoPane);
    if(switched&&!project.songMode)engine->seekRequested.store(0);
    status=String(count)+" notes pasted into "+project.patterns[(size_t)project.activePattern].name+" / "+project.tracks[(size_t)track].name+" and selected. Existing destination notes retained. One Undo removes this paste.";return true;
}
bool Studio::pasteCopiedNotes(NotePasteMode mode){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before pasting notes.";return false;}
    if(activePane!=pianoPane&&activePane!=rackPane&&activePane!=playlistPane&&activePane!=channelPane&&activePane!=synthPane){status="Open the destination Piano roll (F7) before pasting notes.";return false;}
    if(clipboardHasClip){status="The clipboard contains a Playlist clip. Paste it in Playlist, or copy Piano-roll notes first.";return false;}
    if(project.tracks[(size_t)selectedTrack].isBus){status="Select the destination instrument channel, then open Piano roll (F7).";return false;}
    std::vector<Note> output;double length=0;auto valid=pasteLaneNotes(noteClipboard,lane(),project.patterns[(size_t)project.activePattern].length,mode,snapped(pianoNoteCursor),false,output,length);if(valid.failed()){status=valid.getErrorMessage();return false;}
    Project next=project;int first=(int)lane().size();next.patterns[(size_t)next.activePattern].lanes[(size_t)selectedTrack]=std::move(output);next.patterns[(size_t)next.activePattern].length=length;return commitCopiedNotes(std::move(next),selectedTrack,first,(int)noteClipboard.size());
}
void Studio::showCopyNotesToPattern(){
    if(recording||recorder.isActive()||pendingNoteTool()||project.tracks[(size_t)selectedTrack].isBus){status="Choose a source instrument channel and finish the take/note preview.";return;}
    if(lane().empty()){status="This channel has no notes. Open the channel containing the melody.";return;}
    auto selection=patternNoteSelection();auto id=project.patterns[(size_t)project.activePattern].editorId;int track=selectedTrack;auto rev=revision;
    auto* dialog=new AlertWindow("Copy notes to pattern",project.patterns[(size_t)project.activePattern].name+" / "+project.tracks[(size_t)track].name+": "+String(selection.empty()?(int)lane().size():(int)selection.size())+" notes. Empty selection copies the entire source channel. Append keeps destination notes; Replace clears only that destination channel. Source notes and Playlist links stay intact. Pattern length may extend to fit. One Undo restores the edit.",MessageBoxIconType::NoIcon,this);
    StringArray patterns,tracks;int suggested=project.activePattern==0&&project.patterns.size()>1?1:0;for(int i=0;i<(int)project.patterns.size();++i)patterns.add(String(i+1)+". "+project.patterns[(size_t)i].name);for(int i=0;i<(int)project.tracks.size();++i)tracks.add(String(i+1)+". "+project.tracks[(size_t)i].name+(project.tracks[(size_t)i].isBus?" (bus)":""));
    dialog->addComboBox("pattern",patterns,"Destination pattern");dialog->getComboBoxComponent("pattern")->setSelectedItemIndex(suggested);dialog->addComboBox("track",tracks,"Destination channel");dialog->getComboBoxComponent("track")->setSelectedItemIndex(track);dialog->addComboBox("write",{"Append; retain destination notes","Replace destination channel notes"},"Destination notes");dialog->addComboBox("timing",{"Keep original beat positions","Align first onset to beat 0"},"Timing");dialog->addButton("Copy notes",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,id,track,selection,rev](int choice){if(!safe||choice!=1)return;safe->acceptCopyNotesToPattern(id,track,selection,rev,dialog->getComboBoxComponent("pattern")->getSelectedItemIndex(),dialog->getComboBoxComponent("track")->getSelectedItemIndex(),dialog->getComboBoxComponent("write")->getSelectedItemIndex()==1,dialog->getComboBoxComponent("timing")->getSelectedItemIndex()==1);safe->repaint();}),true);
}
bool Studio::acceptCopyNotesToPattern(const String& id,int track,const std::set<int>& selection,uint64_t expected,int target,int targetTrack,bool replace,bool alignStart){
    if(expected!=revision||track!=selectedTrack||patternIndexForEditorId(project,id)!=project.activePattern||selection!=patternNoteSelection()){status="The source or selected notes changed. Reopen Copy notes to pattern.";return false;}
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before copying notes.";return false;}
    Project next;auto valid=copyNotesToPattern(project,project.activePattern,track,selection,target,targetTrack,replace,alignStart,next);if(valid.failed()){status=valid.getErrorMessage();return false;}
    int first=replace?0:(int)project.patterns[(size_t)target].lanes[(size_t)targetTrack].size();int count=(int)next.patterns[(size_t)target].lanes[(size_t)targetTrack].size()-first;
    bool committed=commitCopiedNotes(std::move(next),targetTrack,first,count);if(committed&&replace)status=String(count)+" notes copied; destination channel notes replaced. Source and Playlist links retained. One Undo restores previous destination notes.";return committed;
}
}
