// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::showChorusParameterMenu(int action,int track){
    if(action<chorusMix||action>chorusWidth||track<0||track>=(int)project.tracks.size()||recording||recorder.isActive())return;
    auto sourceRevision=revision;PopupMenu menu;menu.addItem(1,"Reset");menu.addItem(2,"Type in value...");auto safe=Component::SafePointer<Studio>(this);
    menu.showMenuAsync(menuOptions(),[safe,action,track,sourceRevision](int result){
        if(!safe||result==0)return;
        if(safe->recording||safe->recorder.isActive()||safe->revision!=sourceRevision||safe->selectedTrack!=track){safe->status="Channel changed. Reopen the chorus parameter.";return;}
        if(result==2){safe->editChorusValue(action-chorusMix);return;}
        if(result==1){int i=action-chorusMix;safe->checkpoint();safe->setParameter(action,track,(float)chorusNormal(i,chorusParameterSpecs()[(size_t)i].initial));}
    });
}
void Studio::editChorusValue(int index){
    if(index<0||index>=6||recording||recorder.isActive())return;
    auto spec=chorusParameterSpecs()[(size_t)index];bool percent=index==0||index==2||index==4||index==5;double scale=percent?100:1;String unit=percent?"%":String(spec.unit);
    auto* dialog=new AlertWindow("Chorus / "+String(spec.name),"Allowed: "+String(spec.low*scale,2)+" to "+String(spec.high*scale,2)+unit,MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("value",String(chorusValue(project.tracks[(size_t)selectedTrack].mixer.chorus,index)*scale,3));dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=revision;int track=selectedTrack;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,index,sourceRevision,track,scale](int result){
        if(!safe||result!=1)return;
        if(safe->recording||safe->recorder.isActive()||safe->revision!=sourceRevision||safe->selectedTrack!=track){safe->status="Channel changed. Reopen the chorus value.";return;}
        auto text=dialog->getTextEditorContents("value").trim();const char* begin=text.toRawUTF8();char* end=nullptr;double value=std::strtod(begin,&end)/scale;
        if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(value)){safe->status="Enter a finite numeric chorus value.";return;}
        auto next=safe->project.tracks[(size_t)track].mixer.chorus;auto valid=setChorusValue(next,index,value);
        if(valid.failed()){safe->status=valid.getErrorMessage();return;}
        if(next==safe->project.tracks[(size_t)track].mixer.chorus)return;
        safe->checkpoint();safe->project.tracks[(size_t)track].mixer.chorus=next;safe->changed();safe->status="Chorus value updated.";
    }),true);
}
}
