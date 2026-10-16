// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
int Studio::rhythmNumberValue(int parameter)const{switch(parameter){case 0:return rhythmOptions.steps;case 1:return rhythmOptions.pulses;case 2:return rhythmOptions.rotation;case 3:return rhythmOptions.cycles;default:return rhythmOptions.pitch;}}
void Studio::setRhythmNumber(int parameter,int value){
    if(recording)return;int lo=parameter==0||parameter==3?1:0,hi=parameter==0?64:parameter==1?rhythmOptions.steps:parameter==2?rhythmOptions.steps-1:parameter==3?16:127;
    if(parameter<0||parameter>4||value<lo||value>hi){status="Rhythm value is outside its allowed range.";return;}
    switch(parameter){case 0:rhythmOptions.steps=value;rhythmOptions.pulses=jmin(value,rhythmOptions.pulses);rhythmOptions.rotation%=value;break;case 1:rhythmOptions.pulses=value;break;case 2:rhythmOptions.rotation=value;break;case 3:rhythmOptions.cycles=value;break;case 4:rhythmOptions.pitch=value;if(selectedTrack==noteToolTrack)pianoLow=-1;break;}
    if(noteToolCurrent())refreshNoteToolPreview();else repaint();
}
void Studio::paintRhythm(Graphics& g,Rectangle<float> r){
    float x=r.getX()+14,y=r.getY()+10;Colour text(0xffd9ddde);
    auto counter=[&](const String& name,float cx,float cy,int parameter){label(g,name,{cx,cy,148,20},11,text);button(g,"-",{cx,cy+23,28,25},rhythmAdjust,parameter*2);button(g,String(rhythmNumberValue(parameter)),{cx+33,cy+23,61,25},rhythmNumber,parameter);button(g,"+",{cx+99,cy+23,28,25},rhythmAdjust,parameter*2+1);};
    counter("Steps (1-64)",x,y+113,0);counter("Pulses",x+158,y+113,1);counter("Rotation",x+316,y+113,2);counter("Repeats (1-16)",x+474,y+113,3);
    counter("Pitch (MIDI)",x,y+176,4);button(g,"Range: "+String(rhythmOptions.start,2)+" - "+String(rhythmOptions.end,2),{x+158,y+199,170,25},rhythmRange);
    button(g,"Replace this pitch",{x+338,y+199,139,25},rhythmWrite,0,rhythmOptions.write==RhythmWriteMode::replacePitch);button(g,"Fill empty steps",{x+485,y+199,145,25},rhythmWrite,1,rhythmOptions.write==RhythmWriteMode::fill);
    noteToolSlider(g,"Velocity",{x,y+239,r.getWidth()-28,28},7,String(rhythmOptions.velocity*100,1)+"%");noteToolSlider(g,"Gate / step length",{x,y+282,r.getWidth()-28,28},8,String(rhythmOptions.gate*100,1)+"%");noteToolSlider(g,"Velocity variation",{x,y+325,r.getWidth()-28,28},9,"+/- "+String(rhythmOptions.variation*100,1)+"%");
    button(g,"Seed...",{x,y+366,100,25},noteToolSeed);button(g,"New seed",{x+108,y+366,100,25},noteToolReroll);label(g,"Seed "+String((int64)rhythmOptions.seed)+"  |  Whole channel in range; note selection is ignored.",{x+220,y+366,r.getWidth()-248,25},10,text);
}
bool Studio::performRhythmAction(int action,int data){
    if(action==euclideanOpen){openNoteTools(NoteToolMode::euclidean);return true;}
    if(action<rhythmAdjust||action>rhythmWrite)return false;if(recording){status="Finish the take before generating notes.";return true;}
    if(action==rhythmAdjust){int parameter=data/2;setRhythmNumber(parameter,rhythmNumberValue(parameter)+(data%2?1:-1));return true;}
    if(action==rhythmWrite){rhythmOptions.write=data==1?RhythmWriteMode::fill:RhythmWriteMode::replacePitch;if(noteToolCurrent())refreshNoteToolPreview();else repaint();return true;}
    if(action==rhythmNumber){int parameter=data;if(parameter<0||parameter>4)return true;int hi=parameter==0?64:parameter==1?rhythmOptions.steps:parameter==2?rhythmOptions.steps-1:parameter==3?16:127,lo=parameter==0||parameter==3?1:0;auto* dialog=new AlertWindow("Rhythm value","Whole number from "+String(lo)+" to "+String(hi)+".",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("value",String(rhythmNumberValue(parameter)));dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
        dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,parameter](int result){if(!safe||result!=1||safe->recording)return;auto text=dialog->getTextEditorContents("value").trim();if(text.isEmpty()||text.length()>3||!text.containsOnly("0123456789"))return;safe->setRhythmNumber(parameter,text.getIntValue());}),true);return true;}
    auto* dialog=new AlertWindow("Rhythm beat range","Only notes starting inside this range are targeted. Other pitches and outside notes are preserved.",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("start",String(rhythmOptions.start,4),"Start beat");dialog->addTextEditor("end",String(rhythmOptions.end,4),"End beat");dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto capturedRevision=revision;int capturedPattern=project.activePattern;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,capturedRevision,capturedPattern](int result){if(!safe||result!=1||safe->recording||safe->revision!=capturedRevision||safe->project.activePattern!=capturedPattern)return;auto startText=dialog->getTextEditorContents("start").trim(),endText=dialog->getTextEditorContents("end").trim();if(startText.isEmpty()||endText.isEmpty()||!startText.containsOnly("0123456789.")||!endText.containsOnly("0123456789."))return;double start=startText.getDoubleValue(),end=endText.getDoubleValue(),length=safe->project.patterns[(size_t)capturedPattern].length;if(!std::isfinite(start)||!std::isfinite(end)||start<0||end>length||end-start<.01){safe->status="Choose a valid range inside the active pattern.";return;}safe->rhythmOptions.start=start;safe->rhythmOptions.end=end;if(safe->noteToolCurrent())safe->refreshNoteToolPreview();else safe->repaint();}),true);return true;
}
}
