// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::restoreInputSettings(){
    inputMonitor.store(false);auto file=root.getChildFile("data/input-audio.json");if(!file.existsAsFile()||file.getSize()>4096)return;
    auto value=JSON::parse(file.loadFileAsString());if(!value.isObject()||(int)value["schema"]!=1)return;
    if(value["takeSeconds"].isInt()){int seconds=(int)value["takeSeconds"];if(seconds==120||seconds==300||seconds==600||seconds==1200)audioTakeSeconds=seconds;}
    if(value.hasProperty("mode")){int mode=(int)value["mode"];if(mode>=0&&mode<=2)inputMode.store(mode);}
    if(value.hasProperty("monitorLevel")){float level=(float)value["monitorLevel"];if(std::isfinite(level)&&level>=0&&level<=1)monitorLevel.store(level);}
}
void Studio::saveInputSettings(){
    auto* object=new DynamicObject();object->setProperty("schema",1);object->setProperty("takeSeconds",audioTakeSeconds);object->setProperty("mode",inputMode.load());object->setProperty("monitorLevel",monitorLevel.load());
    TemporaryFile temporary(root.getChildFile("data/input-audio.json"));if(!temporary.getFile().replaceWithText(JSON::toString(var(object),false))||!temporary.overwriteTargetFileWithTemporary())status="Input settings could not be saved locally.";
}
Engine::TakePlan Studio::recordingPlan(bool keyboard)const{
    Engine::TakePlan plan;const auto& options=project.recording;plan.countInBeats=options.countInBars*4;
    if(options.punch){plan.startBeat=jmax(0.,options.punchStart-options.preRollBars*4);plan.gateBeat=options.punchStart;plan.endBeat=options.punchEnd;}
    else{plan.startBeat=engine->position.load();if(keyboard&&!project.songMode&&plan.startBeat>=project.patterns[(size_t)project.activePattern].length-.000001)plan.startBeat=0;if(keyboard&&project.songMode&&project.loop)if(auto* section=findSongMarker(project,project.loopSection))if(plan.startBeat<section->beat||plan.startBeat>=section->endBeat)plan.startBeat=section->beat;plan.gateBeat=plan.startBeat;}
    return plan;
}
void Studio::startKeyboardRecording(){
    if(!audioReady())return;if(project.recording.punch&&project.recording.punchEnd>project.patterns[(size_t)project.activePattern].length){status="Typing punch range must fit inside the active pattern. Extend the pattern or shorten the range.";showPane(recordingPane);return;}
    auto plan=recordingPlan(true);releaseKeys();engine->preview(nullptr);engine->stop();checkpoint();takeOldLoop=project.loop;takeOldMode=project.songMode;
    if(project.recording.punch){takeRestoreLoop=true;takeRestoreMode=true;project.loop=false;project.songMode=false;}
    takeScheduled=true;takeGateEnd=plan.endBeat;recording=true;recordedKeys.clear();changed();
    if(!engine->beginTake(plan)){recording=false;restoreTakeTransport();status="Recording command queue busy. Try REC again.";return;}
    status="Keyboard take armed. Count-in and pre-roll notes are ignored; the take is one undo step.";
}
void Studio::applyTap(double time){
    if(recording){status="Finish the take before changing tempo.";return;}
    if(lastTapTime<0||time-lastTapTime>2000){tapHasCheckpoint=false;tapTempo.reset();}lastTapTime=time;double tempo=tapTempo.tap(time);
    if(tempo<=0){status="Tap again to measure tempo.";return;}
    if(!tapHasCheckpoint||tapRevision!=revision){checkpoint();tapHasCheckpoint=true;}project.bpm=jlimit(30.,300.,tempo);changed();tapRevision=revision;status="Tapped tempo: "+String(project.bpm,1)+" BPM. Keep tapping to refine; Ctrl+Z restores the previous tempo.";
}
void Studio::paintRecording(Graphics& g,Rectangle<float> r){
    Colour text(0xffd9ddde),muted(0xffa5b0b2),accent(0xffb8c997),orange(0xffe3aa63);g.setColour(Colour(0xff394349));g.fillRect(r);float x=r.getX()+14,y=r.getY()+12;auto& options=project.recording;
    button(g,"TAP",{x,y,132,68},tapTempoAction,0,false,true);button(g,"Reset taps",{x,y+76,132,25},tapReset);
    label(g,String(project.bpm,1)+" BPM",{x+150,y+4,200,35},25,text,true);label(g,"Tap a steady beat",{x+150,y+44,170,22},12,muted);button(g,"Metronome sound...",{x+330,y+43,153,25},metronomeSettingsOpen);
    button(g,audioHistory.active()?"Input history ON: "+String(audioHistory.frames()/historyRate.load(),1)+" s":"Input history...",{x+150,y+78,260,25},audioHistoryOpen,0,audioHistory.active());
    button(g,"Audio limit: "+String(audioTakeSeconds/60)+" min...",{x+330,y+4,153,28},audioTakeLimit);
    label(g,"Count-in before recording",{x,y+122,220,23},12,text,true);
    for(int i=0;i<3;++i)button(g,i==0?"Off":String(i)+" bar"+(i==1?"":"s"),{x+225+i*81,y+121,75,25},countInSelect,i,options.countInBars==i);
    button(g,"Punch recording",{x,y+164,153,28},punchToggle,0,options.punch);button(g,"Set range...",{x+164,y+164,112,28},punchRange);
    label(g,"Bar "+String(options.punchStart/4+1,2)+" to "+String(options.punchEnd/4+1,2)+" (end excluded)",{x,y+201,r.getWidth()-30,24},13,options.punch?orange:muted);
    button(g,"Set in at playhead",{x,y+238,140,25},punchInHere);button(g,"Set out at playhead",{x+149,y+238,146,25},punchOutHere);
    label(g,"Pre-roll before punch-in",{x,y+281,210,23},12,text,true);for(int i=0;i<3;++i)button(g,i==0?"None":String(i)+" bar"+(i==1?"":"s"),{x+225+i*81,y+280,75,25},preRollSelect,i,options.preRollBars==i);
    g.setColour(Colour(0xff59656a));g.drawHorizontalLine((int)(y+322),x,r.getRight()-14);
    label(g,"Audio input",{x,y+331,200,24},13,text,true);button(g,"Device settings...",{r.getRight()-151,y+330,136,25},inputAudioSettings);
    String names="No input channels enabled";if(auto* device=deviceManager.getCurrentAudioDevice()){auto channels=device->getActiveInputChannels();auto labels=device->getInputChannelNames();StringArray enabled;for(int ch=0;ch<labels.size()&&enabled.size()<2;++ch)if(channels[ch])enabled.add(String(enabled.size()+1)+": "+labels[ch]);if(!enabled.isEmpty())names=enabled.joinIntoString(" / ");}label(g,names,{x+100,y+332,r.getWidth()-285,22},10,muted);
    const char* modes[]={"Stereo 1 + 2","Input 1 (mono)","Input 2 (mono)"};for(int i=0;i<3;++i)button(g,modes[i],{x+i*144,y+365,136,25},inputModeSelect,i,inputMode.load()==i);
    for(int ch=0;ch<2;++ch){float row=y+405+ch*32,peak=inputLevels[(size_t)ch],db=peak>0?Decibels::gainToDecibels(peak):-100;
        label(g,"IN "+String(ch+1),{x,row,42,20},11,muted,true);Rectangle<float> meter{x+47,row+5,195,10};g.setColour(Colour(0xff222c30));g.fillRect(meter);
        g.setColour(peak>=.999f?Colour(0xffed7567):accent);g.fillRect(meter.withWidth(meter.getWidth()*jlimit(0.f,1.f,(db+60)/60)));
        label(g,peak>.000001f?String(db,1)+" dBFS":"-inf dBFS",{x+249,row,78,20},10,muted);if(inputClipTicks[(size_t)ch]>0)label(g,"CLIP",{x+292,row+15,40,16},9,Colour(0xffed7567),true);
    }
    button(g,inputMonitor.load()?"Monitor ON":"Monitor OFF",{x+345,y+405,140,28},monitorToggle,0,inputMonitor.load());
    button(g,"-",{x+345,y+443,29,25},monitorLevelChange,-1);label(g,String((int)std::lround(monitorLevel.load()*100))+"%",{x+381,y+443,61,25},12,text,true,Justification::centred);button(g,"+",{x+449,y+443,29,25},monitorLevelChange,1);
    label(g,project.tracks[(size_t)selectedTrack].isBus?"Monitor paused: select a source channel.":"Monitor follows selected channel: "+project.tracks[(size_t)selectedTrack].name,{x,y+480,r.getWidth()-28,22},12,inputMonitor.load()?accent:muted);
    label(g,"Use headphones. Monitor level and mixer effects do not change the dry WAV take.",{x,y+507,r.getWidth()-28,22},11,muted);
    String phase=recording?(engine->countInRemaining.load()>0?"Count-in: "+String(engine->countInRemaining.load())+" beats left":engine->captureReady()?"Recording":"Pre-roll / waiting for punch-in"):"Ready: choose NOTES or AUDIO in the toolbar, then REC.";
    label(g,phase,{x,r.getBottom()-58,r.getWidth()-28,23},13,recording?orange:accent,true);
    label(g,"Audio ignores pattern length; optional punch/selected time limit bounds the take. NOTES records patterns.",{x,r.getBottom()-31,r.getWidth()-28,22},10,muted);
}
bool Studio::performRecordingAction(int action,int data){
    if(action<recordingOpen||action>inputAudioSettings)return false;if(action==recordingOpen){showPane(recordingPane);return true;}
    if(action==monitorToggle){if(inputMonitor.load()){inputMonitor.store(false);status="Input monitoring off. Mixer effect tails may continue.";}else if(inputChannelCount(activeInputs.load(),(InputMode)inputMode.load())==0)status="No selected input is available. Choose input channels in F10.";else if(project.tracks[(size_t)selectedTrack].isBus)status="Select a source channel to monitor the audio input.";else if(audioReady()){inputMonitorTrack.store(selectedTrack);inputMonitor.store(true);status="Monitoring input through "+project.tracks[(size_t)selectedTrack].name+". WAV capture remains dry.";}return true;}
    if(action==monitorLevelChange){monitorLevel.store(jlimit(0.f,1.f,(float)(std::round(monitorLevel.load()*20)+jlimit(-1,1,data))/20));saveInputSettings();return true;}
    if(recording){status="Finish the current take before changing recording settings.";return true;}
    if(action==inputModeSelect){inputMode.store(jlimit(0,2,data));inputMonitor.store(false);saveInputSettings();status="Input selection changed; monitoring is off. Mono selections create mono WAV takes.";return true;}
    if(action==inputAudioSettings){inputMonitor.store(false);perform(settings);return true;}
    if(action==tapTempoAction){applyTap(Time::getMillisecondCounterHiRes());return true;}
    if(action==tapReset){tapTempo.reset();lastTapTime=-1;tapHasCheckpoint=false;status="Tap tempo reset. Tap twice or more to measure a new tempo.";return true;}
    if(action==punchRange){auto* dialog=new AlertWindow("Punch range","Bars are 1-based. The end bar is excluded; bar 1 to bar 3 records two bars.",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("in",String(project.recording.punchStart/4+1,3),"Start bar");dialog->addTextEditor("out",String(project.recording.punchEnd/4+1,3),"End bar");dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog](int r){if(!safe||r!=1||safe->recording)return;auto a=dialog->getTextEditorContents("in").trim(),b=dialog->getTextEditorContents("out").trim();if(a.isEmpty()||b.isEmpty()||!a.containsOnly("0123456789.")||!b.containsOnly("0123456789."))return;double start=(a.getDoubleValue()-1)*4,end=(b.getDoubleValue()-1)*4;if(!std::isfinite(start)||!std::isfinite(end)||start<0||end>4096||end-start<.25){safe->status="Choose an increasing range of at least 1/4 beat inside bars 1-1025.";return;}safe->checkpoint();safe->project.recording.punchStart=start;safe->project.recording.punchEnd=end;safe->changed(false);}),true);return true;}
    auto options=project.recording;
    if(action==countInSelect)options.countInBars=jlimit(0,2,data);else if(action==preRollSelect)options.preRollBars=jlimit(0,2,data);else if(action==punchToggle)options.punch=!options.punch;
    else if(action==punchInHere){options.punchStart=jlimit(0.,4095.75,snapped(engine->position.load()));if(options.punchEnd-options.punchStart<.25)options.punchEnd=jmin(4096.,options.punchStart+4);}
    else if(action==punchOutHere){options.punchEnd=jlimit(.25,4096.,snapped(engine->position.load()));if(options.punchEnd-options.punchStart<.25)options.punchStart=jmax(0.,options.punchEnd-4);}
    checkpoint();project.recording=options;changed(false);return true;
}
}
