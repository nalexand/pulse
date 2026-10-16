// SPDX-License-Identifier: AGPL-3.0-only
#include "NotePresets.h"
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
const char* modes[]={"quantize","humanize","euclidean","arpeggiator"};
bool number(const var& v){return (v.isInt()||v.isInt64()||v.isDouble())&&std::isfinite((double)v);}
bool integer(const var& v,double low,double high){return number(v)&&(double)v>=low&&(double)v<=high&&std::floor((double)v)==(double)v;}
bool within(double v,double low,double high){return std::isfinite(v)&&v>=low&&v<=high;}
Result validateLibrary(const std::vector<NotePreset>& rows){
    if(rows.size()>maxNotePresets)return Result::fail("The note preset library is limited to 128 presets.");
    std::set<String> ids,names;
    for(auto& p:rows){auto r=validateNotePreset(p);if(r.failed())return r;if(!ids.insert(p.id).second||!names.insert(String((int)p.mode)+":"+p.name.toLowerCase()).second)return Result::fail("Duplicate note preset ID or name in the same tool.");}
    return Result::ok();
}
}
String notePresetModeName(NoteToolMode mode){const char* names[]={"Quantize","Humanize","Euclidean rhythm","Arpeggiator"};return (int)mode>=0&&(int)mode<4?names[(int)mode]:"Unknown";}
Result validateNotePreset(const NotePreset& p){
    if((int)p.mode<0||(int)p.mode>3||p.id.isEmpty()||p.id.length()>64||!p.id.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")||p.name.isEmpty()||p.name.length()>64||p.name!=p.name.trim())return Result::fail("Use a preset name of 1-64 characters and a valid preset identity.");
    for(auto c:p.name)if(c<32||c==127)return Result::fail("Preset names cannot contain control characters.");
    if(p.mode==NoteToolMode::quantize||p.mode==NoteToolMode::humanize){auto options=p.notes;options.mode=p.mode;std::vector<Note> out;return processNotes({},8,{},options,out);}
    if(p.mode==NoteToolMode::arpeggiator){std::vector<Note> out;return arpeggiateNotes({},8,{},p.arp,out);}
    auto& o=p.rhythm;
    if(o.steps<1||o.steps>64||o.pulses<0||o.pulses>o.steps||o.rotation<0||o.rotation>=o.steps||o.cycles<1||o.cycles>16||!within(o.gate,.05,1)||!within(o.velocity,.01f,1)||!within(o.variation,0,.5)||(o.write!=RhythmWriteMode::replacePitch&&o.write!=RhythmWriteMode::fill))return Result::fail("Invalid Euclidean preset settings.");
    return Result::ok();
}
var notePresetToJson(const NotePreset& p){
    auto* row=new DynamicObject();row->setProperty("id",p.id);row->setProperty("name",p.name);row->setProperty("mode",(int)p.mode>=0&&(int)p.mode<4?modes[(int)p.mode]:"invalid");auto* s=new DynamicObject();row->setProperty("settings",var(s));
    if(p.mode==NoteToolMode::quantize||p.mode==NoteToolMode::humanize){auto& o=p.notes;
        if(p.mode==NoteToolMode::quantize){s->setProperty("grid",o.grid);s->setProperty("positionStrength",o.positionStrength);s->setProperty("lengthStrength",o.lengthStrength);s->setProperty("swing",o.swing);s->setProperty("window",o.window);s->setProperty("useWindow",o.useWindow);}
        else{s->setProperty("timing",o.timing);s->setProperty("length",o.length);s->setProperty("velocity",o.velocity);s->setProperty("keepChords",o.keepChords);s->setProperty("seed",(int64)o.seed);}
    }else if(p.mode==NoteToolMode::euclidean){auto& o=p.rhythm;s->setProperty("steps",o.steps);s->setProperty("pulses",o.pulses);s->setProperty("rotation",o.rotation);s->setProperty("cycles",o.cycles);s->setProperty("gate",o.gate);s->setProperty("velocity",o.velocity);s->setProperty("variation",o.variation);s->setProperty("seed",(int64)o.seed);s->setProperty("write",(int)o.write);
    }else{auto& o=p.arp;s->setProperty("order",(int)o.order);s->setProperty("rate",o.rate);s->setProperty("gate",o.gate);s->setProperty("octaves",o.octaves);s->setProperty("transpose",o.transpose);s->setProperty("velocity",o.velocity);s->setProperty("restart",o.restart);s->setProperty("seed",(int64)o.seed);}
    return var(row);
}
Result notePresetFromJson(const var& row,NotePreset& output){
    NotePreset p;if(!row.isObject()||!row["id"].isString()||!row["name"].isString()||!row["mode"].isString()||!row["settings"].isObject())return Result::fail("Invalid note preset object.");
    int mode=-1;for(int i=0;i<4;++i)if(row["mode"].toString()==modes[i])mode=i;if(mode<0)return Result::fail("Unknown note preset tool.");p.mode=(NoteToolMode)mode;p.notes.mode=p.mode;p.id=row["id"].toString();p.name=row["name"].toString();auto s=row["settings"];
    auto nums=[&](std::initializer_list<const char*> keys){for(auto key:keys)if(!number(s[key]))return false;return true;};
    bool good=true;
    if(p.mode==NoteToolMode::quantize){good=nums({"grid","positionStrength","lengthStrength","swing","window"})&&s["useWindow"].isBool();p.notes.grid=(double)s["grid"];p.notes.positionStrength=(double)s["positionStrength"];p.notes.lengthStrength=(double)s["lengthStrength"];p.notes.swing=(double)s["swing"];p.notes.window=(double)s["window"];p.notes.useWindow=(bool)s["useWindow"];
    }else if(p.mode==NoteToolMode::humanize){good=nums({"timing","length","velocity"})&&s["keepChords"].isBool()&&integer(s["seed"],0,4294967295.);p.notes.timing=(double)s["timing"];p.notes.length=(double)s["length"];p.notes.velocity=(double)s["velocity"];p.notes.keepChords=(bool)s["keepChords"];if(good)p.notes.seed=(uint32_t)(int64)s["seed"];
    }else if(p.mode==NoteToolMode::euclidean){good=integer(s["steps"],1,64)&&integer(s["pulses"],0,64)&&integer(s["rotation"],0,63)&&integer(s["cycles"],1,16)&&integer(s["write"],0,1)&&integer(s["seed"],0,4294967295.)&&nums({"gate","velocity","variation"});if(good){p.rhythm.steps=(int)s["steps"];p.rhythm.pulses=(int)s["pulses"];p.rhythm.rotation=(int)s["rotation"];p.rhythm.cycles=(int)s["cycles"];p.rhythm.write=(RhythmWriteMode)(int)s["write"];p.rhythm.seed=(uint32_t)(int64)s["seed"];p.rhythm.gate=(double)s["gate"];p.rhythm.velocity=(float)s["velocity"];p.rhythm.variation=(float)s["variation"];}
    }else{good=integer(s["order"],0,5)&&integer(s["octaves"],1,4)&&integer(s["transpose"],-24,24)&&integer(s["seed"],0,4294967295.)&&s["restart"].isBool()&&nums({"rate","gate","velocity"});if(good){p.arp.order=(ArpOrder)(int)s["order"];p.arp.octaves=(int)s["octaves"];p.arp.transpose=(int)s["transpose"];p.arp.seed=(uint32_t)(int64)s["seed"];p.arp.rate=(double)s["rate"];p.arp.gate=(double)s["gate"];p.arp.velocity=(float)s["velocity"];p.arp.restart=(bool)s["restart"];}}
    if(!good)return Result::fail("Missing or invalid note preset settings.");auto valid=validateNotePreset(p);if(valid.failed())return valid;output=std::move(p);return Result::ok();
}
Result loadNotePresets(const File& file,std::vector<NotePreset>& output){
    if(!file.exists()){output.clear();return Result::ok();}if(!file.existsAsFile()||file.getSize()>262144)return Result::fail("Note preset library must be a file no larger than 256 KiB.");
    var json;auto parsed=JSON::parse(file.loadFileAsString(),json);if(parsed.failed())return Result::fail("Cannot read note preset JSON: "+parsed.getErrorMessage());auto* rows=json["presets"].getArray();
    if(!json.isObject()||!integer(json["schema"],1,1)||json["application"].toString()!="Pulse Studio note presets"||!rows||rows->size()>maxNotePresets)return Result::fail("Unsupported or invalid note preset library.");
    std::vector<NotePreset> next;for(auto& row:*rows){NotePreset p;auto r=notePresetFromJson(row,p);if(r.failed())return r;next.push_back(std::move(p));}auto valid=validateLibrary(next);if(valid.failed())return valid;output=std::move(next);return Result::ok();
}
Result saveNotePresets(const File& file,const std::vector<NotePreset>& rows){
    auto valid=validateLibrary(rows);if(valid.failed())return valid;auto* object=new DynamicObject();object->setProperty("application","Pulse Studio note presets");object->setProperty("schema",1);Array<var> presets;for(auto& p:rows)presets.add(notePresetToJson(p));object->setProperty("presets",presets);auto text=JSON::toString(var(object),false,22);
    if(text.getNumBytesAsUTF8()>262144)return Result::fail("Note preset library is too large.");auto directory=file.getParentDirectory().createDirectory();if(directory.failed())return directory;TemporaryFile temporary(file);if(!temporary.getFile().replaceWithText(text)||!temporary.overwriteTargetFileWithTemporary())return Result::fail("Could not save the note preset file. Existing contents are unchanged.");return Result::ok();
}
Result putNotePreset(const File& file,const NotePreset& preset,bool replace){
    auto valid=validateNotePreset(preset);if(valid.failed())return valid;std::vector<NotePreset> rows;auto loaded=loadNotePresets(file,rows);if(loaded.failed())return loaded;
    auto found=std::find_if(rows.begin(),rows.end(),[&](const NotePreset& row){return row.id==preset.id;});if(replace){if(found==rows.end()||found->mode!=preset.mode)return Result::fail("This preset changed or was removed. Reopen Presets.");*found=preset;}else{if(found!=rows.end())return Result::fail("Preset identity already exists.");rows.push_back(preset);}return saveNotePresets(file,rows);
}
Result removeNotePreset(const File& file,const String& id){std::vector<NotePreset> rows;auto loaded=loadNotePresets(file,rows);if(loaded.failed())return loaded;auto found=std::find_if(rows.begin(),rows.end(),[&](const NotePreset& p){return p.id==id;});if(found==rows.end())return Result::fail("Preset no longer exists.");rows.erase(found);return saveNotePresets(file,rows);}
std::vector<NotePreset> factoryNotePresets(NoteToolMode mode){
    std::vector<NotePreset> rows;auto add=[&](const char* id,const char* name){NotePreset p;p.id=id;p.name=name;p.mode=mode;p.notes.mode=mode;rows.push_back(p);};
    if(mode==NoteToolMode::quantize){add("factory-tight","Tight sixteenths");rows.back().notes.lengthStrength=1;add("factory-soft","Gentle eighths");rows.back().notes.grid=.5;rows.back().notes.positionStrength=.5;add("factory-swing","Swung sixteenths");rows.back().notes.swing=.33;}
    else if(mode==NoteToolMode::humanize){add("factory-subtle","Subtle performance");add("factory-loose","Loose performance");rows.back().notes.timing=.045;rows.back().notes.length=.04;rows.back().notes.velocity=.12;add("factory-velocity","Velocity only");rows.back().notes.timing=0;rows.back().notes.length=0;rows.back().notes.velocity=.15;}
    else if(mode==NoteToolMode::euclidean){add("factory-tresillo","Tresillo 3 in 8");rows.back().rhythm.steps=8;rows.back().rhythm.pulses=3;rows.back().rhythm.variation=0;add("factory-five","Five in 16");add("factory-seven","Seven in 12");rows.back().rhythm.steps=12;rows.back().rhythm.pulses=7;}
    else if(mode==NoteToolMode::arpeggiator){add("factory-up","Up two octaves");rows.back().arp.octaves=2;add("factory-bounce","Bouncing triplets");rows.back().arp.order=ArpOrder::upDown;rows.back().arp.rate=1./3;add("factory-random","Seeded random");rows.back().arp.order=ArpOrder::random;rows.back().arp.octaves=2;}
    return rows;
}
}
