// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
bool Studio::startProjectArchive(const File& file,bool importing,uint64_t expected){
    if(archiveBusy){status="A project archive is already running. Use Cancel project archive.";return false;}
    if(recording||recorder.isActive()||pendingNoteTool()||revision!=expected){status="Project changed, a take is active or a note preview is pending. Reopen the archive command.";return false;}
    if(archiveThread.joinable())archiveThread.join();archiveImport=importing;archiveRevision=revision;archiveCancel.store(false);archiveDone.store(false);archiveSucceeded=false;archiveImportedFile=File();archiveBusy=true;auto source=project;auto base=currentFile==File()?root:currentFile.getParentDirectory();auto destination=root.getChildFile("Projects/Imported").getChildFile("Song_"+Uuid().toString());status=importing?"Importing project ZIP in the background. Cancel project archive stops it.":"Packaging a project snapshot with its samples. You can keep editing; Cancel project archive stops it.";
    archiveThread=std::thread([this,file,importing,source=std::move(source),base,destination]{auto result=importing?importProjectArchive(file,destination,archiveImportedFile,&archiveCancel):exportProjectArchive(source,base,file,&archiveCancel);archiveSucceeded=result.wasOk();archiveMessage=result.wasOk()?(importing?"Imported project: "+archiveImportedFile.getFullPathName():"Project ZIP saved: "+file.getFullPathName()):result.getErrorMessage();archiveDone.store(true,std::memory_order_release);});repaint();return true;
}
bool Studio::applyImportedArchive(const File& file,uint64_t expected){
    if(revision!=expected||recording||recorder.isActive()||pendingNoteTool()){status="Imported files retained at "+file.getFullPathName()+". Current session changed; use File > Open to open the imported project.";return false;}
    Project next;auto result=loadProject(file,next);if(result.failed()){status=result.getErrorMessage();return false;}
    // Confirm playback capacity before replacing the open session.
    {Assets local;auto compiled=std::make_unique<Playback>();String warnings;result=compilePlayback(next,local,file.getParentDirectory(),*compiled,warnings);if(result.failed()||warnings.isNotEmpty()){status="Imported files retained; cannot open playback: "+(result.failed()?result.getErrorMessage():warnings);return false;}}
    auto backup=root.getChildFile("data/History").getChildFile("Before_archive_"+Uuid().toString()+".pulse");result=saveProject(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),backup);if(result.failed()){status="Imported files retained; current session backup failed: "+result.getErrorMessage();return false;}
    synchronizeAutosave();cancelNoteTool(false);releaseKeys();checkpoint();engine->preview(nullptr);engine->stop();project=std::move(next);currentFile=file;selectedTrack=0;trackScroll=0;selectedNote=-1;selectedNotes.clear();selectedClip=-1;selectedPoint=-1;clipboardHasClip=false;pianoLow=-1;pianoStart=pianoNoteCursor=0;pianoSpan=jmin(8.,project.patterns[(size_t)project.activePattern].length);playlistStart=editCursorBeat=0;changed();dirty=false;recoveryRestored=false;rememberProject(file);retireRecovery();assetRevealMissing=audioEnabled;status="Opened imported project with all samples. Previous session backed up in History; Undo restores it.";return true;
}
void Studio::finishProjectArchive(){
    if(!archiveBusy||!archiveDone.load(std::memory_order_acquire))return;if(archiveThread.joinable())archiveThread.join();archiveBusy=false;status=archiveMessage;if(archiveImport&&archiveSucceeded)applyImportedArchive(archiveImportedFile,archiveRevision);repaint();
}
void Studio::chooseProjectArchive(bool importing){
    if(archiveBusy){status="A project archive is running. Wait or use Cancel project archive.";return;}if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and apply/cancel note previews before using project archives.";return;}
    auto start=root.getChildFile("Projects");if(!importing)start=start.getChildFile(project.title.replaceCharacters("/\\:*?\"<>|","_________")+".zip");archiveChooser=std::make_unique<FileChooser>(importing?"Import Pulse Studio project ZIP":"Export project ZIP with all samples",start,"*.zip",true);auto safe=Component::SafePointer<Studio>(this);auto expected=revision;int flags=(importing?FileBrowserComponent::openMode:FileBrowserComponent::saveMode|FileBrowserComponent::warnAboutOverwriting)|FileBrowserComponent::canSelectFiles;
    archiveChooser->launchAsync(flags,[safe,importing,expected](const FileChooser& chooser){auto file=chooser.getResult();if(safe&&file!=File())safe->startProjectArchive(importing?file:file.withFileExtension("zip"),importing,expected);});
}
}
