// SPDX-License-Identifier: AGPL-3.0-only
#include "Shortcuts.h"
#include <juce_cryptography/juce_cryptography.h>
#include <set>
namespace pulse {
using namespace juce;
const std::vector<ShortcutCommand>& shortcutCommands(){static const std::vector<ShortcutCommand> commands={
    {"transport.toggle","Play / Stop","Transport","Space",false,false},
    {"transport.pause","Play / Pause","Transport","Ctrl+Space",false,false},
    {"transport.rewind","Rewind","Transport","Home",false,false},
    {"transport.mode","Pattern / Song mode","Transport","L",true,false},
    {"transport.record","Record / finish take","Transport","R",true,false},
    {"transport.metro","Toggle metronome","Transport","Ctrl+M",false,false},
    {"project.open","Open project","Project","Ctrl+O",false,false},
    {"project.save","Save project","Project","Ctrl+S",false,false},
    {"project.saveAs","Save project as","Project","Ctrl+Shift+S",false,false},
    {"project.version","Save quick version","Project","Ctrl+N",false,false},
    {"project.namedVersion","Save named version","Project","",false,false},
    {"project.versions","Project version history","Project","",false,false},
    {"project.export","Export audio","Project","Ctrl+R",false,false},
    {"project.exportMidi","Export MIDI","Project","Ctrl+Shift+M",false,false},
    {"project.notes","Project notes","Project","F11",false,false},
    {"edit.history","Project edit history","Editing","Ctrl+Alt+H",false,false},
    {"edit.undoToggle","Toggle last Undo / Redo","Editing","Ctrl+Z",false,false},
    {"edit.undo","Undo one step","Editing","Ctrl+Alt+Z",false,false},
    {"edit.redo","Redo","Editing","Ctrl+Y",false,false},
    {"edit.redoAlternate","Redo (alternate)","Editing","Ctrl+Shift+Z",false,false},
    {"edit.copy","Copy selection","Editing","Ctrl+C",false,false},
    {"edit.cut","Cut selection","Editing","Ctrl+X",false,false},
    {"edit.paste","Paste","Editing","Ctrl+V",false,false},
    {"edit.pasteOriginal","Paste original positions / replace waveform","Editing","Ctrl+Shift+V",false,false},
    {"edit.duplicate","Duplicate selection","Editing","Ctrl+B",false,false},
    {"edit.selectAll","Select all notes / clips / waveform","Editing","Ctrl+A",false,false},
    {"edit.deselect","Deselect notes / clips","Editing","Ctrl+D",false,false},
    {"edit.delete","Delete selection","Editing","Delete",false,false},
    {"edit.snap","Toggle snap","Editing","Backspace",false,false},
    {"edit.split","Split audio at cursor","Editing","Alt+S",false,false},
    {"tools.draw","Draw tool","Editor tools","P",true,false},
    {"tools.paint","Paint tool","Editor tools","B",true,false},
    {"tools.delete","Delete tool","Editor tools","D",true,false},
    {"tools.select","Select tool","Editor tools","E",true,false},
    {"pattern.add","Add empty pattern","Pattern / notes","F4",false,false},
    {"pattern.rename","Rename pattern / Mixer channel","Pattern / notes","F2",false,false},
    {"notes.quantize","Quantize with preview","Pattern / notes","Alt+Q",false,false},
    {"notes.humanize","Humanize with preview","Pattern / notes","Alt+R",false,false},
    {"notes.euclidean","Euclidean rhythm","Pattern / notes","Alt+E",false,false},
    {"notes.arpeggio","Arpeggiator","Pattern / notes","Alt+A",false,false},
    {"notes.quickQuantize","Quick quantize","Pattern / notes","Ctrl+Q",false,false},
    {"notes.up","Transpose up an octave","Pattern / notes","Ctrl+Up",false,false},
    {"notes.down","Transpose down an octave","Pattern / notes","Ctrl+Down",false,false},
    {"window.playlist","Toggle Playlist","Workspace","F5",false,false},
    {"window.rack","Toggle Channel rack","Workspace","F6",false,false},
    {"window.piano","Toggle Piano roll","Workspace","F7",false,false},
    {"window.mixer","Toggle Mixer","Workspace","F9",false,false},
    {"window.browser","Toggle Browser","Workspace","Alt+F8",false,false},
    {"window.audio","Audio settings","Workspace","F10",false,false},
    {"window.hideAll","Hide all windows","Workspace","F12",false,false},
    {"window.cycle","Cycle open windows","Workspace","Tab",false,false},
    {"window.maximize","Maximize / restore Playlist","Workspace","Enter",false,false},
    {"window.recover","Recover window positions (fixed)","Workspace","Ctrl+Shift+H",false,true},
    {"command.search","Find commands","Workspace","Ctrl+P",false,false},
    {"help.open","Quick start and help","Workspace","F1",false,false},
    {"shortcuts.open","Keyboard shortcut settings","Workspace","",false,false},
    {"track.previous","Select previous channel","Navigation","Up",false,false},
    {"track.next","Select next channel","Navigation","Down",false,false},
    {"marker.add","Add Song marker","Navigation","Alt+T",false,false},
    {"marker.section","Add Song section","Navigation","Ctrl+Alt+T",false,false},
    {"marker.previous","Previous marker","Navigation","Alt+Left",false,false},
    {"marker.next","Next marker","Navigation","Alt+Right",false,false},
    {"typing.toggle","Typing keyboard on / off","Typing keyboard","Ctrl+T",false,false},
    {"typing.sustain","Toggle sustain latch","Typing keyboard","Ctrl+Shift+K",false,false},
    {"typing.velocityUp","Increase typing velocity","Typing keyboard","Ctrl+Shift+Up",false,false},
    {"typing.velocityDown","Decrease typing velocity","Typing keyboard","Ctrl+Shift+Down",false,false},
    {"typing.transposeUp","Transpose typing up a semitone","Typing keyboard","Ctrl+Shift+Right",false,false},
    {"typing.transposeDown","Transpose typing down a semitone","Typing keyboard","Ctrl+Shift+Left",false,false},
    {"typing.octaveUp","Increase typing octave","Typing keyboard","Alt+]",false,false},
    {"typing.octaveDown","Decrease typing octave","Typing keyboard","Alt+[",false,false},
    {"typing.octaveUpBare","Increase octave (typing off)","Typing keyboard","]",true,false},
    {"typing.octaveDownBare","Decrease octave (typing off)","Typing keyboard","[",true,false},
    {"transport.panic","Release keys and reset playback","Transport","Ctrl+H",false,false},
};return commands;}
const ShortcutCommand* shortcutCommand(const String& id){for(const auto& command:shortcutCommands())if(command.id==id)return &command;return nullptr;}
String shortcutBinding(const ShortcutSettings& settings,const String& id){auto* command=shortcutCommand(id);if(!command)return {};auto it=settings.overrides.find(id);return it==settings.overrides.end()?command->defaultKey:it->second;}
Result canonicalShortcut(const String& raw,String& output){
    if(raw.length()>64)return Result::fail("Shortcut is too long.");auto text=raw.trim();if(text.isEmpty()){output.clear();return Result::ok();}
    auto parts=StringArray::fromTokens(text,"+","");if(parts.size()<1||parts.size()>4)return Result::fail("Use one key with optional Ctrl, Alt and Shift.");
    bool ctrl=false,alt=false,shift=false;String key;
    for(int i=0;i<parts.size();++i){auto part=parts[i].trim().toLowerCase();if(i<parts.size()-1){if(part=="ctrl"&&!ctrl)ctrl=true;else if(part=="alt"&&!alt)alt=true;else if(part=="shift"&&!shift)shift=true;else return Result::fail("Use each modifier once, before the key.");}else key=part;}
    const StringArray named={"Space","Home","Delete","Backspace","Tab","Enter","Up","Down","Left","Right"};String normal;
    for(auto& name:named)if(key==name.toLowerCase())normal=name;
    if(key.length()==1&&String("abcdefghijklmnopqrstuvwxyz0123456789[]").contains(key))normal=key.toUpperCase();
    for(int f=1;f<=12;++f)if(key=="f"+String(f))normal="F"+String(f);
    if(normal.isEmpty())return Result::fail("Use a letter, digit, bracket, F1-F12 or a listed navigation key. Escape is reserved.");
    auto chord=(ctrl?String("Ctrl+"):String())+(alt?String("Alt+"):String())+(shift?String("Shift+"):String())+normal;
    if((alt&&(normal=="F4"||normal=="Tab"||normal=="Space"))||(ctrl&&alt&&normal=="Delete"))return Result::fail("This shortcut belongs to Windows.");
    output=chord;return Result::ok();
}
Result validateShortcuts(const ShortcutSettings& settings){
    if(settings.overrides.size()>shortcutCommands().size())return Result::fail("Too many shortcut overrides.");
    for(const auto& [id,chord]:settings.overrides){auto* command=shortcutCommand(id);if(!command)return Result::fail("Unknown shortcut command: "+id);if(command->locked)return Result::fail("Window recovery uses a fixed shortcut.");String canonical;auto result=canonicalShortcut(chord,canonical);if(result.failed())return result;if(canonical!=chord)return Result::fail("Shortcut must use its canonical key name.");}
    std::map<String,String> occupied;
    for(const auto& command:shortcutCommands()){auto key=shortcutBinding(settings,command.id);if(key.isEmpty())continue;String canonical;auto result=canonicalShortcut(key,canonical);if(result.failed())return result;
        if(key=="Ctrl+Shift+H"&&!command.locked)return Result::fail("Ctrl+Shift+H is reserved for window recovery.");
        auto base=key.fromLastOccurrenceOf("+",false,false);if(!key.contains("+"))base=key;
        bool bare=base.length()==1&&!key.contains("Ctrl+")&&!key.contains("Alt+");
        if(bare&&!command.typingOff)return Result::fail("Letters, digits and brackets without Ctrl or Alt are reserved for piano input.");
        if(occupied.count(key))return Result::fail("Shortcut already used by "+occupied[key]+".");occupied[key]=command.name;
    }return Result::ok();
}
Result setShortcut(ShortcutSettings& settings,const String& id,const String& raw){auto* command=shortcutCommand(id);if(!command)return Result::fail("Unknown shortcut command.");if(command->locked)return Result::fail("Window recovery uses a fixed shortcut.");String key;auto result=canonicalShortcut(raw,key);if(result.failed())return result;auto next=settings;if(key==command->defaultKey)next.overrides.erase(id);else next.overrides[id]=key;result=validateShortcuts(next);if(result.wasOk())settings=std::move(next);return result;}
String findShortcut(const ShortcutSettings& settings,const String& key,bool typingEnabled){if(key.isEmpty())return {};for(const auto& command:shortcutCommands())if((!typingEnabled||!command.typingOff)&&shortcutBinding(settings,command.id)==key)return command.id;return {};}
String shortcutFileToken(const File& file){if(!file.exists())return "missing";if(!file.existsAsFile()||file.isSymbolicLink()||file.getSize()>65536)return "unavailable";return String(file.getSize())+"|"+String(file.getLastModificationTime().toMilliseconds())+"|"+SHA256(file).toHexString();}
Result loadShortcuts(const File& file,ShortcutSettings& output){if(!file.exists()){output={};return Result::ok();}auto token=shortcutFileToken(file);if(token=="unavailable")return Result::fail("Shortcut file is not a regular local file under 64 KiB.");var json;auto parsed=JSON::parse(file.loadFileAsString(),json);if(parsed.failed())return Result::fail("Cannot read keyboard shortcuts: "+parsed.getErrorMessage());auto* object=json.getDynamicObject();if(!object||!json["schema"].isInt()||(int)json["schema"]!=1||!json["overrides"].isObject())return Result::fail("Unsupported keyboard shortcut settings.");ShortcutSettings next;for(auto& item:json["overrides"].getDynamicObject()->getProperties()){if(!item.value.isString())return Result::fail("Shortcut binding must be text.");next.overrides[item.name.toString()]=item.value.toString();}
    // Older valid preference files could assign this formerly unused chord.
    // Preserve that assignment and leave the new history command unbound.
    if(!next.overrides.count("edit.history"))for(const auto& [id,key]:next.overrides)if(id!="edit.history"&&key=="Ctrl+Alt+H"){next.overrides["edit.history"]="";break;}
    auto valid=validateShortcuts(next);if(valid.failed())return valid;if(shortcutFileToken(file)!=token)return Result::fail("Shortcut file changed while reading.");output=std::move(next);return Result::ok();}
Result saveShortcuts(const File& file,const ShortcutSettings& settings){auto valid=validateShortcuts(settings);if(valid.failed())return valid;if(file.isSymbolicLink())return Result::fail("Shortcut file cannot be a symbolic link.");auto directory=file.getParentDirectory().createDirectory();if(directory.failed())return directory;auto* object=new DynamicObject();object->setProperty("schema",1);auto* values=new DynamicObject();for(const auto& [id,key]:settings.overrides)values->setProperty(id,key);object->setProperty("overrides",var(values));TemporaryFile temporary(file);if(!temporary.getFile().replaceWithText(JSON::toString(var(object),false))||!temporary.overwriteTargetFileWithTemporary())return Result::fail("Cannot save keyboard shortcuts.");return Result::ok();}
}
