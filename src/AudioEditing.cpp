// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::addAudioClip(const File& file,int track,double beat){
    if(recording||recorder.isActive()){status="Stop the take before editing.";return;}
    if(track<0||track>=(int)project.tracks.size()||!std::isfinite(beat)){status="Select a valid Playlist lane and cursor.";return;}
    if(project.tracks[(size_t)track].isBus){status="Place audio on a source track, then route it to the bus.";return;}
    if(project.clips.size()>=maxClips){status="Arrangement clip limit reached.";return;}
    String error;auto* sample=assets.load(file,error);if(!sample){status=error;return;}
    double duration=sample->audio.getNumSamples()/sample->sampleRate*project.bpm/60;if(duration>4096||beat<0||beat+duration>4096){status="Full audio does not fit at this cursor inside the 4096-beat timeline. Move the cursor earlier, lower tempo, or select a shorter range in Waveform.";return;}
    checkpoint();Clip c;c.track=jlimit(0,(int)project.tracks.size()-1,track);c.beat=jlimit(0.,4096.,beat);
    c.audioPath=file.getFullPathName();c.length=duration;
    project.clips.push_back(c);selectedClip=(int)project.clips.size()-1;selectedTrack=c.track;project.songMode=true;
    changed();if(c.length>playlistSpan){playlistStart=0;playlistSpan=jlimit(8.,4096.,std::ceil((c.beat+c.length)/4)*4);}showPane(playlistPane);status="Audio clip added. Drag either edge to trim; double-click for gain and fades. Alt+S splits at the cursor. SONG plays the full file once; sampler notes/patterns are not needed.";
}
void Studio::toggleAudioRecording(){
    if(recorder.isActive()){finishAudioRecording();return;}
    if(project.tracks[(size_t)selectedTrack].isBus){status="Select an audio source track to record.";return;}
    if(!audioEnabled){status="Audio input requires the running application.";return;}
    if(project.clips.size()>=maxClips){status="Arrangement clip limit reached.";return;}
    auto* device=deviceManager.getCurrentAudioDevice();
    if(!device||activeInputs.load()==0){status="Choose an input device and enable its input channels in F10, then press REC again.";perform(settings);return;}
    int channels=inputChannelCount(activeInputs.load(),(InputMode)inputMode.load());if(channels==0){status="Input 2 is unavailable. Enable two input channels in F10 or choose Input 1 in recording setup.";showPane(recordingPane);return;}
    if(!audioReady())return;
    auto plan=recordingPlan(false);double maximum=availableTakeSeconds(device->getCurrentSampleRate(),plan.gateBeat);if(plan.endBeat>=0)maximum=jmin(maximum,(plan.endBeat-plan.gateBeat)*60/project.bpm);if(maximum<1/device->getCurrentSampleRate()){status="No timeline/cache space for a playable take. Move the cursor earlier or save/reopen to release unused cached audio.";return;}plan.endBeat=plan.gateBeat+std::floor(maximum*device->getCurrentSampleRate())/device->getCurrentSampleRate()*project.bpm/60;releaseKeys();captureClosing.store(true);engine->preview(nullptr);engine->stop();
    takeFile=root.getChildFile("data/Recordings").getNonexistentChildFile("Take_"+Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"),".wav");
    auto result=recorder.start(takeFile,device->getCurrentSampleRate(),channels,maximum);if(result.failed()){captureClosing.store(false);status=result.getErrorMessage();return;}
    checkpoint();takeOldLoop=project.loop;takeOldMode=project.songMode;takeRestoreMode=true;takeRestoreLoop=true;project.songMode=true;project.loop=false;takeTempo=project.bpm;takeTrack=selectedTrack;takeBeat=plan.gateBeat;takeGateEnd=plan.endBeat;
    takeScheduled=true;captureScheduled.store(true);recordDeviceStopped.store(false);recording=true;changed();
    if(!engine->beginTake(plan)){recorder.finish();recording=false;captureClosing.store(false);restoreTakeTransport();status="Recording command queue busy. Try REC again.";return;}
    captureClosing.store(false);showPane(playlistPane);status="Audio take armed for up to "+String(recorder.limitSeconds()/60,2)+" min (time/cache/timeline limit). Pattern length is ignored. Count-in/pre-roll are not written. REC or Stop finishes early.";
}
void Studio::restoreTakeTransport(){
    bool republish=false;if(takeRestoreLoop){project.loop=takeOldLoop;takeRestoreLoop=false;republish=true;}if(takeRestoreMode){project.songMode=takeOldMode;takeRestoreMode=false;republish=true;}
    takeScheduled=false;captureScheduled.store(false);takeGateEnd=-1;if(republish)changed();
}
void Studio::finishAudioRecording(){
    if(!recording)return;
    if(!audioRecordMode){releaseKeys();recordedKeys.clear();recording=false;engine->stop();restoreTakeTransport();status="Keyboard take finished. Notes are in the active pattern; Ctrl+Z undoes the take.";return;}
    captureClosing.store(true);while(captureCallbacks.load()>0)std::this_thread::yield();engine->stop();auto result=recorder.finish();recording=false;captureClosing.store(false);
    if(recorder.frames()>0&&project.clips.size()<maxClips){takeRestoreMode=false;Clip c;c.track=takeTrack;c.beat=takeBeat;c.length=recorder.frames()/recorder.rate()*takeTempo/60;c.audioPath=takeFile.getFullPathName();project.clips.push_back(c);selectedClip=(int)project.clips.size()-1;changed();status=result.wasOk()?"Take saved locally: "+takeFile.getFileName()+". Audio is now in the Playlist."+(recorder.reachedLimit()?" Selected recording limit reached.":String()):result.getErrorMessage();}
    else{publish();status=result.failed()?result.getErrorMessage():"Take cancelled before capture; no Playlist clip was added.";}
    restoreTakeTransport();
}

void Studio::editAudioClip(){
    if(selectedClip<0||selectedClip>=(int)project.clips.size()||!project.clips[(size_t)selectedClip].isAudio())return;
    int index=selectedClip;auto c=project.clips[(size_t)index];
    auto* dialog=new AlertWindow("Audio clip settings",File(c.audioPath).getFileName()+"\nPlayback keeps the original speed and pitch.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("gain",String(c.gain*100,1),"Gain (0-200%)");
    dialog->addTextEditor("offset",String(c.sourceOffset,4),"Source offset (seconds)");
    dialog->addTextEditor("fadeIn",String(c.fadeIn,3),"Fade in (beats)");
    dialog->addTextEditor("fadeOut",String(c.fadeOut,3),"Fade out (beats)");
    dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Waveform editor",2);dialog->addButton("Fade curves...",3);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    auto safe=Component::SafePointer<Studio>(this);auto expected=revision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,index,expected](int r){
        if(!safe||safe->revision!=expected||safe->recorder.isActive())return;
        if(r==3){safe->editAudioFadeCurves();return;}
        if(r==2){auto& c=safe->project.clips[(size_t)index];auto base=safe->currentFile==File()?safe->root:safe->currentFile.getParentDirectory();safe->openWaveform(File::isAbsolutePath(c.audioPath)?File(c.audioPath):base.getChildFile(c.audioPath),c.sourceOffset,c.sourceOffset+c.length*60/safe->project.bpm);return;}
        if(r!=1)return;
        const char* fields[]={"gain","offset","fadeIn","fadeOut"};double values[4];
        for(int i=0;i<4;++i){auto v=dialog->getTextEditorContents(fields[i]).trim();if(v.isEmpty()||!v.containsOnly("0123456789.-"))return;values[i]=v.getDoubleValue();if(!std::isfinite(values[i]))return;}
        safe->checkpoint();auto& clip=safe->project.clips[(size_t)index];clip.gain=(float)jlimit(0.,2.,values[0]/100);clip.sourceOffset=jlimit(0.,maxAudioSeconds,values[1]);clip.fadeIn=jlimit(0.,clip.length,values[2]);clip.fadeOut=jlimit(0.,clip.length,values[3]);safe->changed();
    }),true);
}
void Studio::splitSelectedClip(){
    if(selectedClip<0||selectedClip>=(int)project.clips.size()||project.clips.size()>=maxClips)return;
    auto c=project.clips[(size_t)selectedClip];double at=snapped(editCursorBeat),left=at-c.beat;
    if(!c.isAudio()){status="Split currently supports audio clips.";return;}
    if(left<=.001||left>=c.length-.001){status="Place the cursor inside the selected audio clip, then Alt+S.";return;}
    auto right=c;right.beat=at;right.length-=left;right.sourceOffset+=left*60/project.bpm;
    // Splitting retains the audible fade envelope at both new boundaries.
    if(c.fadeIn>left||c.fadeOut>right.length){status="Shorten the fades before splitting through a fade.";return;}
    checkpoint();
    project.clips[(size_t)selectedClip].length=left;project.clips[(size_t)selectedClip].fadeOut=0;project.clips[(size_t)selectedClip].fadeOutCurve=FadeCurve::linear;right.fadeIn=0;right.fadeInCurve=FadeCurve::linear;
    project.clips.push_back(right);selectedClip=(int)project.clips.size()-1;changed();
}
}
