// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cstdlib>
namespace pulse {
using namespace juce;
namespace {String sampleStamp(const File& file){return file.existsAsFile()?file.getFullPathName()+"|"+String(file.getSize())+"|"+String(file.getLastModificationTime().toMilliseconds()):String();}}
std::optional<Studio::SamplerContext> Studio::samplerContext(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before tuning a sample.";return {};}
    if(selectedTrack<0||selectedTrack>=(int)project.tracks.size())return {};auto& track=project.tracks[(size_t)selectedTrack];if(track.isBus||track.instrument!=Instrument::sample||track.samplePath.isEmpty()){status="Load a sample on an instrument channel first.";return {};}
    auto file=File::isAbsolutePath(track.samplePath)?File(track.samplePath):(currentFile==File()?root:currentFile.getParentDirectory()).getChildFile(track.samplePath);String error;auto* sample=assets.load(file,error);if(!sample){status=error;return {};}
    double duration=sample->audio.getNumSamples()/sample->sampleRate;if(track.sampleStart>=duration||(track.sampleEnd>0&&track.sampleEnd>duration+1/sample->sampleRate)){status="Correct the sampler range before tuning.";return {};}
    return SamplerContext{selectedTrack,revision,file,sampleStamp(file),track.sampleStart,track.sampleEnd,sample};
}
bool Studio::samplerContextCurrent(const SamplerContext& context)const{
    if(recording||recorder.isActive()||pendingNoteTool()||selectedTrack!=context.track||revision!=context.revision||context.track<0||context.track>=(int)project.tracks.size()||sampleStamp(context.file)!=context.stamp)return false;
    const auto& track=project.tracks[(size_t)context.track];auto file=File::isAbsolutePath(track.samplePath)?File(track.samplePath):(currentFile==File()?root:currentFile.getParentDirectory()).getChildFile(track.samplePath);
    return !track.isBus&&track.instrument==Instrument::sample&&file==context.file&&track.sampleStart==context.start&&track.sampleEnd==context.end;
}
bool Studio::applySamplerTuning(const SamplerContext& context,int rootNote,double cents){
    if(!samplerContextCurrent(context)){status="Channel, sample or project changed. Reopen sampler tuning.";return false;}auto valid=validateSamplerTuning(rootNote,cents);if(valid.failed()){status=valid.getErrorMessage();return false;}
    auto& track=project.tracks[(size_t)context.track];if(track.sampleRoot==rootNote&&track.sampleTune==cents){status="Sampler tuning is already set.";return true;}
    releaseKeys();checkpoint();track.sampleRoot=rootNote;track.sampleTune=cents;changed();status="Sampler root "+samplerNoteName(rootNote)+", fine tune "+String(cents,1)+" cents. Piano roll notes now follow this mapping; one Undo restores it.";return true;
}
void Studio::showSamplerTuning(const SamplerContext& context,int rootNote,double cents,const String& message){
    if(!samplerContextCurrent(context)){status="Channel, sample or project changed. Reopen sampler tuning.";return;}releaseKeys();
    auto* dialog=new AlertWindow("Sampler tuning",message+ (message.isEmpty()?String():"\n\n")+"Root is the note recorded in this sample. Fine tune is playback correction (+ raises pitch). Detect estimates a sustained single note, then lets you review before Apply. Playback speed and duration change with pitch; Playlist audio is unaffected.",MessageBoxIconType::NoIcon,this);
    StringArray names;for(int pitch=0;pitch<128;++pitch)names.add(samplerNoteName(pitch)+" / MIDI "+String(pitch));dialog->addComboBox("root",names,"Sample root note");dialog->getComboBoxComponent("root")->setSelectedItemIndex(rootNote,dontSendNotification);dialog->addTextEditor("cents",String(cents,6),"Fine tune (-100 to +100 cents)");
    dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Detect pitch",2);dialog->addButton("Reset C4 / 0",3);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,context,cents](int result){if(!safe||!result)return;if(!safe->samplerContextCurrent(context)){safe->status="Channel, sample or project changed. Reopen sampler tuning.";return;}if(result==2){safe->startSamplerPitch();return;}if(result==3){safe->applySamplerTuning(context,60,0);return;}
        auto text=dialog->getTextEditorContents("cents").trim();if(text.isEmpty()||text.length()>12||!text.containsOnly("-+0123456789.")){safe->status="Enter fine tune from -100 to +100 cents.";return;}const char* begin=text.toRawUTF8();char* end=nullptr;double parsed=std::strtod(begin,&end);if(end==begin||*end!='\0'||!std::isfinite(parsed)){safe->status="Enter a numeric fine tune from -100 to +100 cents.";return;}safe->applySamplerTuning(context,dialog->getComboBoxComponent("root")->getSelectedItemIndex(),text==String(cents,6)?cents:parsed);
    }),true);
}
bool Studio::startSamplerPitch(bool present){
    if(samplePitchBusy){status="Sample pitch detection is already running.";return false;}auto context=samplerContext();if(!context)return false;if(samplePitchThread.joinable())samplePitchThread.join();
    samplePitchContext=*context;samplePitchValid=false;samplePitchPresent=present;samplePitchCancel.store(false);samplePitchDone.store(false);samplePitchBusy=true;status="Detecting a sustained sample note in the background...";
    auto source=context->sample;int first=(int)std::round(context->start*source->sampleRate),end=context->end>0?jmin(source->audio.getNumSamples(),(int)std::round(context->end*source->sampleRate)):source->audio.getNumSamples();
    samplePitchThread=std::thread([this,source,first,end]{SamplePitch detected;auto result=detectSamplePitch(source->audio,source->sampleRate,first,end,detected,[this]{return samplePitchCancel.load();});samplePitchValid=result.wasOk();samplePitchResult=detected;samplePitchMessage=result.wasOk()?"Estimate: "+samplerNoteName(detected.root)+" / "+String(detected.frequency,2)+" Hz / periodicity "+String(detected.confidence*100,0)+"%. Proposed fine tune "+String(detected.cents,1)+" cents. Octave estimates can be wrong; check by ear.":result.getErrorMessage();samplePitchDone.store(true,std::memory_order_release);});return true;
}
void Studio::finishSamplerPitch(){
    if(samplePitchBusy&&samplePitchDone.load(std::memory_order_acquire)){if(samplePitchThread.joinable())samplePitchThread.join();samplePitchBusy=false;samplePitchDone.store(false);if(samplePitchCancel.load()){samplePitchPresent=false;samplePitchValid=false;status="Sample pitch detection cancelled; tuning retained.";}else if(!samplerContextCurrent(samplePitchContext)){samplePitchPresent=false;samplePitchValid=false;status="Detection retained no changes: channel, source or project changed.";}else{status=samplePitchMessage;if(!samplePitchValid)samplePitchPresent=false;}}
    if(!samplePitchBusy&&samplePitchPresent){if(!samplerContextCurrent(samplePitchContext)){samplePitchPresent=false;return;}if(!Component::getCurrentlyModalComponent()){samplePitchPresent=false;showSamplerTuning(samplePitchContext,samplePitchResult.root,samplePitchResult.cents,samplePitchMessage);}}
}
bool Studio::performSamplerTuningAction(int action){
    if(action==samplerTuningOpen){if(auto context=samplerContext()){auto& track=project.tracks[(size_t)context->track];showSamplerTuning(*context,track.sampleRoot,track.sampleTune);}return true;}
    if(action==samplerDetect){startSamplerPitch();return true;}if(action==samplerDetectCancel){samplePitchCancel.store(true);samplePitchPresent=false;status=samplePitchBusy?"Cancelling sample pitch detection...":"No sample pitch detection is running.";return true;}return false;
}
}
