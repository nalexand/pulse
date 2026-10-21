// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
namespace {
class HistoryForm final:public Component {
public:
    HistoryForm(const HistoryOptions& o,const Project& p,int track,const String& message){setComponentID("audio-history-form");seconds.addItemList({"15 seconds","30 seconds","60 seconds"},1);seconds.setSelectedItemIndex(o.seconds==15?0:o.seconds==30?1:2);memory.addItemList({"4 MiB","16 MiB","32 MiB","64 MiB"},1);memory.setSelectedItemIndex(o.memoryMiB==4?0:o.memoryMiB==16?1:o.memoryMiB==32?2:3);source.addItemList({"Stereo inputs 1 + 2","Input 1 (mono)","Input 2 (mono)"},1);source.setSelectedItemIndex((int)o.mode);
        for(int i=0;i<(int)p.tracks.size();++i)if(!p.tracks[(size_t)i].isBus)target.addItem(String(i+1)+". "+p.tracks[(size_t)i].name,i+1);target.setSelectedId(track+1,dontSendNotification);if(!target.getSelectedId())target.setSelectedItemIndex(0);
        ComboBox* fields[]={&seconds,&memory,&source,&target};const char* names[]={"History duration","Ring memory budget","Dry input source","Destination for To Playlist"};const char* ids[]={"audio-history-seconds","audio-history-memory","audio-history-source","audio-history-track"};for(int i=0;i<4;++i){labels[(size_t)i].setText(names[i],dontSendNotification);labels[(size_t)i].setFont(Font(FontOptions(12)));fields[i]->setComponentID(ids[i]);addAndMakeVisible(labels[(size_t)i]);addAndMakeVisible(fields[i]);}
        report.setComponentID("audio-history-report");report.setReadOnly(true);report.setMultiLine(true,true);report.setScrollbarsShown(true);report.setText(message,false);addAndMakeVisible(report);setSize(660,320);
    }
    void addActions(bool busy,bool ready,const std::function<void(int)>& perform){
        auto add=[&](const char* text,int id){auto b=std::make_unique<TextButton>(text);b->setComponentID("audio-history-action-"+String(id));b->onClick=[perform,id]{perform(id);};addAndMakeVisible(*b);actions.push_back(std::move(b));};
        if(busy)add("Cancel freeze",6);else{add("Start new history",1);add("Freeze / Review",2);if(ready){add("Audition",3);add("Stop",7);add("To Playlist",4);add("Waveform",8);}add("Off / Clear",5);}resized();
    }
    HistoryOptions read()const{int duration[]={15,30,60},budgets[]={4,16,32,64};return {duration[jlimit(0,2,seconds.getSelectedItemIndex())],budgets[jlimit(0,3,memory.getSelectedItemIndex())],(InputMode)source.getSelectedItemIndex()};}
    int destination()const{return target.getSelectedId()-1;}
    void resized()override{int col=(getWidth()-10)/2;ComboBox* fields[]={&seconds,&memory,&source,&target};for(int i=0;i<4;++i){int x=(i%2)*(col+10),y=i/2*60;labels[(size_t)i].setBounds(x,y,col,20);fields[i]->setBounds(x,y+22,col,27);}int rows=((int)actions.size()+3)/4,top=getHeight()-rows*36;report.setBounds(0,124,getWidth(),jmax(1,top-132));int w=(getWidth()-24)/4;for(int i=0;i<(int)actions.size();++i)actions[(size_t)i]->setBounds(i%4*(w+8),top+i/4*36,w,30);}
private:ComboBox seconds,memory,source,target;std::array<Label,4> labels;TextEditor report;std::vector<std::unique_ptr<TextButton>> actions;
};
class HistoryDialog final:public AlertWindow {
public:
    HistoryDialog(const HistoryOptions& o,const Project& p,int track,const String& text,bool busy,bool ready,Component* parent):AlertWindow("Retrospective audio history","Opt-in dry input history during Song playback. Capture is OFF at startup. Freeze pauses memory retention and creates a local review copy; To Playlist uses the captured beat. Start begins a fresh history.",MessageBoxIconType::NoIcon,parent),form(o,p,track,text){auto area=parent->getParentMonitorArea();form.setSize(jmax(260,jmin(660,(int)(area.getWidth()*.54))),jmax(260,jmin(430,area.getHeight()-250)));addCustomComponent(&form);
        form.addActions(busy,ready,[this](int action){exitModalState(action);});addButton("Close",0,KeyPress(KeyPress::escapeKey));}
    HistoryForm form;bool refreshing=false;
};
}
void Studio::restoreAudioHistoryOptions(){auto valid=loadHistoryOptions(root.getChildFile("data/audio-history-settings.json"),historyOptions);if(valid.failed())status=valid.getErrorMessage();}
bool Studio::startAudioHistory(const HistoryOptions& options){
    if(historyTask||recording||recorder.isActive()||exporting.load()||pendingNoteTool()){status="Finish the take, preview, freeze or export before starting input history.";return false;}
    if(validateHistoryOptions(options).failed()){status="Invalid audio history settings.";return false;}
    int channels=inputChannelCount(activeInputs.load(),options.mode);if(channels==0||(options.mode==InputMode::stereo&&channels!=2)){status="Enable the selected inputs in F10; choose mono for a single input.";return false;}
    if(!audioReady())return false;discardAudioHistory();auto valid=audioHistory.start(historyRate.load(),options);if(valid.failed()){status=valid.getErrorMessage();return false;}historyOptions=options;auto saved=saveHistoryOptions(root.getChildFile("data/audio-history-settings.json"),options);status="Input history ON. Play Song to retain complete contiguous input blocks. Freeze to review.";if(saved.failed())status+=" "+saved.getErrorMessage();return true;
}
void Studio::stopAudioHistory(){
    if(historyTask){historyTask->cancel.store(true);historyOffPending=true;historyReveal=false;status="Turning input history off after cancelling its worker...";return;}
    audioHistory.disable();discardAudioHistory();historyMessage="Input history OFF; its ring memory is cleared. Applied/referenced audio stays local.";status=historyMessage;
}
void Studio::discardAudioHistory(bool keep){
    if(historyTask){historyTask->cancel.store(true);return;}engine->cancelPreview();
    if(!keep&&historyFolder!=File()&&historyFolder.isAChildOf(root.getChildFile("data/Retrospective"))){auto file=historyFolder.getChildFile("input.wav");bool used=waveFile==file;
        auto base=currentFile==File()?root:currentFile.getParentDirectory();for(const auto& a:projectAssets(project,base))used|=a.file==file;
        for(const auto& state:undoStack)for(const auto& a:projectAssets(state,root))used|=a.file==file;for(const auto& state:redoStack)for(const auto& a:projectAssets(state,root))used|=a.file==file;
        for(const auto& h:waveHistory)used|=h.file==file;for(const auto& h:waveRedoHistory)used|=h.file==file;
        if(!used){file.deleteFile();historyFolder.getChildFile("capture.json").deleteFile();historyFolder.deleteFile();}
    }{const ScopedLock lock(deviceManager.getAudioCallbackLock());historyPreview.reset();}historySnapshot={};historyFolder={};historyStamp.clear();
}
bool Studio::freezeAudioHistory(bool reveal){
    if(historyTask||recording||recorder.isActive()||exporting.load()||pendingNoteTool()){status="Finish the take, preview, freeze or export before reviewing input history.";return false;}
    audioHistory.pause();if(audioHistory.invalidated()||audioHistory.frames()==0){status="No current input history. Start history, enable its input and play Song first.";return false;}
    discardAudioHistory();historyFolder=root.getChildFile("data/Retrospective").getChildFile(Uuid().toString());historyReveal=reveal;historyMessage="Freezing input history in the background. Retention is paused.";
    try{historyTask=std::make_unique<HistorySaveTask>(audioHistory,historyFolder);if(!historyTask->startThread(Thread::Priority::low)){historyTask.reset();status="Cannot start input-history worker; memory is retained.";return false;}}catch(const std::exception&){historyTask.reset();status="Not enough resources to freeze input history; memory is retained.";return false;}
    status=historyMessage;if(reveal)showAudioHistory();return true;
}
void Studio::finishAudioHistory(){
    if(!historyTask||!historyTask->done.load(std::memory_order_acquire))return;auto* dialog=dynamic_cast<HistoryDialog*>(Component::getCurrentlyModalComponent());bool show=historyReveal;historyReveal=false;auto result=historyTask->result;bool cancelled=historyTask->cancel.load();historySnapshot=std::move(historyTask->snapshot);historyStamp=historyTask->stamp;historyTask.reset();
    if(historyOffPending){historyOffPending=false;stopAudioHistory();}
    else if(cancelled||result.failed()){discardAudioHistory();historyMessage=cancelled?"Freeze cancelled. Input memory is paused and retained; Freeze can retry.":result.getErrorMessage()+" Memory is paused; Freeze can retry.";status=historyMessage;}
    else{try{historyPreview=std::make_unique<Sample>();historyPreview->sampleRate=historySnapshot.rate;historyPreview->audio.setSize(2,historySnapshot.audio.getNumSamples());for(int ch=0;ch<2;++ch)historyPreview->audio.copyFrom(ch,0,historySnapshot.audio,jmin(ch,historySnapshot.audio.getNumChannels()-1),0,historySnapshot.audio.getNumSamples());historyMessage="Frozen input ready. Audition / To Playlist / Waveform. Start begins a fresh history; Off clears unassigned copies.";}catch(const std::exception&){discardAudioHistory();historyMessage="Not enough memory for raw audition; input memory is paused and retained. Freeze can retry.";}status=historyMessage;}
    if(dialog){dialog->refreshing=true;dialog->exitModalState(0);showAudioHistory();}else if(show&&!Component::getCurrentlyModalComponent())showAudioHistory();
}
bool Studio::auditionAudioHistory(){
    if(!historyPreview||historyTask||recording||recorder.isActive()||exporting.load()||inputMonitor.load()||historyFileStamp(historyFolder.getChildFile("input.wav"))!=historyStamp){status="Finish the active task, turn input monitoring off and retain a current frozen copy before audition.";return false;}if(!audioReady())return false;double peak=historyPreview->audio.getMagnitude(0,historyPreview->audio.getNumSamples());perform(stop);return engine->preview(historyPreview.get(),0,0,(float)jmin(.7,peak>0?.98/peak:.7));
}
bool Studio::applyAudioHistory(int track,uint64_t expected){
    if(historyTask||!historyPreview||expected!=revision||historySnapshot.revision!=revision||recording||recorder.isActive()||exporting.load()||pendingNoteTool()||historyFileStamp(historyFolder.getChildFile("input.wav"))!=historyStamp){status="Project/source changed or a task is active. Recapture current Song, or use Waveform for explicit manual assignment.";return false;}
    Project next;auto valid=appendAudioHistory(project,historySnapshot,historyFolder.getChildFile("input.wav"),track,next);if(valid.failed()){status=valid.getErrorMessage();return false;}
    auto playback=std::make_unique<Playback>();String warnings;for(bool song:{true,false}){next.songMode=song;valid=compilePlayback(next,assets,currentFile==File()?root:currentFile.getParentDirectory(),*playback,warnings);if(valid.failed()||warnings.isNotEmpty()){status=valid.failed()?valid.getErrorMessage():warnings;return false;}}next.songMode=true;
    perform(stop);checkpoint();project=std::move(next);selectedTrack=track;selectedClip=(int)project.clips.size()-1;setPlaylistSelection({selectedClip},selectedClip);changed();showPane(playlistPane);double beat=historySnapshot.firstBeat;discardAudioHistory(true);historyMessage="Dry input recalled at Song beat "+String(beat,6)+". One Undo removes the entire clip; Start resumes fresh capture.";status=historyMessage;return true;
}
void Studio::showAudioHistory(){
    closeCommandPalette();String state=audioHistory.active()?"ON / retaining during Song playback":historyTask?"Paused / freezing":audioHistory.frames()>0?"Paused / memory retained":"OFF";
    String text=state+"\n"+String((int64)audioHistory.frames())+" retained frames / "+String(audioHistory.bytes()/(1024.*1024),2)+" MiB allocated\n";
    if(audioHistory.invalidated())text+="Device context changed. Start fresh history before retaining more input.\n";
    text+=historyPreview?historySummary(historySnapshot):"Choose duration, ring memory and raw input; Start. Captures only complete, contiguous Song playback blocks. No disk writes until Freeze. Freeze pauses retention; Off clears memory. Source/mode/device/tempo/revision changes start a fresh segment; raw input is independent of monitoring/effects. Bus/stopped/PAT/take capture and input-latency correction remain planned.\n";
    if(historyMessage.isNotEmpty())text+="\n"+historyMessage;
    auto* d=new HistoryDialog(historyOptions,project,selectedTrack,text,historyTask!=nullptr,historyPreview!=nullptr,this);auto safe=Component::SafePointer<Studio>(this);auto expected=revision;
    d->enterModalState(true,ModalCallbackFunction::create([safe,d,expected](int answer){if(!safe||d->refreshing)return;if(answer==0){safe->historyReveal=false;return;}
        if(answer==1){safe->startAudioHistory(d->form.read());safe->historyMessage=safe->status;safe->showAudioHistory();}
        else if(answer==2){if(!safe->freezeAudioHistory(true))safe->showAudioHistory();}
        else if(answer==3){safe->auditionAudioHistory();safe->showAudioHistory();}
        else if(answer==4){if(!safe->applyAudioHistory(d->form.destination(),expected))safe->showAudioHistory();}
        else if(answer==5){safe->stopAudioHistory();safe->showAudioHistory();}
        else if(answer==6){if(safe->historyTask)safe->historyTask->cancel.store(true);safe->historyMessage="Cancelling freeze; memory stays paused.";safe->showAudioHistory();}
        else if(answer==7){safe->engine->cancelPreview();safe->showAudioHistory();}
        else if(answer==8&&safe->historyPreview){auto file=safe->historyFolder.getChildFile("input.wav");safe->openWaveform(file);safe->discardAudioHistory(true);}
    }),true);
}
}
