// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
void Studio::runAppearanceChecks(const File& folder,const std::function<void(bool,const String&)>& check){
    cancelNoteTool();closeCommandPalette();releaseKeys();recording=false;audioRecordMode=false;engine->stop();project=demoProject();currentFile=File();selectedTrack=7;selectedClip=-1;undoStack.clear();redoStack.clear();dirty=false;publish();
    auto before=projectToJson(project);auto id=project.patterns[0].editorId;auto rev=revision;auto notes=project.patterns[0].lanes;
    perform(patternColourSet,8);check(project.patterns[0].colour==8&&project.patterns[0].lanes==notes&&project.patterns[0].editorId==id&&undoStack.size()==1,"native colour action preserves notes and identity as one undo");
    auto coloured=JSON::toString(projectToJson(project));perform(undoAction);check(JSON::toString(projectToJson(project))==JSON::toString(before),"one undo restores the previous pattern colour and project");perform(redoAction);check(JSON::toString(projectToJson(project))==coloured,"redo restores the complete colour change");
    auto count=undoStack.size();perform(patternColourSet,8);check(undoStack.size()==count,"selecting the existing colour does not create an undo entry");
    check(!applyPatternColour(id,rev,3)&&project.patterns[0].colour==8,"stale palette revision cannot recolour a newer project");
    check(!applyPatternColour("absent-pattern",revision,3)&&!applyPatternColour(id,revision,13)&&project.patterns[0].colour==8,"missing identities and out-of-range palette choices preserve state");
    recording=true;perform(patternColourSet,4);check(project.patterns[0].colour==8&&undoStack.size()==count,"recording blocks pattern colour changes");recording=false;arpOptions={};openNoteTools(NoteToolMode::arpeggiator);perform(patternColourSet,4);check(noteToolCurrent()&&project.patterns[0].colour==8,"pattern colours preserve an unapplied note-tool preview");cancelNoteTool();
    dispatchMenuResult(menuPatternColour+4);check(project.patterns[0].colour==4,"pattern palette menu IDs dispatch the selected colour");
    perform(patternDuplicate);check(project.patterns.back().colour==4,"native Clone pattern carries its source colour");perform(undoAction);project.activePattern=0;publish();
    auto file=folder.getChildFile("native-pattern-colours.pulse");Project restored;check(saveProject(project,file).wasOk()&&loadProject(file,restored).wasOk()&&restored.patterns[0].colour==4,"native colour edits persist in schema 14");
    auto registry=paletteEntries();check(std::count_if(registry.begin(),registry.end(),[&](const PaletteEntry& e){return e.action==patternColourSet;})==patternColourCount&&std::any_of(registry.begin(),registry.end(),[&](const PaletteEntry& e){return e.action==metronomeSettingsOpen;}),"command search exposes all colours and metronome settings");
    perform(patternColourMenu);check(Component::getCurrentlyModalComponent()!=nullptr,"native pattern-colour menu opens");PopupMenu::dismissAllActiveMenus();
    // Distinct colours on the familiar Playlist, rack, Browser and Piano roll.
    project.patterns[1].colour=8;project.patterns[2].colour=10;project.patterns[3].colour=6;resetLayout();showPane(playlistPane,true);snapshot(folder.getChildFile("pattern-colours.png"));
    auto song=JSON::toString(projectToJson(project));count=undoStack.size();auto dirtyBefore=dirty;auto revisionBefore=revision;
    MetronomeSettings settings{2,65,false};check(applyMetronomeSettings(settings,metronomeRevision)&&metronomeSettings==settings,"native metronome preferences apply the complete settings");check(JSON::toString(projectToJson(project))==song&&undoStack.size()==count&&dirty==dirtyBefore&&revision==revisionBefore,"metronome preferences preserve project data, dirty state, revision and undo history");
    MetronomeSettings local;check(loadMetronome(root.getChildFile("data/metronome.json"),local).wasOk()&&local==settings,"metronome settings are saved locally outside the song");
    auto oldRevision=metronomeRevision;check(!applyMetronomeSettings({1,100,true},oldRevision-1)&&metronomeSettings==settings,"stale metronome confirmation cannot overwrite newer preferences");
    check(!applyMetronomeSettings({1,201,true},metronomeRevision)&&metronomeSettings==settings,"invalid metronome levels fail without changing preferences");
    recording=true;check(!applyMetronomeSettings({1,100,true},metronomeRevision)&&metronomeSettings==settings,"recording locks metronome configuration");perform(metronomeSettingsOpen);check(Component::getCurrentlyModalComponent()==nullptr,"recording also blocks opening the metronome dialog");recording=false;
    metronomeSettings={};restoreMetronomeSettings();check(metronomeSettings==settings,"saved metronome settings restore on the UI and audio engine");
    root.getChildFile("data/metronome.json").replaceWithText("{\"schema\":1,\"sound\":99}");restoreMetronomeSettings();check(metronomeSettings==settings,"corrupt local settings do not reset current metronome controls");saveMetronome(root.getChildFile("data/metronome.json"),settings);
    perform(metronomeSettingsOpen);auto* dialog=dynamic_cast<AlertWindow*>(Component::getCurrentlyModalComponent());check(dialog&&dialog->getComboBoxComponent("sound")->getSelectedItemIndex()==2&&dialog->getTextEditorContents("level")=="65"&&dialog->getComboBoxComponent("accent")->getSelectedItemIndex()==0,"metronome dialog reflects the actual sound, level and accent preferences");
    if(dialog){PNGImageFormat png;FileOutputStream stream(folder.getChildFile("metronome-settings.png"));if(stream.openedOk())png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),stream);dialog->exitModalState(0);}check(metronomeSettings==settings,"Cancel keeps metronome preferences unchanged");
    project=emptyProject();project.bpm=120;project.metronome=true;project.loop=false;engine->prepare(48000);publish();engine->setMetronomeSettings({3,50,false});engine->start();AudioBuffer<float> audio(2,30000);getNextAudioBlock(AudioSourceChannelInfo(&audio,0,30000));check(audio.getMagnitude(0,0,1152)>.001f&&audio.getMagnitude(0,24000,1152)>.001f,"configured rim-click metronome reaches the actual native audio callback");
    perform(stop);engine->setMetronomeSettings({1,0,true});engine->start();getNextAudioBlock(AudioSourceChannelInfo(&audio,0,30000));check(audio.getMagnitude(0,0,30000)==0&&engine->position.load()>0,"zero click level is silent through the native callback while transport advances");
    perform(stop);metronomeSettings={};engine->setMetronomeSettings({});saveMetronome(root.getChildFile("data/metronome.json"),{});undoStack.clear();redoStack.clear();dirty=false;
}
}
