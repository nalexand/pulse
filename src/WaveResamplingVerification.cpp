// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::runWaveResampleChecks(const File& folder,const std::function<void(bool,const String&)>& check){
    cancelNoteTool();closeCommandPalette();project=emptyProject();selectedTrack=7;currentFile=File();undoStack.clear();redoStack.clear();publish();
    auto file=folder.getChildFile("resample-source.wav");AudioBuffer<float> source(2,44100);for(int i=0;i<44100;++i){source.setSample(0,i,(float)(.3*std::sin(2*3.14159265358979323846*997*i/44100)));source.setSample(1,i,-.5f*source.getSample(0,i));}
    auto stream=file.createOutputStream();WavAudioFormat format;std::unique_ptr<AudioFormatWriter> writer(stream?format.createWriterFor(stream.get(),44100,2,32,{},0):nullptr);check(writer!=nullptr,"create cross-rate editor fixture");if(!writer)return;stream.release();check(writer->writeFromAudioSampleBuffer(source,0,44100),"write cross-rate fixture");writer.reset();
    auto hash=SHA256(file).toHexString(),musical=JSON::toString(projectToJson(project));auto initialRevision=revision;
    auto wait=[&]{auto deadline=Time::getMillisecondCounterHiRes()+10000;while(waveEditBusy&&Time::getMillisecondCounterHiRes()<deadline){finishWaveResample();Thread::sleep(1);}check(!waveEditBusy,"background audio conversion finishes within fixture deadline");};
    openWaveform(file);waveStart=.25;waveEnd=.5;waveViewStart=.25;waveSpan=.25;waveMarkers={11025};auto beforeRate=waveRevision;
    check(beginWaveResample(48000)&&waveEditBusy&&waveFile==file&&waveHistory.empty(),"conversion starts in background without replacing editor or history");
    perform(waveTrim);perform(waveReverse);perform(waveUndo);check(waveFile==file&&waveHistory.empty()&&waveRevision==beforeRate,"pending conversion blocks destructive edits and audio history");
    check(!beginWaveResample(96000),"second conversion cannot race the active worker");wait();
    check(waveSample->sampleRate==48000&&waveSample->audio.getNumSamples()==48000&&waveStart==.25&&waveEnd==.5&&waveMarkers.empty()&&waveHistory.size()==1,"converted file stores rate, nearest-frame duration and selection and clears stale markers");
    check(waveViewStart==.25&&waveSpan==.25&&waveRevision==beforeRate+1,"conversion retains valid zoom and advances editor revision once");
    auto convertedFile=waveFile;check(convertedFile!=file&&convertedFile.isAChildOf(root.getChildFile("data/Edits")),"conversion creates a separate retained local file");
    AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(convertedFile));check(reader&&reader->sampleRate==48000&&reader->lengthInSamples==48000&&reader->bitsPerSample==32,"converted WAV metadata matches actual sample rate and float32 frames");
    perform(waveUndo);check(waveFile==file&&waveSample->sampleRate==44100&&waveStart==.25&&waveEnd==.5&&waveViewStart==.25&&waveSpan==.25,"one audio Undo restores source sample rate, selection and zoom");perform(waveRedo);check(waveFile==convertedFile&&waveSample->sampleRate==48000&&waveHistory.size()==1,"one audio Redo restores converted file");
    perform(waveUndo);perform(waveCopy);check(waveClipboardRate==44100&&waveClipboard.getNumSamples()==11025,"audio clipboard retains original rate and frames");
    openWaveform(convertedFile);waveStart=.125;waveEnd=.625;perform(wavePaste);check(waveEditBusy&&waveFile==convertedFile,"cross-rate Paste insert automatically launches background conversion");wait();
    check(waveSample->sampleRate==48000&&waveSample->audio.getNumSamples()==60000&&waveStart==.125&&waveEnd==.375&&waveHistory.size()==1,"cross-rate paste uses converted duration and selects only inserted audio");
    check(waveClipboardRate==44100&&waveClipboard.getNumSamples()==11025,"automatic conversion leaves clipboard in original format");
    perform(waveUndo);perform(waveReplace);wait();check(waveSample->audio.getNumSamples()==36000&&waveStart==.125&&waveEnd==.375&&waveHistory.size()==1,"cross-rate Paste replace substitutes selected time range with converted clipboard");perform(waveUndo);
    beginWaveResample(96000);perform(waveCancelConversion);wait();check(waveFile==convertedFile&&waveHistory.empty()&&status.contains("cancelled"),"explicit cancellation discards worker result and retains history");
    beginWaveResample(96000);waveStart=.2;++waveRevision;wait();check(waveFile==convertedFile&&waveHistory.empty()&&status.contains("changed"),"changed selection rejects stale background conversion");
    beginWaveResample(96000);openWaveform(file);wait();check(waveFile==file&&waveHistory.empty()&&status.contains("changed"),"opening another source safely discards previous conversion");
    beginWaveResample(96000);recording=true;wait();recording=false;check(waveFile==file&&waveHistory.empty()&&status.contains("Recording started"),"recording at completion rejects converted result");
    recording=true;check(!beginWaveResample(48000)&&!waveEditBusy,"recording prevents starting conversion");recording=false;
    check(!beginWaveResample(44100)&&!beginWaveResample(7999)&&!beginWaveResample(48000.5)&&waveHistory.empty(),"same-rate and invalid-rate requests leave editor unchanged");
    auto validRoot=root;auto blocked=folder.getChildFile("blocked-conversion-root");blocked.replaceWithText("file blocks directory creation");root=blocked;beginWaveResample(48000);wait();root=validRoot;check(waveFile==file&&waveHistory.empty(),"failed converted-file write retains source and audio history");
    check(SHA256(file).toHexString()==hash&&JSON::toString(projectToJson(project))==musical&&revision==initialRevision&&undoStack.empty(),"all audio conversions retain source bytes and musical project revision and undo");
    auto commands=paletteEntries();bool convert=false,cancel=false;for(const auto& entry:commands){convert|=entry.action==waveResample&&entry.enabled;cancel|=entry.action==waveCancelConversion&&!entry.enabled;}check(convert&&cancel,"Ctrl+P exposes enabled conversion and context-sensitive cancellation");
    perform(waveResample);auto* dialog=dynamic_cast<AlertWindow*>(Component::getCurrentlyModalComponent());check(dialog&&dialog->getComboBoxComponent("rate")->getNumItems()==9&&dialog->getComboBoxComponent("rate")->getText()=="44100","conversion dialog offers nine rates and defaults to source rate");if(dialog){PNGImageFormat png;FileOutputStream output(folder.getChildFile("wave-resample-dialog.png"));if(output.openedOk())png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),output);dialog->exitModalState(0);}
    check(waveFile==file&&waveHistory.empty()&&!waveEditBusy,"opening and cancelling conversion dialog leaves source intact");
    beginWaveResample(48000);wait();perform(waveSampler);check(project.tracks[7].samplePath==waveFile.getFullPathName()&&project.tracks[7].sampleStart==0&&project.tracks[7].sampleEnd==1,"To sampler applies converted file with time-correct range");showPane(rackPane);perform(undoAction);check(project.tracks[7].samplePath.isEmpty(),"project Undo removes converted sampler assignment separately from audio history");
    showPane(wavePane);snapshot(folder.getChildFile("wave-resampling.png"));
}
}
