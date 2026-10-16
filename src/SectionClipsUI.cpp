// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <cstdlib>
namespace pulse {
using namespace juce;
void Studio::showSectionClips(){
    if(recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Finish the take/export and resolve the note preview before arranging a section.";return;}
    SectionClipStats scope;auto valid=inspectSectionClips(project,selectedMarker,scope);if(valid.failed()){status=valid.getErrorMessage();return;}auto* section=findSongMarker(project,selectedMarker);
    auto id=selectedMarker;auto rev=revision;
    auto* dialog=new AlertWindow("Copy or move section clips",section->name+": "+String(scope.patternClips)+" pattern, "+String(scope.audioClips)+" audio and "+String(scope.automationClips)+" automation clips, "+String(scope.end-scope.start,3)+" beats. Every clip must fit wholly inside the source section. Markers, source files, patterns and loop boundaries stay in place. Move relocates the clips; one undo restores the whole edit.",MessageBoxIconType::NoIcon,this);
    dialog->addComboBox("operation",{"Copy complete clips","Move complete clips"},"Operation");dialog->addTextEditor("bar",String(scope.end/4+1,5),"Destination start bar (1-based; fractions allowed)");dialog->addComboBox("overlap",{"Reject lane and linked-automation overlaps","Allow layering; retain all destination clips"},"Destination clips");
    dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,id,rev](int result){if(!safe||result!=1)return;auto text=dialog->getTextEditorContents("bar").trim();auto* begin=text.toRawUTF8();char* end=nullptr;double bar=std::strtod(begin,&end);if(end==begin||*end!=0||!std::isfinite(bar)){safe->status="Enter a numeric destination bar. Nothing was changed.";return;}safe->acceptSectionClips(id,rev,(bar-1)*4,dialog->getComboBoxComponent("operation")->getSelectedItemIndex()==1,dialog->getComboBoxComponent("overlap")->getSelectedItemIndex()==1);safe->repaint();}),true);
}
bool Studio::acceptSectionClips(const String& id,uint64_t sourceRevision,double destination,bool move,bool allowOverlap){
    if(recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Finish the take/export and resolve the note preview before arranging a section.";return false;}
    if(sourceRevision!=revision||id!=selectedMarker){status="The project or selected section changed. Reopen Copy or move section clips.";return false;}
    Project next;SectionClipStats stats;auto valid=transformSectionClips(project,id,destination,move,allowOverlap,next,&stats);if(valid.failed()){status=valid.getErrorMessage();return false;}
    auto playback=std::make_unique<Playback>();auto song=next;song.songMode=true;String warnings;auto base=currentFile==File()?root:currentFile.getParentDirectory();valid=compilePlayback(song,assets,base,*playback,warnings);if(valid.wasOk()&&!next.songMode)valid=compilePlayback(next,assets,base,*playback,warnings);if(valid.failed()){status="Section edit cancelled: "+valid.getErrorMessage();return false;}
    cancelNoteTool(false);if(project.songMode)releaseKeys();checkpoint();project=std::move(next);selectedClip=stats.resultIndices.front();selectedPoint=-1;editCursorBeat=destination;
    if(destination<playlistStart||destination+(stats.end-stats.start)>playlistStart+playlistSpan)playlistStart=jlimit(0.,jmax(0.,4096-playlistSpan),std::floor(destination/4)*4);
    changed();if(project.songMode)engine->seekRequested.store(jmin(engine->position.load(),project.songLength()));showPane(playlistPane);
    status=String((int)stats.sourceIndices.size())+(move?" section clips moved":" section clips copied")+" to bar "+String(destination/4+1,3)+". Markers and loop boundaries stay in place. One undo restores the edit.";return true;
}
}
