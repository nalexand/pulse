// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::showModulationParameterMenu(int action,int track){
    if(action<flangerMix||action>phaserWidth||track< -1||track>=(int)project.tracks.size()||recording||recorder.isActive())return;
    auto kind=action>=phaserMix?ModulationKind::phaser:ModulationKind::flanger;int first=kind==ModulationKind::phaser?phaserMix:flangerMix;auto sourceRevision=revision;PopupMenu menu;menu.addItem(1,"Reset");menu.addItem(2,"Type in value...");auto safe=Component::SafePointer<Studio>(this);
    menu.showMenuAsync(menuOptions(),[safe,action,track,sourceRevision,kind,first](int result){
        if(!safe||result==0)return;
        if(safe->recording||safe->recorder.isActive()||safe->revision!=sourceRevision||safe->effectTarget()!=track){safe->status="Channel changed. Reopen the modulation parameter.";return;}
        if(result==2){safe->editModulationValue(kind,action-first);return;}
        if(result==1){int i=action-first;safe->checkpoint();safe->setParameter(action,track,(float)modulationNormal(kind,i,modulationSpecs(kind)[(size_t)i].initial));}
    });
}
void Studio::editModulationValue(ModulationKind kind,int index){
    if(index<0||index>=6||recording||recorder.isActive())return;
    auto spec=modulationSpecs(kind)[(size_t)index];bool percent=index==0||index==2||index==4||index==5;double scale=percent?100:1;String unit=percent?"%":String(spec.unit);
    auto* dialog=new AlertWindow(String(modulationName(kind))+" / "+String(spec.name),"Allowed: "+String(spec.low*scale,2)+" to "+String(spec.high*scale,2)+unit,MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("value",String(modulationValue(kind==ModulationKind::flanger?effectMixer(effectTarget()).flanger:effectMixer(effectTarget()).phaser,index)*scale,3));dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=revision;int track=effectTarget();
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,index,sourceRevision,track,scale,kind](int result){
        if(!safe||result!=1)return;
        if(safe->recording||safe->recorder.isActive()||safe->revision!=sourceRevision||safe->effectTarget()!=track){safe->status="Channel changed. Reopen the modulation value.";return;}
        auto text=dialog->getTextEditorContents("value").trim();const char* begin=text.toRawUTF8();char* end=nullptr;double value=std::strtod(begin,&end)/scale;
        if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(value)){safe->status="Enter a finite numeric modulation value.";return;}
        auto& destination=kind==ModulationKind::flanger?safe->effectMixer(track).flanger:safe->effectMixer(track).phaser;auto next=destination;auto valid=setModulationValue(next,kind,index,value);
        if(valid.failed()){safe->status=valid.getErrorMessage();return;}
        if(next==destination)return;
        safe->checkpoint();destination=next;safe->changed();safe->status=String(modulationName(kind))+" value updated.";
    }),true);
}
}
