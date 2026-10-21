// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
namespace pulse {
using namespace juce;
void Studio::refreshSampleFavorites(){
    auto file=sampleFavoritesFile();auto stamp=file.getFullPathName()+"|"+String(file.getSize())+"|"+String(file.getLastModificationTime().toMilliseconds());if(stamp==sampleFavoritesStamp)return;sampleFavoritesStamp=stamp;
    StringArray loaded;auto result=loadSampleFavorites(file,loaded);sampleFavoritesError=result.failed()?result.getErrorMessage():String();sampleFavorites=result.wasOk()?std::move(loaded):StringArray{};
}
bool Studio::setSampleFavorite(const File& file,bool add,const String& expected){
    if(recording||recorder.isActive()){status="Finish the take before changing sample favorites.";return false;}
    auto result=editSampleFavorite(sampleFavoritesFile(),file,add,expected);if(result.failed()){status=result.getErrorMessage();return false;}
    sampleFavoritesStamp.clear();refreshSampleFavorites();status=add?"Sample file added to Favorite samples. Click or drag it from the main Browser.":"Sample file removed from Favorite samples. Original audio retained.";repaint();return true;
}
bool Studio::previewBrowserSample(const BrowserAsset& asset,bool original){
    if(recording||recorder.isActive()){status="Finish the take before previewing samples.";return false;}
    String error;auto* sample=assets.load(asset.file,error);if(!sample){status=error;return false;}if(!audioReady())return false;
    SamplePreviewOptions options;if(!original){auto result=loadSamplePreview(samplePreviewFile(),asset.file,options);if(result.failed()){status=result.getErrorMessage()+" Use Preview at original speed to audition without settings.";return false;}}
    if(!engine->preview(sample,asset.start,asset.end,.7f,options.sync?options.sourceBpm:0,options.repeat,options.sync)){status="Preview range is invalid or queue busy. Try again.";return false;}
    status="Previewing "+asset.file.getFileName()+(options.sync?" / "+String(options.sourceBpm,2)+" BPM source -> project tempo (pitch follows speed)":" at original speed")+(options.repeat?" / repeat":"")+". Stop preview or Esc stops audition; channel audio is unchanged.";repaint();return true;
}
bool Studio::performBrowserSampleChoice(const BrowserAsset& asset,int choice,uint64_t expected,int track,const String& favoritesToken){
    if(choice<1||choice>8)return false;if(recording||recorder.isActive()){status="Finish the take before using sample actions.";return false;}
    if(choice==4||choice==5)return setSampleFavorite(asset.file,choice==4,favoritesToken);
    if((choice==1||choice==3)&&(revision!=expected||selectedTrack!=track)){status="Channel or project changed. Reopen the sample menu.";return false;}
    if(choice==1)return applyBrowserAsset(asset,track);
    if(choice==2)return previewBrowserSample(asset,true);
    if(choice==6){showSamplePreviewSettings(asset);return true;}
    if(choice==7)return previewBrowserSample(asset);
    if(choice==8){perform(browserPreviewStop);return true;}
    String error;if(!assets.load(asset.file,error)){status=error;return false;}openWaveform(asset.file,asset.start,asset.end);return true;
}
void Studio::showBrowserSampleMenu(const BrowserAsset& asset){
    if(asset.soundId.isNotEmpty()||asset.file==File())return;
    refreshSampleFavorites();bool favorite=isSampleFavorite(sampleFavorites,asset.file),available=asset.file.existsAsFile();PopupMenu menu;menu.addSectionHeader(asset.file.getFileName());
    menu.addItem(1,"Assign to selected channel: "+project.tracks[(size_t)selectedTrack].name,available&&!project.tracks[(size_t)selectedTrack].isBus);menu.addItem(2,"Preview at original speed",available);menu.addItem(7,"Preview with saved settings",available);menu.addItem(6,"Preview tempo / repeat settings...",available);menu.addItem(8,"Stop preview");menu.addItem(3,"Open in waveform editor",available);menu.addSeparator();
    if(favorite)menu.addItem(5,"Remove source file from sample favorites");else menu.addItem(4,"Add source file to sample favorites",available&&sampleFavorites.size()<maxSampleFavorites);
    auto safe=Component::SafePointer<Studio>(this);auto rev=revision;int track=selectedTrack;auto token=sampleFavoritesToken(sampleFavoritesFile());menu.showMenuAsync(menuOptions(),[safe,asset,rev,track,token](int choice){if(safe&&choice){safe->performBrowserSampleChoice(asset,choice,rev,track,token);safe->repaint();}});
}
bool Studio::setBrowserFilter(const String& query){
    if(recording||recorder.isActive()){status="Finish the take before filtering the Browser.";return false;}
    if(query.length()>160||query.containsChar('\n')||query.containsChar('\r')){status="Use a single-line Browser filter of at most 160 characters.";return false;}
    if(!browserVisible)perform(browserToggle);browserLocal=false;libraryRect={};
    browserFilter=query.trim();browserTreeScroll=0;status=browserFilter.isEmpty()?"Browser filter cleared.":"Browser filter: "+browserFilter+". All words must match a name, category, instrument or source path.";repaint();return true;
}
void Studio::showBrowserFilter(){
    if(recording||recorder.isActive()){status="Finish the take before filtering the Browser.";return;}
    auto* dialog=new AlertWindow("Find Browser sounds and samples","Search names, synth categories/instruments and sample source paths. All words must match. Matching folders open temporarily; Clear restores your normal folder expansion. The filter stays in memory only.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("query",browserFilter,"Name / category / path (up to 160 characters)");dialog->addButton("Find",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog](int choice){if(safe&&choice==1)safe->setBrowserFilter(dialog->getTextEditorContents("query"));}),true);
}
}
