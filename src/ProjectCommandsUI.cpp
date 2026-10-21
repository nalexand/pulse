// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include "ProjectCommands.h"
namespace pulse {
using namespace juce;
namespace {
std::shared_ptr<TextEditor> commandText(const String& text,const char* id,int height,bool readOnly){auto editor=std::make_shared<TextEditor>();editor->setComponentID(id);editor->setMultiLine(true,true);editor->setReturnKeyStartsNewLine(true);editor->setEscapeAndReturnKeysConsumed(false);editor->setReadOnly(readOnly);editor->setScrollbarsShown(true);editor->setSize(690,height);editor->setText(text);if(!readOnly)editor->setInputRestrictions(4096);return editor;}
}
void Studio::showProjectCommands(String text,String error){
    if(recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Finish the take, note preview or export before using project commands.";return;}
    closeCommandPalette();releaseKeys();if(text.isEmpty())text="set tempo to "+String(project.bpm,1)+" bpm";
    auto area=getParentMonitorArea();int width=jmax(160,jmin(690,(int)(area.getWidth()*.54))),height=jmax(45,jmin(170,(area.getHeight()-300)/2));auto editor=commandText(text,"commands",height,false);auto helpText=commandText(projectCommandHelp(),"command-help",height,true);editor->setSize(width,height);helpText->setSize(width,height);
    auto* dialog=new AlertWindow("Local project commands",error.isEmpty()?"Preview resolves every target before anything changes. Commands operate only inside this song.":error.substring(0,160),MessageBoxIconType::NoIcon,this);dialog->addCustomComponent(editor.get());dialog->addCustomComponent(helpText.get());dialog->addButton("Preview",1);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto expected=revision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,editor,helpText,expected](int result){if(safe&&result==1)safe->previewProjectCommands(editor->getText(),expected);}),true);
}
void Studio::previewProjectCommands(const String& text,uint64_t expected){
    if(expected!=revision||recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Project changed or is busy. Reopen local project commands.";return;}
    ProjectCommandDraft draft;auto valid=buildProjectCommands(project,text,draft);if(valid.wasOk()){auto playback=std::make_unique<Playback>();String warnings;for(bool song:{false,true}){auto test=draft.project;test.songMode=song;valid=compilePlayback(test,assets,currentFile==File()?root:currentFile.getParentDirectory(),*playback,warnings);if(valid.failed()||warnings.isNotEmpty()){if(valid.wasOk())valid=Result::fail(warnings);break;}}}
    if(valid.failed()){status=valid.getErrorMessage();showProjectCommands(text,status);return;}
    auto resolved=commandText(draft.resolved.joinIntoString("\n\n"),"resolved-commands",300,true);resolved->setSize(jmax(160,jmin(690,(int)(getParentMonitorArea().getWidth()*.54))),jmax(80,jmin(300,getParentMonitorArea().getHeight()-260)));auto* dialog=new AlertWindow("Review project commands",String(draft.resolved.size())+" resolved commands. Apply commits all changes as one Undo. Cancel leaves the song unchanged.",MessageBoxIconType::NoIcon,this);dialog->addCustomComponent(resolved.get());dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Back",2);dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,resolved,text,expected](int result){if(!safe)return;if(result==1)safe->applyProjectCommands(text,expected);else if(result==2)safe->showProjectCommands(text);}),true);
}
bool Studio::applyProjectCommands(const String& text,uint64_t expected){
    if(expected!=revision||recording||recorder.isActive()||pendingNoteTool()||exporting.load()){status="Project changed or is busy. Reopen and preview the commands again.";return false;}
    ProjectCommandDraft draft;auto valid=buildProjectCommands(project,text,draft);if(valid.failed()){status=valid.getErrorMessage();return false;}
    auto playback=std::make_unique<Playback>();String warnings;for(bool song:{false,true}){auto test=draft.project;test.songMode=song;valid=compilePlayback(test,assets,currentFile==File()?root:currentFile.getParentDirectory(),*playback,warnings);if(valid.failed()||warnings.isNotEmpty()){status=valid.failed()?valid.getErrorMessage():warnings;return false;}}
    if(JSON::toString(projectToJson(project))==JSON::toString(projectToJson(draft.project))){status="The commands already match this song. No edit was added.";return true;}
    bool restart=!draft.project.songMode&&draft.project.patterns[(size_t)draft.project.activePattern].length!=project.patterns[(size_t)project.activePattern].length,seekSong=false;for(const auto& op:*draft.operations.getArray())seekSong|=draft.project.songMode&&op["op"].toString()=="section_clips";
    checkpoint();releaseKeys();project=std::move(draft.project);selectedNote=-1;selectedNotes.clear();if(seekSong){selectedClip=-1;selectedPlaylistIds.clear();}if(restart){pianoStart=pianoNoteCursor=0;pianoSpan=jmin(8.,project.patterns[(size_t)project.activePattern].length);}changed();if(restart)engine->seekRequested.store(0);else if(seekSong)engine->seekRequested.store(jmin(engine->position.load(),project.songLength()));status="Applied "+String(draft.resolved.size())+" local commands as one Undo.";return true;
}
}
