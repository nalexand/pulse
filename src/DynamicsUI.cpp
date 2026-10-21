// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <cstdlib>
namespace pulse {
using namespace juce;
bool Studio::applyDynamicsMode(DynamicsMode mode,int target,uint64_t expected){
    if(recording||recorder.isActive()||pendingNoteTool()||expected!=revision||effectTarget()!=target||target< -1||target>=(int)project.tracks.size()||(int)mode<0||(int)mode>3){status="Dynamics target changed or a take/preview is active. Reopen Mode.";return false;}
    auto& m=effectMixer(target);if(m.dynamics.mode==mode)return true;checkpoint();m.dynamics.mode=mode;changed();status=dynamicsName(mode)+" selected. Enable the insert to process audio.";return true;
}
void Studio::showDynamicsModes(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take or note preview before changing dynamics mode.";return;}
    if(editingMasterEffects()){masterEffectsView=false;masterInsertView=true;}analyzerView=false;updateAnalyzer();selectedEffect=1;showPane(effectsPane);int target=effectTarget();auto expected=revision;PopupMenu menu;for(int i=0;i<4;++i)menu.addItem(i+1,dynamicsName((DynamicsMode)i),true,effectMixer(target).dynamics.mode==(DynamicsMode)i);auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe,target,expected](int result){if(safe&&result>=1&&result<=4)safe->applyDynamicsMode((DynamicsMode)(result-1),target,expected);});
}
void Studio::editDynamicsValue(int action,int target){
    if(recording||recorder.isActive()||pendingNoteTool()||target!=effectTarget()||target< -1||target>=(int)project.tracks.size())return;
    float low=0,high=1;String name,unit;switch(action){case compThreshold:low=-60;high=0;name="Threshold / ceiling";unit="dBFS";break;case compRatio:low=1;high=20;name="Ratio";unit=":1";break;case compAttack:low=.1f;high=100;name="Attack";unit="ms";break;case compRelease:low=5;high=1000;name="Release";unit="ms";break;case compMakeup:low=-12;high=24;name="Makeup / limiter input trim";unit="dB";break;case dynamicsRange:high=96;name="Maximum attenuation";unit="dB";break;case dynamicsHysteresis:high=24;name="Gate hysteresis";unit="dB";break;case dynamicsHold:high=1000;name="Gate hold";unit="ms";break;case dynamicsKnee:high=24;name="Compressor knee";unit="dB";break;default:return;}
    auto expected=revision;auto mode=effectMixer(target).dynamics.mode;auto* dialog=new AlertWindow(dynamicsName(mode)+" - "+name,"Enter "+String(low,1)+" to "+String(high,1)+" "+unit+". Mode changes retain stored parameter values.",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("value",String(low+parameterValue(action,target)*(high-low),5));dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,action,target,expected,mode,low,high](int result){if(!safe||result!=1)return;if(safe->recording||safe->recorder.isActive()||safe->pendingNoteTool()||safe->revision!=expected||safe->effectTarget()!=target||safe->effectMixer(target).dynamics.mode!=mode){safe->status="Dynamics target changed. Reopen the value control.";return;}auto text=dialog->getTextEditorContents("value").trim();const char* begin=text.toRawUTF8();char* end=nullptr;double parsed=std::strtod(begin,&end);if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(parsed)||parsed<low||parsed>high){safe->status="Enter a finite dynamics value inside the displayed range.";return;}float value=((float)parsed-low)/(high-low);if(std::abs(safe->parameterValue(action,target)-value)<1.e-7f)return;safe->checkpoint();safe->setParameter(action,target,value);}),true);
}
}
