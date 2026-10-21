// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
bool Studio::startAutosave(const ProjectSaveControl* control){
    if(!dirty||revision==autosavedRevision||recoveryWriter.active())return false;
    // Only the bounded value copy and asset-path rebasing happen on the UI
    // thread. JSON encoding, validation, hashing, backup, flush and commit run
    // entirely on the writer. No worker touches Studio, engine or live assets.
    return recoveryWriter.start(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),root.getChildFile("data/recovery-v0.29.pulse"),revision,control);
}
bool Studio::finishAutosave(bool wait){
    auto completion=wait?recoveryWriter.wait():recoveryWriter.poll();
    if(!completion)return false;
    if(completion->result.failed()){
        autosaveError=completion->result.getErrorMessage();status="Autosave failed; working session retained: "+autosaveError;
        return false; // Retry at the next interval, never spin on an I/O failure.
    }
    autosavedRevision=completion->revision;autosaveError.clear();
    return dirty&&revision!=autosavedRevision;
}
void Studio::synchronizeAutosave(){finishAutosave(true);autosaveCounter=0;}
void Studio::tickAutosave(){
    bool newer=finishAutosave(false);
    if(++autosaveCounter>=450||newer){autosaveCounter=0;startAutosave();}
}
bool Studio::requiresRecoveryOnClose()const{return dirty||recording||recorder.isActive()||pendingNoteTool();}
bool Studio::saveRecoveryForClose(){
    if(pendingNoteTool()){status="Apply or cancel the note preview before closing.";showPane(noteToolsPane);return false;}
    synchronizeAutosave();finishAudioRecording();releaseKeys();engine->stop();
    auto file=root.getChildFile("data/recovery-v0.29.pulse");auto result=saveProject(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),file);
    if(result.failed()){status="Recovery save failed; session remains open: "+result.getErrorMessage();repaint();return false;}
    autosavedRevision=revision;autosaveError.clear();status="Latest session recovery was written and verified.";return true;
}
void Studio::retireRecovery(){
    synchronizeAutosave();
    auto primary=root.getChildFile("data/recovery-v0.29.pulse");
    // Preserve both generations before clearing startup recovery. Content names
    // avoid accumulating identical copies on repeated manual saves.
    for(auto file:{primary,projectBackupFile(primary)})if(file.exists()){
        auto token=projectFileToken(file);if(token=="unavailable"){status+=" Recovery cleanup postponed; unreadable copies retained.";return;}
        auto archived=root.getChildFile("data/History").getChildFile("Recovery_"+token+".pulse");auto result=archived.getParentDirectory().createDirectory();if(result.failed()){status+=" Recovery cleanup postponed; History is unavailable.";return;}
        if(!archived.exists()){TemporaryFile staging(archived);if(!file.copyFileTo(staging.getFile())||projectFileToken(staging.getFile())!=token||!staging.overwriteTargetFileWithTemporary()){status+=" Recovery cleanup postponed; copies retained.";return;}}
        if(projectFileToken(archived)!=token){status+=" Recovery cleanup postponed; History verification failed.";return;}
    }
    if(!root.getChildFile("data/recovery-v0.29.seen").replaceWithText("current")){status+=" Recovery cleanup postponed; migration marker is not writable.";return;}
    bool cleared=primary.deleteFile();cleared=projectBackupFile(primary).deleteFile()&&cleared;
    if(!cleared)status+=" Recovery cleanup failed; local copies were retained.";
}
bool Studio::restoreRecoveryFile(const File& file,bool checkpointCurrent){
    if(recording||recorder.isActive()){status="Finish the take before restoring recovery.";return false;}
    synchronizeAutosave();Project next;File used;auto result=loadProjectWithBackup(file,next,used);if(result.failed()){status=result.getErrorMessage();repaint();return false;}
    if(checkpointCurrent){auto backup=root.getChildFile("data/History").getChildFile("Before_recovery_"+Uuid().toString()+".pulse");result=saveProject(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),backup);if(result.failed()){status="Current session could not be backed up; recovery was not applied: "+result.getErrorMessage();return false;}}
    int track=selectedTrack;cancelNoteTool(false);releaseKeys();engine->preview(nullptr);engine->stop();if(checkpointCurrent)checkpoint();++revision;project=absoluteAssets(next,used.getParentDirectory());currentFile=File();selectedTrack=jlimit(0,(int)project.tracks.size()-1,track);selectedNote=-1;selectedNotes.clear();selectedClip=-1;selectedPoint=-1;trackScroll=0;pianoLow=-1;pianoStart=pianoNoteCursor=0;dirty=true;recoveryRestored=true;publish();
    bool previous=used!=file;status=previous?"Recovered the previous verified snapshot; the latest recovery was damaged. Save as a project to keep this session.":"Previous local session restored. Save to choose a permanent project file.";
    auto primary=root.getChildFile("data/recovery-v0.29.pulse");if(previous||used!=primary){result=saveProject(project,primary);if(result.wasOk()){root.getChildFile("data/recovery-v0.29.seen").replaceWithText("migrated");autosavedRevision=revision;}else status+=" Recovery refresh failed; original copies are retained: "+result.getErrorMessage();}
    repaint();return true;
}
void Studio::restoreStartupRecovery(bool resume){
    auto recovery=root.getChildFile("data/recovery-v0.29.pulse");
    if(!recovery.exists()&&!root.getChildFile("data/recovery-v0.29.seen").existsAsFile()){
        for(auto version:{"0.28","0.27","0.26","0.25","0.24","0.23","0.22","0.21","0.20","0.19","0.18","0.17","0.16","0.15","0.14","0.13","0.12","0.11","0.10","0.9","0.8","0.7","0.6","0.5","0.4","0.3"}){auto older=root.getChildFile("data/recovery-v"+String(version)+".pulse");if(older.existsAsFile()){recovery=older;break;}if(root.getChildFile("data/recovery-v"+String(version)+".seen").existsAsFile())break;if(String(version)=="0.3")recovery=root.getChildFile("data/recovery.pulse");}
    }
    if(recovery.exists()){
        if(resume){if(!restoreRecoveryFile(recovery,false)){auto problem=status;bool restored=restoreLastProject();status="Recovery files retained: "+problem+(restored?" Last saved project opened.":" Use File > Open to choose a project or .pulse.bak copy.");}}
        else{auto safe=Component::SafePointer<Studio>(this);AlertWindow::showAsync(MessageBoxOptions().withIconType(MessageBoxIconType::QuestionIcon).withTitle("Local recovery available").withMessage("A previous session has unsaved work on this computer. Restore it? If its latest snapshot is damaged, a valid previous copy will be used.").withButton("Restore session").withButton("Start demo"),[safe,recovery](int result){if(safe&&result==1)safe->restoreRecoveryFile(recovery,true);});}
    }else if(resume)restoreLastProject();
}
}
