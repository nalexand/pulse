// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
namespace {
class BounceDialog final:public AlertWindow {
public:
    BounceDialog(const String& text,Component* parent):AlertWindow("Bounce channel source to audio",text,MessageBoxIconType::NoIcon,parent){}
    bool refreshing=false;
};
}
bool Studio::beginTrackBounce(double tail,uint64_t expected){
    if(expected!=revision||bounceTask||recording||recorder.isActive()||pendingNoteTool()||exporting.load()||waveEditBusy||referenceTask||transcriptionTask||restorationTask||mixTask){status="Project changed or app is busy. Finish the current task and reopen Bounce.";return false;}
    BouncePlan plan;auto valid=planTrackBounce(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),selectedTrack,tail,assets.availableBytes(),plan);if(valid.failed()){status=valid.getErrorMessage();return false;}
    releaseKeys();bounceRevision=revision;bounceReviewed=false;bounceMessage="Rendering source snapshot in the background. Close keeps it running; reopen Bounce to review.";
    bounceTask=std::make_unique<BounceTask>(std::move(plan),root.getChildFile("data/Bounces").getChildFile(Uuid().toString()));
    if(!bounceTask->startThread(Thread::Priority::low)){bounceTask.reset();status="Cannot start channel bounce worker.";return false;}status=bounceMessage;return true;
}
bool Studio::trackBounceCurrent()const{
    if(!bounceTask||!bounceReviewed||bounceTask->result.failed()||bounceTask->cancel.load()||bounceRevision!=revision||recording||recorder.isActive()||pendingNoteTool()||exporting.load())return false;
    auto& t=*bounceTask;if(!t.audioFile.existsAsFile()||t.audioFile.getSize()!=t.audioBytes||t.audioFile.getLastModificationTime().toMilliseconds()!=t.audioModified)return false;
    for(const auto& d:t.dependencies)if(!d.file.existsAsFile()||d.file.getSize()!=d.bytes||d.file.getLastModificationTime().toMilliseconds()!=d.modified)return false;
    return true;
}
void Studio::discardTrackBounce(){
    if(!bounceTask)return;
    if(!bounceTask->done.load(std::memory_order_acquire)){bounceTask->cancel.store(true);status="Cancelling channel bounce...";return;}
    auto directory=bounceTask->directory;bool owned=bounceTask->ownsDirectory;bounceTask.reset();bounceReviewed=false;
    if(owned&&directory.getParentDirectory()==root.getChildFile("data/Bounces")){
        directory.getChildFile("source.wav").deleteFile();directory.getChildFile("bounce.json").deleteFile();directory.deleteFile();
    }
    bounceMessage="Unapplied bounce discarded. Original notes, audio and Song are unchanged.";status=bounceMessage;
}
void Studio::finishTrackBounce(){
    if(!bounceTask||bounceReviewed||!bounceTask->done.load(std::memory_order_acquire))return;
    bounceReviewed=true;bounceMessage=bounceTask->cancel.load()?"Channel bounce cancelled.":bounceTask->result.failed()?bounceTask->result.getErrorMessage():"Ready: "+bounceTask->plan.source.tracks[(size_t)bounceTask->plan.track].name+" / "+String(bounceTask->plan.replaced)+" Song blocks / "+String(bounceTask->plan.length*60/bounceTask->plan.source.bpm,2)+" seconds. Apply replaces the channel's source blocks with one audio clip. Mixer effects and automation remain live; pattern notes are retained. One Undo restores all original blocks.";
    if(bounceTask->result.wasOk()&&!trackBounceCurrent())bounceMessage="Project or source audio changed. Discard this bounce and build again.";
    status=bounceMessage;if(auto* dialog=dynamic_cast<BounceDialog*>(Component::getCurrentlyModalComponent())){dialog->refreshing=true;dialog->exitModalState(0);showTrackBounce();}
}
bool Studio::applyTrackBounceResult(){
    if(!trackBounceCurrent()){status="Bounce is stale or app is busy. Discard and build a new snapshot.";return false;}
    auto& task=*bounceTask;String error;auto sample=assets.load(task.audioFile,error);if(!sample){status=error;return false;}
    if(!trackBounceCurrent()||sample->audio.getNumSamples()/(double)sample->sampleRate*project.bpm/60!=task.plan.length){status="Bounce output changed or has an invalid duration. Rebuild it.";return false;}
    auto next=project;auto valid=applyTrackBounce(project,task.plan.track,task.audioFile,task.plan.length,next);if(valid.failed()){status=valid.getErrorMessage();return false;}
    auto playback=std::make_unique<Playback>();String warnings;valid=compilePlayback(next,assets,currentFile==File()?root:currentFile.getParentDirectory(),*playback,warnings);if(valid.failed()){status=valid.getErrorMessage();return false;}
    checkpoint();releaseKeys();engine->stop();selectedTrack=task.plan.track;project=std::move(next);selectedClip=(int)project.clips.size()-1;selectedPlaylistIds={project.clips.back().editorId};bounceTask.reset();bounceReviewed=false;changed();showPane(playlistPane);status="Channel source bounced in place. Mixer, buses, automation and Master remain editable. Pattern notes and original files are retained; Ctrl+Z restores original Song blocks.";return true;
}
void Studio::showTrackBounce(){
    releaseKeys();bool busy=bounceTask&&!bounceReviewed;bool ready=trackBounceCurrent();
    auto message=bounceTask?bounceMessage:String("Selected channel: ")+project.tracks[(size_t)selectedTrack].name+"\nSONG only. Renders source notes/audio at 48 kHz, stereo 32-bit float. Channel FX, gain/pan/filter, buses, automation and Master stay live. Build first, review, then Apply as one Undo. Original patterns/files are kept. This is source bouncing; freeze/unfreeze and baked-effect flattening remain planned.";
    if(busy)message+="\nProgress: "+String((int)(100*bounceTask->progress.load()))+"%";
    auto* dialog=new BounceDialog(message,this);
    if(!bounceTask){dialog->addComboBox("tail",{"0 seconds","1 second","2 seconds","3 seconds","5 seconds","10 seconds"},"Instrument release tail");dialog->getComboBoxComponent("tail")->setSelectedItemIndex(3);dialog->addButton("Build bounce",1);}
    else if(busy)dialog->addButton("Cancel bounce",3);
    else {if(ready)dialog->addButton("Apply bounce",2);dialog->addButton("Discard",3);}
    dialog->addButton("Close",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto expected=revision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,expected](int result){if(!safe||dialog->refreshing)return;if(result==1){const double tails[]={0,1,2,3,5,10};auto* combo=dialog->getComboBoxComponent("tail");auto i=combo?combo->getSelectedItemIndex():-1;if(i>=0&&i<6&&safe->beginTrackBounce(tails[i],expected))safe->showTrackBounce();}else if(result==2)safe->applyTrackBounceResult();else if(result==3)safe->discardTrackBounce();}),true);
}
}
