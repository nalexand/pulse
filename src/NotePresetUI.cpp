// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
NotePreset Studio::captureNotePreset(const String& id,const String& name)const{NotePreset p;p.id=id;p.name=name;p.mode=noteToolOptions.mode;p.notes=noteToolOptions;p.rhythm=rhythmOptions;p.arp=arpOptions;return p;}
bool Studio::applyNotePreset(const NotePreset& p){
    if(recording){status="Finish the take before loading a note preset.";return false;}
    if(p.mode!=noteToolOptions.mode||!noteToolCurrent()){status="Use current selection in the matching note tool before loading this preset.";return false;}
    auto valid=validateNotePreset(p);if(valid.failed()){status=valid.getErrorMessage();return false;}
    if(p.mode==NoteToolMode::quantize){noteToolOptions.grid=p.notes.grid;noteToolOptions.positionStrength=p.notes.positionStrength;noteToolOptions.lengthStrength=p.notes.lengthStrength;noteToolOptions.swing=p.notes.swing;noteToolOptions.window=p.notes.window;noteToolOptions.useWindow=p.notes.useWindow;}
    else if(p.mode==NoteToolMode::humanize){noteToolOptions.timing=p.notes.timing;noteToolOptions.length=p.notes.length;noteToolOptions.velocity=p.notes.velocity;noteToolOptions.keepChords=p.notes.keepChords;noteToolOptions.seed=p.notes.seed;}
    else if(p.mode==NoteToolMode::arpeggiator){arpOptions=p.arp;pianoLow=-1;}
    else{int pitch=rhythmOptions.pitch;double start=rhythmOptions.start,end=rhythmOptions.end;rhythmOptions=p.rhythm;rhythmOptions.pitch=pitch;rhythmOptions.start=start;rhythmOptions.end=end;}
    selectedNotePreset[(size_t)p.mode]=p.id;refreshNoteToolPreview();status="Loaded "+p.name+". Preview only; Apply commits the notes.";if(!noteToolValid)status="Loaded "+p.name+". Adjust settings to resolve the preview error.";repaint();return true;
}
bool Studio::storeNotePreset(const NotePreset& p,bool replace){
    if(recording){status="Finish the take before saving a note preset.";return false;}auto result=putNotePreset(root.getChildFile("data/note-tool-presets.json"),p,replace);if(result.failed()){status=result.getErrorMessage();repaint();return false;}selectedNotePreset[(size_t)p.mode]=p.id;status="Saved local "+notePresetModeName(p.mode)+" preset: "+p.name+". Project notes are unchanged.";repaint();return true;
}
bool Studio::deleteNotePreset(const String& id){
    if(recording){status="Finish the take before deleting a note preset.";return false;}auto result=removeNotePreset(root.getChildFile("data/note-tool-presets.json"),id);if(result.failed()){status=result.getErrorMessage();return false;}for(auto& selected:selectedNotePreset)if(selected==id)selected.clear();status="Local preset deleted. Current settings and project notes are unchanged.";repaint();return true;
}
void Studio::nameNotePreset(NotePreset p,bool rename){
    auto* dialog=new AlertWindow(rename?"Rename note preset":"Save note preset",notePresetModeName(p.mode)+" settings only. Use a unique name for this tool (1-64 characters). Euclidean pitch and range stay with the target channel.",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("name",p.name);dialog->addButton(rename?"Rename":"Save",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,p,rename](int result)mutable{if(!safe||result!=1||safe->recording)return;p.name=dialog->getTextEditorContents("name").trim();safe->storeNotePreset(p,rename);}),true);
}
void Studio::showNotePresets(){
    auto mode=noteToolOptions.mode;auto factories=factoryNotePresets(mode);std::vector<NotePreset> rows;auto loaded=loadNotePresets(root.getChildFile("data/note-tool-presets.json"),rows);if(loaded.failed())status=loaded.getErrorMessage();
    std::vector<NotePreset> personal;for(auto& p:rows)if(p.mode==mode)personal.push_back(p);std::sort(personal.begin(),personal.end(),[](const NotePreset& a,const NotePreset& b){return a.name.compareIgnoreCase(b.name)<0;});
    auto found=std::find_if(personal.begin(),personal.end(),[&](const NotePreset& p){return p.id==selectedNotePreset[(size_t)mode];});bool selected=found!=personal.end();NotePreset current=selected?*found:NotePreset{};
    PopupMenu menu,factoryMenu,personalMenu;menu.addSectionHeader(notePresetModeName(mode)+" presets");for(int i=0;i<(int)factories.size();++i)factoryMenu.addItem(100+i,factories[(size_t)i].name,true,selectedNotePreset[(size_t)mode]==factories[(size_t)i].id);menu.addSubMenu("Factory",factoryMenu);
    for(int i=0;i<(int)personal.size();++i)personalMenu.addItem(1000+i,personal[(size_t)i].name,true,selectedNotePreset[(size_t)mode]==personal[(size_t)i].id);if(personal.empty())personalMenu.addItem(999,"No personal presets yet",false);menu.addSubMenu("Personal",personalMenu);
    menu.addSeparator();menu.addItem(1,"Save settings as new preset...",loaded.wasOk()&&rows.size()<maxNotePresets);menu.addItem(2,"Update selected personal preset",selected);menu.addItem(3,"Rename selected...",selected);menu.addItem(4,"Delete selected...",selected);menu.addSeparator();menu.addItem(5,"Import preset...",loaded.wasOk()&&rows.size()<maxNotePresets);menu.addItem(6,"Export selected preset...",selected);if(loaded.failed())menu.addItem(7,"Library unavailable - see status message",false);
    auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=revision;
    menu.showMenuAsync(menuOptions(notePresetsOpen),[safe,mode,factories,personal,current,selected,sourceRevision](int result){
        if(!safe||!result||safe->recording)return;if(safe->noteToolOptions.mode!=mode||safe->revision!=sourceRevision){safe->status="The note tool changed. Reopen Presets.";return;}
        if(result>=1000&&result<1000+(int)personal.size()){safe->applyNotePreset(personal[(size_t)(result-1000)]);return;}
        if(result>=100&&result<100+(int)factories.size()){safe->applyNotePreset(factories[(size_t)(result-100)]);return;}
        if(result==1){safe->nameNotePreset(safe->captureNotePreset(Uuid().toString(),""));return;}
        if(result==2&&selected){safe->storeNotePreset(safe->captureNotePreset(current.id,current.name),true);return;}
        if(result==3&&selected){safe->nameNotePreset(current,true);return;}
        if(result==4&&selected){auto* dialog=new AlertWindow("Delete note preset","Delete \""+current.name+"\" from the local library? Your current settings and project notes stay intact.",MessageBoxIconType::QuestionIcon,safe.getComponent());dialog->addButton("Delete",1);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));dialog->enterModalState(true,ModalCallbackFunction::create([safe,current](int choice){if(safe&&choice==1)safe->deleteNotePreset(current.id);}),true);return;}
        if(result==5||result==6){bool exporting=result==6;if(exporting&&!selected)return;safe->chooser=std::make_unique<FileChooser>(exporting?"Export one note preset":"Import one note preset",exporting?safe->root.getChildFile("note-preset.json"):safe->root,"*.json",true);int flags=exporting?(FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting):(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles);
            safe->chooser->launchAsync(flags,[safe,exporting,current](const FileChooser& chooser){if(!safe||safe->recording)return;auto file=chooser.getResult();if(file==File())return;if(exporting){auto result=saveNotePresets(file,{current});safe->status=result.wasOk()?"Exported note preset to "+file.getFullPathName():result.getErrorMessage();safe->repaint();return;}std::vector<NotePreset> imported;auto result=loadNotePresets(file,imported);if(result.failed()||imported.size()!=1){safe->status=result.failed()?result.getErrorMessage():"Choose a single preset exported by Pulse Studio.";safe->repaint();return;}auto p=imported.front();p.id=Uuid().toString();safe->nameNotePreset(p);});
        }
    });
}
}
