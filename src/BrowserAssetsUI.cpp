// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <algorithm>
namespace pulse {
using namespace juce;
void Studio::refreshBrowserSounds(){
    auto file=soundLibraryFile();String stamp=file.getFullPathName()+"|"+String(file.getSize())+"|"+String(file.getLastModificationTime().toMilliseconds());if(stamp==browserLibraryStamp)return;
    browserLibraryStamp=stamp;SoundLibrary loaded;if(loadSoundLibrary(file,loaded).wasOk())browserSoundLibrary=std::move(loaded);else browserSoundLibrary={};
}
void Studio::paintFactoryBrowser(Graphics& g,Rectangle<float> r){
    refreshBrowserSounds();refreshSampleFavorites();browserAssets.clear();browserTreeRect={2,178,browserWidth-5,jmax(23.f,r.getBottom()-178-108)};
    struct Row {String name;int action,data;Colour colour;bool folder=false,selected=false;String keywords;};std::vector<Row> rows;String groupName;
    auto folder=[&](const String& name,int id){groupName=name;rows.push_back({name,browserFolder,id,Colour(0xffd9ddde),true});return browserExpanded[(size_t)id]||browserFilter.isNotEmpty();};
    auto row=[&](const String& name,int action,int data,Colour colour){rows.push_back({name,action,data,colour,false,false,groupName});};
    auto asset=[&](BrowserAsset entry,Colour colour){if(entry.file!=File()&&isSampleFavorite(sampleFavorites,entry.file))entry.name="* "+entry.name;browserAssets.push_back(std::move(entry));row(browserAssets.back().name,browserAsset,(int)browserAssets.size()-1,colour);if(browserAssets.back().file!=File())rows.back().keywords+=" "+browserAssets.back().file.getFullPathName();};
    auto sound=[&](const String& id,const SynthPreset& preset){asset({id,(browserSoundLibrary.favorites.contains(id)?"* ":"")+preset.name,{},0,0,false},Colour(0xffb2c98d));rows.back().keywords+=" "+preset.category+" "+instrumentName(preset.instrument);const auto& track=project.tracks[(size_t)selectedTrack];rows.back().selected=track.instrument==preset.instrument&&track.soundName==preset.name&&track.synth==preset.patch&&track.attack==preset.attack&&track.release==preset.release;};
    if(folder("Current project",0))for(int i=0;i<(int)project.patterns.size();++i)row(project.patterns[(size_t)i].name,patternSelect,i,patternColor(i));
    if(folder("Favorite samples",9)){for(const auto& path:sampleFavorites){File file(path);asset({{},file.getFileName()+" / "+file.getParentDirectory().getFileName(),file},Colour(0xffc2a47f));}if(sampleFavorites.isEmpty())row(sampleFavoritesError.isNotEmpty()?"Favorite catalog unavailable":"Right-click a sample to add",0,0,Colour(0xff768184));}
    if(folder("Project samples",3)){
        bool any=false;StringArray paths;auto base=currentFile==File()?root:currentFile.getParentDirectory();
        for(const auto& track:project.tracks)if(track.samplePath.isNotEmpty()){auto file=File::isAbsolutePath(track.samplePath)?File(track.samplePath):base.getChildFile(track.samplePath);asset({{},file.getFileName()+" / "+track.name,file,track.sampleStart,track.sampleEnd,track.sampleLoop,false,track.sampleRoot,track.sampleTune},Colour(0xffc2a47f));paths.addIfNotAlreadyThere(file.getFullPathName());any=true;}
        for(const auto& clip:project.clips)if(clip.isAudio()){auto file=File::isAbsolutePath(clip.audioPath)?File(clip.audioPath):base.getChildFile(clip.audioPath);if(!paths.contains(file.getFullPathName())){asset({{},file.getFileName(),file},Colour(0xffc2a47f));paths.add(file.getFullPathName());any=true;}}
        if(!any)row("No imported samples",0,0,Colour(0xff768184));
    }
    if(folder("Favorite sounds",8)){bool any=false;for(const auto& id:browserSoundLibrary.favorites){SynthPreset preset;if(resolveLibrarySound(browserSoundLibrary,id,preset)){sound(id,preset);any=true;}}if(!any)row("Choose favorites in Sounds...",synthPresets,0,Colour(0xffadb7bd));}
    for(int group=0;group<5;++group){auto instrument=group==0?Instrument::analog:group==1?Instrument::fm:group==2?Instrument::wavetable:group==3?Instrument::additive:Instrument::physical;if(folder(instrumentName(instrument),group>=3?7+group:4+group))for(const auto& preset:factorySynthPresets())if(preset.instrument==instrument)sound(factorySoundId(preset),preset);}
    if(folder("Personal sounds",7)){std::vector<const PersonalSound*> sorted;for(const auto& entry:browserSoundLibrary.personal)sorted.push_back(&entry);std::sort(sorted.begin(),sorted.end(),[](auto* a,auto* b){return a->sound.name.compareNatural(b->sound.name)<0;});for(auto* entry:sorted)sound(entry->id,entry->sound);if(sorted.empty())row("Save / manage sounds...",synthLibraryManage,0,Colour(0xffadb7bd));}
    if(folder("Drums",1))for(int i=0;i<5;++i)row(instrumentName((Instrument)i),instrumentSelect,i,Colour(0xffa4b191));
    if(folder("Instruments",2))for(int i=5;i<instrumentCount;++i)row(instrumentName((Instrument)i),instrumentSelect,i,Colour(0xff8eabad));
    row("Manage sounds...",synthPresets,0,Colour(0xffb2c98d));
    if(browserFilter.isNotEmpty()){std::vector<Row> filtered;for(size_t i=0;i<rows.size();){if(!rows[i].folder){if(sampleNameMatches(browserFilter,rows[i].name,rows[i].keywords))filtered.push_back(rows[i]);++i;continue;}size_t end=i+1;while(end<rows.size()&&!rows[end].folder)++end;std::vector<Row> matches;for(size_t child=i+1;child<end;++child)if(sampleNameMatches(browserFilter,rows[child].name,rows[child].keywords))matches.push_back(rows[child]);if(!matches.empty()){filtered.push_back(rows[i]);filtered.insert(filtered.end(),matches.begin(),matches.end());}i=end;}rows=std::move(filtered);if(rows.empty())rows.push_back({"No matching sounds or samples",0,0,Colour(0xffadb7bd)});}
    int height=0;for(const auto& item:rows)height+=item.folder?25:23;browserTreeMaximum=jmax(0,height-(int)browserTreeRect.getHeight());browserTreeScroll=jlimit(0,browserTreeMaximum,browserTreeScroll);
    float y=browserTreeRect.getY()-browserTreeScroll;g.saveState();g.reduceClipRegion(browserTreeRect.toNearestInt());
    for(const auto& item:rows){float height=item.folder?25.f:23.f;Rectangle<float> bounds{item.folder?2.f:30.f,y,browserWidth-(item.folder?13.f:43.f),height-1};
        if(bounds.intersects(browserTreeRect)){
            if(item.folder){g.setColour(Colour(0xff354045));g.fillRect(bounds);label(g,(browserExpanded[(size_t)item.data]||browserFilter.isNotEmpty())?"v":">",{8,y,13,23},9,Colour(0xffadb7bd));g.setColour(Colour(0xffb59870));g.fillRect(25.f,y+8,12.f,9.f);g.fillRect(25.f,y+5,6.f,4.f);label(g,item.name,{45,y,browserWidth-58,23},11,item.colour,true);}
            else{if(item.selected){g.setColour(Colour(0xff4b5c53));g.fillRect(bounds);}g.setColour(item.colour.withAlpha(.85f));g.fillRect(39.f,y+7,4.f,10.f);label(g,item.name,{51,y,browserWidth-64,23},11,item.colour);}
            auto hit=bounds.getIntersection(browserTreeRect);if(item.action&&hit.getHeight()>8)hits.push_back({hit,item.action,item.data});
        }y+=height;
    }
    if(browserTreeMaximum>0){float thumb=jmax(18.f,browserTreeRect.getHeight()*browserTreeRect.getHeight()/height),at=browserTreeRect.getY()+(browserTreeRect.getHeight()-thumb)*browserTreeScroll/browserTreeMaximum;g.setColour(Colour(0xff69777c));g.fillRoundedRectangle(browserWidth-9,at,4,thumb,2);}
    g.restoreState();float footer=r.getBottom()-101;button(g,"Import sample...",{12,footer,browserWidth-27,25},importSample);button(g,"Import MIDI...",{12,footer+32,browserWidth-27,25},importMidiAction);button(g,"Export WAV...",{12,footer+64,browserWidth-27,25},exportAudio);
}
bool Studio::applyBrowserAsset(const BrowserAsset& asset,int track){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and note preview before loading a Browser sound.";return false;}
    if(track<0||track>=(int)project.tracks.size()||project.tracks[(size_t)track].isBus){status="Choose an instrument channel for this Browser sound.";return false;}
    if(asset.soundId.isNotEmpty()){
        SynthPreset preset;bool found=false;for(const auto& sound:factorySynthPresets())if(factorySoundId(sound)==asset.soundId){preset=sound;found=true;break;}
        if(!found){SoundLibrary library;auto result=loadSoundLibrary(soundLibraryFile(),library);if(result.failed()){status=result.getErrorMessage();return false;}found=resolveLibrarySound(library,asset.soundId,preset);}
        if(!found){status="Sound was removed. Refresh the Browser.";return false;}return applySynthPreset(preset,track,false);
    }
    String error;auto* sample=assets.load(asset.file,error);if(!sample){status=error;return false;}double duration=sample->audio.getNumSamples()/sample->sampleRate;
    if(!std::isfinite(asset.start)||!std::isfinite(asset.end)||asset.start<0||asset.end<0||asset.start>=duration||(asset.end>0&&(asset.end<=asset.start||asset.end>duration+1/sample->sampleRate))){status="This project sample range is invalid. Open its source channel to correct it.";return false;}
    if(validateSamplerTuning(asset.sampleRoot,asset.sampleTune).failed()){status="Invalid Browser sampler tuning.";return false;}
    engine->cancelPreview();releaseKeys();checkpoint();auto& destination=project.tracks[(size_t)track];destination.instrument=Instrument::sample;destination.samplePath=asset.file.getFullPathName();destination.sampleStart=asset.start;destination.sampleEnd=asset.end;destination.sampleLoop=asset.loop;destination.sampleRoot=asset.sampleRoot;destination.sampleTune=asset.sampleTune;destination.soundName.clear();selectedTrack=track;changed();status="Assigned "+asset.file.getFileName()+" to "+destination.name+". Notes, mixer routing and source sample retained; Undo restores the previous sound.";return true;
}
bool Studio::finishBrowserAssetGesture(const MouseEvent& event){
    if(!browserDraggingAsset)return false;auto asset=*browserDraggingAsset;browserDraggingAsset.reset();
    if(revision!=browserDragRevision||selectedTrack!=browserDragTrack){status="Channel or project changed. Select the Browser sound again.";repaint();return true;}
    int track=browserDragTrack;
    if(event.position.getDistanceFrom(dragOrigin)<=5&&asset.previewOnClick){previewBrowserSample(asset);repaint();return true;}
    if(event.position.getDistanceFrom(dragOrigin)>5){track=-1;for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->rect.contains(event.position)){if(it->action==trackSelect||it->action==channelOpen||it->action==rackGrid||it->action==mixFader)track=it->data;break;}if(track<0){status="Drop sounds onto a Channel rack or Mixer channel.";repaint();return true;}}
    applyBrowserAsset(asset,track);repaint();return true;
}
}
