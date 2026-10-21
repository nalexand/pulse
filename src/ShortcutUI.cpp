// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
String shortcutForKey(const KeyPress& key){
    auto mods=key.getModifiers();int code=key.getKeyCode();String name;
    for(int f=1;f<=12;++f)if(code==KeyPress::F1Key+f-1)name="F"+String(f);
    const std::pair<int,const char*> names[]={{KeyPress::spaceKey,"Space"},{KeyPress::homeKey,"Home"},{KeyPress::deleteKey,"Delete"},{KeyPress::backspaceKey,"Backspace"},{KeyPress::tabKey,"Tab"},{KeyPress::returnKey,"Enter"},{KeyPress::upKey,"Up"},{KeyPress::downKey,"Down"},{KeyPress::leftKey,"Left"},{KeyPress::rightKey,"Right"}};
    for(auto [value,label]:names)if(code==value)name=label;
    if(code>='a'&&code<='z')code-=32;
    if((code>='A'&&code<='Z')||(code>='0'&&code<='9')||code=='['||code==']')name=String::charToString((juce_wchar)code);
    if(name.isEmpty())return {};return (mods.isCommandDown()?String("Ctrl+"):String())+(mods.isAltDown()?String("Alt+"):String())+(mods.isShiftDown()?String("Shift+"):String())+name;
}
namespace {
class ShortcutCapture final:public TextEditor {
public:
    std::function<void(String)> captured;
    ShortcutCapture(){setReadOnly(true);setTextToShowWhenEmpty("Click here, then press a shortcut",Colours::grey);setComponentID("shortcut-capture");}
    bool keyPressed(const KeyPress& key)override{if(key.getKeyCode()==KeyPress::escapeKey)return false;auto chord=shortcutForKey(key);if(captured)captured(chord);return true;}
};
class ShortcutPanel final:public Component {
public:
    ShortcutSettings draft;std::function<Result(const ShortcutSettings&)> apply;
    ShortcutPanel(const ShortcutSettings& source,const File& workspace):draft(source),exchangeRoot(workspace){
        setSize(610,365);setComponentID("shortcut-panel");
        filter.setComponentID("shortcut-filter");filter.setTextToShowWhenEmpty("Find a command or current key...",Colours::grey);filter.onTextChange=[this]{refresh();};
        commands.setComponentID("shortcut-command");commands.onChange=[this]{update();};
        current.setComponentID("shortcut-current");message.setComponentID("shortcut-message");message.setJustificationType(Justification::topLeft);
        capture.captured=[this](String key){pending=key;capture.setText(key,dontSendNotification);String canonical;auto result=canonicalShortcut(key,canonical);message.setText(key.isEmpty()?"That key cannot be assigned.":result.failed()?result.getErrorMessage():"Press Set binding to stage this key, then Apply changes.",dontSendNotification);};
        set.setButtonText("Set binding");clear.setButtonText("Disable binding");oneDefault.setButtonText("Command default");allDefault.setButtonText("All defaults");commit.setButtonText("Apply changes");importFile.setButtonText("Import bindings...");exportFile.setButtonText("Export draft...");
        set.setComponentID("shortcut-set");clear.setComponentID("shortcut-disable");oneDefault.setComponentID("shortcut-default");allDefault.setComponentID("shortcut-reset");commit.setComponentID("shortcut-apply");importFile.setComponentID("shortcut-import");exportFile.setComponentID("shortcut-export");
        importFile.onClick=[this]{exchange(true);};exportFile.onClick=[this]{exchange(false);};
        set.onClick=[this]{if(pending.isEmpty()){message.setText("Click the key field and press a shortcut first.",dontSendNotification);return;}stage(pending);};
        clear.onClick=[this]{stage({});};oneDefault.onClick=[this]{if(auto* command=selected())stage(command->defaultKey);};
        allDefault.onClick=[this]{draft={};update();message.setText("Factory defaults staged. Apply changes to save, or Close to discard.",dontSendNotification);};
        commit.onClick=[this]{if(!apply)return;auto result=apply(draft);message.setText(result.wasOk()?"Keyboard shortcuts saved. Close this window to use them.":result.getErrorMessage(),dontSendNotification);};
        for(Component* child:std::initializer_list<Component*>{&filter,&commands,&current,&capture,&set,&clear,&oneDefault,&allDefault,&commit,&message,&importFile,&exportFile})addAndMakeVisible(child);refresh();
    }
    void exchange(bool loading){
        if(chooser)return;auto captured=draft;auto self=Component::SafePointer<ShortcutPanel>(this);
        chooser=std::make_unique<FileChooser>(loading?"Import keyboard shortcuts":"Export keyboard shortcut draft",exchangeRoot.getChildFile("Keyboard.pulseshortcuts"),"*.pulseshortcuts");
        chooser->launchAsync((loading?FileBrowserComponent::openMode:FileBrowserComponent::saveMode|FileBrowserComponent::warnAboutOverwriting)|FileBrowserComponent::canSelectFiles,[self,loading,captured](const FileChooser& browser){
            if(!self)return;auto file=browser.getResult();if(file==File()){self->chooser.reset();return;}auto& panel=*self;
            if(!file.hasFileExtension("pulseshortcuts")){panel.message.setText("Choose a file ending in .pulseshortcuts.",dontSendNotification);panel.chooser.reset();return;}
            if(loading){ShortcutSettings next;auto result=loadShortcuts(file,next);if(result.failed())panel.message.setText(result.getErrorMessage(),dontSendNotification);else if(!file.existsAsFile())panel.message.setText("Shortcut import file is missing.",dontSendNotification);else{panel.draft=std::move(next);panel.refresh();panel.message.setText("Imported bindings replace the draft. Apply changes to save, or Close to discard.",dontSendNotification);}}
            else{auto result=saveShortcuts(file,captured);panel.message.setText(result.wasOk()?"Exported the draft; active shortcuts change only after Apply.":result.getErrorMessage(),dontSendNotification);}
            panel.chooser.reset();
        });
    }
    const ShortcutCommand* selected()const{int index=commands.getSelectedId()-1;return index>=0&&index<(int)shortcutCommands().size()?&shortcutCommands()[(size_t)index]:nullptr;}
    void refresh(){auto id=commands.getSelectedId();commands.clear(dontSendNotification);auto tokens=StringArray::fromTokens(filter.getText().toLowerCase()," \t","");tokens.removeEmptyStrings();for(int i=0;i<(int)shortcutCommands().size();++i){auto& command=shortcutCommands()[(size_t)i];auto text=command.group+" / "+command.name;auto all=(text+" "+shortcutBinding(draft,command.id)).toLowerCase();bool match=true;for(auto& token:tokens)match&=all.contains(token);if(match)commands.addItem(text,i+1);}if(commands.indexOfItemId(id)<0)id=commands.getNumItems()?commands.getItemId(0):0;commands.setSelectedId(id,dontSendNotification);update();}
    void update(){auto* command=selected();pending.clear();capture.clear();bool editable=command&&!command->locked;set.setEnabled(editable);clear.setEnabled(editable);oneDefault.setEnabled(editable);capture.setEnabled(editable);current.setText(command?"Current: "+(shortcutBinding(draft,command->id).isEmpty()?String("Disabled"):shortcutBinding(draft,command->id))+"    |    Factory: "+(command->defaultKey.isEmpty()?String("Unassigned"):command->defaultKey):"No matching commands",dontSendNotification);message.setText(command?(command->locked?String("Ctrl+Shift+H always recovers offscreen windows. Escape always closes/cancels and stops a take."):command->typingOff?String("This command runs only when typing-to-piano is OFF. Changes are staged until Apply."):String("Changes are staged until Apply. Conflicting keys are rejected; disable the existing binding first.")):String(),dontSendNotification);}
    void stage(const String& key){if(auto* command=selected()){auto result=setShortcut(draft,command->id,key);if(result.failed()){message.setText(result.getErrorMessage(),dontSendNotification);return;}update();message.setText("Binding staged. Apply changes to save, or Close to discard.",dontSendNotification);}}
    void resized()override{filter.setBounds(0,0,getWidth(),28);commands.setBounds(0,38,getWidth(),30);current.setBounds(0,73,getWidth(),38);capture.setBounds(0,119,getWidth(),32);set.setBounds(0,162,135,28);clear.setBounds(144,162,140,28);oneDefault.setBounds(293,162,160,28);allDefault.setBounds(462,162,148,28);message.setBounds(0,205,getWidth(),63);commit.setBounds(0,278,160,30);importFile.setBounds(176,278,170,30);exportFile.setBounds(362,278,170,30);}
    TextEditor filter;ComboBox commands;Label current,message;ShortcutCapture capture;TextButton set,clear,oneDefault,allDefault,commit,importFile,exportFile;String pending;File exchangeRoot;std::unique_ptr<FileChooser> chooser;
};
class ShortcutDialog final:public AlertWindow {
public:
    ShortcutPanel panel;
    ShortcutDialog(Studio* owner,const ShortcutSettings& settings,const File& workspace):AlertWindow("Keyboard shortcuts","Local workspace preferences. Piano input and text fields keep their own keys.",MessageBoxIconType::NoIcon,owner),panel(settings,workspace){addCustomComponent(&panel);addButton("Close",0,KeyPress(KeyPress::escapeKey));}
};
}
void Studio::restoreShortcutSettings(){auto next=shortcutSettings;auto result=loadShortcuts(root.getChildFile("data/shortcuts.json"),next);if(result.failed()){status=result.getErrorMessage();return;}shortcutSettings=std::move(next);++shortcutRevision;}
bool Studio::applyShortcutSettings(const ShortcutSettings& next,uint64_t sourceRevision,const String& token){
    if(recording||recorder.isActive()){status="Finish the take before changing shortcuts.";return false;}
    auto file=root.getChildFile("data/shortcuts.json");if(sourceRevision!=shortcutRevision||shortcutFileToken(file)!=token){status="Shortcut preferences changed. Close and reopen Keyboard shortcuts.";return false;}
    auto result=saveShortcuts(file,next);if(result.failed()){status=result.getErrorMessage();return false;}releaseKeys();shortcutSettings=next;++shortcutRevision;status="Keyboard shortcuts saved locally.";return true;
}
void Studio::showShortcutSettings(){
    if(recording||recorder.isActive()){status="Finish the take before changing shortcuts.";return;}restoreShortcutSettings();releaseKeys();
    auto* dialog=new ShortcutDialog(this,shortcutSettings,root);auto safe=Component::SafePointer<Studio>(this);auto sourceRevision=shortcutRevision;auto token=shortcutFileToken(root.getChildFile("data/shortcuts.json"));
    dialog->panel.apply=[safe,sourceRevision,token](const ShortcutSettings& next)mutable{if(!safe)return Result::fail("The workspace closed.");if(!safe->applyShortcutSettings(next,sourceRevision,token))return Result::fail(safe->status);sourceRevision=safe->shortcutRevision;token=shortcutFileToken(safe->root.getChildFile("data/shortcuts.json"));return Result::ok();};dialog->enterModalState(true,nullptr,true);
}
String Studio::shortcutHint(const String& id)const{return shortcutBinding(shortcutSettings,id);}
String Studio::shortcutLabel(const String& text,const String& id)const{auto hint=shortcutHint(id);return text+(hint.isEmpty()?String():"   "+hint);}
String Studio::commandShortcutHint(int action,int data,const String& fallback)const{
    const std::pair<int,const char*> mapping[]={{play,"transport.pause"},{stop,"transport.toggle"},{mode,"transport.mode"},{record,"transport.record"},{metro,"transport.metro"},{open,"project.open"},{saveAction,"project.save"},{saveVersion,"project.version"},{versionSaveNamed,"project.namedVersion"},{versionsOpen,"project.versions"},{exportAudio,"project.export"},{exportMidiAction,"project.exportMidi"},{notesAction,"project.notes"},{editHistoryOpen,"edit.history"},{undoAction,"edit.undo"},{redoAction,"edit.redo"},{copySelection,"edit.copy"},{cutSelection,"edit.cut"},{pasteSelection,"edit.paste"},{notePasteOriginal,"edit.pasteOriginal"},{duplicateSelection,"edit.duplicate"},{splitClip,"edit.split"},{patternAdd,"pattern.add"},{patternRename,"pattern.rename"},{quantize,"notes.quantize"},{humanize,"notes.humanize"},{euclideanOpen,"notes.euclidean"},{arpeggiatorOpen,"notes.arpeggio"},{quickQuantize,"notes.quickQuantize"},{transposeUp,"notes.up"},{transposeDown,"notes.down"},{browserToggle,"window.browser"},{settings,"window.audio"},{layoutReset,"window.recover"},{commandSearch,"command.search"},{help,"help.open"},{shortcutsOpen,"shortcuts.open"},{markerAdd,"marker.add"},{sectionAdd,"marker.section"},{markerPrevious,"marker.previous"},{markerNext,"marker.next"},{typingToggle,"typing.toggle"},{typingSustainToggle,"typing.sustain"}};
    for(auto [value,id]:mapping)if(value==action)return shortcutHint(id);
    if(action==paletteWindow){if(data==playlistPane)return shortcutHint("window.playlist");if(data==rackPane)return shortcutHint("window.rack");if(data==pianoPane)return shortcutHint("window.piano");if(data==mixerPane)return shortcutHint("window.mixer");}
    return fallback;
}
bool Studio::dispatchShortcut(const String& id){
    if(id.isEmpty())return false;
    if(recording&&id!="typing.sustain"&&id!="transport.toggle"&&id!="transport.pause")return true;
    const std::pair<const char*,int> actions[]={{"transport.pause",play},{"transport.mode",mode},{"transport.record",record},{"transport.metro",metro},{"project.open",open},{"project.save",saveAction},{"project.version",saveVersion},{"project.namedVersion",versionSaveNamed},{"project.versions",versionsOpen},{"project.export",exportAudio},{"project.exportMidi",exportMidiAction},{"project.notes",notesAction},{"edit.history",editHistoryOpen},{"edit.undo",undoAction},{"edit.redo",redoAction},{"edit.redoAlternate",redoAction},{"edit.copy",copySelection},{"edit.cut",cutSelection},{"edit.paste",pasteSelection},{"edit.duplicate",duplicateSelection},{"edit.split",splitClip},{"pattern.add",patternAdd},{"notes.quantize",quantize},{"notes.humanize",humanize},{"notes.euclidean",euclideanOpen},{"notes.arpeggio",arpeggiatorOpen},{"notes.quickQuantize",quickQuantize},{"notes.up",transposeUp},{"notes.down",transposeDown},{"window.browser",browserToggle},{"window.audio",settings},{"window.recover",layoutReset},{"command.search",commandSearch},{"help.open",help},{"shortcuts.open",shortcutsOpen},{"marker.add",markerAdd},{"marker.section",sectionAdd},{"marker.previous",markerPrevious},{"marker.next",markerNext},{"typing.toggle",typingToggle},{"typing.sustain",typingSustainToggle}};
    for(auto [name,action]:actions)if(id==name){perform(action);return true;}
    if(id=="transport.toggle"){perform(engine->playing.load()||recording?stop:play);return true;}
    if(id=="transport.rewind"){engine->seekRequested.store(0);return true;}
    if(id=="transport.panic"){releaseKeys();engine->resetRequested.store(true);return true;}
    if(id=="project.saveAs"){chooseFile(saveAction);return true;}
    if(id=="edit.undoToggle"){if(activePane==wavePane)perform(waveUndo);else applyHistory(!redoStack.empty());return true;}
    if(id=="edit.pasteOriginal"){perform(activePane==wavePane?waveReplace:activePane==automationPane?pasteSelection:notePasteOriginal);return true;}
    if(id=="edit.selectAll"){if(activePane==playlistPane)perform(playlistAll);else if(activePane==wavePane)perform(waveAll);else if(activePane==pianoPane){selectedNotes.clear();for(int i=0;i<(int)lane().size();++i)selectedNotes.insert(i);selectedNote=selectedNotes.empty()?-1:*selectedNotes.begin();repaint();}return true;}
    if(id=="edit.deselect"){if(activePane==playlistPane)perform(playlistNone);else if(activePane==pianoPane){selectedNotes.clear();selectedNote=-1;repaint();}return true;}
    if(id=="edit.delete"){deleteSelection();return true;}if(id=="edit.snap"){snap=snap>0?0:.25;repaint();return true;}
    if(id=="pattern.rename"){if(activePane==mixerPane)editName(false);else renamePattern();return true;}
    const char* tools[]={"tools.draw","tools.paint","tools.delete","tools.select"};for(int i=0;i<4;++i)if(id==tools[i]){perform(toolSelect,i);return true;}
    const std::pair<const char*,int> windows[]={{"window.playlist",playlistPane},{"window.rack",rackPane},{"window.piano",pianoPane},{"window.mixer",mixerPane}};for(auto [name,pane]:windows)if(id==name){showPane(pane,true);return true;}
    if(id=="window.hideAll"){cancelNoteTool();releaseKeys();for(auto& pane:panes)pane.visible=false;saveLayout();repaint();return true;}
    if(id=="window.cycle"){auto start=std::find(paneOrder.begin(),paneOrder.end(),activePane);int index=start==paneOrder.end()?0:(int)(start-paneOrder.begin());for(int n=1;n<=paneCount;++n){int pane=paneOrder[(size_t)((index+n)%paneCount)];if(panes[(size_t)pane].visible){showPane(pane);break;}}return true;}
    if(id=="window.maximize"){if(activePane==playlistPane)perform(paneMaximize,playlistPane);return true;}
    if(id=="track.previous"||id=="track.next"){perform(trackSelect,jlimit(0,(int)project.tracks.size()-1,selectedTrack+(id=="track.next"?1:-1)));repaint();return true;}
    if(id=="typing.velocityUp"||id=="typing.velocityDown"){perform(typingVelocitySet,jlimit(1,127,typingVelocity+(id=="typing.velocityUp"?8:-8)));return true;}
    if(id=="typing.transposeUp"||id=="typing.transposeDown"){perform(typingTransposeSet,jlimit(-12,12,typingTranspose+(id=="typing.transposeUp"?1:-1)));return true;}
    if(id.startsWith("typing.octave")){perform(typingOctaveSet,jlimit(1,7,octave+(id.contains("Up")?1:-1)));return true;}
    return false;
}
}
