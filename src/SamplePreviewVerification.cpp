// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <juce_cryptography/juce_cryptography.h>
namespace pulse {
using namespace juce;
namespace {
void fixture(const File& file){
    file.getParentDirectory().createDirectory();file.deleteFile();AudioBuffer<float> audio(2,48000);for(int i=0;i<48000;++i){audio.setSample(0,i,.25f);audio.setSample(1,i,-.125f);}
    auto stream=file.createOutputStream();WavAudioFormat format;std::unique_ptr<AudioFormatWriter> writer(stream?format.createWriterFor(stream.get(),48000,2,24,{},0):nullptr);if(writer){stream.release();writer->writeFromAudioSampleBuffer(audio,0,48000);}
}
AlertWindow* dialog(){auto* d=dynamic_cast<AlertWindow*>(Component::getCurrentlyModalComponent());return d&&d->getName()=="Sample audition tempo"?d:nullptr;}
void close(int answer){if(auto* d=dialog())d->exitModalState(answer);}
}
void Studio::runSamplePreviewChecks(const File& folder,const std::function<void(bool,const String&)>& check){
    auto previous=std::move(project);auto oldFile=currentFile;auto undo=std::move(undoStack),redo=std::move(redoStack);auto oldRevision=revision;bool wasDirty=dirty;auto oldSelected=selectedTrack;
    perform(stop);project=emptyProject();project.bpm=120;currentFile={};selectedTrack=7;publish();engine->prepare(48000);undoStack.clear();redoStack.clear();dirty=false;
    auto file=folder.getChildFile("preview-source.wav");fixture(file);auto hash=SHA256(file).toHexString(),song=JSON::toString(projectToJson(project));auto rev=revision;BrowserAsset asset{{},"Source",file,.1,.2,false,true};
    auto catalog=samplePreviewFile();bool existed=catalog.existsAsFile();auto original=existed?catalog.loadFileAsString():String();catalog.deleteFile();
    SamplePreviewOptions options{true,true,60};
    check(configureSamplePreview(asset,options,rev,samplePreviewToken(catalog),samplePreviewStamp(file),true),"saved tempo audition starts from Browser configuration");
    AudioBuffer<float> block(2,8192);getNextAudioBlock(AudioSourceChannelInfo(&block,0,8192));check(engine->previewPlaying.load()&&block.getMagnitude(0,0,8192)>.17f&&engine->previewPosition.load()>=.1&&engine->previewPosition.load()<.2,"repeating trimmed Browser source reaches stereo audio callback");
    check(JSON::toString(projectToJson(project))==song&&revision==rev&&!dirty&&undoStack.empty(),"sample audition never edits project notes routing schema revision or Undo");
    perform(browserPreviewStop);getNextAudioBlock(AudioSourceChannelInfo(&block,0,8192));check(!engine->previewPlaying.load()&&block.getMagnitude(0,0,8192)==0,"persistent Browser Stop action silences repeated audition");
    check(previewBrowserSample(asset)&&status.contains("project tempo"),"ordinary Browser click uses saved source tempo");getNextAudioBlock(AudioSourceChannelInfo(&block,0,8192));keyPressed(KeyPress(KeyPress::escapeKey));getNextAudioBlock(AudioSourceChannelInfo(&block,0,8192));check(!engine->previewPlaying.load(),"Escape stops sample audition through the actual key handler");
    check(previewBrowserSample(asset,true)&&status.contains("original speed"),"original-speed override ignores saved synchronization and repeat");getNextAudioBlock(AudioSourceChannelInfo(&block,0,8192));check(!engine->previewPlaying.load(),"original-speed override plays trimmed range only once");
    options.sourceBpm=100;check(!configureSamplePreview(asset,options,rev-1,samplePreviewToken(catalog),samplePreviewStamp(file),false),"stale project prevents an old sample settings dialog from saving");
    check(!configureSamplePreview(asset,options,rev,"old catalog",samplePreviewStamp(file),false),"stale catalog prevents overwriting another settings edit");
    check(!configureSamplePreview(asset,options,rev,samplePreviewToken(catalog),"old source",false),"changed source file prevents stale BPM publication");
    auto bad=options;bad.sourceBpm=0;auto token=samplePreviewToken(catalog);check(!configureSamplePreview(asset,bad,rev,token,samplePreviewStamp(file),false)&&samplePreviewToken(catalog)==token,"invalid BPM keeps saved settings intact");
    recording=true;check(!configureSamplePreview(asset,options,rev,token,samplePreviewStamp(file),false)&&!previewBrowserSample(asset),"recording guards preview and source preferences");recording=false;
    asset.end=.05;check(!configureSamplePreview(asset,options,rev,token,samplePreviewStamp(file),false),"invalid trimmed range cannot update source preferences");asset.end=.2;
    check(configureSamplePreview(asset,options,rev,token,samplePreviewStamp(file),false),"Save stores settings without audition");getNextAudioBlock(AudioSourceChannelInfo(&block,0,8192));check(!engine->previewPlaying.load(),"Save without preview silences previous audition");
    SamplePreviewOptions loaded;check(loadSamplePreview(catalog,file,loaded).wasOk()&&loaded==options,"native source options survive independent catalog reload");
    check(SHA256(file).toHexString()==hash&&JSON::toString(projectToJson(project))==song&&revision==rev&&undoStack.empty(),"all audition actions retain original audio and musical state");
    perform(stop);if(existed)catalog.replaceWithText(original);else catalog.deleteFile();project=std::move(previous);currentFile=oldFile;undoStack=std::move(undo);redoStack=std::move(redo);revision=oldRevision;dirty=wasDirty;selectedTrack=oldSelected;publish();
}
void Studio::runSamplePreviewTransportTick(){
    auto put=[&](const char* key,bool value){transportResults.getDynamicObject()->setProperty(key,value);};
    auto file=transportReport.getSiblingFile("native-preview-source.wav");BrowserAsset asset{{},"Native audition",file,.1,.2,false,true};
    if(transportTicks==6400){
        perform(stop);project=emptyProject();project.bpm=120;currentFile={};selectedTrack=7;publish();fixture(file);samplePreviewFile().deleteFile();
        transportResults.getDynamicObject()->setProperty("samplePreviewSourceHash",SHA256(file).toHexString());showSamplePreviewSettings(asset);
        auto* d=dialog();put("nativeSamplePreviewDialog",d&&d->getTextEditor("bpm")&&d->getComboBoxComponent("mode"));
        if(d){d->getTextEditor("bpm")->setText("60");d->getComboBoxComponent("mode")->setSelectedItemIndex(1);d->getComboBoxComponent("repeat")->setSelectedItemIndex(1);PNGImageFormat png;FileOutputStream stream(transportReport.getSiblingFile("native-sample-preview-dialog.png"));png.writeImageToStream(d->createComponentSnapshot(d->getLocalBounds()),stream);close(1);}
    }
    if(transportTicks==6410){SamplePreviewOptions options;put("nativeSamplePreviewSettings",loadSamplePreview(samplePreviewFile(),file,options).wasOk()&&options==SamplePreviewOptions{true,true,60});transportResults.getDynamicObject()->setProperty("samplePreviewPosition",engine->previewPosition.load());transportResults.getDynamicObject()->setProperty("samplePreviewPeak",engine->masterMeter.load());transportResults.getDynamicObject()->setProperty("samplePreviewStatus",status);put("nativeSamplePreviewAudio",engine->previewPlaying.load()&&engine->masterMeter.load()>.1f&&engine->previewPosition.load()>=.1&&engine->previewPosition.load()<.2);perform(browserPreviewStop);}
    if(transportTicks==6420){put("nativeSamplePreviewStop",!engine->previewPlaying.load());auto token=samplePreviewToken(samplePreviewFile());transportResults.getDynamicObject()->setProperty("samplePreviewSavedToken",token);showSamplePreviewSettings(asset);if(auto* d=dialog())d->getTextEditor("bpm")->setText("200");close(0);}
    if(transportTicks==6430){put("nativeSamplePreviewCancel",samplePreviewToken(samplePreviewFile())==transportResults["samplePreviewSavedToken"].toString());showSamplePreviewSettings(asset);if(auto* d=dialog())d->getTextEditor("bpm")->setText("60junk");close(1);}
    if(transportTicks==6440){put("nativeSamplePreviewInvalid",!engine->previewPlaying.load()&&samplePreviewToken(samplePreviewFile())==transportResults["samplePreviewSavedToken"].toString());showSamplePreviewSettings(asset);changed(false);close(1);}
    if(transportTicks==6450){put("nativeSamplePreviewStale",status.contains("changed")&&!engine->previewPlaying.load());showSamplePreviewSettings(asset);engine->playing.store(true);recording=true;close(1);}
    if(transportTicks==6460){put("nativeSamplePreviewRecording",status.contains("take")&&!engine->previewPlaying.load());recording=false;put("nativeSamplePreviewRetained",SHA256(file).toHexString()==transportResults["samplePreviewSourceHash"].toString()&&project.tracks[7].samplePath.isEmpty());perform(stop);}
}
}
