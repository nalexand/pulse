// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectPersistence.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
namespace pulse {
using namespace juce;
namespace {
constexpr int64 fileLimit=32*1024*1024;
Result readBytes(const File& file,MemoryBlock& output){
    if(!file.existsAsFile()||file.isSymbolicLink()||file.getSize()<0||file.getSize()>fileLimit)return Result::fail("Missing, empty, linked or oversized project file (32 MiB maximum).");
    auto input=file.createInputStream();if(!input||input->getStatus().failed())return Result::fail("Cannot read project file.");auto size=input->getTotalLength();if(size<0||size>fileLimit)return Result::fail("Invalid project file size.");MemoryBlock next((size_t)size);if(input->read(next.getData(),(int)size)!=size||input->getStatus().failed()||!input->isExhausted())return Result::fail("Project read was incomplete or the file changed.");output=std::move(next);return Result::ok();
}
String digest(const MemoryBlock& bytes){return SHA256(bytes).toHexString();}
String changedField(const var& a,const var& b,const String& path){
    auto numeric=[](const var& v){return v.isInt()||v.isInt64()||v.isDouble();};
    if(numeric(a)&&numeric(b))return std::isfinite((double)a)&&std::isfinite((double)b)&&(double)a==(double)b?String():path;
    if(a.getDynamicObject()!=nullptr&&b.getDynamicObject()!=nullptr){const auto& properties=a.getDynamicObject()->getProperties();if(properties.size()!=b.getDynamicObject()->getProperties().size())return path;for(int i=0;i<properties.size();++i){auto key=properties.getName(i);if(!b.hasProperty(key))return path+"."+key.toString();auto changed=changedField(properties.getValueAt(i),b[key],path+"."+key.toString());if(changed.isNotEmpty())return changed;}return {};}
    if(a.isArray()&&b.isArray()){auto* first=a.getArray();auto* second=b.getArray();if(first->size()!=second->size())return path;for(int i=0;i<first->size();++i){auto changed=changedField((*first)[i],(*second)[i],path+"["+String(i)+"]");if(changed.isNotEmpty())return changed;}return {};}
    return a==b?String():path;
}
Result parseBytes(const MemoryBlock& bytes,Project& output){var json;auto parsed=JSON::parse(String::createStringFromData(bytes.getData(),(int)bytes.getSize()),json);return parsed.failed()?Result::fail("Project JSON is damaged: "+parsed.getErrorMessage()):projectFromJson(json,output);}
Result writeBytes(const File& file,const MemoryBlock& bytes){auto output=file.createOutputStream();if(!output||output->getStatus().failed())return Result::fail("Cannot create project staging file.");if(!output->write(bytes.getData(),bytes.getSize()))return Result::fail("Project write failed; the original file is retained.");output->flush();return output->getStatus();}
Result preserveBytes(const File& target,const MemoryBlock& bytes){
    TemporaryFile temporary(target);auto result=writeBytes(temporary.getFile(),bytes);if(result.failed())return result;MemoryBlock checked;result=readBytes(temporary.getFile(),checked);if(result.failed()||digest(checked)!=digest(bytes))return Result::fail("Backup verification failed; original project retained.");return temporary.overwriteTargetFileWithTemporary()?Result::ok():Result::fail("Cannot commit the project backup; original retained.");
}
}
File projectBackupFile(const File& file){return file.getSiblingFile(file.getFileName()+".bak");}
String projectFileToken(const File& file){if(!file.exists())return "missing";MemoryBlock bytes;return readBytes(file,bytes).wasOk()?digest(bytes):"unavailable";}
Result loadProject(const File& file,Project& output){
    if(file.hasFileExtension("flp"))return Result::fail("FL Studio .flp import is not implemented. This build opens .pulse projects and imports MIDI or audio samples.");
    MemoryBlock bytes;auto result=readBytes(file,bytes);if(result.failed())return result;Project next;result=parseBytes(bytes,next);if(result.failed())return result;if(projectFileToken(file)!=digest(bytes))return Result::fail("Project changed while reading. Reopen the file.");output=std::move(next);return Result::ok();
}
Result saveProject(const Project& project,const File& file){return saveProjectChecked(project,file);}
Result saveProjectChecked(const Project& project,const File& file,const ProjectSaveControl* control){
    // Validate the complete serialized representation before touching any file.
    auto representation=projectToJson(project);auto text=JSON::toString(representation,false,22);auto bytes=MemoryBlock(text.toRawUTF8(),text.getNumBytesAsUTF8());if(bytes.getSize()>fileLimit)return Result::fail("Project exceeds the 32 MiB save limit. Existing files retained.");Project validated;auto result=parseBytes(bytes,validated);if(result.failed())return Result::fail("Project cannot be saved: "+result.getErrorMessage());
    auto roundtrip=projectToJson(validated);for(auto* value:{&representation,&roundtrip})if(auto* markers=(*value)["markers"].getArray())std::sort(markers->begin(),markers->end(),[](const var& a,const var& b){return a["id"].toString()<b["id"].toString();});auto changed=changedField(representation,roundtrip,"project");if(changed.isNotEmpty())return Result::fail("Save would repair or truncate "+changed+". Resolve that value before saving; original files retained.");
    auto originalToken=projectFileToken(file);if(originalToken=="unavailable")return Result::fail("Destination cannot be safely read or is not an ordinary project file. Choose Save as.");if(!control&&originalToken==digest(bytes))return Result::ok();result=file.getParentDirectory().createDirectory();if(result.failed())return result;TemporaryFile temporary(file);auto output=temporary.getFile().createOutputStream();if(!output||output->getStatus().failed())return Result::fail("Cannot create project staging file. Original retained.");
    auto checkpoint=[&](ProjectSaveStage stage){return control&&control->checkpoint?control->checkpoint(stage,temporary.getFile()):Result::ok();};
    if(!output->write(bytes.getData(),bytes.getSize()))return Result::fail("Project write failed; original retained.");result=checkpoint(ProjectSaveStage::written);if(result.failed())return result;output->flush();result=output->getStatus();if(result.failed())return Result::fail("Project flush failed; original retained: "+result.getErrorMessage());result=checkpoint(ProjectSaveStage::flushed);if(result.failed())return result;output.reset();
    MemoryBlock checked;result=readBytes(temporary.getFile(),checked);if(result.failed()||digest(checked)!=digest(bytes))return Result::fail("Project staging verification failed. Original retained.");Project readback;result=parseBytes(checked,readback);if(result.failed())return Result::fail("Project staging validation failed. Original retained.");result=checkpoint(ProjectSaveStage::verified);if(result.failed())return result;
    if(projectFileToken(file)!=originalToken)return Result::fail("Project destination changed during save. External changes retained; save under a new name.");
    if(originalToken!="missing"){
        MemoryBlock previous;result=readBytes(file,previous);if(result.failed()||digest(previous)!=originalToken)return Result::fail("Previous project changed before backup. Save cancelled.");Project old;
        // Keep the last valid .bak when the primary was damaged externally.
        bool validPrevious=parseBytes(previous,old).wasOk();auto backup=validPrevious?projectBackupFile(file):file.getSiblingFile(file.getFileName()+".damaged-"+originalToken);
        if(backup.isSymbolicLink()||backup.isDirectory())return Result::fail("Backup destination is unavailable. Original project retained.");auto backupToken=projectFileToken(backup);if(!validPrevious&&backup.exists()&&backupToken!=originalToken)return Result::fail("Damaged-file archive changed. Existing evidence and original project retained.");if(backupToken!=originalToken){result=preserveBytes(backup,previous);if(result.failed())return result;}
    }
    result=checkpoint(ProjectSaveStage::backup);if(result.failed())return result;result=checkpoint(ProjectSaveStage::beforeCommit);if(result.failed())return result;
    checked.reset();result=readBytes(temporary.getFile(),checked);if(result.failed()||digest(checked)!=digest(bytes))return Result::fail("Project staging changed before commit. Original retained.");
    if(projectFileToken(file)!=originalToken)return Result::fail("Project changed before commit. External changes retained; save under a new name.");
    if(!temporary.overwriteTargetFileWithTemporary())return Result::fail("Cannot commit project save. The original and any verified backup are retained.");
    // A successful commit is already complete; post-commit probes may terminate
    // the test process but cannot turn a committed save into a reported failure.
    checkpoint(ProjectSaveStage::committed);return Result::ok();
}
Result loadProjectWithBackup(const File& primary,Project& output,File& used){
    Project next;auto result=loadProject(primary,next);if(result.wasOk()){output=std::move(next);used=primary;return result;}
    // A missing primary can mean a deliberate successful-save cleanup. Never
    // resurrect a stale backup in that case. Atomic replacement retains primary.
    if(!primary.existsAsFile())return result;auto backup=projectBackupFile(primary);auto fallback=loadProject(backup,next);if(fallback.failed())return Result::fail("Latest recovery is unreadable and no valid previous snapshot is available. Files retained. "+result.getErrorMessage());output=std::move(next);used=backup;return Result::ok();
}
}
