// SPDX-License-Identifier: AGPL-3.0-only
#include "AssetFingerprints.h"
#include <set>
namespace pulse {
using namespace juce;
namespace {
constexpr int64 catalogLimit=4LL*1024*1024;
bool stopped(const std::atomic<bool>* flag){return flag&&flag->load();}
bool validHash(const String& value){return value.length()==64&&value.containsOnly("0123456789abcdef");}
Result validate(const AssetFingerprints& rows){
    if(rows.size()>maxAssetFingerprints)return Result::fail("Fingerprint cache exceeds 1024 sources.");std::set<String> seen;
    for(const auto& row:rows){if(row.path.isEmpty()||row.path.length()>4096||!File::isAbsolutePath(row.path)||!validHash(row.hash)||row.bytes<=0||row.bytes>maxFingerprintFileBytes||row.modified<0||row.seen<0)return Result::fail("Invalid audio fingerprint record.");if(!seen.insert(assetPathKey(File(row.path))).second)return Result::fail("Duplicate source in audio fingerprint cache.");}return Result::ok();
}
class ObservedStream final:public InputStream {
public:
    ObservedStream(FileInputStream& file,const std::atomic<bool>* flag):source(file),cancel(flag){}
    int64 getTotalLength()override{return source.getTotalLength();}
    int64 getPosition()override{return source.getPosition();}
    bool setPosition(int64 value)override{return source.setPosition(value);}
    bool isExhausted()override{return stopped(cancel)||source.isExhausted();}
    int read(void* target,int amount)override{if(stopped(cancel))return 0;int read=source.read(target,jmin(amount,65536));consumed+=read;return read;}
    int64 consumed=0;
private:FileInputStream& source;const std::atomic<bool>* cancel;
};
}
String assetPathKey(const File& file){auto path=file.getFullPathName();
    #if JUCE_WINDOWS
    path=path.toLowerCase();
    #endif
    return path;
}
String collectedAssetHash(const File& file){auto name=file.getFileNameWithoutExtension().toLowerCase();return validHash(name)?name:String();}
Result fingerprintAssetFile(const File& file,AssetFingerprint& output,const std::atomic<bool>* cancel){
    if(stopped(cancel))return Result::fail("Fingerprint cancelled.");if(!file.existsAsFile()||file.isSymbolicLink())return Result::fail("Audio source is not an ordinary file.");
    auto bytes=file.getSize(),modified=file.getLastModificationTime().toMilliseconds();auto identity=file.getFileIdentifier();if(bytes<=0||bytes>maxFingerprintFileBytes)return Result::fail("Fingerprint file limit is 512 MiB.");
    FileInputStream input(file);if(input.failedToOpen())return Result::fail("Cannot read audio for fingerprinting.");ObservedStream observed(input,cancel);auto hash=SHA256(observed,bytes).toHexString();
    if(stopped(cancel))return Result::fail("Fingerprint cancelled.");if(observed.consumed!=bytes||input.getStatus().failed()||file.isSymbolicLink()||file.getSize()!=bytes||file.getLastModificationTime().toMilliseconds()!=modified||file.getFileIdentifier()!=identity)return Result::fail("Audio changed while fingerprinting; refresh to try again.");
    output={file.getFullPathName(),hash,bytes,modified,Time::currentTimeMillis()};return Result::ok();
}
String assetFingerprintToken(const File& file){if(!file.exists())return "missing";if(!file.existsAsFile()||file.isSymbolicLink()||file.getParentDirectory().isSymbolicLink()||file.getSize()<0||file.getSize()>catalogLimit)return "unavailable";FileInputStream input(file);if(input.failedToOpen())return "unavailable";return SHA256(input).toHexString();}
Result loadAssetFingerprints(const File& file,AssetFingerprints& output){
    auto token=assetFingerprintToken(file);if(token=="missing"){output.clear();return Result::ok();}if(token=="unavailable")return Result::fail("Audio fingerprint cache is unavailable; existing cache retained.");
    var json;auto parsed=JSON::parse(file.loadFileAsString(),json);if(parsed.failed()||!json.isObject()||!json["schema"].isInt()||(int)json["schema"]!=1||!json["sources"].isArray())return Result::fail("Invalid audio fingerprint cache; existing cache retained.");
    const auto& values=*json["sources"].getArray();if(values.size()>maxAssetFingerprints)return Result::fail("Audio fingerprint cache exceeds 1024 sources.");AssetFingerprints next;
    for(const auto& row:values){if(!row.isObject()||!row["path"].isString()||!row["sha256"].isString())return Result::fail("Invalid audio fingerprint fields.");for(const char* key:{"bytes","modified","seen"})if(!(row[key].isInt()||row[key].isInt64()))return Result::fail("Audio fingerprint sizes and dates must be integers.");next.push_back({row["path"].toString(),row["sha256"].toString(),(int64)row["bytes"],(int64)row["modified"],(int64)row["seen"]});}
    auto valid=validate(next);if(valid.failed())return valid;if(assetFingerprintToken(file)!=token)return Result::fail("Audio fingerprint cache changed during reading.");output=std::move(next);return Result::ok();
}
Result saveAssetFingerprints(const File& file,const AssetFingerprints& rows,const String& expected,const std::atomic<bool>* cancel){
    auto valid=validate(rows);if(valid.failed())return valid;if(stopped(cancel))return Result::fail("Fingerprint cache update cancelled.");if(expected=="unavailable"||assetFingerprintToken(file)!=expected)return Result::fail("Audio fingerprint cache changed; existing cache retained.");
    if(file.getParentDirectory().isSymbolicLink()||file.isSymbolicLink())return Result::fail("Fingerprint cache cannot follow links.");valid=file.getParentDirectory().createDirectory();if(valid.failed())return valid;
    DynamicObject::Ptr object=new DynamicObject();object->setProperty("schema",1);Array<var> sources;for(const auto& row:rows){DynamicObject::Ptr source=new DynamicObject();source->setProperty("path",row.path);source->setProperty("sha256",row.hash);source->setProperty("bytes",row.bytes);source->setProperty("modified",row.modified);source->setProperty("seen",row.seen);sources.add(var(source));}object->setProperty("sources",sources);auto text=JSON::toString(var(object),true);if(text.getNumBytesAsUTF8()>catalogLimit)return Result::fail("Audio fingerprint cache exceeds 4 MiB; existing cache retained.");
    TemporaryFile staged(file);auto stream=staged.getFile().createOutputStream();if(!stream||!stream->writeText(text,false,false,nullptr))return Result::fail("Cannot stage audio fingerprint cache.");stream->flush();valid=stream->getStatus();stream.reset();if(valid.failed())return valid;AssetFingerprints read;valid=loadAssetFingerprints(staged.getFile(),read);if(valid.failed()||read!=rows)return Result::fail("Audio fingerprint cache verification failed.");
    if(stopped(cancel))return Result::fail("Fingerprint cache update cancelled.");if(assetFingerprintToken(file)!=expected)return Result::fail("Audio fingerprint cache changed; existing cache retained.");if(!staged.overwriteTargetFileWithTemporary())return Result::fail("Cannot publish audio fingerprint cache; existing cache retained.");return Result::ok();
}
}
