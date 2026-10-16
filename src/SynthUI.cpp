// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
bool Studio::applySynthPreset(const SynthPreset& preset,int track){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and apply/cancel the note preview before changing a sound.";return false;}
    if(track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus){status="Select an instrument channel for this sound.";return false;}
    auto result=validateSynthPreset(preset);if(result.failed()){status=result.getErrorMessage();return false;}
    releaseKeys();checkpoint();auto& t=project.tracks[(size_t)track];t.instrument=preset.instrument;t.synth=preset.patch;t.attack=preset.attack;t.release=preset.release;t.soundName=preset.name;t.samplePath.clear();t.sampleStart=t.sampleEnd=0;t.sampleLoop=false;selectedTrack=track;changed();showPane(synthPane);status="Loaded "+preset.name+". Notes, pattern length and mixer routing are preserved; Preview or the typing keyboard plays it.";return true;
}
void Studio::showSynthPresets(){showSoundLibrary();}
void Studio::chooseSynthFile(bool save){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before loading/saving a sound.";return;}
    auto& track=project.tracks[(size_t)selectedTrack];if(track.isBus||(save&&!isSynth(track.instrument))){status="Select a synth source channel first.";return;}
    SynthPreset captured{track.soundName.isEmpty()?track.name:track.soundName,"User",track.instrument,track.synth,track.attack,track.release};int destination=selectedTrack;auto sourceRevision=revision;
    auto soundFolder=root.getChildFile("data/Sounds");if(save){auto result=soundFolder.createDirectory();if(result.failed()){status=result.getErrorMessage();return;}}else if(!soundFolder.isDirectory())soundFolder=root.getChildFile("data");
    chooser=std::make_unique<FileChooser>(save?"Save local synth sound":"Load local synth sound",soundFolder.getChildFile("My sound.pulsesound"),"*.pulsesound;*.json",true);auto safe=Component::SafePointer<Studio>(this);
    chooser->launchAsync(FileBrowserComponent::canSelectFiles|(save?FileBrowserComponent::saveMode|FileBrowserComponent::warnAboutOverwriting:FileBrowserComponent::openMode),[safe,save,captured,destination,sourceRevision](const FileChooser& c){if(!safe)return;auto file=c.getResult();if(file==File())return;if(save){auto result=saveSynthPreset(captured,file);safe->status=result.wasOk()?"Sound saved locally: "+file.getFileName():result.getErrorMessage();}
        else{if(safe->revision!=sourceRevision||safe->selectedTrack!=destination){safe->status="Channel changed. Reopen Load sound.";return;}SynthPreset preset;auto result=loadSynthPreset(file,preset);if(result.failed())safe->status=result.getErrorMessage();else safe->applySynthPreset(preset,destination);}safe->repaint();});
}
bool Studio::changeSynthValue(int index,double value,bool saveUndo){
    if(recording||recorder.isActive()||pendingNoteTool()||!isSynth(project.tracks[(size_t)selectedTrack].instrument)||project.tracks[(size_t)selectedTrack].isBus){status="Select a synth and finish the take/note preview before editing its sound.";return false;}
    auto next=project.tracks[(size_t)selectedTrack];auto result=setSynthParameter(next,index,value);if(result.failed()){status=result.getErrorMessage();return false;}
    if(synthParameterValue(next,index)==synthParameterValue(project.tracks[(size_t)selectedTrack],index))return true;
    if(saveUndo)checkpoint();project.tracks[(size_t)selectedTrack]=std::move(next);changed();return true;
}
void Studio::editSynthValue(int index){
    if(index<0||index>=synthParameterCount||recording||recorder.isActive()||pendingNoteTool()||!isSynth(project.tracks[(size_t)selectedTrack].instrument))return;auto spec=synthParameterSpecs()[(size_t)index];auto* d=new AlertWindow(spec.name,"Allowed: "+String(spec.low,3)+" to "+String(spec.high,3)+spec.unit,MessageBoxIconType::NoIcon,this);d->addTextEditor("value",String(synthParameterValue(project.tracks[(size_t)selectedTrack],index),5));d->addButton("Apply",1,KeyPress(KeyPress::returnKey));d->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=revision;int track=selectedTrack;
    d->enterModalState(true,ModalCallbackFunction::create([safe,d,index,sourceRevision,track](int r){if(!safe||r!=1)return;if(safe->revision!=sourceRevision||safe->selectedTrack!=track){safe->status="Channel changed. Reopen the synth value.";return;}auto text=d->getTextEditorContents("value").trim();auto* begin=text.toRawUTF8();char* end=nullptr;double value=std::strtod(begin,&end);if(text.isEmpty()||end==begin||*end!='\0'||!std::isfinite(value)){safe->status="Enter a finite numeric synth value.";return;}safe->changeSynthValue(index,value);}),true);
}
void Studio::paintSynth(Graphics& g,Rectangle<float> r){
    g.setColour(Colour(0xff39454b));g.fillRect(r);auto& t=project.tracks[(size_t)selectedTrack];Colour text(0xffd9ddde),muted(0xffa5b0b2),accent(0xffb8c997);float x=r.getX()+12,y=r.getY()+10,w=r.getWidth()-24;
    if(!isSynth(t.instrument)||t.isBus){label(g,"Select Pulse Analog, Pulse FM or Pulse Wavetable, or load a factory sound.",{x,y,w,40},15,text);button(g,"Factory sounds...",{x,y+48,155,28},synthPresets);return;}
    label(g,instrumentName(t.instrument)+"  /  "+(t.soundName.isEmpty()?String("Custom sound"):t.soundName),{x,y,w-300,26},15,text,true);button(g,"Sounds...",{r.getRight()-290,y,85,26},synthPresets);button(g,"Save",{r.getRight()-200,y,55,26},synthSave);button(g,"Load",{r.getRight()-140,y,55,26},synthLoad);button(g,"Preview",{r.getRight()-80,y,68,26},synthPreview);
    button(g,"Oscillators & filter",{x,y+39,164,27},synthTab,0,synthPage==0);button(g,"Envelopes & LFO",{x+170,y+39,160,27},synthTab,1,synthPage==1);button(g,"Save to library...",{x+340,y+39,144,27},synthLibrarySave);button(g,"Channel settings",{r.getRight()-148,y+39,136,27},synthChannel);
    bool fm=t.instrument==Instrument::fm,table=t.instrument==Instrument::wavetable;
    if(synthPage==0){if(table){button(g,"Table: "+String(wavetableName(t.synth.table)),{x,y+80,174,28},synthWave,2);Rectangle<float> graph{x+190,y+77,w-190,39};g.setColour(Colour(0xff263137));g.fillRect(graph);if(!preparedWavetables())prepareWavetables();Path wave;for(int i=0;i<=(int)graph.getWidth();++i){double phase=i/graph.getWidth();float value=readWavetable(preparedWavetables(),t.synth.table,t.synth.tablePosition,phase,1./65536);float xx=graph.getX()+i,yy=graph.getCentreY()-value*(graph.getHeight()*.44f);if(i==0)wave.startNewSubPath(xx,yy);else wave.lineTo(xx,yy);}g.setColour(accent);g.strokePath(wave,PathStrokeType(1.2f));}else if(!fm){button(g,"Osc 1: "+String(synthWaveName(t.synth.wave1)),{x,y+80,174,28},synthWave,0);button(g,"Osc 2: "+String(synthWaveName(t.synth.wave2)),{x+185,y+80,174,28},synthWave,1);}else label(g,"Two sine operators / ratio, index and decay shape the FM tone",{x,y+80,w,28},12,accent);}
    else label(g,"Amplitude ADSR / decaying filter envelope / sine LFO",{x,y+80,w,28},12,accent);
    std::vector<int> controls=synthPage==0?(table?std::vector<int>{0,1,2,20,21,22,23,6,7,4,5,8}:fm?std::vector<int>{0,1,2,9,10,11,4,5,6,7,8,16}:std::vector<int>{0,1,2,3,4,5,6,7,8,16}):std::vector<int>{12,13,14,15,16,17,18,19};
    float column=w/3,spacing=jmin(70.f,(r.getHeight()-210)/4);for(int i=0;i<(int)controls.size();++i){int index=controls[(size_t)i];auto spec=synthParameterSpecs()[(size_t)index];double value=synthParameterValue(t,index);float cx=x+(i%3)*column,cy=y+126+(i/3)*spacing;String name=fm&&index==0?"Operator mix":String(spec.name);label(g,name,{cx,cy,column-10,19},11,muted);dial(g,String(value,index==2?0:index==6?0:2)+spec.unit,{cx,cy+19,column-15,31},synthValue,(float)synthParameterNormal(index,value),index);}
    label(g,"Drag a knob or use the wheel. Right-click types a value. All sounds are local; use the typing keyboard to play.",{x,r.getBottom()-42,w,18},10,muted);label(g,table?"Position + decaying envelope + sine LFO. The shared LFO rate is on Envelopes & LFO.":"Channel volume, tone filter, effects and routing still process this instrument.",{x,r.getBottom()-23,w,17},10,muted);
}
bool Studio::performSynthAction(int action,int data){
    if(action<synthOpen||action>synthChannel)return false;
    if(recording||recorder.isActive()){status="Finish the take before changing synth sounds.";return true;}
    if(action==synthOpen)showPane(synthPane);else if(action==synthPresets)showSynthPresets();else if(action==synthSave||action==synthLoad)chooseSynthFile(action==synthSave);else if(action==synthTab)synthPage=jlimit(0,1,data);else if(action==synthChannel)showPane(channelPane);
    else if(action==synthPreview){if(isSynth(project.tracks[(size_t)selectedTrack].instrument)&&audioReady())engine->audition(selectedTrack,60,true,.8f,1.2f);}
    else if(action==synthWave){if(data==2){showWavetableMenu();return true;}if(!isSynth(project.tracks[(size_t)selectedTrack].instrument)||pendingNoteTool())return true;checkpoint();auto& t=project.tracks[(size_t)selectedTrack];auto& wave=data==0?t.synth.wave1:t.synth.wave2;wave=(wave+1)%4;t.soundName.clear();changed();}return true;
}
bool Studio::applyWavetableFamily(int family,int track,uint64_t sourceRevision){
    if(family<0||family>=wavetableCount||recording||recorder.isActive()||pendingNoteTool()||track!=selectedTrack||revision!=sourceRevision||track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus||project.tracks[(size_t)track].instrument!=Instrument::wavetable){status="Sound changed. Reopen the wavetable menu.";return false;}return changeSynthValue(24,family);
}
void Studio::showWavetableMenu(){
    if(recording||recorder.isActive()||pendingNoteTool()||project.tracks[(size_t)selectedTrack].isBus||project.tracks[(size_t)selectedTrack].instrument!=Instrument::wavetable)return;PopupMenu menu;for(int i=0;i<wavetableCount;++i)menu.addItem(i+1,wavetableName(i),true,project.tracks[(size_t)selectedTrack].synth.table==i);auto safe=Component::SafePointer<Studio>(this);int track=selectedTrack;auto sourceRevision=revision;
    menu.showMenuAsync(menuOptions(synthWave),[safe,track,sourceRevision](int result){if(safe&&result>0)safe->applyWavetableFamily(result-1,track,sourceRevision);});
}
}
