// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
bool Studio::selectedPatternClip()const{return selectedClip>=0&&selectedClip<(int)project.clips.size()&&!project.clips[(size_t)selectedClip].isAudio()&&!project.clips[(size_t)selectedClip].isAutomation();}
bool Studio::performPatternToolAction(int action,int data){
    if((action<patternMakeUnique||action>patternDeleteUnused)&&action!=patternMergeSequence&&action!=patternDeleteUsed&&action!=patternExtract&&action!=patternMergeOptions)return false;
    if(recording||recorder.isActive()){status="Finish the take before editing patterns.";return true;}
    if(action==patternClipMenu){showPatternClipMenu(data);return true;}
    if(pendingNoteTool()){status="Apply or cancel the note preview before reorganizing patterns.";showPane(noteToolsPane);return true;}
    if(action==patternExtract){showPatternExtraction();return true;}
    if(action==patternMergeOptions){showPatternMergeOptions();return true;}
    if(action==patternDeleteUnused||action==patternDeleteUsed){confirmPatternDelete(action==patternDeleteUsed);return true;}
    Project next;PatternEditStats stats;Result result=Result::ok();
    if(action==patternMoveEarlier||action==patternMoveLater)result=movePattern(project,project.activePattern,project.activePattern+(action==patternMoveEarlier?-1:1),next,&stats);
    else result=action==patternMakeUnique?makeClipPatternUnique(project,selectedClip,next,&stats):action==patternSplitChannels?splitPatternChannels(project,project.activePattern,next,&stats):action==patternMergeSequence?mergePatternSequence(project,project.activePattern,data,next,&stats):mergePatternLayers(project,project.activePattern,data,next,&stats);
    if(result.failed()){status=result.getErrorMessage();return true;}commitPatternToolProject(std::move(next),action,stats);return true;
}
void Studio::confirmPatternDelete(bool removeClips){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before deleting a pattern.";return;}
    Project next;auto result=removeClips?deletePatternWithClips(project,project.activePattern,next):deleteUnusedPattern(project,project.activePattern,next);if(result.failed()){status=result.getErrorMessage();return;}
    auto& pattern=project.patterns[(size_t)project.activePattern];int notes=0;for(const auto& lane:pattern.lanes)notes+=(int)lane.size();auto id=pattern.editorId;auto sourceRevision=revision;
    int usage=patternUsage(project,project.activePattern);String message="Delete \""+pattern.name+"\" ("+String(notes)+(notes==1?" note":" notes")+")? ";
    message+=removeClips?"This also removes all "+String(usage)+" of its pattern clips from the Playlist. Audio and automation clips remain intact. One undo restores the pattern and clips.":"It has no Playlist instances. One undo restores the pattern. Other pattern links and audio stay intact.";
    auto* dialog=new AlertWindow(removeClips?"Delete pattern and clips":"Delete unused pattern",message,MessageBoxIconType::QuestionIcon,this);dialog->addButton(removeClips?"Delete pattern and clips":"Delete",1);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,id,sourceRevision,removeClips](int choice){if(!safe||choice!=1)return;safe->acceptPatternDelete(id,sourceRevision,removeClips);safe->repaint();}),true);
}
bool Studio::acceptPatternDelete(const String& id,uint64_t sourceRevision,bool removeClips){
    if(revision!=sourceRevision){status="The project changed. Reopen pattern deletion.";return false;}int index=patternIndexForEditorId(project,id);if(index<0){status="This pattern is no longer in the project.";return false;}return deletePatternNow(index,removeClips);
}
bool Studio::deletePatternNow(int index,bool removeClips){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before deleting a pattern.";return false;}
    Project next;PatternEditStats stats;auto result=removeClips?deletePatternWithClips(project,index,next,&stats):deleteUnusedPattern(project,index,next,&stats);if(result.failed()){status=result.getErrorMessage();return false;}return commitPatternToolProject(std::move(next),removeClips?patternDeleteUsed:patternDeleteUnused,stats);
}
bool Studio::commitPatternToolProject(Project next,int action,const PatternEditStats& stats){
    // Validate current transport and the new active pattern before a checkpoint.
    auto playback=std::make_unique<Playback>();String warnings;auto base=currentFile==File()?root:currentFile.getParentDirectory();auto result=compilePlayback(next,assets,base,*playback,warnings);
    if(result.wasOk()&&next.songMode){auto patternPreview=next;patternPreview.songMode=false;result=compilePlayback(patternPreview,assets,base,*playback,warnings);}
    if(result.failed()){status="Pattern edit cancelled: "+result.getErrorMessage();return false;}
    bool moving=action==patternMoveEarlier||action==patternMoveLater;
    bool switched=project.patterns[(size_t)project.activePattern].editorId!=next.patterns[(size_t)next.activePattern].editorId;
    cancelNoteTool(false);releaseKeys();checkpoint();project=std::move(next);
    if(action==patternDeleteUsed&&selectedClip>=0){if(std::find(stats.removedClips.begin(),stats.removedClips.end(),selectedClip)!=stats.removedClips.end()){selectedClip=-1;selectedPoint=-1;}else selectedClip-=(int)std::count_if(stats.removedClips.begin(),stats.removedClips.end(),[&](int index){return index<selectedClip;});}
    if(!moving){selectedNote=-1;selectedNotes.clear();pianoLow=-1;pianoStart=0;pianoSpan=jmin(8.,project.patterns[(size_t)project.activePattern].length);}
    if(action==patternMakeUnique)selectedTrack=project.clips[(size_t)selectedClip].track;
    else if(action==patternSplitChannels||action==patternMergeLayers||action==patternMergeSequence||action==patternMergeOptions){for(int i=0;i<(int)project.tracks.size();++i)if(!project.patterns[(size_t)project.activePattern].lanes[(size_t)i].empty()){selectedTrack=i;break;}showPane(rackPane);}
    if(action==patternExtract){selectedTrack=stats.sourceTrack;for(int i=0;i<stats.extractedNotes;++i)selectedNotes.insert(i);selectedNote=stats.extractedNotes?0:-1;showPane(pianoPane);}
    trackScroll=jlimit(0,jmax(0,(int)project.tracks.size()-8),selectedTrack);changed();
    if(switched&&!project.songMode)engine->seekRequested.store(0);
    else if(action==patternDeleteUsed||(action==patternExtract&&(stats.movedNotes>0||!project.songMode))||(action==patternMergeOptions&&!project.songMode))engine->seekRequested.store(jmin(engine->position.load(),project.songMode?project.songLength():project.patterns[(size_t)project.activePattern].length));
    if(action==patternExtract)status=String(stats.extractedNotes)+(stats.movedNotes?" notes moved":" notes copied")+" into a new pattern. "+(stats.shiftBeats>0?"First onset aligned to beat 0. ":"Original beat positions retained. ")+"Playlist clips are not relinked; place the new pattern explicitly. One undo restores all.";
    else if(action==patternMergeOptions)status="Created merged pattern; "+String(stats.skippedNotes)+" notes omitted by the selected conflict policy. Originals and Playlist clips are unchanged. One undo removes it.";
    else if(moving)status="Pattern moved. Active notes, Playlist links and copied pattern identity are preserved. One undo restores the order.";
    else if(action==patternDeleteUnused)status="Unused pattern deleted. Other pattern links are preserved; one undo restores it.";
    else if(action==patternDeleteUsed)status="Pattern and "+String((int)stats.removedClips.size())+" Playlist clips deleted. Audio, automation and other pattern links are preserved. One undo restores all.";
    else if(action==patternMergeSequence)status="Created a new pattern with the current phrase followed by the chosen phrase. Original patterns and Playlist clips are unchanged. One undo removes it.";
    else status=action==patternMakeUnique?"This clip has an independent pattern. Other clips keep the original. One undo restores the link.":action==patternSplitChannels?"Created "+String((int)stats.created.size())+" channel patterns; relinked "+String(stats.relinked)+" clips. Original retained. One undo restores all links.":"Merged one pass of both patterns as layers. Originals and Playlist clips are unchanged. One undo removes the new pattern.";
    return true;
}
void Studio::showPatternClipMenu(int clip){
    if(clip<0||clip>=(int)project.clips.size())return;releaseKeys();auto selected=playlistSelection();if(!selected.count(clip))selected={clip};setPlaylistSelection(selected,clip);activePane=playlistPane;auto& c=project.clips[(size_t)clip];selectedTrack=c.track;bool pattern=!c.isAudio()&&!c.isAutomation();
    PopupMenu menu;menu.addItem(playlistLoopSelection,"Loop selected clips (all Song tracks)");menu.addItem(playlistLoopClear,"Clear Playlist loop range");menu.addItem(playlistGroupOpen,"Move / copy selected clips...");menu.addItem(playlistAll,"Select all Playlist clips");menu.addItem(playlistNone,"Deselect Playlist clips");menu.addSeparator();menu.addItem(pianoOpen,"Open pattern in Piano roll",pattern);menu.addItem(patternMakeUnique,"Make unique",pattern&&project.patterns.size()<maxPatterns&&!pendingNoteTool());menu.addSeparator();menu.addItem(copySelection,shortcutLabel("Copy selection","edit.copy"));menu.addItem(duplicateSelection,shortcutLabel("Duplicate to the right","edit.duplicate"));if(c.isAudio()){menu.addSeparator();menu.addItem(audioCrossfadeOpen,"Crossfade selected audio clips...");menu.addItem(audioFadesClear,"Clear selected audio fades");menu.addItem(audioClipSettings,"Audio clip settings...");menu.addItem(splitClip,shortcutLabel("Split audio at cursor","edit.split"));}
    auto safe=Component::SafePointer<Studio>(this);auto menuRevision=revision;
    menu.showMenuAsync(menuOptions(),[safe,clip,menuRevision](int action){if(!safe||!action)return;if(safe->recording||safe->revision!=menuRevision||clip<0||clip>=(int)safe->project.clips.size()){safe->status="The Playlist changed. Reopen the clip menu.";return;}safe->selectedClip=clip;safe->activePane=playlistPane;if(action==pianoOpen){auto c=safe->project.clips[(size_t)clip];if(c.isAudio()||c.isAutomation())return;safe->perform(patternSelect,c.pattern);safe->perform(pianoOpen,c.track);}else safe->perform(action);});
}
}
