// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
namespace {
class VersionComparison final:public AlertWindow {
public:
    VersionComparison(const String& name,const String& summary,Component* parent):AlertWindow("Compare project version","Selected: "+name.substring(0,110)+"\nRestore replaces the working song with this snapshot. A local backup and Undo preserve your current session; saved version files remain intact.",MessageBoxIconType::NoIcon,parent){
        comparison.setComponentID("version-comparison");comparison.setMultiLine(true,true);comparison.setReadOnly(true);comparison.setScrollbarsShown(true);comparison.setCaretVisible(false);comparison.setText(summary,false);comparison.setSize(520,235);addCustomComponent(&comparison);addButton("Restore version",1);addButton("Back to versions",2);addButton("Close",0,KeyPress(KeyPress::escapeKey));
    }
private:TextEditor comparison;
};
}
bool Studio::startVersionSave(const String& label){
    if(versionBusy||recording||recorder.isActive()||pendingNoteTool()){status="Finish the take/note preview and wait for the current version task.";return false;}if(versionThread.joinable())versionThread.join();versionTask=2;versionBusy=true;versionShowResult=false;versionCancel.store(false);versionDone.store(false);versionSucceeded=false;versionSavedFile=File();auto source=project;auto base=currentFile==File()?root:currentFile.getParentDirectory();auto library=root.getChildFile("data/Versions");status="Saving a project version snapshot and collecting its samples in the background...";
    versionThread=std::thread([this,source=std::move(source),base,library,label]{auto result=saveProjectVersion(source,base,library,label,versionSavedFile,&versionCancel);versionSucceeded=result.wasOk();versionMessage=result.wasOk()?"Version snapshot saved: "+versionSavedFile.getFileName()+". Working song and existing versions retained.":result.getErrorMessage();versionDone.store(true,std::memory_order_release);});repaint();return true;
}
bool Studio::startVersionScan(bool showResult){
    if(versionBusy||recording||recorder.isActive()||pendingNoteTool()){status="Finish the take/note preview and wait for the current version task.";return false;}if(versionThread.joinable())versionThread.join();versionTask=1;versionBusy=true;versionShowResult=showResult;versionRevision=revision;versionCancel.store(false);versionDone.store(false);auto library=root.getChildFile("data/Versions");auto query=versionQuery;status="Reading local project versions...";
    versionThread=std::thread([this,library,query]{versionWorkerRows=scanProjectVersions(library,query,versionMessage,&versionCancel);versionSucceeded=!versionCancel.load();versionDone.store(true,std::memory_order_release);});repaint();return true;
}
bool Studio::startVersionRead(const ProjectVersion& row,bool showResult){
    if(versionBusy||recording||recorder.isActive()||pendingNoteTool()){status="Finish the take/note preview and wait for the current version task.";return false;}if(row.file.getParentDirectory()!=root.getChildFile("data/Versions")){status="Choose a snapshot from this workspace's version library.";return false;}if(versionThread.joinable())versionThread.join();versionTask=3;versionBusy=true;versionReview.reset();versionWorkerReview.reset();versionShowResult=showResult;versionRevision=revision;versionCancel.store(false);versionDone.store(false);auto current=project;auto base=currentFile==File()?root:currentFile.getParentDirectory();auto expected=revision;status="Reading version and comparing project data...";
    versionThread=std::thread([this,row,current=std::move(current),base,expected]{auto review=std::make_shared<VersionReview>();review->entry=row;review->revision=expected;auto result=readProjectVersion(row,review->project,review->token);if(result.wasOk()&&!versionCancel.load())review->comparison=compareProjectVersion(current,base,review->project,row.file.getParentDirectory());versionSucceeded=result.wasOk()&&!versionCancel.load();versionMessage=versionCancel.load()?"Version comparison cancelled.":result.failed()?result.getErrorMessage():"Version ready to compare: "+row.name;versionWorkerReview=versionSucceeded?review:nullptr;versionDone.store(true,std::memory_order_release);});repaint();return true;
}
void Studio::finishVersionTask(){
    bool completed=false;
    if(versionBusy){if(!versionDone.load(std::memory_order_acquire))return;if(versionThread.joinable())versionThread.join();versionBusy=false;completed=true;status=versionTask==2&&versionSucceeded?versionMessage:versionCancel.load()?"Version task cancelled. Existing snapshots retained.":versionMessage;
        if(versionSucceeded&&!versionCancel.load()){if(versionTask==1){versionRows=std::move(versionWorkerRows);++versionMenuSerial;}else if(versionTask==3)versionReview=std::move(versionWorkerReview);}else versionShowResult=false;
    }
    // A scan can finish while its launching popup/dialog is still closing.
    // Keep the result pending until the UI can present it, rather than losing it.
    if(!versionBusy&&versionShowResult&&(versionTask==1||versionTask==3)){
        if(versionCancel.load()||revision!=versionRevision||recording||recorder.isActive()||pendingNoteTool()){versionShowResult=false;status="Version result retained. Current session changed or is busy; reopen Project version history.";}
        else if(!Component::getCurrentlyModalComponent()){versionShowResult=false;if(versionTask==1)showVersionMenu();else showVersionComparison(versionReview);}
    }
    if(completed)repaint();
}
void Studio::showVersionMenu(){
    if(recording||recorder.isActive()||pendingNoteTool()||versionBusy){status="Finish the current operation before browsing versions.";return;}PopupMenu menu;menu.addSectionHeader("Project versions"+(versionQuery.isEmpty()?String():" / "+versionQuery));menu.addItem(1,"Refresh version list");menu.addItem(2,"Find versions...");menu.addItem(3,"Clear search",versionQuery.isNotEmpty());menu.addItem(4,"Save named version...");menu.addSeparator();
    for(int page=0;page<(int)versionRows.size();page+=24){PopupMenu group;int end=jmin(page+24,(int)versionRows.size());for(int i=page;i<end;++i){const auto& row=versionRows[(size_t)i];group.addItem(100+i,row.modified.formatted("%Y-%m-%d %H:%M:%S")+" | "+row.name.substring(0,115)+" ("+File::descriptionOfSizeInBytes(row.bytes)+")");}menu.addSubMenu(String(page+1)+" - "+String(end)+" / newest first",group);}if(versionRows.empty())menu.addItem(99,"No matching saved versions",false);
    auto safe=Component::SafePointer<Studio>(this);auto serial=versionMenuSerial,expected=revision;menu.showMenuAsync(menuOptions(menuOpen,0),[safe,serial,expected](int choice){if(!safe||!choice)return;if(serial!=safe->versionMenuSerial||expected!=safe->revision){safe->status="Version list or project changed. Reopen Project version history.";return;}if(choice==1)safe->startVersionScan();else if(choice==2)safe->findVersionDialog();else if(choice==3){safe->versionQuery.clear();safe->startVersionScan();}else if(choice==4)safe->nameVersionDialog();else if(choice>=100&&choice<100+(int)safe->versionRows.size())safe->startVersionRead(safe->versionRows[(size_t)(choice-100)]);});
}
void Studio::findVersionDialog(){
    auto* dialog=new AlertWindow("Find project versions","Search snapshot file names and labels. Every word must match. This searches this workspace's data/Versions folder; it does not search recovery/History backups or other projects on disk.",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("query",versionQuery);dialog->addButton("Find",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog](int choice){if(!safe||choice!=1)return;auto query=dialog->getTextEditorContents("query").trim();if(query.length()>160||query.containsAnyOf("\r\n")){safe->status="Use a single-line version search of at most 160 characters.";return;}safe->versionQuery=query;safe->startVersionScan();}),true);
}
void Studio::nameVersionDialog(){
    if(versionBusy||recording||recorder.isActive()||pendingNoteTool()){status="Finish the take/note preview and current version task first.";return;}auto* dialog=new AlertWindow("Save named project version","Create a separate snapshot with collected samples. Add an optional label of up to 64 characters. The working file, dirty state and Undo history stay intact.",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("label","","Version label");dialog->addButton("Save version",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto expected=revision;auto safe=Component::SafePointer<Studio>(this);dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,expected](int choice){if(!safe||choice!=1)return;if(expected!=safe->revision){safe->status="Project changed. Reopen Save named version.";return;}safe->startVersionSave(dialog->getTextEditorContents("label").trim());}),true);
}
void Studio::showVersionComparison(const std::shared_ptr<VersionReview>& review){
    if(!review||recording||recorder.isActive()||pendingNoteTool()||versionBusy||review->revision!=revision){status="Project changed. Reopen Project version history to compare again.";return;}auto* dialog=new VersionComparison(review->entry.name,review->comparison,this);auto safe=Component::SafePointer<Studio>(this);dialog->enterModalState(true,ModalCallbackFunction::create([safe,review](int choice){if(!safe)return;if(choice==1)safe->restoreProjectVersion(review);else if(choice==2){if(review->revision==safe->revision)safe->showVersionMenu();else safe->startVersionScan();}}),true);
}
bool Studio::restoreProjectVersion(const std::shared_ptr<VersionReview>& review){
    if(!review||review->revision!=revision||versionBusy||recording||recorder.isActive()||pendingNoteTool()){status="Current session changed or is busy. Reopen the version comparison.";return false;}if(review->entry.file.getParentDirectory()!=root.getChildFile("data/Versions")||projectVersionToken(review->entry.file)!=review->token){status="Saved version changed or disappeared. Refresh the version list.";return false;}
    const auto& next=review->project;auto references=projectAssets(next,review->entry.file.getParentDirectory());inspectProjectAssets(references,next);for(const auto& row:references)if(row.issue!=AssetIssue::ready&&row.issue!=AssetIssue::emptySampler){status="Version samples need attention: "+row.file.getFileName()+" / "+row.detail;return false;}
    auto compiled=std::make_unique<Playback>();String warnings;auto valid=compilePlayback(next,assets,review->entry.file.getParentDirectory(),*compiled,warnings);if(valid.wasOk()){auto other=next;other.songMode=!next.songMode;valid=compilePlayback(other,assets,review->entry.file.getParentDirectory(),*compiled,warnings);}if(valid.failed()){status="Version cannot restore: "+valid.getErrorMessage();return false;}
    auto backup=root.getChildFile("data/History").getChildFile("Before_version_restore_"+Uuid().toString()+".pulse");valid=saveProject(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),backup);if(valid.failed()){status="Current session retained; version-restore backup failed: "+valid.getErrorMessage();return false;}
    synchronizeAutosave();releaseKeys();cancelNoteTool(false);checkpoint();engine->preview(nullptr);engine->stop();project=next;selectedTrack=0;trackScroll=0;selectedNote=-1;selectedNotes.clear();selectedClip=-1;selectedPoint=-1;clipboardHasClip=false;pianoLow=-1;pianoStart=pianoNoteCursor=playlistStart=editCursorBeat=0;pianoSpan=jmin(8.,project.patterns[(size_t)project.activePattern].length);lastVersionBackup=backup;changed();showPane(project.songMode?playlistPane:rackPane);status="Version restored into the working song. Save commits it; Undo restores the previous session. Original snapshot and backup retained.";return true;
}
bool Studio::performVersionAction(int action){
    if(action!=saveVersion&&(action<versionsOpen||action>versionCancelTask))return false;if(action==versionCancelTask){if(versionBusy){versionCancel.store(true);status="Cancelling version task...";}else status="No version task is running.";return true;}
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and apply/cancel the note preview before using project versions.";return true;}if(action==saveVersion)startVersionSave();else if(action==versionsOpen)startVersionScan();else if(action==versionSaveNamed)nameVersionDialog();return true;
}
}
