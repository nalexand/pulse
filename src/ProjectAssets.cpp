// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectAssets.h"
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <set>
#include <cmath>
namespace pulse {
using namespace juce;
String assetIssueName(AssetIssue issue){switch(issue){case AssetIssue::ready:return "Ready";case AssetIssue::missing:return "Missing";case AssetIssue::unreadable:return "Cannot decode";case AssetIssue::unsupported:return "Outside limits";case AssetIssue::emptySampler:return "No sample chosen";case AssetIssue::invalidRange:return "Range outside file";case AssetIssue::checksumMismatch:return "Checksum differs";}return {};}
std::vector<ProjectAsset> projectAssets(const Project& p,const File& base){
    std::vector<ProjectAsset> rows;std::map<String,size_t> positions;
    auto add=[&](const String& path,AssetUse use){
        File file=path.isEmpty()?File():File::isAbsolutePath(path)?File(path):base.getChildFile(path);
        auto key=path.isEmpty()?"empty:"+String(use.index):file.getFullPathName();
        #if JUCE_WINDOWS
        key=key.toLowerCase();
        #endif
        auto [it,inserted]=positions.emplace(key,rows.size());if(inserted){ProjectAsset row;row.key=key;row.file=file;row.contentHash=collectedAssetHash(file);if(path.isEmpty())row.issue=AssetIssue::emptySampler;rows.push_back(std::move(row));}rows[it->second].uses.push_back(use);
    };
    for(int i=0;i<(int)p.tracks.size();++i){auto& t=p.tracks[(size_t)i];if(t.samplePath.isNotEmpty()||(t.instrument==Instrument::sample&&!t.isBus))add(t.samplePath,{false,i});}
    for(int i=0;i<(int)p.clips.size();++i)if(p.clips[(size_t)i].isAudio())add(p.clips[(size_t)i].audioPath,{true,i});
    return rows;
}
static void inspect(ProjectAsset& row,const Project& p,AudioFormatManager& formats){
    row.rate=0;row.duration=0;row.channels=0;row.detail.clear();
    if(row.file==File()){row.issue=AssetIssue::emptySampler;row.detail="Choose a sample for this sampler channel.";return;}
    if(!row.file.existsAsFile()){row.issue=AssetIssue::missing;row.detail="The referenced audio file is missing or its drive is unavailable.";return;}
    std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(row.file));if(!reader){row.issue=AssetIssue::unreadable;row.detail="This file cannot be decoded as supported audio.";return;}
    row.rate=reader->sampleRate;row.channels=reader->numChannels;row.duration=row.rate>0?reader->lengthInSamples/row.rate:0;
    if(!std::isfinite(row.rate)||row.rate<8000||row.rate>192000||reader->lengthInSamples<=0||(reader->lengthInSamples>row.rate*maxAudioSeconds||(uint64_t)reader->lengthInSamples*8>maxDecodedAudioBytes)){row.issue=AssetIssue::unsupported;row.detail="Audio supports up to 30 minutes at 8-192 kHz, within the 512 MiB decoded-cache budget.";return;}
    for(auto use:row.uses){
        if(use.clip){auto& c=p.clips[(size_t)use.index];if(c.sourceOffset>=row.duration){row.issue=AssetIssue::invalidRange;row.detail="A Playlist source offset starts beyond the end of this file.";return;}}
        else{auto& t=p.tracks[(size_t)use.index];if(t.sampleStart>=row.duration||(t.sampleEnd>row.duration+.5/row.rate)){row.issue=AssetIssue::invalidRange;row.detail="The saved sampler selection does not fit inside this file.";return;}}
    }
    row.issue=AssetIssue::ready;row.detail=String(row.duration,3)+" s  |  "+String(row.rate,0)+" Hz  |  "+String(row.channels)+" channels";
}
void inspectProjectAssets(std::vector<ProjectAsset>& rows,const Project& p,const std::atomic<bool>* cancel){AudioFormatManager formats;formats.registerBasicFormats();for(auto& row:rows){if(cancel&&cancel->load())return;inspect(row,p,formats);}}
String projectAssetInventory(const Project& project,const File& base){StringArray paths;for(const auto& row:projectAssets(project,base))if(row.file!=File())paths.add(row.key);paths.sort(false);return paths.joinIntoString("\n");}
String learnProjectAssetFingerprints(std::vector<ProjectAsset>& rows,AssetFingerprints& catalog,bool force,const std::atomic<bool>* cancel,int64 budget){
    int64 consumed=0;int skipped=0;budget=jlimit((int64)0,maxFingerprintPassBytes,budget);
    std::vector<std::pair<int,size_t>> order;for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];auto key=assetPathKey(row.file);auto known=std::find_if(catalog.begin(),catalog.end(),[&](const auto& f){return assetPathKey(File(f.path))==key;});bool unchanged=known!=catalog.end()&&known->bytes==row.file.getSize()&&known->modified==row.file.getLastModificationTime().toMilliseconds();order.push_back({unchanged?1:0,i});}std::stable_sort(order.begin(),order.end(),[](auto a,auto b){return a.first<b.first;});
    for(auto entry:order){auto& row=rows[entry.second];if(cancel&&cancel->load())return "Fingerprint learning cancelled.";if(row.file==File())continue;auto key=assetPathKey(row.file);auto known=std::find_if(catalog.begin(),catalog.end(),[&](const auto& f){return assetPathKey(File(f.path))==key;});
        if(row.issue!=AssetIssue::ready){if(known!=catalog.end()&&(row.contentHash.isEmpty()||row.contentHash==known->hash)){row.contentHash=known->hash;row.contentBytes=known->bytes;}continue;}
        if(row.file.isSymbolicLink()){++skipped;continue;}auto namedHash=collectedAssetHash(row.file);if(known!=catalog.end()&&(namedHash.isEmpty()||namedHash==known->hash)){row.contentHash=known->hash;row.contentBytes=known->bytes;}
        AssetFingerprint fingerprint;bool cached=!force&&known!=catalog.end()&&known->bytes==row.file.getSize()&&known->modified==row.file.getLastModificationTime().toMilliseconds()&&(namedHash.isEmpty()||known->hash==namedHash);
        if(cached){fingerprint=*known;}else{auto bytes=row.file.getSize();if(bytes<=0||bytes>maxFingerprintFileBytes||bytes>budget-consumed){++skipped;continue;}consumed+=bytes;if(fingerprintAssetFile(row.file,fingerprint,cancel).failed()){++skipped;continue;}}
        auto expected=collectedAssetHash(row.file);if(expected.isNotEmpty()&&fingerprint.hash!=expected){row.issue=AssetIssue::checksumMismatch;row.detail="Collected source bytes differ from their original SHA-256 name. Locate the original audio.";if(known!=catalog.end()&&known->hash==expected)row.contentBytes=known->bytes;continue;}
        row.contentHash=fingerprint.hash;row.contentBytes=fingerprint.bytes;if(known==catalog.end())catalog.push_back(fingerprint);else *known=fingerprint;
    }
    std::stable_sort(catalog.begin(),catalog.end(),[](const auto& a,const auto& b){return a.seen>b.seen;});if(catalog.size()>maxAssetFingerprints)catalog.resize(maxAssetFingerprints);
    return skipped?String(skipped)+" sources not fingerprinted (file/read/pass limits); Refresh can learn more.":String();
}
String findProjectAssets(std::vector<ProjectAsset>& rows,const File& folder,const std::atomic<bool>* cancel,const AssetSearchLimits& limits){
    for(auto& row:rows){row.candidates.clear();row.searchComplete=false;row.candidatesLimited=false;}
    if(!folder.isDirectory()||folder.isSymbolicLink())return "Search folder must be an ordinary local folder.";
    if(limits.entries<1||limits.entries>20000||limits.hashBytes<0||limits.hashBytes>maxFingerprintPassBytes)return "Invalid audio search limits.";
    auto stopped=[&]{return cancel&&cancel->load();};auto cancelled=[&]{for(auto& row:rows){row.candidates.clear();row.searchComplete=false;}return String("Search cancelled.");};
    std::map<String,std::vector<size_t>> names,hashes;std::set<int64> sizes;bool unknownSize=false;
    for(size_t i=0;i<rows.size();++i)if(rows[i].issue!=AssetIssue::ready&&rows[i].file!=File()){auto& row=rows[i];if(row.contentHash.isEmpty())row.contentHash=collectedAssetHash(row.file);if(row.contentHash.isEmpty())names[row.file.getFileName().toLowerCase()].push_back(i);else{hashes[row.contentHash].push_back(i);if(row.contentBytes>0)sizes.insert(row.contentBytes);else unknownSize=true;}row.searchComplete=true;}
    AudioFormatManager formats;formats.registerBasicFormats();std::set<String> extensions;for(int i=0;i<formats.getNumKnownFormats();++i)for(auto ext:formats.getKnownFormat(i)->getFileExtensions())extensions.insert(ext.trimCharactersAtStart(".").toLowerCase());
    int visited=0;int64 bytesRead=0;bool entryLimited=false,hashLimited=false,readFailure=false;
    auto add=[&](ProjectAsset& row,const File& file){if(row.candidates.size()<16)row.candidates.push_back(file);else row.candidatesLimited=true;};
    for(const auto& entry:RangedDirectoryIterator(folder,true,"*",File::findFilesAndDirectories|File::ignoreHiddenFiles,File::FollowSymlinks::no)){
        if(stopped())return cancelled();if(++visited>limits.entries){entryLimited=true;break;}auto file=entry.getFile();if(file.isDirectory()||file.isSymbolicLink())continue;
        auto named=names.find(file.getFileName().toLowerCase());if(named!=names.end())for(auto index:named->second)add(rows[index],file);
        auto bytes=file.getSize();if(hashes.empty()||bytes<=0)continue;bool plausible=sizes.count(bytes)||(unknownSize&&extensions.count(file.getFileExtension().trimCharactersAtStart(".").toLowerCase()));if(!plausible)continue;
        if(bytes>maxFingerprintFileBytes||bytes>limits.hashBytes-bytesRead){hashLimited=true;continue;}bytesRead+=bytes;AssetFingerprint fingerprint;auto result=fingerprintAssetFile(file,fingerprint,cancel);if(stopped())return cancelled();if(result.failed()){readFailure=true;continue;}
        auto match=hashes.find(fingerprint.hash);if(match!=hashes.end())for(auto index:match->second)if(rows[index].contentBytes==0||rows[index].contentBytes==fingerprint.bytes)add(rows[index],file);
    }
    if(stopped())return cancelled();int unique=0,ambiguous=0,content=0;
    for(auto& row:rows){if(entryLimited||(row.contentHash.isNotEmpty()&&(hashLimited||readFailure)))row.searchComplete=false;std::sort(row.candidates.begin(),row.candidates.end(),[](const File& a,const File& b){return a.getFullPathName().compareNatural(b.getFullPathName())<0;});if(row.candidates.size()==1&&row.searchComplete)++unique;else if(row.candidates.size()>1)++ambiguous;if(!row.candidates.empty()&&row.contentHash.isNotEmpty())++content;}
    return String(unique)+" unique matches, "+String(ambiguous)+" ambiguous / "+String(content)+" content-matched sources. Review before applying."+(entryLimited?" Entry limit reached.":String())+(hashLimited?" Content read limit reached (2 GiB/pass, 512 MiB/file).":String())+(readFailure?" Some content could not be read stably.":String())+((entryLimited||hashLimited||readFailure)?" Incomplete results require explicit Choose match or a smaller search.":String());
}
Result relinkProjectAssets(Project& project,const File& base,const std::vector<AssetReplacement>& replacements,int* references){
    if(references)*references=0;if(replacements.empty())return Result::fail("No replacement files selected.");
    auto rows=projectAssets(project,base);auto next=project;std::set<String> seen;int count=0;AudioFormatManager formats;formats.registerBasicFormats();
    for(auto& replacement:replacements){
        if(!seen.insert(replacement.key).second)return Result::fail("An asset appears more than once in this relink request.");
        auto it=std::find_if(rows.begin(),rows.end(),[&](const ProjectAsset& row){return row.key==replacement.key;});if(it==rows.end())return Result::fail("Project references changed. Refresh the asset list.");
        auto candidate=*it;candidate.file=replacement.file;inspect(candidate,next,formats);if(candidate.issue!=AssetIssue::ready)return Result::fail(replacement.file.getFileName()+": "+candidate.detail);
        auto hash=collectedAssetHash(it->file);if(replacement.contentHash.isNotEmpty()){if(replacement.contentHash.length()!=64||!replacement.contentHash.containsOnly("0123456789abcdef")||(hash.isNotEmpty()&&hash!=replacement.contentHash))return Result::fail("Replacement fingerprint context is invalid.");hash=replacement.contentHash;}
        if(hash.isNotEmpty()){AssetFingerprint fingerprint;auto result=fingerprintAssetFile(replacement.file,fingerprint);if(result.failed())return result;if(fingerprint.hash!=hash)return Result::fail("Replacement checksum differs from the last observed original. Load different audio as a new channel sample instead.");}
        for(auto use:it->uses){if(use.clip)next.clips[(size_t)use.index].audioPath=replacement.file.getFullPathName();else next.tracks[(size_t)use.index].samplePath=replacement.file.getFullPathName();++count;}
    }
    project=std::move(next);if(references)*references=count;return Result::ok();
}
}
