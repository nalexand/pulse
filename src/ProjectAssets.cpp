// SPDX-License-Identifier: AGPL-3.0-only
#include "ProjectAssets.h"
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <set>
#include <cmath>
namespace pulse {
using namespace juce;
String assetIssueName(AssetIssue issue){switch(issue){case AssetIssue::ready:return "Ready";case AssetIssue::missing:return "Missing";case AssetIssue::unreadable:return "Cannot decode";case AssetIssue::unsupported:return "Outside limits";case AssetIssue::emptySampler:return "No sample chosen";case AssetIssue::invalidRange:return "Range outside file";}return {};}
std::vector<ProjectAsset> projectAssets(const Project& p,const File& base){
    std::vector<ProjectAsset> rows;std::map<String,size_t> positions;
    auto add=[&](const String& path,AssetUse use){
        File file=path.isEmpty()?File():File::isAbsolutePath(path)?File(path):base.getChildFile(path);
        auto key=path.isEmpty()?"empty:"+String(use.index):file.getFullPathName();
        #if JUCE_WINDOWS
        key=key.toLowerCase();
        #endif
        auto [it,inserted]=positions.emplace(key,rows.size());if(inserted){ProjectAsset row;row.key=key;row.file=file;if(path.isEmpty())row.issue=AssetIssue::emptySampler;rows.push_back(std::move(row));}rows[it->second].uses.push_back(use);
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
    if(!std::isfinite(row.rate)||row.rate<8000||row.rate>192000||reader->lengthInSamples<=0||reader->lengthInSamples>row.rate*120){row.issue=AssetIssue::unsupported;row.detail="Engine supports nonempty audio up to 120 seconds at 8-192 kHz.";return;}
    for(auto use:row.uses){
        if(use.clip){auto& c=p.clips[(size_t)use.index];if(c.sourceOffset>=row.duration){row.issue=AssetIssue::invalidRange;row.detail="A Playlist source offset starts beyond the end of this file.";return;}}
        else{auto& t=p.tracks[(size_t)use.index];if(t.sampleStart>=row.duration||(t.sampleEnd>row.duration+.5/row.rate)){row.issue=AssetIssue::invalidRange;row.detail="The saved sampler selection does not fit inside this file.";return;}}
    }
    row.issue=AssetIssue::ready;row.detail=String(row.duration,3)+" s  |  "+String(row.rate,0)+" Hz  |  "+String(row.channels)+" channels";
}
void inspectProjectAssets(std::vector<ProjectAsset>& rows,const Project& p,const std::atomic<bool>* cancel){AudioFormatManager formats;formats.registerBasicFormats();for(auto& row:rows){if(cancel&&cancel->load())return;inspect(row,p,formats);}}
static String expectedHash(const File& file){auto name=file.getFileNameWithoutExtension().toLowerCase();return name.length()==64&&name.containsOnly("0123456789abcdef")?name:String();}
String findProjectAssets(std::vector<ProjectAsset>& rows,const File& folder,const std::atomic<bool>* cancel){
    for(auto& row:rows)row.candidates.clear();if(!folder.isDirectory())return "Search folder is unavailable.";
    std::map<String,std::vector<size_t>> names;for(size_t i=0;i<rows.size();++i)if(rows[i].issue!=AssetIssue::ready&&rows[i].file!=File())names[rows[i].file.getFileName().toLowerCase()].push_back(i);
    int visited=0;bool limited=false;
    // Never recurse through links/junctions or follow them outside the chosen tree.
    for(const auto& entry:RangedDirectoryIterator(folder,true,"*",File::findFilesAndDirectories|File::ignoreHiddenFiles,File::FollowSymlinks::no)){
        if(cancel&&cancel->load())return "Search cancelled.";
        if(++visited>20000){limited=true;break;}auto file=entry.getFile();if(file.isDirectory()||file.isSymbolicLink())continue;
        auto match=names.find(file.getFileName().toLowerCase());if(match==names.end())continue;
        for(auto i:match->second){auto& row=rows[i];auto hash=expectedHash(row.file);if(hash.isNotEmpty()&&SHA256(file).toHexString()!=hash)continue;if(row.candidates.size()<16)row.candidates.push_back(file);}
    }
    int unique=0,ambiguous=0;for(auto& row:rows){std::sort(row.candidates.begin(),row.candidates.end(),[](const File& a,const File& b){return a.getFullPathName().compareNatural(b.getFullPathName())<0;});if(row.candidates.size()==1)++unique;else if(row.candidates.size()>1)++ambiguous;}
    return String(unique)+" unique matches, "+String(ambiguous)+" ambiguous. Review before applying."+(limited?" Search stopped at 20,000 entries.":String());
}
Result relinkProjectAssets(Project& project,const File& base,const std::vector<AssetReplacement>& replacements,int* references){
    if(references)*references=0;if(replacements.empty())return Result::fail("No replacement files selected.");
    auto rows=projectAssets(project,base);auto next=project;std::set<String> seen;int count=0;AudioFormatManager formats;formats.registerBasicFormats();
    for(auto& replacement:replacements){
        if(!seen.insert(replacement.key).second)return Result::fail("An asset appears more than once in this relink request.");
        auto it=std::find_if(rows.begin(),rows.end(),[&](const ProjectAsset& row){return row.key==replacement.key;});if(it==rows.end())return Result::fail("Project references changed. Refresh the asset list.");
        auto candidate=*it;candidate.file=replacement.file;inspect(candidate,next,formats);if(candidate.issue!=AssetIssue::ready)return Result::fail(replacement.file.getFileName()+": "+candidate.detail);
        auto hash=expectedHash(it->file);if(hash.isNotEmpty()&&SHA256(replacement.file).toHexString()!=hash)return Result::fail("Replacement checksum differs from the collected original.");
        for(auto use:it->uses){if(use.clip)next.clips[(size_t)use.index].audioPath=replacement.file.getFullPathName();else next.tracks[(size_t)use.index].samplePath=replacement.file.getFullPathName();++count;}
    }
    project=std::move(next);if(references)*references=count;return Result::ok();
}
}
