// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectArchive.h"
#include "ProjectAssets.h"
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <set>
namespace pulse {
using namespace juce;
namespace {
constexpr int64 projectLimit=32LL*1024*1024,manifestLimit=256*1024;
bool cancelled(const std::atomic<bool>* flag){return flag&&flag->load();}
Result stopped(){return Result::fail("Project archive cancelled. Existing files retained.");}
bool hashName(const String& name){return name.length()==64&&name.containsOnly("0123456789abcdef");}
bool sampleName(const String& name){if(!name.startsWith("Samples/"))return false;auto leaf=name.substring(8);int dot=leaf.indexOfChar('.');return dot==64&&hashName(leaf.substring(0,dot))&&StringArray{".wav",".aif",".aiff",".flac",".ogg"}.contains(leaf.substring(dot));}
String targetToken(const File& file){return !file.exists()?String("missing"):file.existsAsFile()?String(file.getSize())+"|"+String(file.getLastModificationTime().toMilliseconds())+"|"+SHA256(file).toHexString():String("not-file");}
bool entryName(const String& name){return name=="project.pulse"||name=="manifest.json"||sampleName(name);}
struct Stage {
    File folder;
    explicit Stage(const File& parent):folder(parent.getChildFile(".pulse-archive-"+Uuid().toString())){}
    ~Stage(){if(folder.isDirectory()&&!folder.isSymbolicLink())folder.deleteRecursively();}
};
Result copyBounded(InputStream& input,OutputStream& output,int64 length,const std::atomic<bool>* cancel){
    char block[65536];int64 left=length;
    while(left>0){if(cancelled(cancel))return stopped();int read=input.read(block,(int)jmin<int64>(left,sizeof(block)));if(read<=0)return Result::fail("Incomplete archive entry or source audio.");if(!output.write(block,(size_t)read))return Result::fail("Cannot write project archive data.");left-=read;}
    char extra;if(input.read(&extra,1)!=0)return Result::fail("Archive entry size changed or exceeds its declared bound.");if(cancelled(cancel))return stopped();return Result::ok();
}
class CancelOutput final:public OutputStream {
public:
    CancelOutput(FileOutputStream& destination,const std::atomic<bool>* flag):out(destination),cancel(flag){}
    void flush() override{out.flush();}
    int64 getPosition() override{return out.getPosition();}
    bool setPosition(int64 at) override{return out.setPosition(at);}
    bool write(const void* data,size_t size) override{return !cancelled(cancel)&&getPosition()<=maxProjectArchiveBytes-(int64)size&&out.write(data,size);}
private:FileOutputStream& out;const std::atomic<bool>* cancel;
};
Result verifyFiles(ZipFile& zip,DynamicObject*& manifest, var& storage){
    if(zip.getNumEntries()<2||zip.getNumEntries()>maxProjectArchiveEntries)return Result::fail("Project ZIP entry count is outside limits.");
    std::set<String> names;int64 total=0;
    for(int i=0;i<zip.getNumEntries();++i){const auto& e=*zip.getEntry(i);if(e.isSymbolicLink||!entryName(e.filename)||!names.insert(e.filename.toLowerCase()).second)return Result::fail("Project ZIP has an unsupported, duplicate or unsafe entry name.");auto limit=e.filename=="manifest.json"?manifestLimit:e.filename=="project.pulse"?projectLimit:maxProjectArchiveBytes;
        if(e.uncompressedSize<=0||e.uncompressedSize>limit||total>maxProjectArchiveBytes-e.uncompressedSize)return Result::fail("Project ZIP exceeds its expanded-size limits.");total+=e.uncompressedSize;}
    int index=zip.getIndexOfFileName("manifest.json");if(index<0||zip.getIndexOfFileName("project.pulse")<0)return Result::fail("This is not a Pulse Studio project archive.");
    std::unique_ptr<InputStream> stream(zip.createStreamForEntry(index));if(!stream)return Result::fail("Cannot read project archive manifest.");MemoryOutputStream text;auto result=copyBounded(*stream,text,zip.getEntry(index)->uncompressedSize,nullptr);if(result.failed())return result;
    storage=JSON::parse(text.toUTF8());manifest=storage.getDynamicObject();if(!manifest||!storage["schema"].isInt()||(int)storage["schema"]!=1||storage["format"].toString()!="pulse-project-archive"||!storage["files"].isObject())return Result::fail("Unsupported project archive manifest.");
    auto& files=storage["files"].getDynamicObject()->getProperties();if(files.size()!=zip.getNumEntries()-1)return Result::fail("Archive manifest does not cover every file.");
    for(int i=0;i<files.size();++i){auto name=files.getName(i).toString();if(name=="manifest.json"||!entryName(name)||zip.getIndexOfFileName(name)<0||!files.getValueAt(i).isString()||!hashName(files.getValueAt(i).toString()))return Result::fail("Invalid archive checksum manifest.");}
    return Result::ok();
}
Result readArchive(const File& archive,const File& staging,Project& project,const std::atomic<bool>* cancel){
    if(cancelled(cancel))return stopped();if(!archive.existsAsFile()||archive.getSize()>maxProjectArchiveBytes)return Result::fail("Missing project ZIP or archive larger than 512 MiB.");ZipFile zip(archive);DynamicObject* metadata=nullptr;var manifest;auto result=verifyFiles(zip,metadata,manifest);if(result.failed())return result;
    result=staging.createDirectory();if(result.failed())return result;
    for(int i=0;i<zip.getNumEntries();++i){if(cancelled(cancel))return stopped();const auto& entry=*zip.getEntry(i);if(entry.filename=="manifest.json")continue;auto file=staging.getChildFile(entry.filename);result=file.getParentDirectory().createDirectory();if(result.failed())return result;std::unique_ptr<InputStream> input(zip.createStreamForEntry(i));if(!input)return Result::fail("Cannot read archive entry.");
        {FileOutputStream output(file);if(!output.openedOk())return output.getStatus();result=copyBounded(*input,output,entry.uncompressedSize,cancel);output.flush();if(result.failed())return result;if(output.getStatus().failed())return output.getStatus();}
        auto hash=SHA256(file).toHexString();if(hash!=manifest["files"][Identifier(entry.filename)].toString()||(sampleName(entry.filename)&&hash!=entry.filename.substring(8,72)))return Result::fail("Archive checksum differs: "+entry.filename);
    }
    result=loadProject(staging.getChildFile("project.pulse"),project);if(result.failed())return result;std::set<String> referenced;
    auto reference=[&](const String& path){if(!sampleName(path)||zip.getIndexOfFileName(path)<0)return false;referenced.insert(path);return true;};
    for(const auto& track:project.tracks)if(track.samplePath.isNotEmpty()&&!reference(track.samplePath))return Result::fail("Archive project refers to audio outside its Samples folder.");
    for(const auto& clip:project.clips)if(clip.isAudio()&&!reference(clip.audioPath))return Result::fail("Archive clip refers to audio outside its Samples folder.");
    if((int)referenced.size()!=zip.getNumEntries()-2)return Result::fail("Archive contains audio not referenced by its project.");
    auto rows=projectAssets(project,staging);inspectProjectAssets(rows,project,cancel);for(const auto& row:rows)if(row.issue!=AssetIssue::ready&&row.issue!=AssetIssue::emptySampler)return Result::fail(row.file.getFileName()+": "+row.detail);
    if(cancelled(cancel))return stopped();return Result::ok();
}
}
Result exportProjectArchive(const Project& source,const File& base,const File& target,const std::atomic<bool>* cancel){
    if(cancelled(cancel))return stopped();if(target.exists()&&!target.existsAsFile())return Result::fail("Archive destination is not a writable file.");auto initialTarget=targetToken(target);Project validated;auto result=projectFromJson(projectToJson(source),validated);if(result.failed())return result;validated=absoluteAssets(validated,base);auto rows=projectAssets(validated,base);inspectProjectAssets(rows,validated,cancel);if(cancelled(cancel))return stopped();
    for(const auto& row:rows)if(row.issue!=AssetIssue::ready&&row.issue!=AssetIssue::emptySampler)return Result::fail("Cannot package "+row.file.getFileName()+": "+row.detail);
    // A project ZIP may never replace one of the samples it is collecting.
    for(const auto& row:rows)if(row.file==target)return Result::fail("Choose an archive destination different from its source audio.");
    result=target.getParentDirectory().createDirectory();if(result.failed())return result;Stage stage(target.getParentDirectory());result=stage.folder.createDirectory();if(result.failed())return result;result=stage.folder.getChildFile("Samples").createDirectory();if(result.failed())return result;
    int64 total=0;std::map<String,File> files;Project packed=validated;
    for(const auto& row:rows){if(row.file==File())continue;if(cancelled(cancel))return stopped();auto size=row.file.getSize();if(size<=0||size>maxProjectArchiveBytes )return Result::fail("Project samples exceed the 512 MiB archive limit.");
        auto copy=stage.folder.getChildFile("sample.tmp");{FileInputStream input(row.file);FileOutputStream output(copy);if(!input.openedOk()||!output.openedOk())return Result::fail("Cannot read or stage a project sample.");result=copyBounded(input,output,size,cancel);output.flush();if(result.failed())return result;if(output.getStatus().failed())return output.getStatus();}
        auto name="Samples/"+SHA256(copy).toHexString()+row.file.getFileExtension().toLowerCase();if(!sampleName(name))return Result::fail("Unsupported sample format in project archive.");
        if(!files.count(name)){if(total>maxProjectArchiveBytes-size)return Result::fail("Project samples exceed the 512 MiB archive limit.");auto destination=stage.folder.getChildFile(name);if(!copy.moveFileTo(destination))return Result::fail("Cannot stage archive sample.");files.emplace(name,destination);total+=size;}else copy.deleteFile();
        for(auto use:row.uses){if(use.clip)packed.clips[(size_t)use.index].audioPath=name;else packed.tracks[(size_t)use.index].samplePath=name;}
    }
    auto projectFile=stage.folder.getChildFile("project.pulse");result=saveProject(packed,projectFile);if(result.failed())return result;if(projectFile.getSize()>projectLimit)return Result::fail("Project JSON exceeds 32 MiB.");files.emplace("project.pulse",projectFile);auto* hashes=new DynamicObject();for(const auto& [name,file]:files)hashes->setProperty(name,SHA256(file).toHexString());auto* manifest=new DynamicObject();manifest->setProperty("schema",1);manifest->setProperty("format","pulse-project-archive");manifest->setProperty("files",var(hashes));auto manifestFile=stage.folder.getChildFile("manifest.json");if(!manifestFile.replaceWithText(JSON::toString(var(manifest),false)))return Result::fail("Cannot stage archive manifest.");files.emplace("manifest.json",manifestFile);
    ZipFile::Builder builder;for(const auto& [name,file]:files)builder.addFile(file,6,name);TemporaryFile temporary(target);
    {FileOutputStream output(temporary.getFile());if(!output.openedOk())return output.getStatus();CancelOutput guarded(output,cancel);bool ok=builder.writeToStream(guarded,nullptr);guarded.flush();if(cancelled(cancel))return stopped();if(!ok||output.getStatus().failed())return Result::fail("Cannot write project ZIP within its 512 MiB limit.");}
    // Read the actual ZIP back before replacing any existing destination.
    Stage verify(target.getParentDirectory());Project checked;result=readArchive(temporary.getFile(),verify.folder,checked,cancel);if(result.failed())return result;if(cancelled(cancel))return stopped();if(targetToken(target)!=initialTarget)return Result::fail("Archive destination changed during packaging. Newer file retained.");if(!temporary.overwriteTargetFileWithTemporary())return Result::fail("Cannot finalize project ZIP. Existing file retained.");return Result::ok();
}
Result importProjectArchive(const File& archive,const File& destination,File& projectFile,const std::atomic<bool>* cancel){
    if(cancelled(cancel))return stopped();if(destination.exists()||destination.isSymbolicLink())return Result::fail("Import destination already exists. Choose a new folder.");auto result=destination.getParentDirectory().createDirectory();if(result.failed())return result;Stage stage(destination.getParentDirectory());Project checked;result=readArchive(archive,stage.folder,checked,cancel);if(result.failed())return result;if(cancelled(cancel))return stopped();if(destination.exists()||!stage.folder.moveFileTo(destination))return Result::fail("Cannot publish imported project folder. Existing files retained.");projectFile=destination.getChildFile("project.pulse");return Result::ok();
}
}
