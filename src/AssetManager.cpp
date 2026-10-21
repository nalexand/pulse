// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <algorithm>
namespace pulse {
using namespace juce;
void Studio::startAssetAudit(const File& searchFolder,bool forceFingerprints){
    if(assetBusy){assetNeedsRefresh=true;status="Audio-file scan in progress.";return;}
    if(assetThread.joinable())assetThread.join();
    auto snapshot=project;auto base=currentFile==File()?root:currentFile.getParentDirectory();auto cacheFile=root.getChildFile("data/asset-fingerprints.json");bool force=forceFingerprints||assetLearnPending;assetLearnPending=false;
    assetTaskGeneration=assetGeneration;assetLastCheck=Time::currentTimeMillis();assetNeedsRefresh=false;assetCancel.store(false);assetDone.store(false);assetBusy=true;
    assetMessage=searchFolder==File()?"Checking project audio files...":"Searching local folder and subfolders...";
    assetThread=std::thread([this,snapshot=std::move(snapshot),base,searchFolder,cacheFile,force]{
        auto rows=projectAssets(snapshot,base);inspectProjectAssets(rows,snapshot,&assetCancel);
        auto token=assetFingerprintToken(cacheFile);AssetFingerprints fingerprints;auto loaded=loadAssetFingerprints(cacheFile,fingerprints);auto prior=fingerprints;
        auto learned=learnProjectAssetFingerprints(rows,fingerprints,force,&assetCancel);
        String message;
        if(searchFolder!=File())message=findProjectAssets(rows,searchFolder,&assetCancel);
        else{int issues=0;for(auto& row:rows)issues+=row.issue!=AssetIssue::ready;message=String((int)rows.size())+" audio sources; "+String(issues)+" need attention.";}
        if(!assetCancel.load()&&loaded.wasOk()&&fingerprints!=prior){auto saved=saveAssetFingerprints(cacheFile,fingerprints,token,&assetCancel);if(saved.failed())message+=" "+saved.getErrorMessage();}
        if(loaded.failed())message=loaded.getErrorMessage()+" "+message;if(learned.isNotEmpty())message+=" "+learned;
        assetWorkerRows=std::move(rows);assetWorkerMessage=message;assetDone.store(true,std::memory_order_release);
    });repaint();
}
void Studio::finishAssetAudit(){
    if(!assetBusy||!assetDone.load(std::memory_order_acquire))return;if(assetThread.joinable())assetThread.join();assetBusy=false;
    if(assetCancel.load()){assetNeedsRefresh=true;assetMessage="Search cancelled. Refreshing current references...";repaint();return;}
    if(assetTaskGeneration!=assetGeneration){assetNeedsRefresh=true;assetMessage="Project changed during scan. Refreshing references...";return;}
    String selectedKey;if(selectedAsset>=0&&selectedAsset<(int)assetRows.size())selectedKey=assetRows[(size_t)selectedAsset].key;
    assetRows=std::move(assetWorkerRows);assetMessage=assetWorkerMessage;selectedAsset=-1;
    for(int i=0;i<(int)assetRows.size();++i)if(assetRows[(size_t)i].key==selectedKey){selectedAsset=i;break;}
    if(selectedAsset<0){for(int i=0;i<(int)assetRows.size();++i)if(assetRows[(size_t)i].issue!=AssetIssue::ready){selectedAsset=i;break;}if(selectedAsset<0&&!assetRows.empty())selectedAsset=0;}
    assetScroll=jlimit(0,jmax(0,(int)assetRows.size()-1),assetScroll);
    if(assetRevealMissing){assetRevealMissing=false;bool missing=false;for(auto& row:assetRows)missing|=row.issue!=AssetIssue::ready;if(missing){showPane(assetsPane);status="Some project audio needs attention. Locate files in Project audio files; playback remains available.";}}
    repaint();
}
bool Studio::applyAssetReplacements(const std::vector<AssetReplacement>& replacements){
    if(recording||recorder.isActive()){status="Finish the take before relinking audio.";return false;}
    if(assetBusy||assetNeedsRefresh||assetTaskGeneration!=assetGeneration){status="Refresh the audio-file list before relinking.";return false;}
    auto checked=replacements;for(auto& replacement:checked)if(replacement.contentHash.isEmpty()){auto row=std::find_if(assetRows.begin(),assetRows.end(),[&](const auto& source){return source.key==replacement.key;});if(row!=assetRows.end())replacement.contentHash=row->contentHash;}
    auto next=project;int references=0;auto result=relinkProjectAssets(next,currentFile==File()?root:currentFile.getParentDirectory(),checked,&references);
    if(result.failed()){status=result.getErrorMessage();return false;}
    if(JSON::toString(projectToJson(project),true,22)==JSON::toString(projectToJson(next),true,22)){status="These audio references are already linked; no edit was added.";return true;}
    for(auto& replacement:replacements){String error;if(!assets.load(replacement.file,error)){status=error;return false;}}
    checkpoint();engine->preview(nullptr);project=std::move(next);changed();assetRevealMissing=false;
    status="Relinked "+String(references)+" references. Notes, trims, fades and selections are preserved. Ctrl+Z undoes this change.";startAssetAudit();return true;
}
void Studio::paintProjectAssets(Graphics& g,Rectangle<float> r){
    Colour text(0xffd9ddde),muted(0xffa5b0b2),accent(0xffb8c997),warning(0xffe2ae7e);g.setColour(Colour(0xff394349));g.fillRect(r);
    float x=r.getX()+10,y=r.getY()+8;
    button(g,"Refresh",{x,y,62,25},assetsRefresh);button(g,"Locate file...",{x+67,y,91,25},assetLocate);button(g,"Search folder...",{x+163,y,113,25},assetFind);
    button(g,"Apply unique matches",{x+281,y,143,25},assetApplyMatches);button(g,"Choose match",{x+429,y,99,25},assetChooseMatch);
    button(g,engine->previewPlaying.load()?"Stop":"Preview",{x+533,y,64,25},assetPreview);button(g,"Edit",{x+602,y,44,25},assetEdit);
    button(g,"Cancel",{x+651,y,52,25},assetCancelScan);
    label(g,assetMessage,{x,y+31,r.getWidth()-20,21},11,assetBusy?warning:muted);
    float statusX=r.getRight()-225,usedX=r.getRight()-300;
    g.setColour(Colour(0xff4b585e));g.fillRect(Rectangle<float>(x,y+58,r.getWidth()-20,23));
    label(g,"Referenced audio",{x+7,y+58,r.getWidth()-330,23},11,text,true);label(g,"Uses",{usedX,y+58,58,23},11,text);label(g,"Status",{statusX,y+58,211,23},11,text,true);
    assetListRect={x,y+83,r.getWidth()-20,jmax(40.f,r.getHeight()-230)};
    g.saveState();g.reduceClipRegion(assetListRect.toNearestInt());
    int visible=jmax(1,(int)(assetListRect.getHeight()/29));
    for(int row=0;row<visible&&assetScroll+row<(int)assetRows.size();++row){int index=assetScroll+row;auto& item=assetRows[(size_t)index];float top=assetListRect.getY()+row*29;
        g.setColour(index==selectedAsset?Colour(0xff526153):Colour(row%2?0xff364147:0xff3d494f));g.fillRect(Rectangle<float>(x,top,assetListRect.getWidth(),28));
        String name=item.file==File()?String("Empty sampler"):item.file.getFileName();label(g,name,{x+7,top,usedX-x-15,28},11,text);
        label(g,String((int)item.uses.size()),{usedX,top,49,28},11,muted);
        String detail=assetIssueName(item.issue);if(!item.candidates.empty())detail+=" / "+String((int)item.candidates.size())+(item.candidatesLimited?"+":"")+" matches";
        label(g,detail,{statusX,top,211,28},11,item.issue==AssetIssue::ready?accent:warning);hits.push_back({{x,top,assetListRect.getWidth(),28},assetSelect,index});
    }
    if(assetRows.empty())label(g,assetBusy?"Checking audio references...":"This project uses synthesized sounds; no audio files are referenced.",assetListRect.reduced(8),12,muted);
    g.restoreState();float bottom=r.getBottom()-130;
    if(selectedAsset>=0&&selectedAsset<(int)assetRows.size()){
        auto& item=assetRows[(size_t)selectedAsset];label(g,item.file==File()?"No file selected":item.file.getFullPathName(),{x,bottom,r.getWidth()-20,25},11,text);
        label(g,item.detail+(item.contentHash.isNotEmpty()?" / SHA-256 "+item.contentHash.substring(0,12):" / no remembered fingerprint"),{x,bottom+27,r.getWidth()-20,23},11,item.issue==AssetIssue::ready?muted:warning);
        String uses="Used by: ";for(auto use:item.uses){String name;if(use.clip&&use.index<(int)project.clips.size()){auto& clip=project.clips[(size_t)use.index];name="Playlist clip "+String(use.index+1)+" ("+project.tracks[(size_t)clip.track].name+")";}else if(!use.clip&&use.index<(int)project.tracks.size())name=project.tracks[(size_t)use.index].name;if(uses.length()<400)uses+=name+"; ";}
        label(g,uses,{x,bottom+53,r.getWidth()-20,23},10,muted);
        String candidate=item.candidates.size()==1?(item.searchComplete?String("Proposed ")+(item.contentHash.isNotEmpty()?"(exact content): ":"(name only): "):"Incomplete search / choose explicitly: ")+item.candidates[0].getFullPathName():item.candidates.size()>1?"Several matches found. Choose one explicitly; ambiguous files are never applied automatically.":"Locate a file manually or search a local folder. Relinking is undoable.";
        label(g,candidate,{x,bottom+80,r.getWidth()-20,39},10,accent);
    }
}
bool Studio::performAssetAction(int action,int data){
    if(action<assetsOpen||action>assetCancelScan)return false;
    if(recording||recorder.isActive()){status="Finish the take before relinking audio.";return true;}
    if(action==assetsOpen){showPane(assetsPane);startAssetAudit();return true;}
    if(action==assetsRefresh){startAssetAudit({},true);return true;}
    if(action==assetCancelScan){if(assetBusy){assetCancel.store(true);status="Cancelling audio-file search...";}return true;}
    if(action==assetSelect){if(data>=0&&data<(int)assetRows.size())selectedAsset=data;return true;}
    if(assetBusy||assetNeedsRefresh||assetTaskGeneration!=assetGeneration){status="Refresh the audio-file list before relinking.";if(!assetBusy)startAssetAudit();return true;}
    if(action==assetFind){chooser=std::make_unique<FileChooser>("Search for missing project audio",root,"*",true);auto safe=Component::SafePointer<Studio>(this);chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectDirectories,[safe](const FileChooser& fc){if(safe&&fc.getResult()!=File())safe->startAssetAudit(fc.getResult());});return true;}
    if(action==assetApplyMatches){std::vector<AssetReplacement> replacements;for(auto& row:assetRows)if(row.issue!=AssetIssue::ready&&row.searchComplete&&row.candidates.size()==1)replacements.push_back({row.key,row.candidates[0],row.contentHash});if(replacements.empty())status="No complete unique matches. Choose a match explicitly or search a smaller folder.";else applyAssetReplacements(replacements);return true;}
    if(selectedAsset<0||selectedAsset>=(int)assetRows.size()){status="Select an audio source first.";return true;}
    auto item=assetRows[(size_t)selectedAsset];auto expected=assetGeneration;
    if(action==assetLocate){chooser=std::make_unique<FileChooser>("Locate replacement for "+(item.file==File()?String("empty sampler"):item.file.getFileName()),item.file==File()?root:item.file.getParentDirectory(),"*.wav;*.aif;*.aiff;*.flac;*.ogg",true);auto safe=Component::SafePointer<Studio>(this);chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,[safe,item,expected](const FileChooser& fc){if(!safe||fc.getResult()==File())return;if(safe->assetGeneration!=expected){safe->status="Project changed. Refresh references and choose the file again.";return;}safe->applyAssetReplacements({{item.key,fc.getResult()}});safe->repaint();});}
    else if(action==assetChooseMatch){PopupMenu menu;for(int i=0;i<(int)item.candidates.size();++i)menu.addItem(i+1,item.candidates[(size_t)i].getFullPathName());if(item.candidates.empty()){status="Search a folder to find possible replacements.";return true;}auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe,item,expected](int r){if(!safe||r<=0||r>(int)item.candidates.size())return;if(safe->assetGeneration!=expected){safe->status="Project changed. Refresh the audio-file list.";return;}safe->applyAssetReplacements({{item.key,item.candidates[(size_t)r-1]}});safe->repaint();});}
    else{File file=item.issue==AssetIssue::ready?item.file:item.candidates.size()==1?item.candidates[0]:File();if(file==File()){status="Locate a file or choose an unambiguous match first.";return true;}
        if(action==assetEdit)openWaveform(file);else if(action==assetPreview&&engine->previewPlaying.load())engine->preview(nullptr);else if(action==assetPreview&&audioReady()){String error;auto* source=assets.load(file,error);if(source)engine->preview(source);else status=error;}}
    return true;
}
}
