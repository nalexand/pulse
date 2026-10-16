// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
Colour Studio::patternColor(int index)const{
    if(index>=0&&index<(int)project.patterns.size())if(auto argb=patternColourARGB(project.patterns[(size_t)index].colour))return Colour(argb);
    return Colour(0xffb2c98d);
}
bool Studio::applyPatternColour(const String& id,uint64_t sourceRevision,int colour){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before changing pattern colour.";return false;}
    int index=patternIndexForEditorId(project,id);
    if(sourceRevision!=revision||index<0){status="The project changed. Reopen pattern colours.";return false;}
    if(colour<0||colour>=patternColourCount){status="Choose a colour from the pattern palette.";return false;}
    if(project.patterns[(size_t)index].colour==colour)return true;
    checkpoint();project.patterns[(size_t)index].colour=colour;changed(false);status="Pattern colour updated for every instance. One undo restores it.";return true;
}
void Studio::showPatternColours(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before changing pattern colour.";return;}
    PopupMenu menu;auto id=project.patterns[(size_t)project.activePattern].editorId;auto rev=revision;
    for(int c=0;c<patternColourCount;++c)menu.addColouredItem(c+1,patternColourName(c),c?Colour(patternColourARGB(c)):Colour(0xffd9ddde),true,c==project.patterns[(size_t)project.activePattern].colour);
    auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(patternMenu),[safe,id,rev](int result){if(safe&&result>0){safe->applyPatternColour(id,rev,result-1);safe->repaint();}});
}
void Studio::restoreMetronomeSettings(){
    auto next=metronomeSettings;auto result=loadMetronome(root.getChildFile("data/metronome.json"),next);
    if(result.failed()){status=result.getErrorMessage();return;}metronomeSettings=next;++metronomeRevision;engine->setMetronomeSettings(next);
}
bool Studio::applyMetronomeSettings(const MetronomeSettings& next,uint64_t sourceRevision){
    if(recording||recorder.isActive()){status="Finish the take before changing the metronome sound.";return false;}
    if(sourceRevision!=metronomeRevision){status="Metronome preferences changed. Reopen its settings.";return false;}
    auto valid=validateMetronome(next);if(valid.failed()){status=valid.getErrorMessage();return false;}if(next==metronomeSettings)return true;
    auto saved=saveMetronome(root.getChildFile("data/metronome.json"),next);if(saved.failed()){status=saved.getErrorMessage();return false;}
    metronomeSettings=next;++metronomeRevision;engine->setMetronomeSettings(next);status="Local metronome: "+metronomeSoundName(next.sound)+", "+String(next.level)+"%. The same sound is used for count-in.";return true;
}
void Studio::showMetronomeSettings(){
    if(recording||recorder.isActive()){status="Finish the take before changing the metronome sound.";return;}
    auto* dialog=new AlertWindow("Metronome sound and level","Local listening preferences also apply to count-in. 100% retains the original level; 0% silences clicks. Song notes and exports are unchanged.",MessageBoxIconType::NoIcon,this);
    StringArray sounds;for(int i=0;i<metronomeSoundCount;++i)sounds.add(metronomeSoundName(i));dialog->addComboBox("sound",sounds,"Click sound");dialog->getComboBoxComponent("sound")->setSelectedItemIndex(metronomeSettings.sound,dontSendNotification);
    dialog->addTextEditor("level",String(metronomeSettings.level),"Level (0-200%)");dialog->addComboBox("accent",{"Every beat equal","Accent first beat of bar"},"Accent");dialog->getComboBoxComponent("accent")->setSelectedItemIndex(metronomeSettings.accent?1:0,dontSendNotification);
    dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=metronomeRevision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,sourceRevision](int result){if(!safe||result!=1)return;auto text=dialog->getTextEditorContents("level").trim();if(text.isEmpty()||text.length()>3||!text.containsOnly("0123456789")){safe->status="Enter a whole-number metronome level from 0 to 200%.";return;}MetronomeSettings next{dialog->getComboBoxComponent("sound")->getSelectedItemIndex(),text.getIntValue(),dialog->getComboBoxComponent("accent")->getSelectedItemIndex()==1};safe->applyMetronomeSettings(next,sourceRevision);safe->repaint();}),true);
}
bool Studio::performAppearanceAction(int action,int data){
    if(action==patternColourMenu){showPatternColours();return true;}
    if(action==patternColourSet){applyPatternColour(project.patterns[(size_t)project.activePattern].editorId,revision,data);return true;}
    if(action==metronomeSettingsOpen){showMetronomeSettings();return true;}return false;
}
}
