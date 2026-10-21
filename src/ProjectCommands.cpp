// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectCommands.h"
#include "ControlEdits.h"
#include "SynthPresets.h"
#include <cstdlib>
#include <cmath>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif
namespace pulse {
using namespace juce;
namespace {
bool sameName(const String& a,const String& b){
   #if JUCE_WINDOWS
    // The CRT's default C locale only folds ASCII. Use the platform's ordinal
    // Unicode comparison without changing the app or audio thread's locale.
    return CompareStringOrdinal(a.toWideCharPointer(),-1,b.toWideCharPointer(),-1,TRUE)==CSTR_EQUAL;
   #else
    return a.equalsIgnoreCase(b);
   #endif
}
struct Word {String text;bool quoted=false;};
Result tokenize(const String& line,std::vector<Word>& out){
    int i=0;while(i<line.length()){while(i<line.length()&&CharacterFunctions::isWhitespace(line[i]))++i;if(i==line.length())break;Word word;
        if(line[i]=='"'){word.quoted=true;++i;bool closed=false;while(i<line.length()){auto c=line[i++];if(c=='"'){closed=true;break;}if(c=='\\'){if(i==line.length()||(line[i]!='"'&&line[i]!='\\'))return Result::fail("Inside names use only \\\" or \\\\ escapes.");c=line[i++];}if(c<32)return Result::fail("Names cannot contain control characters.");word.text+=String::charToString(c);}if(!closed||word.text.isEmpty()||(i<line.length()&&!CharacterFunctions::isWhitespace(line[i])))return Result::fail("Use a complete double-quoted name, followed by a space.");}
        else{int start=i;while(i<line.length()&&!CharacterFunctions::isWhitespace(line[i]))++i;word.text=line.substring(start,i);if(word.text.containsChar('"'))return Result::fail("Put double quotes around the entire name.");}
        out.push_back(std::move(word));if(out.size()>32)return Result::fail("A command has too many words.");
    }return Result::ok();
}
struct Reader {
    const std::vector<Word>& words;size_t at=0;String error;
    bool take(const char* text){if(at<words.size()&&!words[at].quoted&&words[at].text.equalsIgnoreCase(text)){++at;return true;}return false;}
    bool need(const char* text){if(take(text))return true;error="Expected '"+String(text)+"'.";return false;}
    bool number(double& out){if(at>=words.size()||words[at].quoted){error="Expected a finite number.";return false;}auto s=words[at++].text;auto* start=s.toRawUTF8();char* end=nullptr;out=std::strtod(start,&end);if(end==start||*end||!std::isfinite(out)){error="Expected a finite number without extra text.";return false;}return true;}
    bool name(String& out){if(at>=words.size()||!words[at].quoted){error="Use a double-quoted name.";return false;}out=words[at++].text;return true;}
    bool done(){if(at==words.size())return true;error="Unexpected words after the command.";return false;}
};
template<class Rows,class GetName> int target(Reader& r,const Rows& rows,GetName name,const char* kind){
    if(r.at>=r.words.size()){r.error="Choose a "+String(kind)+" number (starting at 1) or a double-quoted name.";return -1;}const auto& word=r.words[r.at++];
    if(word.quoted){int found=-1;for(int i=0;i<(int)rows.size();++i)if(sameName(name(rows[(size_t)i]),word.text)){if(found>=0){r.error="Ambiguous "+String(kind)+" name. Use its visible number.";return -1;}found=i;}if(found<0)r.error="No "+String(kind)+" named \""+word.text+"\".";return found;}
    if(!word.text.containsOnly("0123456789")||word.text.length()>5){r.error="Use a visible "+String(kind)+" number or a double-quoted name.";return -1;}int index=word.text.getIntValue()-1;if(index<0||index>=(int)rows.size()){r.error="The "+String(kind)+" number is outside this project.";return -1;}return index;
}
var object(const char* op){auto* o=new DynamicObject();o->setProperty("op",op);return var(o);}
String channel(const Project& p,int index){return "Track "+String(index+1)+" (\""+p.tracks[(size_t)index].name+"\")";}
String pattern(const Project& p,int index){return "Pattern "+String(index+1)+" (\""+p.patterns[(size_t)index].name+"\")";}
Result parse(const Project& p,const std::vector<Word>& words,var& op,String& summary){
    Reader r{words};auto track=[&]{return target(r,p.tracks,[](const Track& t){return t.name;},"track");};auto pat=[&]{return target(r,p.patterns,[](const Pattern& t){return t.name;},"pattern");};double value=0;String name;
    if(r.take("set")){
        if(r.take("tempo")){if(!r.need("to")||!r.number(value))return Result::fail(r.error);r.take("bpm");op=object("set_song");op.getDynamicObject()->setProperty("bpm",value);summary="Tempo: "+String(p.bpm,2)+" -> "+String(value,2)+" BPM";}
        else if(r.take("loop")||r.take("metronome")){String field=words[1].text.toLowerCase();bool enabled=r.take("on");if(!enabled&&!r.take("off"))return Result::fail("Choose on or off.");op=object("set_song");op.getDynamicObject()->setProperty(field,enabled);summary=field+": "+String(enabled?"on":"off");}
        else if(r.take("track")){
            int t=track();if(t<0)return Result::fail(r.error);if(r.at==words.size())return Result::fail("Choose volume, pan, send or cutoff.");String field=words[r.at++].text.toLowerCase();if(!r.need("to")||!r.number(value))return Result::fail(r.error);
            String unit=field=="cutoff"?"hz":"percent";if(!r.need(unit.toRawUTF8()))return Result::fail(r.error);if(field!="volume"&&field!="pan"&&field!="send"&&field!="cutoff")return Result::fail("Choose volume, pan, send or cutoff.");
            op=object("set_track");op.getDynamicObject()->setProperty("track",t);auto* params=new DynamicObject();params->setProperty(field,field=="cutoff"?value:value/100);op.getDynamicObject()->setProperty("params",var(params));summary=channel(p,t)+" / "+field+" -> "+String(value,2)+" "+unit;
        }else if(r.take("pattern")){int n=pat();if(n<0||!r.need("length")||!r.need("to")||!r.number(value)||!r.need("beats"))return Result::fail(r.error);op=object("set_pattern");op.getDynamicObject()->setProperty("pattern",n);op.getDynamicObject()->setProperty("length",value);summary=pattern(p,n)+" / length: "+String(p.patterns[(size_t)n].length,2)+" -> "+String(value,2)+" beats";}
        else return Result::fail("After set choose tempo, loop, metronome, track or pattern.");
    }else if(r.take("rename")){
        bool song=r.take("song"),isTrack=!song&&r.take("track");int index=-1;if(!song){if(!isTrack&&!r.need("pattern"))return Result::fail(r.error);index=isTrack?track():pat();if(index<0)return Result::fail(r.error);}if(!r.need("to")||!r.name(name))return Result::fail(r.error);op=object(song?"set_song":isTrack?"set_track":"set_pattern");if(!song)op.getDynamicObject()->setProperty(isTrack?"track":"pattern",index);op.getDynamicObject()->setProperty(song?"title":"name",name);summary=(song?"Song":isTrack?channel(p,index):pattern(p,index))+" -> \""+name+"\"";
    }else if(r.take("mute")||r.take("unmute")||r.take("solo")||r.take("unsolo")){
        String verb=words[0].text.toLowerCase();if(!r.need("track"))return Result::fail(r.error);int t=track();if(t<0)return Result::fail(r.error);op=object("set_track");op.getDynamicObject()->setProperty("track",t);op.getDynamicObject()->setProperty(verb.contains("solo")?"solo":"mute",verb=="mute"||verb=="solo");summary=channel(p,t)+" / "+verb;
    }else if(r.take("lower")||r.take("raise")){
        bool lower=words[0].text.equalsIgnoreCase("lower");if(!r.need("track"))return Result::fail(r.error);int t=track();if(t<0||!r.need("by")||!r.number(value)||!r.need("db"))return Result::fail(r.error);if(value<0||value>96)return Result::fail("Use a positive adjustment from 0 to 96 dB.");double gain=p.tracks[(size_t)t].volume*std::pow(10.,(lower?-value:value)/20);op=object("set_track");op.getDynamicObject()->setProperty("track",t);auto* params=new DynamicObject();params->setProperty("volume",gain);op.getDynamicObject()->setProperty("params",var(params));summary=channel(p,t)+" / volume: "+String(p.tracks[(size_t)t].volume*100,3)+" -> "+String(gain*100,3)+" percent ("+(lower?"-":"+")+String(value,2)+" dB)";
    }else if(r.take("use")){
        if(!r.need("sound")||!r.name(name)||!r.need("on")||!r.need("track"))return Result::fail(r.error);int t=track();if(t<0)return Result::fail(r.error);auto& sounds=factorySynthPresets();auto found=std::find_if(sounds.begin(),sounds.end(),[&](const SynthPreset& s){return sameName(s.name,name);});if(found==sounds.end())return Result::fail("Unknown factory sound. Use a name from the sound Browser.");op=object("set_track");op.getDynamicObject()->setProperty("track",t);op.getDynamicObject()->setProperty("preset",found->name);summary=channel(p,t)+" / sound -> "+found->name;
    }else if(r.take("transpose")){
        if(!r.need("track"))return Result::fail(r.error);int t=track();if(t<0||!r.need("in")||!r.need("pattern"))return Result::fail(r.error);int n=pat();if(n<0)return Result::fail(r.error);bool up=r.take("up");if(!up&&!r.take("down"))return Result::fail("Choose up or down.");if(!r.number(value)||!r.need("semitones"))return Result::fail(r.error);if(value<0||value>48||value!=std::floor(value))return Result::fail("Transpose by an integer from 0 to 48 semitones.");if(p.tracks[(size_t)t].isBus)return Result::fail("Buses have no note lanes.");Array<var> notes;for(auto note:p.patterns[(size_t)n].lanes[(size_t)t]){note.pitch+=(up?1:-1)*(int)value;if(note.pitch<0||note.pitch>127)return Result::fail("Transposition would move a note outside MIDI 0-127; nothing was changed.");auto* o=new DynamicObject();o->setProperty("beat",note.beat);o->setProperty("length",note.length);o->setProperty("pitch",note.pitch);o->setProperty("velocity",note.velocity);o->setProperty("probability",note.probability);o->setProperty("ratchets",note.ratchets);o->setProperty("conditionEvery",note.conditionEvery);o->setProperty("conditionStep",note.conditionStep);notes.add(var(o));}op=object("set_notes");op.getDynamicObject()->setProperty("track",t);op.getDynamicObject()->setProperty("pattern",n);op.getDynamicObject()->setProperty("notes",notes);summary=channel(p,t)+" / "+pattern(p,n)+" / "+String(notes.size())+" notes "+(up?"up ":"down ")+String((int)value)+" semitones";
    }else if(r.take("copy")||r.take("move")){
        bool move=words[0].text.equalsIgnoreCase("move");if(!r.need("section")||!r.name(name)||!r.need("to")||!r.need("beat")||!r.number(value))return Result::fail(r.error);const SongMarker* found=nullptr;for(const auto& m:p.markers)if(m.isSection()&&sameName(m.name,name)){if(found)return Result::fail("Ambiguous section name. Rename one section first.");found=&m;}if(!found)return Result::fail("No named section matches \""+name+"\".");op=object("section_clips");op.getDynamicObject()->setProperty("section",found->id);op.getDynamicObject()->setProperty("destination",value);op.getDynamicObject()->setProperty("move",move);summary=String(move?"Move":"Copy")+" section \""+found->name+"\" ["+String(found->beat,2)+", "+String(found->endBeat,2)+") to beat "+String(value,2)+"; whole clips only, overlaps rejected";
    }else return Result::fail("Unsupported command. Use the examples; export/file operations and free-form requests are not supported.");
    return r.done()?Result::ok():Result::fail(r.error);
}
}
Result buildProjectCommands(const Project& source,const String& text,ProjectCommandDraft& output){
    if(text.length()>4096||text.trim().isEmpty())return Result::fail("Enter 1-16 commands, up to 4096 characters.");Project checked;auto sourceValid=projectFromJson(projectToJson(source),checked);if(sourceValid.failed())return Result::fail("Source project is invalid: "+sourceValid.getErrorMessage());ProjectCommandDraft draft;draft.project=source;Array<var> operations;int lineNo=0;
    for(auto line:StringArray::fromLines(text)){++lineNo;line=line.trim();if(line.isEmpty())continue;if(operations.size()==16)return Result::fail("Use at most sixteen commands per edit.");std::vector<Word> words;auto valid=tokenize(line,words);var op;String resolved;if(valid.wasOk())valid=parse(draft.project,words,op,resolved);Project next;if(valid.wasOk())valid=applyControlEdits(draft.project,var(Array<var>{op}),next);if(valid.failed())return Result::fail("Line "+String(lineNo)+": "+valid.getErrorMessage());draft.project=std::move(next);operations.add(op);draft.resolved.add(String(draft.resolved.size()+1)+". "+resolved);
    }if(operations.isEmpty())return Result::fail("Enter at least one command.");draft.operations=var(operations);output=std::move(draft);return Result::ok();
}
String projectCommandHelp(){return "One command per line, up to 16. Track/pattern numbers start at 1; names use double quotes.\n\nset tempo to 120 bpm\nlower track \"Bass\" by 2 db\nset track 8 volume to 60 percent\nset track 8 pan to -20 percent\nset track 8 cutoff to 4000 hz\nmute track 2 / unmute track 2 / solo track 8 / unsolo track 8\nuse sound \"Cedar string\" on track 8\nrename track 8 to \"Strings\"\nrename pattern 1 to \"Verse\"\nrename song to \"My song\"\nset pattern 1 length to 8 beats\ntranspose track 8 in pattern 1 up 2 semitones\ncopy section \"Chorus\" to beat 32 (or move)\nset loop on / set metronome off\n\nSend uses percent (0-80). Volume 0-150; pan -100 to 100; cutoff 60-20000 Hz. Tempo 30-300. Commands never export files or execute code.";}
}
