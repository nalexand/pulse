// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
bool Studio::performStereoAction(int action,int){
    if(action<stereoOpen||action>stereoReset)return false;
    if(action==stereoOpen){if(analyzerView){analyzerView=false;saveAnalyzerSettings();updateAnalyzer();}selectedEffect=6;showPane(effectsPane);return true;}
    if(recording||recorder.isActive()){status="Finish the current take before changing stereo tools.";return true;}
    auto& value=project.tracks[(size_t)selectedTrack].mixer.stereo;auto next=value;
    switch(action){case stereoEnable:next.enabled=!next.enabled;break;case stereoMono:next.mono=!next.mono;break;case stereoSwap:next.swap=!next.swap;break;case stereoInvertLeft:next.invertLeft=!next.invertLeft;break;case stereoInvertRight:next.invertRight=!next.invertRight;break;case stereoReset:next={};break;default:break;}
    if(next!=value){checkpoint();value=next;changed();status=next.enabled?"Stereo tools updated. Width can raise peaks; check the analyzer.":"Stereo tools bypassed. Stored settings are retained unless Reset was used.";}return true;
}
void Studio::paintStereo(Graphics& g,Rectangle<float> r){
    Colour text(0xffd9ddde),muted(0xffadb7bd);g.setColour(Colour(0xff3b464c));g.fillRect(r);const auto& track=project.tracks[(size_t)selectedTrack];const auto& s=track.mixer.stereo;float x=r.getX()+16,y=r.getY()+9,w=r.getWidth()-32;
    label(g,track.name+" / Stereo tools",{x,y,w-205,25},13,text,true);button(g,"Effects",{r.getRight()-194,y,78,25},effectsTab,0);button(g,"Analyzer",{r.getRight()-108,y,92,25},analyzerOpen);
    button(g,s.enabled?"ENABLED":"BYPASSED",{x,y+37,108,25},stereoEnable,0,s.enabled);button(g,"Reset",{x+116,y+37,74,25},stereoReset);
    float column=(w-18)/2;dial(g,"Width  "+String(s.width*100,0)+"%",{x,y+80,column,42},stereoWidth,s.width/2,selectedTrack);dial(g,"Output trim  "+String(s.trim,1)+" dB",{x+column+18,y+80,column,42},stereoTrim,(s.trim+24)/30,selectedTrack);
    button(g,"Mono",{x,y+139,column,27},stereoMono,0,s.mono);button(g,"Swap L/R",{x+column+18,y+139,column,27},stereoSwap,0,s.swap);
    button(g,"Invert input L",{x,y+176,column,27},stereoInvertLeft,0,s.invertLeft);button(g,"Invert input R",{x+column+18,y+176,column,27},stereoInvertRight,0,s.invertRight);
    label(g,"0% width = mono / 100% = original / 200% = wider",{x,y+218,w,22},11,muted);
    label(g,"Input polarity > swap > width / mono > trim. Mono keeps the stored width.",{x,y+241,w,22},10,muted);
    float bottom=r.getBottom()-72;button(g,"Output: "+(track.mixer.output<0?String("Master"):project.tracks[(size_t)track.mixer.output].name),{x,bottom,w-122,27},outputRoute);button(g,"Add bus",{r.getRight()-125,bottom,108,27},addBus);
    label(g,"After the six effects, before pan/fader and delay send. Settings are stored in the project.",{x,bottom+33,w,22},10,muted);
}
void Studio::showStereoParameterMenu(int action,int track){
    if((action!=stereoWidth&&action!=stereoTrim)||recording||recorder.isActive()||track<0||track>=(int)project.tracks.size())return;
    auto sourceRevision=revision;PopupMenu menu;menu.addItem(1,"Reset");menu.addItem(2,"Type in value...");auto safe=Component::SafePointer<Studio>(this);
    menu.showMenuAsync(menuOptions(),[safe,sourceRevision,action,track](int result){if(!safe||!result)return;if(safe->recording||safe->recorder.isActive()||safe->revision!=sourceRevision||safe->selectedTrack!=track){safe->status="Channel changed. Reopen the stereo parameter.";return;}if(result==2)safe->editStereoValue(action-stereoWidth);else if(result==1){safe->checkpoint();safe->setParameter(action,track,action==stereoWidth?.5f:.8f);}});
}
void Studio::editStereoValue(int index){
    if(index<0||index>1||recording||recorder.isActive())return;int track=selectedTrack;auto rev=revision;const auto& s=project.tracks[(size_t)track].mixer.stereo;auto* dialog=new AlertWindow(index==0?"Stereo width":"Stereo output trim",index==0?"Allowed: 0 to 200%":"Allowed: -24 to +6 dB",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("value",String(index==0?s.width*100:s.trim,3));dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,index,track,rev](int result){if(!safe||result!=1)return;if(safe->recording||safe->recorder.isActive()||safe->selectedTrack!=track||safe->revision!=rev){safe->status="Channel changed. Reopen the stereo value.";return;}auto text=dialog->getTextEditorContents("value").trim();const char* begin=text.toRawUTF8();char* end=nullptr;double value=std::strtod(begin,&end);if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(value)||value<(index==0?0:-24)||value>(index==0?200:6)){safe->status="Enter a finite stereo value inside the displayed range.";return;}auto next=safe->project.tracks[(size_t)track].mixer.stereo;if(index==0)next.width=(float)(value/100);else next.trim=(float)value;if(next==safe->project.tracks[(size_t)track].mixer.stereo)return;safe->checkpoint();safe->project.tracks[(size_t)track].mixer.stereo=next;safe->changed();safe->status="Stereo value updated.";}),true);
}
}
