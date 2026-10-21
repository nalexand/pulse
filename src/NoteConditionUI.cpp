// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
std::set<int> Studio::conditionSelection()const{
    auto result=selectedNotes;if(result.empty()&&selectedNote>=0)result.insert(selectedNote);return result;
}
bool Studio::noteConditionContext(uint64_t expected,int pattern,int track,const std::set<int>& selection)const{
    if(recording||recorder.isActive()||pendingNoteTool()||expected!=revision||pattern!=project.activePattern||track!=selectedTrack||pattern<0||pattern>=(int)project.patterns.size()||track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus||selection.empty()||selection!=conditionSelection())return false;
    const auto& notes=project.patterns[(size_t)pattern].lanes[(size_t)track];for(int index:selection)if(index<0||index>=(int)notes.size())return false;return true;
}
bool Studio::applyNoteCondition(int every,int step,uint64_t expected,int pattern,int track,const std::set<int>& selection){
    if(!noteConditionContext(expected,pattern,track,selection)){status="The selected notes changed, or a take/preview is active. Reopen Repeat condition.";return false;}
    Note candidate;candidate.conditionEvery=every;candidate.conditionStep=step;if(!validNoteCondition(candidate)){status="Choose pass 1-N in a cycle of 1-16 repeats.";return false;}
    auto& notes=project.patterns[(size_t)pattern].lanes[(size_t)track];bool different=false;for(int i:selection)different|=notes[(size_t)i].conditionEvery!=every||notes[(size_t)i].conditionStep!=step;
    if(!different){status="Selected notes already use this repeat condition.";return true;}
    checkpoint();for(int i:selection){notes[(size_t)i].conditionEvery=every;notes[(size_t)i].conditionStep=step;}changed();status=String((int)selection.size())+" notes: "+(every==1?String("every repeat"):"pass "+String(step)+" of "+String(every))+". Song uses timeline-aligned pattern repeats; PAT counts loops.";return true;
}
void Studio::editNoteCondition(uint64_t expected,int pattern,int track,const std::set<int>& selection){
    if(!noteConditionContext(expected,pattern,track,selection)){status="Select notes and reopen Repeat condition.";return;}
    const auto& note=project.patterns[(size_t)pattern].lanes[(size_t)track][(size_t)*selection.begin()];
    auto* dialog=new AlertWindow("Note repeat condition","Apply to "+String((int)selection.size())+" selected notes. PAT counts loops from Play/seek. Song counts source-pattern lengths from song beat 0; separate clips share that grid. Song/section loops and export repeat the same timeline choices. Chance also applies; all ratchets share the condition.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("every",String(note.conditionEvery),"Cycle length (1-16 repeats)");dialog->addTextEditor("step",String(note.conditionStep),"Play on pass (1-cycle length)");dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    auto safe=Component::SafePointer<Studio>(this);dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,expected,pattern,track,selection](int result){if(!safe||result!=1)return;auto every=dialog->getTextEditorContents("every").trim(),step=dialog->getTextEditorContents("step").trim();if(every.isEmpty()||step.isEmpty()||every.length()>2||step.length()>2||!every.containsOnly("0123456789")||!step.containsOnly("0123456789")){safe->status="Enter whole-number repeat counts from 1 to 16.";return;}safe->applyNoteCondition(every.getIntValue(),step.getIntValue(),expected,pattern,track,selection);}),true);
}
void Studio::showNoteConditions(){
    auto selection=conditionSelection();auto expected=revision;int pattern=project.activePattern,track=selectedTrack;if(!noteConditionContext(expected,pattern,track,selection)){status="Select notes in Piano roll, or Alt-right-click an enabled rack step. Finish any take or note preview first.";return;}
    PopupMenu menu;menu.addSectionHeader("Selected notes: repeat condition");
    const int every[]={1,2,2,4,4,8,8},step[]={1,1,2,1,4,1,8};
    for(int i=0;i<7;++i){bool matches=true;for(int index:selection){const auto& note=project.patterns[(size_t)pattern].lanes[(size_t)track][(size_t)index];matches&=note.conditionEvery==every[i]&&note.conditionStep==step[i];}menu.addItem(i+1,every[i]==1?String("Always"):"Pass "+String(step[i])+" of "+String(every[i]),true,matches);}
    menu.addSeparator();menu.addItem(8,"Custom cycle (1-16)...");auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe,expected,pattern,track,selection](int result){if(!safe||result<1||result>8)return;if(result==8){safe->editNoteCondition(expected,pattern,track,selection);return;}const int every[]={1,2,2,4,4,8,8},step[]={1,1,2,1,4,1,8};safe->applyNoteCondition(every[result-1],step[result-1],expected,pattern,track,selection);});
}
}
