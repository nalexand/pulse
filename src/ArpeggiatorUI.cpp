// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {const double rates[]={2,1,.5,1./3,.25,1./6,.125,.0625};}
void Studio::setArpNumber(int parameter,int value){
    if(recording)return;if((parameter==0&&(value<1||value>4))||(parameter==1&&(value< -24||value>24))||parameter<0||parameter>1){status="Choose 1-4 octaves or transposition from -24 to +24 semitones.";return;}
    if(parameter==0)arpOptions.octaves=value;else arpOptions.transpose=value;if(selectedTrack==noteToolTrack)pianoLow=-1;if(noteToolCurrent())refreshNoteToolPreview();else repaint();
}
void Studio::paintArpeggiator(Graphics& g,Rectangle<float> r){
    float x=r.getX()+14,y=r.getY()+10;Colour text(0xffd9ddde);label(g,"Order",{x,y+113,75,25},12,text,true);const char* directions[]={"Up","Down","Up / Down","Down / Up","Source order","Random"};
    for(int i=0;i<6;++i)button(g,directions[i],{x+82+(i%3)*158,y+113+(i/3)*30,150,25},arpDirection,i,(int)arpOptions.order==i);
    label(g,"Step rate",{x,y+183,75,25},12,text,true);const char* names[]={"2 beats","1 beat","1/2 beat","1/3 beat","1/4 beat","1/6 beat","1/8 beat","1/16 beat"};for(int i=0;i<8;++i)button(g,names[i],{x+82+(i%4)*112,y+183+(i/4)*29,104,25},arpRate,i,std::abs(arpOptions.rate-rates[i])<1e-9);
    auto counter=[&](const String& name,float cx,int parameter,int value){label(g,name,{cx,y+252,70,26},11,text);button(g,"-",{cx+72,y+252,26,25},arpAdjust,parameter*2);button(g,String(value),{cx+103,y+252,43,25},arpNumber,parameter);button(g,"+",{cx+151,y+252,26,25},arpAdjust,parameter*2+1);};counter("Octaves",x,0,arpOptions.octaves);counter("Transpose",x+212,1,arpOptions.transpose);
    button(g,"Restart on chord changes",{x+414,y+252,218,25},arpRestart,0,arpOptions.restart);
    noteToolSlider(g,"Gate / step length",{x,y+294,r.getWidth()-28,28},10,String(arpOptions.gate*100,1)+"%");noteToolSlider(g,"Velocity scale",{x,y+335,r.getWidth()-28,28},11,String(arpOptions.velocity*100,1)+"%");
    button(g,"Seed...",{x,y+375,100,25},noteToolSeed);button(g,"New seed",{x+108,y+375,100,25},noteToolReroll);label(g,"Random order seed: "+String((int64)arpOptions.seed)+"  |  Input is sampled at step starts.",{x+220,y+375,r.getWidth()-248,25},10,text);
}
bool Studio::performArpAction(int action,int data){
    if(action==arpeggiatorOpen){openNoteTools(NoteToolMode::arpeggiator);return true;}
    if(action<arpDirection||action>arpRestart)return false;if(recording){status="Finish the take before arpeggiating notes.";return true;}
    if(action==arpDirection){if(data>=0&&data<6)arpOptions.order=(ArpOrder)data;}
    else if(action==arpRate){if(data>=0&&data<8)arpOptions.rate=rates[data];}
    else if(action==arpRestart)arpOptions.restart=!arpOptions.restart;
    else if(action==arpAdjust){if(data>=0&&data<4){int parameter=data/2;setArpNumber(parameter,(parameter==0?arpOptions.octaves:arpOptions.transpose)+(data%2?1:-1));}return true;}
    else if(action==arpNumber){if(data<0||data>1)return true;int parameter=data;auto* dialog=new AlertWindow(parameter==0?"Arpeggiator octaves":"Arpeggiator transposition",parameter==0?"Whole number from 1 to 4.":"Semitones from -24 to +24.",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("value",String(parameter==0?arpOptions.octaves:arpOptions.transpose));dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
        dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,parameter](int result){if(!safe||result!=1||safe->recording)return;auto text=dialog->getTextEditorContents("value").trim(),digits=text.startsWithChar('-')?text.substring(1):text;if(text.length()>3||digits.isEmpty()||!digits.containsOnly("0123456789"))return;safe->setArpNumber(parameter,text.getIntValue());}),true);return true;}
    if(noteToolCurrent())refreshNoteToolPreview();else repaint();return true;
}
}
