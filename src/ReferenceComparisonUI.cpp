// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
class ReferenceDialog final:public AlertWindow {
public:
    ReferenceDialog(const String& message,const String& summary,bool busy,bool ready,bool match,Component* parent):AlertWindow("Reference track comparison",message,MessageBoxIconType::NoIcon,parent){
        auto area=parent->getParentMonitorArea();text.setComponentID("reference-report");text.setMultiLine(true,true);text.setReadOnly(true);text.setScrollbarsShown(true);text.setText(summary,false);text.setSize(jmax(230,jmin(650,(int)(area.getWidth()*.54))),jmax(100,jmin(300,area.getHeight()-380)));addCustomComponent(&text);
        if(ready){addComboBox("matching",{"RMS matched (attenuation only)","Original relative levels"},"Audition levels");getComboBoxComponent("matching")->setSelectedItemIndex(match?0:1);addButton("A: Mix",1);addButton("B: Reference",2);addButton("Stop",3);}
        if(busy)addButton("Cancel task",3);else{addButton("Passages...",4);if(!ready)addButton("Choose file...",5);}
        addButton(ready?"Discard / Close":"Close",0,KeyPress(KeyPress::escapeKey));
    }
    bool refreshing=false;TextEditor text;
};
String fileStamp(const File& f){return f.getFullPathName()+"|"+String(f.getSize())+"|"+String(f.getLastModificationTime().toMilliseconds());}
}
void Studio::restoreReferenceReport(){
    auto file=root.getChildFile("data/reference-report.json");if(!file.existsAsFile()||file.getSize()>1024*1024)return;auto json=JSON::parse(file);if(!json.isObject()||!json["schema"].isInt()||(int)json["schema"]!=1||json["application"].toString()!="Pulse Studio reference comparison"||!json["summary"].isString()||json["summary"].toString().length()>100000)return;
    referenceReportJson=json;
    if(json["referencePath"].isString()&&File::isAbsolutePath(json["referencePath"].toString())&&json["referencePath"].toString().length()<=4096)referenceSource=File(json["referencePath"].toString());
    auto numeric=[](const var& v){return v.isDouble()||v.isInt()||v.isInt64();};if(numeric(json["mixBeat"])&&numeric(json["referenceSeconds"])&&numeric(json["duration"])){ReferenceOptions o{(double)json["mixBeat"],(double)json["referenceSeconds"],(double)json["duration"]};if(validateReferenceOptions(o).wasOk())referenceOptions=o;}
}
bool Studio::referenceCurrent()const{
    if(referenceTask||!referenceMix||!referenceAudio||referenceRevision!=revision)return false;
    if(fileStamp(referenceReport.mixFile)!=referenceReport.mixStamp||fileStamp(referenceReport.referenceCopy)!=referenceReport.copyStamp)return false;
    for(const auto& a:referenceReport.sources){File f(a.path);if(!f.existsAsFile()||f.getSize()!=a.bytes||f.getLastModificationTime().toMilliseconds()!=a.modified)return false;}return true;
}
void Studio::discardReference(){
    referenceReveal=false;if(referenceTask){referenceTask->cancel.store(true);status="Cancelling reference comparison...";return;}
    if(referenceAudition)engine->cancelPreview();referenceAudition=false;referenceMix=referenceAudio=nullptr;
    for(const auto& file:{referenceReport.mixFile,referenceReport.referenceCopy}){
        if(file==File()||!file.isAChildOf(root.getChildFile("data/ReferenceComparisons")))continue;
        bool used=file==waveFile||file==referenceSource;for(const auto& a:projectAssets(project,currentFile==File()?root:currentFile.getParentDirectory()))used|=file==a.file;for(const auto& history:undoStack)for(const auto& a:projectAssets(history,root))used|=file==a.file;for(const auto& history:redoStack)for(const auto& a:projectAssets(history,root))used|=file==a.file;for(const auto& h:waveHistory)used|=file==h.file;for(const auto& h:waveRedoHistory)used|=file==h.file;
        if(!used){auto parent=file.getParentDirectory();file.deleteFile();parent.deleteFile();}
    }
    referenceReport.mixFile={};referenceReport.referenceCopy={};
}
void Studio::chooseReferenceFile(){
    if(referenceTask||recording||recorder.isActive()){status="Finish the current comparison/take first.";return;}
    chooser=std::make_unique<FileChooser>("Choose local reference audio",referenceSource==File()?root:referenceSource,"*.wav;*.aif;*.aiff;*.flac;*.ogg",true);auto safe=Component::SafePointer<Studio>(this);auto rev=revision;
    chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,[safe,rev](const FileChooser& fc){if(!safe||fc.getResult()==File())return;if(safe->revision!=rev||safe->recording||safe->recorder.isActive()){safe->status="Project changed. Choose the reference again.";return;}auto file=fc.getResult();safe->referenceSource=file;safe->discardReference();AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(file));if(!reader){safe->status="Cannot read reference audio.";return;}double length=(safe->project.songMode?safe->project.songLength():safe->project.patterns[(size_t)safe->project.activePattern].length)*60/safe->project.bpm;safe->referenceOptions={0,0,jmin(8.,length,reader->lengthInSamples/reader->sampleRate)};safe->showReferenceSettings();});
}
void Studio::showReferenceSettings(){
    if(referenceTask||recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Finish the current comparison/take/preview/export first.";return;}
    if(referenceSource==File()||!referenceSource.existsAsFile()){chooseReferenceFile();return;}
    auto* d=new AlertWindow("Reference passages",referenceSource.getFileName()+"\nCompare current Song/PAT with local reference audio. Choose equal-duration passages (0.25-60 s). Mix start uses zero-based quarter-note beats; reference start uses seconds. The mix passage must end within ten minutes. No extra tail is appended.",MessageBoxIconType::NoIcon,this);
    d->addTextEditor("mixBeat",String(referenceOptions.mixBeat,6),"Mix start beat (0 = beginning)");d->addTextEditor("referenceSeconds",String(referenceOptions.referenceSeconds,6),"Reference start seconds");d->addTextEditor("duration",String(referenceOptions.duration,6),"Duration seconds (0.25-60)");d->addButton("Build comparison",1,KeyPress(KeyPress::returnKey));d->addButton("Choose file...",2);d->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto expected=revision;auto source=referenceSource;auto stamp=fileStamp(source);
    d->enterModalState(true,ModalCallbackFunction::create([safe,d,expected,source,stamp](int answer){if(!safe)return;if(answer==2){safe->chooseReferenceFile();return;}if(answer!=1)return;ReferenceOptions options;double* fields[]={&options.mixBeat,&options.referenceSeconds,&options.duration};const char* names[]={"mixBeat","referenceSeconds","duration"};for(int i=0;i<3;++i){auto text=d->getTextEditorContents(names[i]).trim();auto* begin=text.toRawUTF8();char* end=nullptr;*fields[i]=std::strtod(begin,&end);if(text.isEmpty()||end==begin||*end!=0){safe->status="Enter numeric passage values.";return;}}if(fileStamp(source)!=stamp){safe->status="Reference audio changed. Reopen Passages.";return;}safe->beginReference(source,options,expected,true);}),true);
}
bool Studio::beginReference(const File& source,const ReferenceOptions& options,uint64_t expected,bool reveal){
    if(expected!=revision||referenceTask||recording||recorder.isActive()||pendingNoteTool()||exporting.load()||mixTask||restorationTask||waveEditBusy){status="Project changed or app is busy. Finish the active task and rebuild comparison.";return false;}
    auto valid=validateReferenceOptions(options);if(valid.failed()){status=valid.getErrorMessage();return false;}
    RenderSettings settings;settings.bits=32;settings.tailSeconds=0;settings.startBeat=options.mixBeat;settings.endBeat=options.mixBeat+options.duration*project.bpm/60;RenderPlan plan;valid=planRender(project,settings,plan);if(valid.failed()||plan.endFrame>48000*600LL){status=valid.failed()?valid.getErrorMessage():"Comparison mix passage must end within ten minutes.";return false;}
    referenceSource=source;discardReference();referenceOptions=options;referenceRevision=revision;referenceReveal=reveal;auto base=currentFile==File()?root:currentFile.getParentDirectory();auto folder=root.getChildFile("data/ReferenceComparisons").getChildFile(Uuid().toString());referenceTask=std::make_unique<ReferenceTask>(absoluteAssets(project,base),root,source,options,folder);
    if(!referenceTask->startThread(Thread::Priority::low)){referenceTask.reset();status="Cannot start reference comparison worker.";return false;}referenceMessage="Rendering and measuring comparison snapshots in the background...";status=referenceMessage;if(reveal)showReferenceComparison();return true;
}
void Studio::finishReference(){
    if(!referenceTask||!referenceTask->done.load(std::memory_order_acquire))return;
    auto* d=dynamic_cast<ReferenceDialog*>(Component::getCurrentlyModalComponent());bool show=referenceReveal;referenceReveal=false;auto result=referenceTask->result;bool cancelled=referenceTask->cancel.load();referenceReport=std::move(referenceTask->report);referenceTask.reset();
    if(cancelled||result.failed()||referenceRevision!=revision||recording||recorder.isActive()||exporting.load()){
        discardReference();referenceMessage=cancelled?"Reference comparison cancelled.":result.failed()?result.getErrorMessage():"Project changed or app became busy. Rebuild the comparison.";
    }else{
        String error;referenceMix=assets.load(referenceReport.mixFile,error);if(referenceMix)referenceAudio=assets.load(referenceReport.referenceCopy,error);
        if(!referenceMix||!referenceAudio){discardReference();referenceMessage=error;}
        else{referenceReportJson=referenceToJson(referenceReport);auto saved=saveReferenceReport(root.getChildFile("data/reference-report.json"),referenceReport);referenceMessage="Comparison ready. A/B auditions from the passage beginning; RMS matching affects audition only.";if(saved.failed())referenceMessage+=" "+saved.getErrorMessage();}
    }
    status=referenceMessage;if(d){d->refreshing=true;d->exitModalState(0);showReferenceComparison();}else if(show&&!Component::getCurrentlyModalComponent())showReferenceComparison();
}
bool Studio::auditionReference(bool ref){
    if(!referenceCurrent()||recording||recorder.isActive()||exporting.load()||mixProjectHash(project,currentFile==File()?root:currentFile.getParentDirectory())!=referenceReport.projectHash){status="Comparison changed or app is busy. Rebuild before audition.";return false;}if(inputMonitor.load()){status="Turn off input monitoring in the recording input settings before A/B audition.";return false;}if(!audioReady())return false;auto gains=referenceGains(referenceReport,referenceMatch);perform(stop);referenceAudition=engine->preview(ref?referenceAudio:referenceMix,0,0,gains[ref?1:0]);status=referenceAudition?String(ref?"B: reference":"A: mix")+(referenceMatch?" / RMS matched":" / original relative levels"):"Audition queue busy.";return referenceAudition;
}
void Studio::showReferenceComparison(){
    closeCommandPalette();bool ready=referenceCurrent(),busy=referenceTask!=nullptr;
    if(!busy&&!referenceMix&&!referenceReportJson.isObject()&&referenceSource==File()){chooseReferenceFile();return;}
    String summary=referenceReportJson.isObject()?referenceReportJson["summary"].toString():"Choose local audio and passages to build a comparison. Equal-duration 48 kHz stereo snapshots compare sample peak, RMS, crest, correlation and three FFT power bands. Reference audio never enters the song or file export.";
    auto* d=new ReferenceDialog(referenceMessage+(busy?"\nWorking...":ready?"\nCurrent project snapshot.":"\nSaved/stale report: read-only. Rebuild passages for A/B."),summary,busy,ready,referenceMatch,this);auto safe=Component::SafePointer<Studio>(this);
    d->enterModalState(true,ModalCallbackFunction::create([safe,d](int answer){if(!safe||d->refreshing)return;if(auto* combo=d->getComboBoxComponent("matching"))safe->referenceMatch=combo->getSelectedItemIndex()==0;if(answer==1||answer==2){safe->auditionReference(answer==2);safe->referenceMessage=safe->status;safe->showReferenceComparison();}else if(answer==3){if(safe->referenceTask)safe->discardReference();else{safe->engine->cancelPreview();safe->referenceAudition=false;safe->referenceMessage="Comparison audition stopped.";safe->showReferenceComparison();}}else if(answer==4){if(safe->referenceAudition)safe->engine->cancelPreview();safe->referenceAudition=false;safe->showReferenceSettings();}else if(answer==5)safe->chooseReferenceFile();else safe->discardReference();}),true);
}
}
