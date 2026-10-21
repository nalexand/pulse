// SPDX-License-Identifier: AGPL-3.0-only
#include "SamplePreview.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
constexpr int maxEntries=128,maxBytes=256*1024;
Result read(const File& file,Array<var>& rows){
    if(!file.exists()){rows.clear();return Result::ok();}
    if(!file.existsAsFile()||file.getSize()>maxBytes)return Result::fail("Sample preview settings are unavailable or too large. Existing file retained.");
    auto json=JSON::parse(file.loadFileAsString());
    if(!json.isObject()||!json["schema"].isInt()||(int)json["schema"]!=1||!json["sources"].isArray()||json["sources"].getArray()->size()>maxEntries)
        return Result::fail("Invalid sample preview settings. Existing file retained.");
    StringArray paths;Array<var> next;
    for(const auto& row:*json["sources"].getArray()){
        auto path=row["path"].toString();
        if(!row.isObject()||!row["path"].isString()||path.isEmpty()||path.length()>4096||!File::isAbsolutePath(path)||path.containsChar('\n')||path.containsChar('\r')||paths.contains(path,true)
           ||!row["stamp"].isString()||row["stamp"].toString().length()>128||!row["sync"].isBool()||!row["repeat"].isBool()||!(row["sourceBpm"].isDouble()||row["sourceBpm"].isInt()||row["sourceBpm"].isInt64()))
            return Result::fail("Invalid sample preview source. Existing file retained.");
        SamplePreviewOptions options{(bool)row["sync"],(bool)row["repeat"],(double)row["sourceBpm"]};auto valid=validateSamplePreview(options);if(valid.failed())return valid;
        paths.add(path);next.add(row);
    }
    rows=std::move(next);return Result::ok();
}
}
Result validateSamplePreview(const SamplePreviewOptions& options){
    return std::isfinite(options.sourceBpm)&&options.sourceBpm>=20&&options.sourceBpm<=400?Result::ok():Result::fail("Enter a source tempo from 20 to 400 BPM.");
}
String samplePreviewStamp(const File& file){return file.existsAsFile()?String(file.getSize())+"|"+String(file.getLastModificationTime().toMilliseconds()):String();}
String samplePreviewToken(const File& file){if(!file.exists())return "missing";if(!file.existsAsFile()||file.getSize()>maxBytes)return "unavailable";return SHA256(file).toHexString();}
Result loadSamplePreview(const File& catalog,const File& source,SamplePreviewOptions& output){
    Array<var> rows;auto result=read(catalog,rows);if(result.failed())return result;SamplePreviewOptions next;
    for(const auto& row:rows)if(File(row["path"].toString())==source&&row["stamp"].toString()==samplePreviewStamp(source)){
        next={(bool)row["sync"],(bool)row["repeat"],(double)row["sourceBpm"]};break;
    }
    output=next;return Result::ok();
}
Result saveSamplePreview(const File& catalog,const File& source,const SamplePreviewOptions& options,const String& expectedCatalog,const String& expectedSource){
    auto result=validateSamplePreview(options);if(result.failed())return result;
    auto token=samplePreviewToken(catalog);if(token!=expectedCatalog)return Result::fail("Sample preview settings changed. Reopen the dialog.");
    if(expectedSource.isEmpty()||samplePreviewStamp(source)!=expectedSource)return Result::fail("Source audio changed. Reopen the sample preview dialog.");
    Array<var> rows;result=read(catalog,rows);if(result.failed())return result;
    for(int i=rows.size()-1;i>=0;--i)if(File(rows[i]["path"].toString())==source)rows.remove(i);
    if(rows.size()>=maxEntries)rows.remove(0); // Bounded least-recently-configured catalog.
    auto* row=new DynamicObject();row->setProperty("path",source.getFullPathName());row->setProperty("stamp",expectedSource);row->setProperty("sync",options.sync);row->setProperty("repeat",options.repeat);row->setProperty("sourceBpm",options.sourceBpm);rows.add(var(row));
    auto* json=new DynamicObject();json->setProperty("schema",1);json->setProperty("sources",rows);auto text=JSON::toString(var(json),false);
    if(text.getNumBytesAsUTF8()>maxBytes)return Result::fail("Sample preview settings exceed 256 KiB.");
    result=catalog.getParentDirectory().createDirectory();if(result.failed())return result;
    TemporaryFile temporary(catalog);if(!temporary.getFile().replaceWithText(text))return Result::fail("Cannot write sample preview settings.");
    Array<var> verified;if(read(temporary.getFile(),verified).failed()||verified.size()!=rows.size())return Result::fail("Cannot verify sample preview settings.");
    if(samplePreviewToken(catalog)!=token||samplePreviewStamp(source)!=expectedSource)return Result::fail("Source or settings changed during save. Existing settings retained.");
    return temporary.overwriteTargetFileWithTemporary()?Result::ok():Result::fail("Cannot save sample preview settings. Existing file retained.");
}
}
