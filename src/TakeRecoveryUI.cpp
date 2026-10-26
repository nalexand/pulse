// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
void Studio::announceTakeRecovery(){TakeRecoveryScan scan;auto r=scanTakeCheckpoints(root.getChildFile("data/Recordings"),scan);if(r.wasOk()&&!scan.takes.empty())status=String((int)scan.takes.size())+" interrupted audio take checkpoints available. FILE > Recover interrupted audio take...";}
void Studio::showTakeRecovery(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the current take or note preview before reviewing recovery.";return;}
    if(takeRecoveryBusy){status="Building recovery copy in the background. Ctrl+P > Cancel take recovery stops copying; originals are retained.";return;}
    TakeRecoveryScan scan;auto result=scanTakeCheckpoints(root.getChildFile("data/Recordings"),scan);if(result.failed()){status=result.getErrorMessage();return;}
    if(scan.takes.empty()){status="No interrupted take with acknowledged audio was found."+String(scan.invalid?" Invalid checkpoints were retained.":"")+String(scan.limited?" Scan limit reached; original files retained.":"");return;}
    PopupMenu menu;if(scan.limited)menu.addSectionHeader("Scan/display limit reached; showing retained candidates");if(scan.invalid)menu.addSectionHeader(String(scan.invalid)+" invalid checkpoints retained");for(size_t i=0;i<scan.takes.size();++i){const auto& c=scan.takes[i];menu.addItem((int)i+1,Time(c.created).formatted("%Y-%m-%d %H:%M:%S")+" / "+String(c.frames/c.rate,2)+" s / channel "+String(c.track+1));}
    auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe,takes=std::move(scan.takes)](int selected){if(!safe||selected<1||selected>(int)takes.size())return;auto c=takes[(size_t)selected-1];auto* dialog=new AlertWindow("Recover interrupted audio take","Acknowledged audio: "+String(c.frames/c.rate,3)+" seconds at "+String(c.rate,0)+" Hz / "+String(c.channels)+" channels.\nOriginal position: bar "+String(c.beat/4+1,3)+", channel "+String(c.track+1)+".\n\nBuild a separate project from the take's pre-recording session and acknowledged audio prefix. The original WAV/checkpoint and current project are retained. The latest unflushed audio may be unavailable. An observed header interruption is repaired only in the copy.",MessageBoxIconType::NoIcon,safe.getComponent());dialog->addButton("Build recovery copy",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));dialog->enterModalState(true,ModalCallbackFunction::create([safe,c](int r){if(safe&&r==1)safe->startTakeRecovery(c);}),true);});
}
bool Studio::startTakeRecovery(const TakeCheckpoint& checkpoint){
    if(takeRecoveryBusy||recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Finish the active take, preview, export or recovery first.";return false;}
    if(takeRecoveryThread.joinable())takeRecoveryThread.join();takeRecoveryBusy=true;takeRecoveryDone=false;takeRecoveryCancelRequested=false;takeRecoveryRevision=revision;recoveredTakeProject={};takeRecoveryMessage.clear();
    try{takeRecoveryThread=std::thread([this,checkpoint]{try{File output;auto result=recoverTakeCheckpoint(checkpoint,output,&takeRecoveryCancelRequested);recoveredTakeProject=output;takeRecoveryMessage=result.wasOk()?"Verified recovery copy ready: "+output.getFullPathName():result.getErrorMessage();takeRecoveryDone.store(true,std::memory_order_release);}catch(...){recoveredTakeProject={};takeRecoveryMessage="Take recovery worker failed; original files and any draft are retained.";takeRecoveryDone.store(true,std::memory_order_release);}});}
    catch(const std::exception&){takeRecoveryBusy=false;status="Cannot start take recovery worker. Originals retained.";return false;}
    status="Building a separate recovered take/project in the background. Current music is unchanged.";return true;
}
void Studio::finishTakeRecovery(){
    if(!takeRecoveryDone.exchange(false,std::memory_order_acquire))return;if(takeRecoveryThread.joinable())takeRecoveryThread.join();takeRecoveryBusy=false;status=takeRecoveryMessage;
    if(takeRecoveryCancelRequested.load()){status="Take recovery cancelled. Originals and any draft/verified copy are retained.";return;}
    if(recoveredTakeProject==File())return;if(revision!=takeRecoveryRevision||recording||pendingNoteTool()||exporting.load()){status+=" Project changed; use File > Open to inspect this copy later.";return;}
    auto safe=Component::SafePointer<Studio>(this);auto file=recoveredTakeProject;auto reviewed=revision;AlertWindow::showAsync(MessageBoxOptions().withIconType(MessageBoxIconType::InfoIcon).withTitle("Recovered audio take ready").withMessage("The verified recovered project is ready. Opening it backs up unsaved current work through the normal Open workflow. Original files remain intact.\n\n"+file.getFullPathName()).withButton("Open recovered project").withButton("Keep current project"),[safe,file,reviewed](int result){if(!safe||result!=1)return;if(safe->revision!=reviewed||safe->recording||safe->pendingNoteTool()){safe->status="Project changed; recovered copy retained. Open it with File > Open when ready.";return;}safe->openProject(file);});
}
}
