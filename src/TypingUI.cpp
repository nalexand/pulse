// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>

namespace pulse {
using namespace juce;
void Studio::restoreTypingSettings(){TypingSettings prefs;auto result=loadTypingSettings(root.getChildFile("data/typing-keyboard.json"),prefs);if(result.failed()){status=result.getErrorMessage();return;}typingVelocity=prefs.velocity;octave=prefs.octave;typingEnabled=prefs.enabled;typingTranspose=prefs.transpose;typingCaptureEnabled=prefs.capture;typingCaptureSeconds=prefs.captureSeconds;typingCapture.setWindow(typingCaptureSeconds*1000.);typingSustain=false;}
void Studio::saveTypingPreferences(){auto result=saveTypingSettings(root.getChildFile("data/typing-keyboard.json"),{typingVelocity,octave,typingEnabled,typingTranspose,typingCaptureEnabled,typingCaptureSeconds});if(result.failed())status=result.getErrorMessage();}
String Studio::typingContext()const{
    if(selectedTrack<0||selectedTrack>=(int)project.tracks.size()||project.patterns.empty())return {};
    const auto& t=project.tracks[(size_t)selectedTrack];
    return project.patterns.front().editorId+":"+String((int)project.tracks.size())+":"+String(selectedTrack)+":"+String((int)t.instrument)+":"+t.name+":"+t.samplePath;
}
bool Studio::typingKeyOn(int key,int pitch){
    if(!typingEnabled||heldKeys.count(key)||project.tracks[(size_t)selectedTrack].isBus)return false;
    pitch+=typingTranspose;if(pitch<0||pitch>127){status="Typing note is outside MIDI pitch 0-127. Lower the octave or transposition.";return false;}
    auto context=typingContext();if(context!=typingCaptureContext){releaseKeys();typingCapture.clear();typingCaptureContext=context;}
    // Rearticulation closes an older sustained instance of the same pitch first.
    for(auto it=sustainedKeys.begin();it!=sustainedKeys.end();)if(it->track==selectedTrack&&it->pitch==pitch){releaseTypingNote(*it);it=sustainedKeys.erase(it);}else++it;
    if(!engine->audition(selectedTrack,pitch,true,typingVelocity/127.f)){status="Typing input queue busy; note was not started.";return false;}
    heldKeys[key]={selectedTrack,pitch};if(typingCaptureEnabled)typingCapture.noteOn(pitch,typingVelocity/127.f,Time::getMillisecondCounterHiRes());recordOn(pitch);return true;
}
void Studio::releaseTypingNote(TypingHeld note){
    if(!engine->audition(note.track,note.pitch,false))engine->auditionPanicRequested.store(true);
    typingCapture.noteOff(note.pitch,Time::getMillisecondCounterHiRes());recordOff(note.pitch);
}
void Studio::typingKeyOff(int key){auto it=heldKeys.find(key);if(it==heldKeys.end())return;if(typingSustain)sustainedKeys.push_back(it->second);else releaseTypingNote(it->second);heldKeys.erase(it);}
void Studio::setTypingSustain(bool enabled){typingSustain=enabled;if(!enabled){for(const auto& n:sustainedKeys)releaseTypingNote(n);sustainedKeys.clear();}status=enabled?"Typing sustain on. Ctrl+Shift+K releases sustained notes.":"Typing sustain off.";}
bool Studio::recallTypingPhrase(double now){
    if(recording||recorder.isActive()||exporting.load()){status="Finish the take or export before recalling a typing phrase.";return false;}
    if(pendingNoteTool())return false;
    if(!typingCaptureEnabled){status="Retrospective typing capture is disabled. Enable it in the KEYS menu before playing.";return false;}
    if(project.tracks[(size_t)selectedTrack].isBus||typingCaptureContext!=typingContext()){status="No recent typing performance on this channel. Play a phrase first.";return false;}
    if(project.patterns.size()>=maxPatterns){status="Pattern bank is full. Free an unused pattern before recalling a typing phrase.";return false;}
    std::vector<Note> notes;double length=4;auto result=typingCapture.phrase(now,project.bpm,notes,length);if(result.failed()){status=result.getErrorMessage();return false;}
    auto candidate=project;Pattern pattern;pattern.name="Captured typing "+String((int)project.patterns.size()+1);pattern.length=length;pattern.lanes.resize(project.tracks.size());pattern.lanes[(size_t)selectedTrack]=std::move(notes);candidate.patterns.push_back(std::move(pattern));candidate.activePattern=(int)candidate.patterns.size()-1;candidate.songMode=false;
    auto playback=std::make_unique<Playback>();String warnings;result=compilePlayback(candidate,assets,currentFile==File()?root:currentFile.getParentDirectory(),*playback,warnings);if(result.failed()){status="Cannot recall phrase: "+result.getErrorMessage();return false;}
    releaseKeys();checkpoint();engine->stop();project=std::move(candidate);selectedNotes.clear();selectedNote=-1;pianoStart=0;pianoSpan=jmin(16.,length);pianoLow=-1;typingCapture.clear();changed();showPane(pianoPane);
    status="Typing phrase recalled into a new pattern in PAT mode. Notes keep performed timing/velocity; one Undo restores the song and mode.";return true;
}
void Studio::showTypingMenu(){
    releaseKeys();PopupMenu menu;menu.addSectionHeader("Typing keyboard / velocity "+String(typingVelocity)+" / octave "+String(octave));
    menu.addItem(typingToggle,"Enabled   Ctrl+T",true,typingEnabled);menu.addItem(typingSustainToggle,"Sustain latch   Ctrl+Shift+K",true,typingSustain);
    PopupMenu velocities;for(int v:{32,64,80,102,127})velocities.addItem(1000+v,String(v),true,typingVelocity==v);velocities.addItem(typingVelocityEdit,"Type velocity (1-127)...");menu.addSubMenu("Velocity",velocities);
    PopupMenu octaves;for(int v=1;v<=7;++v)octaves.addItem(2000+v,"Octave "+String(v),true,octave==v);menu.addSubMenu("Octave   [ / ]",octaves);
    PopupMenu transpose;for(int v=-12;v<=12;++v)transpose.addItem(3012+v,(v>0?"+":"")+String(v)+" semitones",true,typingTranspose==v);menu.addSubMenu("Transpose",transpose);
    menu.addItem(typingCaptureToggle,"Retrospective capture enabled",true,typingCaptureEnabled);PopupMenu history;for(int seconds:{15,30,60})history.addItem(4000+seconds,String(seconds)+" seconds",true,typingCaptureSeconds==seconds);menu.addSubMenu("Capture history",history);
    menu.addSeparator();menu.addItem(typingRecall,"Recall last phrase (new pattern / PAT mode)",project.patterns.size()<maxPatterns&&!project.tracks[(size_t)selectedTrack].isBus);menu.addItem(typingCaptureClear,"Clear recent typing performance");
    menu.addSectionHeader("Memory only / last "+String(typingCaptureSeconds)+" seconds / up to 64 beats");auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(typingToggle),[safe](int id){if(!safe||!id)return;if(id>=1001&&id<=1127)safe->perform(typingVelocitySet,id-1000);else if(id>=2001&&id<=2007)safe->perform(typingOctaveSet,id-2000);else if(id>=3000&&id<=3024)safe->perform(typingTransposeSet,id-3012);else if(id==4015||id==4030||id==4060)safe->perform(typingCaptureWindow,id-4000);else safe->perform(id);});
}
bool Studio::performTypingAction(int action,int data){
    if(action<typingMenu||action>typingCaptureWindow)return false;
    if(recording&&action!=typingSustainToggle){status="Finish the take before changing typing preferences or recalling a phrase.";return true;}
    switch(action){
    case typingMenu:showTypingMenu();break;
    case typingSustainToggle:setTypingSustain(!typingSustain);break;
    case typingVelocitySet:if(data>=1&&data<=127){typingVelocity=data;saveTypingPreferences();status="Typing velocity: "+String(data)+" / 127 (new notes only).";}else status="Typing velocity must be an integer from 1 to 127.";break;
    case typingOctaveSet:if(data>=1&&data<=7){releaseKeys();octave=data;saveTypingPreferences();status="Typing octave: "+String(data)+". Use [ / ] to change it.";}break;
    case typingTransposeSet:if(data>=-12&&data<=12){releaseKeys();typingTranspose=data;saveTypingPreferences();status="Typing transpose: "+String(data)+" semitones (new notes only).";}break;
    case typingCaptureToggle:releaseKeys();typingCapture.clear();typingCaptureEnabled=!typingCaptureEnabled;saveTypingPreferences();status=typingCaptureEnabled?"Retrospective typing capture enabled; notes stay in memory until recall.":"Retrospective typing capture disabled; its memory is cleared.";break;
    case typingCaptureWindow:if(data==15||data==30||data==60){typingCaptureSeconds=data;typingCapture.setWindow(data*1000.);typingCapture.expire(Time::getMillisecondCounterHiRes());saveTypingPreferences();status="Typing capture history: "+String(data)+" seconds, up to 64 beats.";}break;
    case typingVelocityEdit:{releaseKeys();auto* d=new AlertWindow("Typing velocity","Velocity for new typing notes and recorded takes (integer 1-127). Ctrl+Shift+Up/Down adjusts by 8.",MessageBoxIconType::NoIcon,this);d->addTextEditor("velocity",String(typingVelocity));d->addButton("Apply",1,KeyPress(KeyPress::returnKey));d->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);d->enterModalState(true,ModalCallbackFunction::create([safe,d](int result){if(!safe||result!=1)return;auto text=d->getTextEditorContents("velocity").trim();if(text.isEmpty()||!text.containsOnly("0123456789")||text.length()>3){safe->status="Typing velocity must be an integer from 1 to 127.";return;}safe->perform(typingVelocitySet,text.getIntValue());}),true);break;}
    case typingRecall:recallTypingPhrase(Time::getMillisecondCounterHiRes());break;
    case typingCaptureClear:releaseKeys();typingCapture.clear();typingCaptureContext.clear();status="Recent typing performance cleared from memory.";break;
    default:break;
    }return true;
}
}
