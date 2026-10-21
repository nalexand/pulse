// SPDX-License-Identifier: AGPL-3.0-only
#include "RecoveryRetention.h"
#include <algorithm>
namespace pulse {
using namespace juce;
namespace {
bool cancelled(const std::atomic<bool>* cancel){return cancel&&cancel->load();}
Result ordinaryHistory(const File& root){
    if(root==File()||!root.isDirectory()||root.isSymbolicLink())return Result::fail("Workspace must be an ordinary local folder.");
    for(auto folder:{root.getChildFile("data"),root.getChildFile("data/History")})if(folder.exists()&&(!folder.isDirectory()||folder.isSymbolicLink()))return Result::fail("Recovery History must be an ordinary local folder.");
    return Result::ok();
}
bool managed(const File& file){auto name=file.getFileName();return name.startsWith("Recovery_")&&name.endsWith(".pulse")&&name.length()==79&&name.substring(9,73).containsOnly("0123456789abcdef");}
bool same(const RecoveryCleanupPlan& a,const RecoveryCleanupPlan& b){return a.archives==b.archives&&a.remove==b.remove&&a.totalBytes==b.totalBytes&&a.removeBytes==b.removeBytes;}
bool integer(const var& value,int low,int high){return (value.isInt()||value.isInt64())&&(int64)value>=low&&(int64)value<=high;}
}
Result validateRecoveryRetention(const RecoveryRetentionPolicy& policy){
    if(policy.keepNewest<2||policy.keepNewest>2000||policy.maxAgeDays<0||policy.maxAgeDays>3650||policy.maxMiB<1||policy.maxMiB>32768)return Result::fail("Keep 2-2000 copies, age 0-3650 days (0 disables age), and size 1-32768 MiB. The newest two verified copies are always protected.");return Result::ok();
}
Result loadRecoveryRetention(const File& file,RecoveryRetentionPolicy& output){
    if(!file.exists())return Result::ok();if(!file.existsAsFile()||file.isSymbolicLink()||file.getSize()>16384)return Result::fail("Recovery retention preferences are unavailable.");auto value=JSON::parse(file.loadFileAsString());
    if(!value.getDynamicObject()||!integer(value["schema"],1,1)||!integer(value["keepNewest"],2,2000)||!integer(value["maxAgeDays"],0,3650)||!integer(value["maxMiB"],1,32768)||!value["automatic"].isBool())return Result::fail("Invalid recovery retention preferences; existing settings retained.");
    RecoveryRetentionPolicy next;next.keepNewest=(int)value["keepNewest"];next.maxAgeDays=(int)value["maxAgeDays"];next.maxMiB=(int)value["maxMiB"];next.automatic=(bool)value["automatic"];output=next;return Result::ok();
}
Result saveRecoveryRetention(const File& file,const RecoveryRetentionPolicy& policy){
    auto result=validateRecoveryRetention(policy);if(result.failed())return result;if(file.getParentDirectory().isSymbolicLink()||file.isSymbolicLink()||(file.exists()&&!file.existsAsFile()))return Result::fail("Recovery retention preferences are not writable.");result=file.getParentDirectory().createDirectory();if(result.failed())return result;
    DynamicObject::Ptr object=new DynamicObject();object->setProperty("schema",1);object->setProperty("keepNewest",policy.keepNewest);object->setProperty("maxAgeDays",policy.maxAgeDays);object->setProperty("maxMiB",policy.maxMiB);object->setProperty("automatic",policy.automatic);auto text=JSON::toString(var(object),false);TemporaryFile staged(file);auto stream=staged.getFile().createOutputStream();if(!stream||!stream->writeText(text,false,false,nullptr))return Result::fail("Cannot write retention preferences.");stream->flush();result=stream->getStatus();stream.reset();if(result.failed())return result;
    RecoveryRetentionPolicy read;result=loadRecoveryRetention(staged.getFile(),read);if(result.failed()||read!=policy)return Result::fail("Retention preference verification failed.");if(!staged.overwriteTargetFileWithTemporary())return Result::fail("Cannot publish retention preferences.");return Result::ok();
}
Result planRecoveryCleanup(const File& workspace,const RecoveryRetentionPolicy& policy,RecoveryCleanupPlan& output,const std::atomic<bool>* cancel,int64 now){
    auto result=validateRecoveryRetention(policy);if(result.failed())return result;result=ordinaryHistory(workspace);if(result.failed())return result;RecoveryCleanupPlan next;next.workspace=workspace;next.policy=policy;next.evaluatedAt=now>0?now:Time::currentTimeMillis();auto history=workspace.getChildFile("data/History");int visited=0;
    if(history.exists())for(const auto& entry:RangedDirectoryIterator(history,false,"*",File::findFilesAndDirectories,File::FollowSymlinks::no)){
        if(cancelled(cancel))return Result::fail("Recovery cleanup cancelled; remaining files retained.");if(++visited>5000)return Result::fail("History scan limit reached (5000 entries); no cleanup plan was published.");auto file=entry.getFile();if(!managed(file)||file.isSymbolicLink()||!file.existsAsFile()||file.getSize()<=0||file.getSize()>32LL*1024*1024){++next.excluded;continue;}
        auto bytes=file.getSize(),modified=file.getLastModificationTime().toMilliseconds();auto token=projectFileToken(file);Project project;
        if(token!=file.getFileName().substring(9,73)||loadProject(file,project).failed()||file.getSize()!=bytes||file.getLastModificationTime().toMilliseconds()!=modified||projectFileToken(file)!=token){++next.excluded;continue;}
        next.archives.push_back({file,token,bytes,modified});next.totalBytes+=bytes;
    }
    if(cancelled(cancel))return Result::fail("Recovery cleanup cancelled; remaining files retained.");
    std::sort(next.archives.begin(),next.archives.end(),[](const auto& a,const auto& b){if(a.modified!=b.modified)return a.modified>b.modified;return a.file.getFileName()<b.file.getFileName();});
    auto remaining=next.totalBytes;int count=(int)next.archives.size();auto maxBytes=(int64)policy.maxMiB*1024*1024;auto oldestAllowed=next.evaluatedAt-(int64)policy.maxAgeDays*86400000;
    // Delete oldest first; count/size are limits on the retained set, while age
    // expires copies independently. Never sacrifice the newest two for a quota.
    for(int i=(int)next.archives.size()-1;i>=2;--i){const auto& archive=next.archives[(size_t)i];if(count>policy.keepNewest||remaining>maxBytes||(policy.maxAgeDays>0&&archive.modified<oldestAllowed)){next.remove.push_back(i);next.removeBytes+=archive.bytes;remaining-=archive.bytes;--count;}}
    output=std::move(next);return Result::ok();
}
Result applyRecoveryCleanup(const RecoveryCleanupPlan& plan,RecoveryCleanupResult& output,const std::atomic<bool>* cancel){
    output={};RecoveryCleanupPlan fresh;auto result=planRecoveryCleanup(plan.workspace,plan.policy,fresh,cancel,plan.evaluatedAt);if(result.failed())return result;if(!same(plan,fresh))return Result::fail("Recovery History changed. Review a fresh cleanup plan; no files removed.");auto history=plan.workspace.getChildFile("data/History");
    for(int index:plan.remove){
        if(cancelled(cancel))return Result::fail("Recovery cleanup cancelled; remaining files retained.");result=ordinaryHistory(plan.workspace);if(result.failed())return result;
        for(size_t i=0;i<jmin((size_t)2,plan.archives.size());++i){const auto& keep=plan.archives[i];if(keep.file.getParentDirectory()!=history||keep.file.isSymbolicLink()||projectFileToken(keep.file)!=keep.token)return Result::fail("A protected recovery changed; remaining files retained.");}
        if(index<2||index>=(int)plan.archives.size())return Result::fail("Invalid cleanup index; remaining copies retained.");const auto& archive=plan.archives[(size_t)index];if(archive.file.getParentDirectory()!=history||!managed(archive.file)||archive.file.isSymbolicLink()||archive.file.getSize()!=archive.bytes||archive.file.getLastModificationTime().toMilliseconds()!=archive.modified||projectFileToken(archive.file)!=archive.token)return Result::fail("A recovery copy changed; remaining files retained.");
        if(!archive.file.deleteFile())return Result::fail("Could not remove "+archive.file.getFileName()+"; remaining files retained.");++output.removed;output.bytes+=archive.bytes;
    }
    return Result::ok();
}
String recoveryCleanupSummary(const RecoveryCleanupPlan& plan){
    return String((int)plan.archives.size())+" verified archived copies ("+File::descriptionOfSizeInBytes(plan.totalBytes)+").\nRemove "+String((int)plan.remove.size())+" oldest copies ("+File::descriptionOfSizeInBytes(plan.removeBytes)+"); keep "+String((int)(plan.archives.size()-plan.remove.size()))+".\nNewest two verified copies always stay, even if the limits cannot be met. "+String(plan.excluded)+" unrelated, damaged or linked entries excluded.\nOnly data/History/Recovery_<hash>.pulse files are eligible. Current recovery, .bak files, manual versions, pre-operation backups and samples are retained.";
}
}
