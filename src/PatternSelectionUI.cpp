// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
std::set<int> Studio::patternNoteSelection()const{
    auto selection=selectedNotes;if(selection.empty()&&selectedNote>=0)selection.insert(selectedNote);return selection;
}
void Studio::showPatternExtraction(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before extracting notes.";return;}
    auto selection=patternNoteSelection();Project candidate;auto valid=extractPatternNotes(project,project.activePattern,selectedTrack,selection,false,false,candidate);if(valid.failed()){status=valid.getErrorMessage();return;}
    auto id=project.patterns[(size_t)project.activePattern].editorId;int track=selectedTrack;auto rev=revision;int usage=patternUsage(project,project.activePattern);
    auto* dialog=new AlertWindow("Pattern from selected notes",String((int)selection.size())+" notes from "+project.patterns[(size_t)project.activePattern].name+" / "+project.tracks[(size_t)track].name+". Move removes them from the shared source pattern (used by "+String(usage)+" Playlist clips). The new pattern keeps the source length/colour; place it in the Playlist explicitly. One undo restores the complete edit.",MessageBoxIconType::NoIcon,this);
    dialog->addComboBox("operation",{"Copy; retain source notes","Move; remove selected source notes"},"Source notes");dialog->addComboBox("timing",{"Keep original beat positions","Align first selected onset to beat 0"},"New pattern timing");dialog->addButton("Create pattern",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,id,track,selection,rev](int result){if(!safe||result!=1)return;safe->acceptPatternExtraction(id,track,selection,rev,dialog->getComboBoxComponent("operation")->getSelectedItemIndex()==1,dialog->getComboBoxComponent("timing")->getSelectedItemIndex()==1);safe->repaint();}),true);
}
bool Studio::acceptPatternExtraction(const String& id,int track,const std::set<int>& selection,uint64_t sourceRevision,bool move,bool alignStart){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before extracting notes.";return false;}
    int index=patternIndexForEditorId(project,id);if(sourceRevision!=revision||index!=project.activePattern||selectedTrack!=track||patternNoteSelection()!=selection){status="The source or selected notes changed. Reopen Pattern from selected notes.";return false;}
    Project next;PatternEditStats stats;auto valid=extractPatternNotes(project,index,track,selection,move,alignStart,next,&stats);if(valid.failed()){status=valid.getErrorMessage();return false;}return commitPatternToolProject(std::move(next),patternExtract,stats);
}
void Studio::showPatternMergeOptions(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before merging patterns.";return;}
    if(project.patterns.size()<2||project.patterns.size()>=maxPatterns){status="Merging needs another pattern and one free slot (32 maximum).";return;}
    auto first=project.patterns[(size_t)project.activePattern].editorId;auto rev=revision;StringArray names;std::vector<String> ids;
    for(int i=0;i<(int)project.patterns.size();++i)if(i!=project.activePattern){const auto& p=project.patterns[(size_t)i];names.add(String(i+1)+". "+p.name+" ("+String(p.length,2)+" beats)");ids.push_back(p.editorId);}
    StringArray policies;for(int i=0;i<4;++i)policies.add(patternMergePolicyName((PatternMergePolicy)i));
    auto* dialog=new AlertWindow("Merge patterns with conflict options","Layer one pass of both patterns into a new pattern. Exact duplicates match every note field. Prefer current/incoming resolves only identical onset and pitch on the same channel; other overlaps remain. Originals and Playlist clips stay intact. One undo removes the result.",MessageBoxIconType::NoIcon,this);
    dialog->addComboBox("incoming",names,"Incoming pattern");dialog->addComboBox("policy",policies,"Conflict handling");dialog->addButton("Create merged pattern",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,first,ids,rev](int result){if(!safe||result!=1)return;int index=dialog->getComboBoxComponent("incoming")->getSelectedItemIndex();if(index<0||index>=(int)ids.size())return;safe->acceptPatternMerge(first,ids[(size_t)index],rev,(PatternMergePolicy)dialog->getComboBoxComponent("policy")->getSelectedItemIndex());safe->repaint();}),true);
}
bool Studio::acceptPatternMerge(const String& first,const String& second,uint64_t sourceRevision,PatternMergePolicy policy){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before merging patterns.";return false;}
    int a=patternIndexForEditorId(project,first),b=patternIndexForEditorId(project,second);if(sourceRevision!=revision||a!=project.activePattern||a<0||b<0){status="The patterns changed. Reopen Merge patterns with conflict options.";return false;}
    Project next;PatternEditStats stats;auto valid=mergePatternLayers(project,a,b,next,&stats,policy);if(valid.failed()){status=valid.getErrorMessage();return false;}return commitPatternToolProject(std::move(next),patternMergeOptions,stats);
}
}
