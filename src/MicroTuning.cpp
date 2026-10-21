// SPDX-License-Identifier: AGPL-3.0-only
#include "MicroTuning.h"
#include <cmath>
#include <cstdlib>
#include <vector>
namespace pulse {
using namespace juce;
namespace {
bool number(const var& v){return (v.isInt()||v.isInt64()||v.isDouble())&&std::isfinite((double)v);}
bool parseNumber(const String& s,double& value){if(s.isEmpty()||s.length()>64)return false;auto bytes=s.toRawUTF8();char* end=nullptr;value=std::strtod(bytes,&end);return end!=bytes&&*end==0&&std::isfinite(value);}
}
Result validateMicroTuning(const MicroTuning& t){
    if(t.name.isEmpty()||t.name.length()>100||t.name!=t.name.trim())return Result::fail("Tuning names need 1-100 characters without surrounding spaces.");
    for(auto c:t.name)if(c<32||c==127)return Result::fail("Tuning names cannot contain control characters.");
    if(!std::isfinite(t.referenceHz)||t.referenceHz<220||t.referenceHz>880)return Result::fail("Reference A4 must be 220-880 Hz.");
    for(double cents:t.cents)if(!std::isfinite(cents)||std::abs(cents)>24000)return Result::fail("Each tuning offset must be within -24000 to +24000 cents.");
    return Result::ok();
}
double microTuningCents(const MicroTuning& t,int pitch){return t.enabled?t.cents[(size_t)jlimit(0,127,pitch)]+(t.referenceHz==440?0:1200*std::log2(t.referenceHz/440)):0;}
double microTuningFrequency(const MicroTuning& t,int pitch){return 440*std::exp2((jlimit(0,127,pitch)-69)/12.+microTuningCents(t,pitch)/1200);}
String microTuningLabel(const MicroTuning& t,int pitch){auto offset=microTuningCents(t,pitch);return (offset>=0?String("+"):String())+String(offset,1)+" c / "+String(microTuningFrequency(t,pitch),2)+" Hz";}
var microTuningToJson(const MicroTuning& t){auto* o=new DynamicObject();o->setProperty("enabled",t.enabled);o->setProperty("name",t.name);o->setProperty("referenceHz",t.referenceHz);Array<var> a;for(double c:t.cents)a.add(c);o->setProperty("cents",a);return var(o);}
Result microTuningFromJson(const var& json,MicroTuning& output){
    if(!json.isObject())return Result::fail("Tuning must be a complete object.");
    for(const auto& field:json.getDynamicObject()->getProperties())if(field.name.toString()!="enabled"&&field.name.toString()!="name"&&field.name.toString()!="referenceHz"&&field.name.toString()!="cents")return Result::fail("Unknown tuning field.");
    if(!json["enabled"].isBool()||!json["name"].isString()||!number(json["referenceHz"])||!json["cents"].isArray()||json["cents"].size()!=128)return Result::fail("Tuning requires enabled, name, referenceHz and exactly 128 numeric cents offsets.");
    MicroTuning next;next.enabled=(bool)json["enabled"];next.name=json["name"].toString();next.referenceHz=(double)json["referenceHz"];
    for(int i=0;i<128;++i){if(!number(json["cents"][i]))return Result::fail("Tuning offsets must be finite numbers.");next.cents[(size_t)i]=(double)json["cents"][i];}
    auto valid=validateMicroTuning(next);if(valid.wasOk())output=std::move(next);return valid;
}
Result equalDivisionTuning(int divisions,int root,double reference,MicroTuning& output){
    if(divisions<5||divisions>72||root<0||root>127)return Result::fail("Choose 5-72 equal divisions and a root from MIDI 0-127.");
    MicroTuning next;next.enabled=true;next.name=String(divisions)+" equal divisions / root MIDI "+String(root);next.referenceHz=reference;
    for(int n=0;n<128;++n)next.cents[(size_t)n]=(n-root)*(1200./divisions-100);
    auto valid=validateMicroTuning(next);if(valid.wasOk())output=std::move(next);return valid;
}
Result scalaTuning(const String& text,int root,double reference,MicroTuning& output){
    if(text.getNumBytesAsUTF8()>65536||root<0||root>127)return Result::fail("Scala files are limited to 64 KiB; root must be MIDI 0-127.");
    StringArray lines;for(auto line:StringArray::fromLines(text)){if(line.trimStart().startsWithChar('!'))continue;line=line.upToFirstOccurrenceOf("!",false,false).trim();if(lines.isEmpty()||line.isNotEmpty())lines.add(line);}
    if(lines.size()<3||!lines[1].containsOnly("0123456789")||lines[1].length()>3)return Result::fail("Scala needs a description, degree count and scale intervals.");
    int count=lines[1].getIntValue();if(count<1||count>128||lines.size()!=count+2)return Result::fail("Scala needs exactly 1-128 intervals after its degree count.");
    std::vector<double> intervals{0};
    for(int i=0;i<count;++i){auto token=lines[i+2].upToFirstOccurrenceOf(" ",false,false).upToFirstOccurrenceOf("\t",false,false);double cents=0,a=0,b=1;
        if(token.containsChar('/')){auto first=token.upToFirstOccurrenceOf("/",false,false),last=token.fromFirstOccurrenceOf("/",false,false);if(!first.containsOnly("0123456789")||!last.containsOnly("0123456789")||!parseNumber(first,a)||!parseNumber(last,b)||a<=0||b<=0)return Result::fail("Invalid positive Scala ratio.");cents=1200*std::log2(a/b);}
        else if(token.containsChar('.')){if(!parseNumber(token,cents))return Result::fail("Invalid Scala cents interval.");}
        else {if(!token.containsOnly("0123456789")||!parseNumber(token,a)||a<=0)return Result::fail("Invalid integer Scala ratio.");cents=1200*std::log2(a);}
        if(!std::isfinite(cents)||std::abs(cents)>4800)return Result::fail("Scala intervals must be finite and within -4800 to +4800 cents.");intervals.push_back(cents);
    }
    MicroTuning next;next.enabled=true;next.name=lines[0].isEmpty()?String("Imported Scala scale"):lines[0].substring(0,100).trim();next.referenceHz=reference;double period=intervals.back();if(period<=0)return Result::fail("The Scala repeating period must be positive.");
    for(int n=0;n<128;++n){int distance=n-root,cycle=(int)std::floor((double)distance/count),degree=distance-cycle*count;next.cents[(size_t)n]=cycle*period+intervals[(size_t)degree]-distance*100.;}
    auto valid=validateMicroTuning(next);if(valid.wasOk())output=std::move(next);return valid;
}
Result loadMicroTuning(const File& file,int root,double reference,MicroTuning& output){
    if(!file.existsAsFile()||file.getSize()>65536)return Result::fail("Choose a tuning file no larger than 64 KiB.");auto text=file.loadFileAsString();
    if(file.hasFileExtension("scl"))return scalaTuning(text,root,reference,output);
    var json;auto valid=JSON::parse(text,json);if(valid.failed())return valid;
    if(!json.isObject()||!json["schema"].isInt()||(int)json["schema"]!=1||json["application"].toString()!="Pulse Studio tuning")return Result::fail("Unsupported Pulse tuning preset.");return microTuningFromJson(json["tuning"],output);
}
Result saveMicroTuning(const MicroTuning& t,const File& file){
    auto valid=validateMicroTuning(t);if(valid.failed())return valid;auto* o=new DynamicObject();o->setProperty("schema",1);o->setProperty("application","Pulse Studio tuning");o->setProperty("tuning",microTuningToJson(t));
    valid=file.getParentDirectory().createDirectory();if(valid.failed())return valid;TemporaryFile tmp(file);if(!tmp.getFile().replaceWithText(JSON::toString(var(o),false,22)))return Result::fail("Could not write tuning preset.");MicroTuning checked;valid=loadMicroTuning(tmp.getFile(),60,440,checked);if(valid.failed()||checked!=t)return Result::fail("Tuning preset verification failed.");return tmp.overwriteTargetFileWithTemporary()?Result::ok():Result::fail("Could not commit tuning preset.");
}
}

