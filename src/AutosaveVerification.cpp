// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
struct AutosaveVerificationState {
    WaitableEvent reached,release;
    std::atomic<bool> offThread{false};
};
void Studio::releaseAutosaveVerification(){if(autosaveVerification)autosaveVerification->release.signal();}
void Studio::runAutosaveChecks(const File& folder,const std::function<void(bool,const String&)>& check){
    synchronizeAutosave();perform(stop);cancelNoteTool(false);project=emptyProject();currentFile=File();selectedTrack=7;selectedNote=-1;selectedNotes.clear();project.title="Async captured";lane()={{0,1,60,.8f}};changed();
    auto recovery=root.getChildFile("data/recovery-v0.29.pulse");auto owner=std::this_thread::get_id();auto gate=std::make_shared<AutosaveVerificationState>();ProjectSaveControl paused;paused.checkpoint=[gate,owner](ProjectSaveStage at,const File&){if(at==ProjectSaveStage::beforeCommit){gate->offThread.store(std::this_thread::get_id()!=owner);gate->reached.signal();if(!gate->release.wait(5000))return Result::fail("Test gate timed out");}return Result::ok();};
    auto captured=revision;check(startAutosave(&paused)&&gate->reached.wait(5000)&&gate->offThread.load(),"autosave performs checked persistence on a background thread");
    lane()[0].pitch=72;project.title="Async newest";changed();auto latest=revision;check(!finishAutosave(false)&&autosavedRevision!=captured&&dirty,"polling an incomplete recovery leaves unsaved edits dirty and unacknowledged");
    check(!startAutosave(),"autosave permits one in-flight snapshot and does not queue stale project copies");gate->release.signal();bool newer=finishAutosave(true);Project read;
    check(newer&&autosavedRevision==captured&&revision==latest&&dirty,"old completion acknowledges its captured revision without marking later edits saved");
    check(loadProject(recovery,read).wasOk()&&read.title=="Async captured"&&read.patterns[0].lanes[7][0].pitch==60,"background snapshot remains immutable while the live notes change");
    startAutosave();synchronizeAutosave();check(autosavedRevision==latest&&dirty&&loadProject(recovery,read).wasOk()&&read.title=="Async newest","latest followup recovery retains the permanent-save dirty flag");
    check(!startAutosave(),"already acknowledged revisions do not repeatedly rewrite recovery");
    auto checkpointCount=undoStack.size();lane()[0].pitch=67;changed();startAutosave();auto saved=folder.getChildFile("Projects/Async.pulse");commitSave(saved);
    tickAutosave();check(!dirty&&!recoveryWriter.active()&&!recovery.exists()&&!projectBackupFile(recovery).exists(),"manual save drains background work before retiring both recovery generations");
    check(currentFile==saved&&undoStack.size()==checkpointCount&&loadProject(saved,read).wasOk()&&read.patterns[0].lanes[7][0].pitch==67,"autosave does not change current file or add history entries");
    project.title="Before async open";changed();startAutosave();auto other=emptyProject();other.title="New opened song";auto otherFile=folder.getChildFile("Projects/Async-other.pulse");saveProject(other,otherFile);openProject(otherFile);tickAutosave();check(!recoveryWriter.active()&&!dirty&&!recovery.exists()&&project.title==other.title,"opening a saved project joins the prior writer before recovery cleanup");
    project.title="Close newest";changed();startAutosave();project.title="Close edited during recovery";changed();check(saveRecoveryForClose()&&!recoveryWriter.active()&&autosavedRevision==revision&&loadProject(recovery,read).wasOk()&&read.title==project.title,"close waits for the older worker then verifies the newest live revision");
    auto goodToken=projectFileToken(recovery);project.title="Failure retained live";changed();ProjectSaveControl failure;failure.checkpoint=[](ProjectSaveStage at,const File&){return at==ProjectSaveStage::beforeCommit?Result::fail("Injected async disk failure"):Result::ok();};auto acknowledged=autosavedRevision;startAutosave(&failure);synchronizeAutosave();check(autosaveError.contains("Injected async disk failure")&&dirty&&autosavedRevision==acknowledged&&projectFileToken(recovery)==goodToken,"background I/O failure reports on the UI and preserves the last acknowledged recovery");
    autosaveCounter=449;tickAutosave();synchronizeAutosave();check(autosaveError.isEmpty()&&autosavedRevision==revision&&loadProject(recovery,read).wasOk()&&read.title==project.title,"failed autosave retries at the next interval and clears its error after success");
    // The next successful completion must schedule one fresh snapshot immediately,
    // even if the normal fifteen-second interval has not elapsed.
    project.title="Coalesced old";changed();gate=std::make_shared<AutosaveVerificationState>();paused.checkpoint=[gate](ProjectSaveStage at,const File&){if(at==ProjectSaveStage::beforeCommit){gate->reached.signal();if(!gate->release.wait(5000))return Result::fail("Test gate timed out");}return Result::ok();};startAutosave(&paused);bool reached=gate->reached.wait(5000);project.title="Coalesced latest";changed();gate->release.signal();auto until=Time::getMillisecondCounterHiRes()+5000;while(autosavedRevision!=revision&&Time::getMillisecondCounterHiRes()<until){tickAutosave();Thread::sleep(1);}synchronizeAutosave();check(reached&&autosavedRevision==revision&&loadProject(recovery,read).wasOk()&&read.title=="Coalesced latest","edits during a write are coalesced into the next snapshot without waiting another autosave interval");
    project.title="Sample paths";project.tracks[7].samplePath="Samples/source.wav";changed(false);startAutosave();synchronizeAutosave();check(loadProject(recovery,read).wasOk()&&read.tracks[7].samplePath==otherFile.getParentDirectory().getChildFile("Samples/source.wav").getFullPathName(),"recovery captures absolute sample paths before moving work to its worker thread");
    project=emptyProject();currentFile=File();selectedTrack=7;selectedNote=-1;selectedNotes.clear();undoStack.clear();redoStack.clear();dirty=false;publish();retireRecovery();
}
void Studio::runAutosaveTransportTick(){
    auto put=[&](const char* key,bool value){transportResults.getDynamicObject()->setProperty(key,value);};auto recovery=root.getChildFile("data/recovery-v0.29.pulse");
    if(transportTicks==3260){
        synchronizeAutosave();perform(stop);cancelNoteTool(false);PopupMenu::dismissAllActiveMenus();project=emptyProject();currentFile=File();selectedTrack=7;selectedNote=-1;selectedNotes.clear();project.title="Native async captured";project.loop=true;lane()={{0,4,60,.8f}};changed();
        autosaveVerification=std::make_shared<AutosaveVerificationState>();auto gate=autosaveVerification;auto owner=std::this_thread::get_id();ProjectSaveControl paused;paused.checkpoint=[gate,owner](ProjectSaveStage at,const File&){if(at==ProjectSaveStage::beforeCommit){gate->offThread.store(std::this_thread::get_id()!=owner);gate->reached.signal();if(!gate->release.wait(10000))return Result::fail("Native gate timed out");}return Result::ok();};put("nativeAutosaveStart",startAutosave(&paused));transportFrames=engine->processedFrames.load();engine->observedPeak.store(0);engine->start();
    }
    if(transportTicks==3270){
        put("nativeAutosaveBackground",recoveryWriter.active()&&autosaveVerification&&autosaveVerification->offThread.load());lane()[0].pitch=67;project.title="Native async latest";changed();put("nativeAutosaveEdit",dirty&&recoveryWriter.active()&&lane()[0].pitch==67);put("nativeAutosavePlayback",engine->playing.load()&&engine->processedFrames.load()>transportFrames&&engine->observedPeak.load()>.001f);snapshot(transportReport.getSiblingFile("background-autosave.png"));releaseAutosaveVerification();
    }
    if(transportTicks==3310){
        Project read;put("nativeAutosaveLatest",!recoveryWriter.active()&&autosavedRevision==revision&&dirty&&loadProject(recovery,read).wasOk()&&read.title=="Native async latest"&&read.patterns[0].lanes[7][0].pitch==67);perform(stop);project.title="Native save barrier";changed();startAutosave();auto file=transportReport.getSiblingFile("native-async.pulse");commitSave(file);put("nativeAutosaveSaveBarrier",!recoveryWriter.active()&&!dirty&&!recovery.exists()&&!projectBackupFile(recovery).exists());project.title="Native close before";changed();startAutosave();project.title="Native close latest";changed();put("nativeAutosaveCloseBarrier",saveRecoveryForClose()&&!recoveryWriter.active()&&loadProject(recovery,read).wasOk()&&read.title=="Native close latest");
        auto other=emptyProject();other.title="Native opened after async";auto otherFile=transportReport.getSiblingFile("native-async-other.pulse");saveProject(other,otherFile);project.title="Native open pending";changed();startAutosave();openProject(otherFile);put("nativeAutosaveOpenBarrier",!recoveryWriter.active()&&!dirty&&!recovery.exists()&&project.title==other.title);autosaveVerification.reset();
    }
}
}
