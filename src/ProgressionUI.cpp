// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <cstdlib>
namespace pulse {
using namespace juce;
bool Studio::acceptProgressionSettings(const ProgressionOptions& next,uint64_t expected,uint64_t serial){
    if(!harmonyContext(expected,serial)||noteToolOptions.mode!=NoteToolMode::buildProgression){status="Progression target or settings changed. Reopen the control.";return false;}
    std::vector<Note> checked;auto result=buildProgression(noteToolSource,noteToolLength,activeHarmonyGuide(),next,checked);
    if(result.failed()){status=result.getErrorMessage();return false;}
    progressionOptions=next;++harmonyToolSerial;pianoLow=-1;refreshNoteToolPreview();return true;
}
void Studio::showProgressionSettings(bool style){
    if(!harmonyContext(revision,harmonyToolSerial)||noteToolOptions.mode!=NoteToolMode::buildProgression)return;
    auto* dialog=new AlertWindow(style?"Progression voicing and articulation":"Chord progression",style?"Triads or sevenths follow the selected scale. Voice leading chooses the nearest closed voicing after the first root-position chord.":"Use 1-7 or I-VII; quality follows the scale. Start and duration are beats. The full sequence must fit this pattern. C3 = pitch 48.",MessageBoxIconType::NoIcon,this);
    auto options=progressionOptions;
    if(!style){dialog->addTextEditor("degrees",progressionDegrees(options.degrees),"Degrees (up to 16)");
    dialog->addTextEditor("octave",String(options.octave),"Tonic octave (C3 = pitch 48; 0-7)");
    dialog->addTextEditor("beat",String(options.beat,4),"Start beat (zero-based)");
    dialog->addTextEditor("step",String(options.step,4),"Beats per chord (0.0625-64)");
    }else{dialog->addTextEditor("gate",String(options.gate*100,3),"Gate (5-100%)");
    dialog->addTextEditor("velocity",String(options.velocity*100,3),"Velocity (1-100%)");
    dialog->addComboBox("voices",{"Scale triads","Scale sevenths"},"Chord voices");dialog->getComboBoxComponent("voices")->setSelectedItemIndex(options.sevenths?1:0);
    dialog->addComboBox("leading",{"Nearest closed voicing","Root position for every chord"},"Voice leading");dialog->getComboBoxComponent("leading")->setSelectedItemIndex(options.voiceLeading?0:1);
    dialog->addComboBox("duplicates",{"Skip identical notes","Keep duplicate layers"},"Duplicate policy");dialog->getComboBoxComponent("duplicates")->setSelectedItemIndex(options.skipExact?0:1);
    }
    dialog->addButton("Preview",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto expected=revision,serial=harmonyToolSerial;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,options,expected,serial,style](int result)mutable{
        if(!safe||result!=1)return;
        auto number=[&](const char* key,double& output){auto text=dialog->getTextEditorContents(key).trim();auto* begin=text.toRawUTF8();char* end=nullptr;auto value=std::strtod(begin,&end);if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(value))return false;output=value;return true;};
        if(!style){auto parsed=parseProgression(dialog->getTextEditorContents("degrees"),options.degrees);if(parsed.failed()){safe->status=parsed.getErrorMessage();return;}double octave=0;if(!number("octave",octave)||!number("beat",options.beat)||!number("step",options.step)||octave<0||octave>7||octave!=std::floor(octave)){safe->status="Enter a whole octave 0-7 and finite start/step beats.";return;}options.octave=(int)octave;}
        else{double gate=0,velocity=0;if(!number("gate",gate)||!number("velocity",velocity)||gate<5||gate>100||velocity<1||velocity>100){safe->status="Enter gate 5-100% and velocity 1-100%.";return;}options.gate=gate/100;options.velocity=(float)(velocity/100);options.sevenths=dialog->getComboBoxComponent("voices")->getSelectedItemIndex()==1;options.voiceLeading=dialog->getComboBoxComponent("leading")->getSelectedItemIndex()==0;options.skipExact=dialog->getComboBoxComponent("duplicates")->getSelectedItemIndex()==0;}
        safe->acceptProgressionSettings(options,expected,serial);
    }),true);
}
void Studio::showProgressionExamples(){
    if(!harmonyContext(revision,harmonyToolSerial)||noteToolOptions.mode!=NoteToolMode::buildProgression)return;
    PopupMenu menu;const char* names[]={"1 - 5 - 6 - 4","6 - 4 - 1 - 5","2 - 5 - 1","1 - 6 - 4 - 5","1 - 4 - 5 - 1","1 - 4 - 7 - 3 - 6 - 2 - 5 - 1"};for(int i=0;i<6;++i)menu.addItem(i+1,names[i]);auto safe=Component::SafePointer<Studio>(this);auto expected=revision,serial=harmonyToolSerial;
    menu.showMenuAsync(menuOptions(notePresetsOpen),[safe,expected,serial](int result){if(!safe||result<1||result>6)return;if(!safe->harmonyContext(expected,serial)||safe->noteToolOptions.mode!=NoteToolMode::buildProgression){safe->status="Progression changed. Reopen Examples.";return;}static const std::vector<std::vector<int>> degrees={{1,5,6,4},{6,4,1,5},{2,5,1},{1,6,4,5},{1,4,5,1},{1,4,7,3,6,2,5,1}};safe->progressionOptions.degrees=degrees[(size_t)(result-1)];++safe->harmonyToolSerial;safe->pianoLow=-1;safe->refreshNoteToolPreview();});
}
void Studio::paintProgressionControls(Graphics& g,Rectangle<float> r){
    float x=r.getX()+14,y=r.getY()+10,w=r.getWidth()-28;Colour text(0xffd9ddde),muted(0xffa5b0b2);
    button(g,"Degrees: "+progressionDegrees(progressionOptions.degrees)+"...",{x,y+159,w,28},progressionSettings);
    label(g,"Start: "+String(progressionOptions.beat,3)+"    Beats/chord: "+String(progressionOptions.step,3)+"    Gate: "+String(progressionOptions.gate*100,1)+"%",{x,y+202,w,27},12,text);
    label(g,"Tonic octave: "+String(progressionOptions.octave)+"    "+(progressionOptions.sevenths?"Scale sevenths":"Scale triads")+"    Velocity: "+String(progressionOptions.velocity*100,1)+"%",{x,y+237,w,27},12,text);
    label(g,progressionOptions.voiceLeading?"Voice leading: nearest closed voicing after the first root-position chord":"Voice leading: root position for every chord",{x,y+273,w,25},11,text);
    button(g,"Edit progression...",{x,y+312,190,28},progressionSettings);button(g,"Voicing / gate / velocity...",{x+200,y+312,220,28},progressionSettings,1);
    label(g,"Append only. Seven-note scales; no automatic pattern extension.",{x,y+352,w,24},11,muted);
}
}
