// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <juce_cryptography/juce_cryptography.h>
namespace pulse {
using namespace juce;
namespace {
bool spectralFixture(const File& file,float amplitude=.2f){
    AudioBuffer<float> audio(2,96000);for(int i=0;i<96000;++i){float v=(float)(amplitude*std::sin(2*MathConstants<double>::pi*1000*i/48000)+.1*std::sin(2*MathConstants<double>::pi*400*i/48000));audio.setSample(0,i,v);audio.setSample(1,i,-v);}
    file.getParentDirectory().createDirectory();TemporaryFile temporary(file);std::unique_ptr<OutputStream> out(temporary.getFile().createOutputStream());WavAudioFormat wav;auto writer=out?wav.createWriterFor(out,AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(32).withSampleFormat(AudioFormatWriterOptions::SampleFormat::floatingPoint)):nullptr;
    if(!writer||!writer->writeFromAudioSampleBuffer(audio,0,96000))return false;writer.reset();out.reset();return temporary.overwriteTargetFileWithTemporary();
}
AlertWindow* spectralDialog(){return dynamic_cast<AlertWindow*>(Component::getCurrentlyModalComponent());}
void spectralClose(int answer){if(auto* d=spectralDialog())d->exitModalState(answer);}
void spectralSnapshot(const File& file){if(auto* d=spectralDialog()){FileOutputStream out(file);if(out.openedOk())PNGImageFormat().writeImageToStream(d->createComponentSnapshot(d->getLocalBounds()),out);}}
RestorationOptions spectralOptions(){RestorationOptions o;o.kind=RestorationKind::spectral;return o;}
}
void Studio::runSpectralChecks(const File& folder,const std::function<void(bool,const String&)>& check){
    auto saved=project;auto previousFile=currentFile;auto undo=undoStack,redo=redoStack;auto rev=revision;bool wasDirty=dirty;int track=selectedTrack;auto wave=waveFile;auto* sample=waveSample;
    auto history=waveHistory,forward=waveRedoHistory;auto markers=waveMarkers;double a=waveStart,b=waveEnd,view=waveViewStart,span=waveSpan;auto options=restorationOptions;
    perform(stop);cancelNoteTool();discardRestoration();project=emptyProject();currentFile={};selectedTrack=7;publish();undoStack.clear();redoStack.clear();engine->prepare(48000);auto json=JSON::toString(projectToJson(project));auto currentRevision=revision;
    auto file=folder.getChildFile("spectral-source.wav");check(spectralFixture(file),"create stereo spectral fixture");auto hash=SHA256(file).toHexString();openWaveform(file);waveStart=.25;waveEnd=1.75;
    auto wait=[&]{auto deadline=Time::getMillisecondCounterHiRes()+15000;while(restorationTask&&!restorationTask->done.load(std::memory_order_acquire)&&Time::getMillisecondCounterHiRes()<deadline)Thread::sleep(1);finishRestoration();return !restorationTask;};
    check(!searchCommands(paletteEntries(),"spectral band").empty(),"Ctrl+P discovers dedicated spectral band editing action");perform(spectralOpen);
    auto* d=spectralDialog();auto* form=d?d->findChildWithID("restoration-form"):nullptr;
    auto* low=form?dynamic_cast<TextEditor*>(form->findChildWithID("spectral-low")):nullptr;auto* fft=form?dynamic_cast<ComboBox*>(form->findChildWithID("spectral-fft")):nullptr;
    check(low&&low->isVisible()&&fft&&fft->getNumItems()==4,"native spectral settings exposes frequency band, dB and four FFT resolutions");spectralSnapshot(folder.getChildFile("spectral-settings.png"));spectralClose(0);
    check(!restorationTask&&waveFile==file&&waveHistory.empty(),"cancelling spectral settings changes no audio or history");
    auto o=spectralOptions();check(beginRestoration(o,waveRevision,false)&&!beginRestoration(o,waveRevision,false),"immutable spectral worker starts once and duplicate processing rejects");
    check(wait()&&restorationCurrent()&&restorationImages.valid()&&restorationFile.existsAsFile()&&waveFile==file&&waveHistory.empty(),"spectral worker creates unapplied float copy plus original/edited spectrograms");
    check(restorationImages.original!=restorationImages.edited&&restorationStats.changed[0]>0&&restorationStats.changed[0]==restorationStats.changed[1],"spectrogram measures actual edit while antiphase channel edits stay symmetric");
    showRestorationReview();d=spectralDialog();check(d&&d->findChildWithID("spectral-spectrogram")&&d->findChildWithID("restoration-report"),"review exposes paired spectrograms and read-only processing report");spectralSnapshot(folder.getChildFile("spectral-review.png"));spectralClose(0);discardRestoration();
    check(!restorationCurrent()&&waveFile==file&&waveHistory.empty(),"Discard retains original waveform and history");
    check(beginRestoration(o,waveRevision,false)&&wait(),"fresh spectral review can be rebuilt after Discard");
    AudioBuffer<float> block(2,16384);check(auditionRestoration(false),"Original spectral audition starts without applying");getNextAudioBlock(AudioSourceChannelInfo(&block,0,16384));float original=block.getMagnitude(0,0,16384);
    check(auditionRestoration(true),"Edited spectral audition starts using common preview gain");getNextAudioBlock(AudioSourceChannelInfo(&block,0,16384));
    check(original>.15f&&block.getMagnitude(0,1000,14000)<.085f&&!engine->playing.load(),"audible spectral attenuation reaches Engine independently of Song transport");
    inputMonitor.store(true);check(!auditionRestoration(true),"input monitoring cannot contaminate Original/Edited spectral audition");inputMonitor.store(false);
    auto candidate=restorationFile;check(applyRestoration()&&waveFile==candidate&&waveHistory.size()==1&&waveSample->sampleRate==48000&&waveSample->audio.getNumSamples()==96000,"Apply opens verified spectral copy with one waveform Undo and exact duration/rate");
    perform(waveUndo);check(waveFile==file&&waveStart==.25&&waveEnd==1.75,"waveform Undo restores original audio selection");perform(waveRedo);check(waveFile==candidate&&waveHistory.size()==1,"waveform Redo restores retained spectral copy");
    check(SHA256(file).toHexString()==hash&&JSON::toString(projectToJson(project))==json&&revision==currentRevision&&undoStack.empty(),"spectral processing and waveform history preserve original bytes and musical project");
    openWaveform(file);check(beginRestoration(o,waveRevision,false),"cancellable spectral fixture starts");discardRestoration();check(wait()&&!restorationPreview&&waveFile==file&&status.contains("cancelled"),"worker cancellation applies no partial audio");
    check(beginRestoration(o,waveRevision,false),"selection-bound spectral fixture starts");++waveRevision;check(wait()&&!restorationPreview&&status.contains("changed"),"changed waveform revision rejects stale spectral output");
    check(!beginRestoration(o,waveRevision-1,false),"stale spectral settings cannot start processing");auto invalid=o;invalid.spectral.highHz=25000;check(!beginRestoration(invalid,waveRevision,false),"above-Nyquist spectral band rejects before allocating worker");
    recording=true;check(!beginRestoration(o,waveRevision,false)&&!applyRestoration(),"active take blocks spectral processing and Apply");recording=false;
    check(beginRestoration(o,waveRevision,false)&&wait(),"source-bound spectral preview completes");auto stamp=file.getLastModificationTime();file.setLastModificationTime(stamp+RelativeTime::seconds(3));check(!auditionRestoration(true)&&!applyRestoration(),"changed source stamp blocks spectral audition and Apply");file.setLastModificationTime(stamp);discardRestoration();
    auto stale=folder.getChildFile("spectral-replaced.wav");spectralFixture(stale);openWaveform(stale);stamp=stale.getLastModificationTime();spectralFixture(stale,.3f);stale.setLastModificationTime(stamp);
    check(beginRestoration(o,waveRevision,false)&&wait()&&!restorationPreview&&status.contains("Reopen"),"decoded-source check catches replacement with unchanged size/time");
    openWaveform(file);o.spectral.gainDb=0;check(beginRestoration(o,waveRevision,false)&&wait()&&!restorationPreview&&waveHistory.empty()&&status.contains("no sample changes"),"zero-dB spectral edit creates neither preview file nor Undo entry");
    RestorationOptions stored;check(loadRestorationOptions(root.getChildFile("data/restoration-settings.json"),stored).wasOk()&&stored.kind==RestorationKind::spectral&&stored.spectral.gainDb==-24,"successful spectral settings persist separately from musical project");
    discardRestoration();perform(stop);project=std::move(saved);currentFile=previousFile;undoStack=std::move(undo);redoStack=std::move(redo);revision=rev;dirty=wasDirty;selectedTrack=track;waveFile=wave;waveSample=sample;waveHistory=std::move(history);waveRedoHistory=std::move(forward);waveMarkers=markers;waveStart=a;waveEnd=b;waveViewStart=view;waveSpan=span;restorationOptions=options;++waveRevision;publish();
}
void Studio::runSpectralTransportTick(){
    auto put=[&](const char* key,bool ok){transportResults.getDynamicObject()->setProperty(key,ok);};auto source=transportReport.getSiblingFile("native-spectral-source.wav");
    auto pending=[&]{if(!restorationTask)return false;int n=(int)transportResults["spectralWait"];if(n>=500)return false;transportResults.getDynamicObject()->setProperty("spectralWait",n+1);--transportTicks;return true;};
    if(transportTicks==7000){perform(stop);cancelNoteTool();discardRestoration();project=emptyProject();currentFile={};selectedTrack=7;publish();spectralFixture(source);transportResults.getDynamicObject()->setProperty("spectralSourceHash",SHA256(source).toHexString());openWaveform(source);restorationOptions=spectralOptions();perform(spectralOpen);put("nativeSpectralSettings",spectralDialog()&&spectralDialog()->findChildWithID("restoration-form"));spectralSnapshot(transportReport.getSiblingFile("native-spectral-settings.png"));spectralClose(1);}
    if(transportTicks==7010){if(pending())return;put("nativeSpectralReady",restorationCurrent()&&restorationImages.valid()&&spectralDialog()&&spectralDialog()->findChildWithID("spectral-spectrogram"));spectralSnapshot(transportReport.getSiblingFile("native-spectral-review.png"));engine->observedPeak.store(0);spectralClose(2);}
    if(transportTicks==7020){put("nativeSpectralOriginal",engine->previewPlaying.load()&&engine->observedPeak.load()>.15f&&!engine->playing.load());spectralClose(3);}
    if(transportTicks==7025)engine->observedPeak.store(0);
    if(transportTicks==7030){put("nativeSpectralEdited",engine->previewPlaying.load()&&engine->observedPeak.load()>.06f&&engine->observedPeak.load()<.085f);spectralClose(6);}
    if(transportTicks==7040){put("nativeSpectralStop",!engine->previewPlaying.load());spectralClose(4);}
    if(transportTicks==7050){put("nativeSpectralApply",waveHistory.size()==1&&waveFile!=source);auto edited=waveFile;perform(waveUndo);put("nativeSpectralUndo",waveFile==source);perform(waveRedo);put("nativeSpectralRedo",waveFile==edited);openWaveform(source);beginRestoration(spectralOptions(),waveRevision,false);discardRestoration();}
    if(transportTicks==7060){if(pending())return;put("nativeSpectralCancel",!restorationPreview&&status.contains("cancelled"));beginRestoration(spectralOptions(),waveRevision,false);++waveRevision;}
    if(transportTicks==7070){if(pending())return;put("nativeSpectralStale",!restorationPreview&&status.contains("changed"));recording=true;put("nativeSpectralRecording",!beginRestoration(spectralOptions(),waveRevision,false)&&!applyRestoration());recording=false;put("nativeSpectralSource",SHA256(source).toHexString()==transportResults["spectralSourceHash"].toString());perform(stop);}
}
}
