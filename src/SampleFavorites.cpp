// SPDX-License-Identifier: AGPL-3.0-only
#include "SampleFavorites.h"
#include <juce_cryptography/juce_cryptography.h>
namespace pulse {
using namespace juce;
namespace {
Result validate(const StringArray& files){
    if(files.size()>maxSampleFavorites)return Result::fail("Maximum 256 favorite sample files.");StringArray seen;
    for(const auto& path:files){if(path.isEmpty()||path.length()>4096||path.containsChar('\n')||path.containsChar('\r')||!File::isAbsolutePath(path)||!File(path).hasFileExtension("wav;aif;aiff;flac;ogg"))return Result::fail("Favorite samples must be absolute paths to WAV, AIFF, FLAC or Ogg files.");auto normalized=File(path).getFullPathName();if(seen.contains(normalized,true))return Result::fail("Duplicate file in sample favorites.");seen.add(normalized);}return Result::ok();
}
}
bool sampleNameMatches(const String& query,const String& name,const String& keywords){auto tokens=StringArray::fromTokens(query.trim().toLowerCase()," \t\r\n","");auto text=(name+" "+keywords).toLowerCase();for(const auto& token:tokens)if(token.isNotEmpty()&&!text.contains(token))return false;return true;}
bool isSampleFavorite(const StringArray& files,const File& audio){auto path=audio.getFullPathName();for(const auto& existing:files)if(File::isAbsolutePath(existing)&&File(existing).getFullPathName().equalsIgnoreCase(path))return true;return false;}
String sampleFavoritesToken(const File& file){if(!file.exists())return "missing";if(!file.existsAsFile())return "not-file";if(file.getSize()>maxSampleFavoritesBytes)return "oversized";return SHA256(file).toHexString();}
Result loadSampleFavorites(const File& file,StringArray& output){
    if(!file.exists()){output.clear();return Result::ok();}if(!file.existsAsFile()||file.getSize()>maxSampleFavoritesBytes)return Result::fail("Sample favorites catalog is unavailable or too large.");
    auto value=JSON::parse(file.loadFileAsString());if(!value.isObject()||!value["schema"].isInt()||(int)value["schema"]!=1||!value["files"].isArray())return Result::fail("Invalid sample favorites catalog. Existing file retained.");
    const auto* rows=value["files"].getArray();if(rows->size()>maxSampleFavorites)return Result::fail("Maximum 256 favorite sample files.");StringArray next;for(const auto& row:*rows){if(!row.isString())return Result::fail("Invalid favorite sample path.");next.add(row.toString());}
    auto valid=validate(next);if(valid.failed())return valid;for(auto& path:next)path=File(path).getFullPathName();output=std::move(next);return Result::ok();
}
Result saveSampleFavorites(const File& file,const StringArray& files,const String& expected){
    auto valid=validate(files);if(valid.failed())return valid;auto token=sampleFavoritesToken(file);if(expected.isNotEmpty()&&token!=expected)return Result::fail("Sample favorites changed. Reopen the sample menu.");StringArray current;valid=loadSampleFavorites(file,current);if(valid.failed())return valid;
    auto* value=new DynamicObject();value->setProperty("schema",1);Array<var> paths;for(const auto& path:files)paths.add(File(path).getFullPathName());value->setProperty("files",paths);auto text=JSON::toString(var(value),false);
    if(text.getNumBytesAsUTF8()>maxSampleFavoritesBytes)return Result::fail("Sample favorites catalog exceeds 1 MiB.");valid=file.getParentDirectory().createDirectory();if(valid.failed())return valid;
    TemporaryFile temporary(file);if(!temporary.getFile().replaceWithText(text))return Result::fail("Cannot write sample favorites temporary file.");if(sampleFavoritesToken(file)!=token)return Result::fail("Sample favorites changed during save. Existing file retained.");if(!temporary.overwriteTargetFileWithTemporary())return Result::fail("Cannot save sample favorites. Existing file retained.");return Result::ok();
}
Result editSampleFavorite(const File& catalog,const File& audio,bool add,const String& expected){
    auto token=sampleFavoritesToken(catalog);if(expected.isNotEmpty()&&expected!=token)return Result::fail("Sample favorites changed. Reopen the sample menu.");StringArray files;auto valid=loadSampleFavorites(catalog,files);if(valid.failed())return valid;
    bool present=isSampleFavorite(files,audio);if(add){if(!audio.existsAsFile()||!audio.hasFileExtension("wav;aif;aiff;flac;ogg"))return Result::fail("Choose an available WAV, AIFF, FLAC or Ogg file to favorite.");if(present)return Result::ok();files.add(audio.getFullPathName());}
    else{if(!present)return Result::ok();for(int i=files.size()-1;i>=0;--i)if(File(files[i])==audio)files.remove(i);}
    return saveSampleFavorites(catalog,files,token);
}
}
