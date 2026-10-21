// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
namespace pulse {
using namespace juce;
bool Studio::saveWaveEdit(const AudioBuffer<float>& audio,int first,int end,double rate){
    if(!waveSample)return false;if(rate==0)rate=waveSample->sampleRate;auto valid=validateWaveAudio(audio,rate);
    if(valid.failed()){status=valid.getErrorMessage();return false;}
    if(first<0||end>audio.getNumSamples()||end<=first){status="Invalid edited selection.";return false;}
    auto folder=root.getChildFile("data/Edits");auto result=folder.createDirectory();if(result.failed()){status=result.getErrorMessage();return false;}
    auto target=folder.getNonexistentChildFile(waveFile.getFileNameWithoutExtension().substring(0,64)+"_edit",".wav");TemporaryFile temp(target);auto stream=temp.getFile().createOutputStream();
    WavAudioFormat format;std::unique_ptr<AudioFormatWriter> writer(stream?format.createWriterFor(stream.get(),rate,2,32,{},0):nullptr);
    if(!writer){status="Could not create local edited WAV.";return false;}stream.release();bool ok=writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples());writer.reset();
    if(!ok||!temp.overwriteTargetFileWithTemporary()){status="Could not save edited WAV.";return false;}
    String error;auto* sample=assets.load(target,error);if(!sample){status=error;return false;}
    waveHistory.push_back({waveFile,waveStart,waveEnd,waveViewStart,waveSpan});if(waveHistory.size()>20)waveHistory.erase(waveHistory.begin());waveRedoHistory.clear();waveMarkers.clear();
    engine->preview(nullptr);waveFile=target;waveSample=sample;waveStart=first/sample->sampleRate;waveEnd=end/sample->sampleRate;
    double duration=audio.getNumSamples()/sample->sampleRate;waveSpan=jlimit(1/sample->sampleRate,duration,waveSpan);waveViewStart=jlimit(0.,duration-waveSpan,waveViewStart);
    if(waveStart<waveViewStart||waveEnd>waveViewStart+waveSpan){waveViewStart=0;waveSpan=duration;}
    ++waveRevision;status="Edited copy saved locally. To sampler / To Playlist applies it; Undo edit restores the previous copy.";repaint();return true;
}
void Studio::applyWaveHistory(bool redo){
    if(waveEditBusy){status="Audio conversion in progress. Cancel it before Undo or Redo.";return;}
    if(recording||recorder.isActive()){status="Finish the take before editing audio.";return;}
    auto& from=redo?waveRedoHistory:waveHistory;auto& to=redo?waveHistory:waveRedoHistory;
    if(from.empty()){status=redo?"No audio edit to redo.":"No audio edit to undo.";return;}
    const auto previous=from.back();String error;auto* source=assets.load(previous.file,error);if(!source){status=error;return;}
    to.push_back({waveFile,waveStart,waveEnd,waveViewStart,waveSpan});if(to.size()>20)to.erase(to.begin());from.pop_back();engine->preview(nullptr);
    waveFile=previous.file;waveSample=source;waveStart=previous.start;waveEnd=previous.end;waveViewStart=previous.viewStart;waveSpan=previous.span;waveMarkers.clear();++waveRevision;
    status=redo?"Audio edit restored.":"Previous audio edit restored.";repaint();
}
void Studio::waveClipboardAction(int action){
    if(waveEditBusy){status="Audio conversion in progress. Cancel it before editing.";return;}
    if(recording||recorder.isActive()){status="Finish the take before editing audio.";return;}
    if(!waveSample){status="Open an audio file in the waveform editor first.";return;}
    if((action==wavePaste||action==waveReplace)&&waveClipboard.getNumSamples()>0&&waveClipboardRate!=waveSample->sampleRate){beginWaveResample(waveSample->sampleRate,true,action==waveReplace);return;}
    int first=jlimit(0,waveSample->audio.getNumSamples()-1,(int)std::llround(waveStart*waveSample->sampleRate));
    int end=jlimit(first+1,waveSample->audio.getNumSamples(),(int)std::llround(waveEnd*waveSample->sampleRate));
    bool copy=action==waveCopy||action==waveCut;AudioBuffer<float> copied;
    if(copy){auto result=copyWaveRange(waveSample->audio,waveSample->sampleRate,first,end,copied);if(result.failed()){status=result.getErrorMessage();return;}
        if(action==waveCopy){waveClipboard=std::move(copied);waveClipboardRate=waveSample->sampleRate;++waveRevision;status="Audio copied locally. Paste inserts before the selection; Paste replace replaces it.";return;}}
    WaveOperation operation=action==waveTrim?WaveOperation::trim:action==waveDelete||action==waveCut?WaveOperation::erase:action==waveSilence?WaveOperation::silence:action==waveDuplicate?WaveOperation::duplicate:action==waveReplace?WaveOperation::replace:WaveOperation::insert;
    AudioBuffer<float> output;int a=0,b=0;auto result=spliceWave(waveSample->audio,waveSample->sampleRate,first,end,operation,waveClipboard,waveClipboardRate,output,a,b);
    if(result.failed()){status=result.getErrorMessage();return;}
    double rate=waveSample->sampleRate;if(saveWaveEdit(output,a,b)&&copy){waveClipboard=std::move(copied);waveClipboardRate=rate;}
}
bool Studio::sliceWave(double step){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before inserting slices.";return false;}
    if(!waveSample){status="Open an audio file first.";return false;}
    int first=jlimit(0,waveSample->audio.getNumSamples()-1,(int)std::llround(waveStart*waveSample->sampleRate));int end=jlimit(first+1,waveSample->audio.getNumSamples(),(int)std::llround(waveEnd*waveSample->sampleRate));
    Project next;auto result=sliceWaveToPlaylist(project,selectedTrack,waveFile.getFullPathName(),waveSample->sampleRate,waveSample->audio.getNumSamples(),first,end,step,jmax(0.,snapped(editCursorBeat)),next);
    if(result.failed()){status=result.getErrorMessage();return false;}
    return commitWaveSlices(std::move(next));
}
bool Studio::commitWaveSlices(Project next){
    auto candidate=std::make_unique<Playback>();String warnings;auto result=compilePlayback(next,assets,currentFile==File()?root:currentFile.getParentDirectory(),*candidate,warnings);
    if(result.failed()||warnings.isNotEmpty()){status=result.failed()?result.getErrorMessage():warnings;return false;}
    int firstClip=(int)project.clips.size(),count=(int)next.clips.size()-firstClip;checkpoint();project=std::move(next);selectedClip=firstClip;changed();showPane(playlistPane);
    status=String(count)+" contiguous audio slices inserted at original speed. Ctrl+Z undoes the whole batch.";return true;
}
void Studio::showWaveMenu(bool slicing){
    PopupMenu menu;bool loaded=waveSample!=nullptr;
    if(waveEditBusy){menu.addItem(waveCancelConversion,"Cancel audio conversion");auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe](int result){if(safe&&result==waveCancelConversion)safe->perform(result);});return;}
    if(slicing){menu.addSectionHeader("Slice selection at project tempo (original speed)");const char* names[]={"1/16 note (1/4 beat)","1/8 note (1/2 beat)","1/4 note (1 beat)","1/2 note (2 beats)","1 bar (4 beats)"};for(int i=0;i<5;++i)menu.addItem(i+1,names[i],loaded);menu.addSeparator();menu.addItem(waveDetect,"Detect transient markers...",loaded);menu.addItem(waveSliceMarks,"Slice at markers in selection",loaded&&!waveMarkers.empty());menu.addItem(waveClearMarks,"Clear slice markers",!waveMarkers.empty());}
    else{menu.addItem(waveUndo,"Undo audio edit   Ctrl+Z",!waveHistory.empty());menu.addItem(waveRedo,"Redo audio edit   Ctrl+Y",!waveRedoHistory.empty());menu.addSeparator();menu.addItem(waveCopy,"Copy audio   Ctrl+C",loaded);menu.addItem(waveCut,"Cut audio   Ctrl+X",loaded);menu.addItem(wavePaste,"Paste insert before selection   Ctrl+V",loaded&&waveClipboard.getNumSamples()>0);menu.addItem(waveReplace,"Paste replace selection   Ctrl+Shift+V",loaded&&waveClipboard.getNumSamples()>0);menu.addItem(waveDuplicate,"Duplicate after selection   Ctrl+B",loaded);menu.addSeparator();menu.addItem(waveTrim,"Trim to selection",loaded);menu.addItem(waveDelete,"Delete selection   Delete",loaded);menu.addItem(waveSilence,"Silence selection",loaded);menu.addItem(waveSliceMenu,"Slice to Playlist...",loaded);menu.addItem(waveDetect,"Detect transient markers...",loaded);menu.addItem(waveSliceMarks,"Slice at markers in selection",loaded&&!waveMarkers.empty());menu.addItem(waveClearMarks,"Clear slice markers",!waveMarkers.empty());}
    if(!slicing){menu.addItem(waveResample,"Convert sample rate...",loaded);menu.addItem(restorationOpen,"Audio restoration...",loaded);menu.addItem(spectralOpen,"Spectral band edit...",loaded);if(restorationTask)menu.addItem(restorationCancel,"Cancel audio restoration");}
    auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=waveRevision,projectRevision=revision;int track=selectedTrack;double cursor=editCursorBeat;
    menu.showMenuAsync(menuOptions(),[safe,sourceRevision,projectRevision,track,cursor,slicing](int result){if(!safe||!result)return;
        if(safe->waveRevision!=sourceRevision||safe->revision!=projectRevision||safe->selectedTrack!=track||safe->editCursorBeat!=cursor){safe->status="Audio or destination changed. Reopen the waveform menu.";return;}
        if(slicing){static const double steps[]={.25,.5,1,2,4};if(result>=1&&result<=5)safe->sliceWave(steps[result-1]);else safe->perform(result);}else safe->perform(result);safe->repaint();});
}
bool Studio::performWaveAction(int action,int data){
    if(action==waveCancelConversion){if(waveEditBusy){waveEditCancel.store(true);status="Cancelling audio conversion...";}return true;}
    if(action==waveResample){if(recording||recorder.isActive()||waveEditBusy)status="Finish the take or conversion before converting audio.";else showWaveResample();return true;}
    if(action<waveEditMenu||action>waveSliceMarks)return false;
    if(recording||recorder.isActive()){status="Finish the take before editing audio.";return true;}
    if(action==waveEditMenu||action==waveSliceMenu)showWaveMenu(action==waveSliceMenu);
    else if(action==waveDetect)detectWaveTransients();
    else if(action==waveClearMarks){waveMarkers.clear();++waveRevision;status="Slice markers cleared.";}
    else if(action==waveSliceMarks)sliceWaveMarks();
    else if(action==waveRedo)applyWaveHistory(true);
    else if(action==waveSlice){static const double steps[]={.25,.5,1,2,4};if(data>=0&&data<5)sliceWave(steps[data]);}
    else waveClipboardAction(action);return true;
}
}
