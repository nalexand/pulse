// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <algorithm>
#include <cmath>
namespace pulse {
using namespace juce;
void Studio::scanFolder(const File& folder){
    if(scanning){status="Folder scan in progress.";return;}
    if(!folder.isDirectory()){status="That folder is unavailable.";return;}
    if(scanThread.joinable())scanThread.join();
    libraryFolder=folder;librarySearch.clear();libraryScroll=0;libraryEntries.clear();scanEntries.clear();scanError.clear();scanning=true;scanCancel.store(false);scanDone.store(false);
    scanThread=std::thread([this,folder]{
        int visited=0;
        for(const auto& entry:RangedDirectoryIterator(folder,false,"*",File::findFilesAndDirectories|File::ignoreHiddenFiles)){
            if(scanCancel.load())break;
            auto f=entry.getFile();if(++visited>20000){scanError="Folder limited to first 20,000 entries.";break;}
            if(f.isDirectory()||f.hasFileExtension("wav;aif;aiff;flac;ogg"))scanEntries.push_back(f);
            if(scanEntries.size()>=2000){scanError="Showing first 2,000 folders and audio files.";break;}
        }
        std::sort(scanEntries.begin(),scanEntries.end(),[](const File& a,const File& b){if(a.isDirectory()!=b.isDirectory())return a.isDirectory();return a.getFileName().compareNatural(b.getFileName())<0;});
        scanDone.store(true,std::memory_order_release);
    });saveLibrary();repaint();
}
void Studio::finishFolderScan(){
    if(!scanning||!scanDone.load(std::memory_order_acquire))return;
    if(scanThread.joinable())scanThread.join();libraryEntries=std::move(scanEntries);scanning=false;
    status=scanError.isNotEmpty()?scanError:String((int)libraryEntries.size())+" local entries. Click to preview; double-click opens waveform editor.";repaint();
}
void Studio::saveLibrary(){
    if(!audioEnabled)return;auto* d=new DynamicObject();d->setProperty("folder",libraryFolder.getFullPathName());d->setProperty("local",browserLocal);Array<var> rows;for(auto& b:bookmarks)rows.add(b);d->setProperty("bookmarks",rows);
    TemporaryFile file(root.getChildFile("data/sample-library.json"));if(file.getFile().replaceWithText(JSON::toString(var(d))))file.overwriteTargetFileWithTemporary();
}
void Studio::restoreLibrary(){
    auto f=root.getChildFile("data/sample-library.json");if(f.getSize()>65536)return;auto v=JSON::parse(f.loadFileAsString());if(!v.isObject())return;
    if(auto* rows=v["bookmarks"].getArray())for(auto& row:*rows)if(bookmarks.size()<32&&File::isAbsolutePath(row.toString()))bookmarks.addIfNotAlreadyThere(row.toString());
    browserLocal=(bool)v["local"];auto path=v["folder"].toString();if(File::isAbsolutePath(path)&&File(path).isDirectory())scanFolder(File(path));
}
void Studio::paintLocalBrowser(Graphics& g,Rectangle<float> r){
    refreshSampleFavorites();
    const Colour text(0xffd9ddde),muted(0xffa5b0b2),accent(0xffb8c997);
    float x=8,y=r.getY();button(g,"Up",{x,y,33,23},folderUp);button(g,"Search",{x+37,y,65,23},folderSearch);button(g,"Bookmarks",{x+106,y,browserWidth-122,23},folderBookmarks);
    label(g,libraryFolder==File()?"Choose a sample folder":libraryFolder.getFullPathName(),{x,y+28,browserWidth-16,34},10,muted);
    label(g,librarySearch.isEmpty()?"Audio files and folders":"Filter: "+librarySearch,{x,y+64,browserWidth-16,19},10,accent);
    libraryRect={2,y+87,browserWidth-5,jmax(20.f,r.getHeight()-157)};
    g.saveState();g.reduceClipRegion(libraryRect.toNearestInt());float rowY=libraryRect.getY();int visibleIndex=0;
    for(int i=0;i<(int)libraryEntries.size();++i){auto& file=libraryEntries[(size_t)i];if(librarySearch.isNotEmpty()&&!file.getFileName().containsIgnoreCase(librarySearch))continue;if(visibleIndex++<libraryScroll)continue;if(rowY+23>libraryRect.getBottom())break;
        if(file==waveFile){g.setColour(Colour(0xff4b5c53));g.fillRect(Rectangle<float>(3,rowY,browserWidth-8,23));}
        label(g,(file.isDirectory()?">  ":isSampleFavorite(sampleFavorites,file)?"*  ":"~  ")+file.getFileName(),{9,rowY,browserWidth-18,23},11,file.isDirectory()?accent:text);hits.push_back({{3,rowY,browserWidth-8,23},browserEntry,i});rowY+=24;
    }
    if(libraryEntries.empty())label(g,scanning?"Reading folder...":"No supported audio files here",libraryRect.reduced(8),11,muted);
    g.restoreState();button(g,"Open audio editor...",{8,r.getBottom()-62,browserWidth-16,25},waveformOpen);button(g,"Stop preview",{8,r.getBottom()-31,browserWidth-16,25},waveStop);
}
void Studio::openWaveform(const File& file,double start,double end){
    String error;auto* source=assets.load(file,error);if(!source){status=error;return;}
    engine->preview(nullptr);waveFile=file;waveSample=source;waveHistory.clear();waveRedoHistory.clear();waveMarkers.clear();++waveRevision;double duration=source->audio.getNumSamples()/source->sampleRate;
    waveViewStart=0;waveSpan=duration;waveStart=jlimit(0.,jmax(0.,duration-1/source->sampleRate),start);waveEnd=end>0?jlimit(waveStart+1/source->sampleRate,duration,end):duration;
    showPane(wavePane);status="Drag to select audio. Wheel zooms; Shift+wheel scrolls. Edits create local copies.";
}
double Studio::waveTime(float x)const{return waveViewStart+jlimit(0.,1.,(double)(x-waveRect.getX())/jmax(1.f,waveRect.getWidth()))*waveSpan;}
void Studio::selectWave(double at){
    if(!waveSample)return;double duration=waveSample->audio.getNumSamples()/waveSample->sampleRate;
    auto anchor=(int64)std::round(waveAnchor*waveSample->sampleRate),frame=(int64)std::round(at*waveSample->sampleRate);
    waveStart=jlimit(0.,jmax(0.,duration-1/waveSample->sampleRate),jmin(anchor,frame)/waveSample->sampleRate);
    waveEnd=jlimit(waveStart+1/waveSample->sampleRate,duration,jmax(anchor,frame)/waveSample->sampleRate);++waveRevision;repaint();
}
void Studio::paintWaveform(Graphics& g,Rectangle<float> r){
    const Colour text(0xffd9ddde),muted(0xffa5b0b2),accent(0xffb8c997);g.setColour(Colour(0xff394349));g.fillRect(r);
    float x=r.getX()+9,y=r.getY()+7;
    button(g,"Open...",{x,y,61,25},waveformOpen);button(g,engine->previewPlaying.load()?"Playing":"Preview",{x+66,y,65,25},wavePreview,0,engine->previewPlaying.load());button(g,"Stop",{x+136,y,43,25},waveStop);
    button(g,"Select all",{x+184,y,69,25},waveAll);button(g,"Zoom selection",{x+258,y,105,25},waveZoom);button(g,"Full",{x+368,y,40,25},waveZoom,1);button(g,"Undo edit",{x+413,y,73,25},waveUndo);
    button(g,"To sampler",{r.getRight()-106,y,96,25},waveSampler);
    button(g,"Reverse",{x,y+32,66,25},waveReverse);button(g,"Normalize",{x+71,y+32,78,25},waveNormalize);button(g,"Fade in",{x+154,y+32,62,25},waveFadeIn);button(g,"Fade out",{x+221,y+32,67,25},waveFadeOut);
    button(g,"Loop sampler",{x+293,y+32,102,25},waveLoop,0,project.tracks[(size_t)selectedTrack].sampleLoop);
    button(g,"To Playlist",{r.getRight()-106,y+32,96,25},wavePlaylist);
    button(g,"Edit...",{x,y+64,59,25},waveEditMenu);button(g,"Copy",{x+64,y+64,49,25},waveCopy);button(g,"Cut",{x+118,y+64,44,25},waveCut);button(g,"Paste",{x+167,y+64,52,25},wavePaste);button(g,"Trim",{x+224,y+64,47,25},waveTrim);button(g,"Silence",{x+276,y+64,59,25},waveSilence);button(g,"Redo edit",{x+340,y+64,73,25},waveRedo);if(r.getWidth()>665)button(g,"Restore...",{x+418,y+64,78,25},restorationOpen);button(g,"Slice to Playlist",{r.getRight()-132,y+64,122,25},waveSliceMenu);
    waveRect={x,y+124,r.getWidth()-20,jmax(80.f,r.getHeight()-187)};g.setColour(Colour(0xff202d31));g.fillRect(waveRect);
    if(!waveSample){label(g,"Open or drop an audio file here",waveRect,15,muted,false,Justification::centred);return;}
    auto& audio=waveSample->audio;double rate=waveSample->sampleRate;
    label(g,String(rate,0)+" Hz  |  Stereo  |  "+String(audio.getNumSamples()/rate,3)+" s",{x,y+96,r.getWidth()-250,20},11,muted);button(g,"Audio to notes...",{r.getRight()-139,y+96,129,23},transcriptionOpen);if(r.getWidth()>630)button(g,"Spectral...",{r.getRight()-240,y+96,94,23},spectralOpen);
    g.saveState();g.reduceClipRegion(waveRect.toNearestInt());
    float a=waveRect.getX()+(float)((waveStart-waveViewStart)/waveSpan)*waveRect.getWidth(),b=waveRect.getX()+(float)((waveEnd-waveViewStart)/waveSpan)*waveRect.getWidth();
    g.setColour(accent.withAlpha(.19f));g.fillRect(Rectangle<float>(a,waveRect.getY(),b-a,waveRect.getHeight()));
    for(int channel=0;channel<2;++channel){float centre=waveRect.getY()+waveRect.getHeight()*(channel==0?.25f:.75f),amp=waveRect.getHeight()*.22f;
        g.setColour(muted.withAlpha(.2f));g.drawHorizontalLine((int)centre,waveRect.getX(),waveRect.getRight());g.setColour(channel==0?accent:Colour(0xff8eb4c4));
        for(int px=0;px<(int)waveRect.getWidth();++px){int first=jlimit(0,audio.getNumSamples()-1,(int)((waveViewStart+px/waveRect.getWidth()*waveSpan)*rate)),last=jlimit(first+1,audio.getNumSamples(),(int)((waveViewStart+(px+1)/waveRect.getWidth()*waveSpan)*rate));
            auto range=FloatVectorOperations::findMinAndMax(audio.getReadPointer(channel,first),last-first);g.drawVerticalLine((int)waveRect.getX()+px,centre-range.getEnd()*amp,centre-range.getStart()*amp);}
    }
    g.setColour(accent);g.drawVerticalLine((int)a,waveRect.getY(),waveRect.getBottom());g.drawVerticalLine((int)b,waveRect.getY(),waveRect.getBottom());
    if(engine->previewPlaying.load()){float pos=waveRect.getX()+(float)((engine->previewPosition.load()-waveViewStart)/waveSpan)*waveRect.getWidth();g.setColour(Colours::white);g.drawVerticalLine((int)pos,waveRect.getY(),waveRect.getBottom());}
    for(size_t i=0;i<waveMarkers.size();++i){float px=waveRect.getX()+(float)((waveMarkers[i]/rate-waveViewStart)/waveSpan)*waveRect.getWidth();g.setColour(Colour(0xffe3ad68));g.drawVerticalLine((int)px,waveRect.getY(),waveRect.getBottom());if(px>=waveRect.getX()&&px<waveRect.getRight()-20)label(g,String((int)i+1),{px+2,waveRect.getY()+2,27,14},9,Colour(0xffe3ad68));}
    g.restoreState();hits.push_back({waveRect,waveGrid});
    for(int tick=0;tick<=8;++tick){float tx=waveRect.getX()+waveRect.getWidth()*tick/8;label(g,String(waveViewStart+waveSpan*tick/8,3),{tx-18,waveRect.getBottom()+2,50,17},9,muted);}
    label(g,"Selection: "+String(waveStart,4)+" - "+String(waveEnd,4)+" s  ("+String(waveEnd-waveStart,4)+" s)  |  "+String((int)waveMarkers.size())+" marks  |  Destination: "+project.tracks[(size_t)selectedTrack].name,{x,r.getBottom()-24,r.getWidth()-20,20},11,text);
}
void Studio::editWave(int action){
    if(waveEditBusy){status="Audio conversion in progress. Cancel it before editing.";return;}
    if(!waveSample)return;
    int first=jlimit(0,waveSample->audio.getNumSamples()-1,(int)std::round(waveStart*waveSample->sampleRate)),end=jlimit(first+1,waveSample->audio.getNumSamples(),(int)std::round(waveEnd*waveSample->sampleRate));
    AudioBuffer<float> audio;audio.makeCopyOf(waveSample->audio);float peak=audio.getMagnitude(first,end-first);
    for(int ch=0;ch<2;++ch){auto* data=audio.getWritePointer(ch,first);int count=end-first;
        if(action==waveReverse)std::reverse(data,data+count);
        else if(action==waveNormalize&&peak>.000001f)FloatVectorOperations::multiply(data,.98f/peak,count);
        else if(action==waveFadeIn||action==waveFadeOut)for(int i=0;i<count;++i)data[i]*=(float)(action==waveFadeIn?i:count-1-i)/jmax(1,count-1);
    }
    saveWaveEdit(audio,first,end);
}
bool Studio::performSampleAction(int action,int data){
    if(action<localBrowser||action>waveUndo)return false;
    if(recording||recorder.isActive()){status="Finish the take before browsing or editing samples.";return true;}
    switch(action){
    case localBrowser:browserLocal=!browserLocal;libraryRect={};saveLibrary();break;
    case folderChoose:chooseFile(folderChoose);break;
    case folderUp:if(libraryFolder!=File())scanFolder(libraryFolder.getParentDirectory());break;
    case folderSearch:{auto* d=new AlertWindow("Find audio in this folder","Filter file and folder names. Leave blank to show all.",MessageBoxIconType::NoIcon,this);d->addTextEditor("query",librarySearch);d->addButton("Find",1,KeyPress(KeyPress::returnKey));d->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);d->enterModalState(true,ModalCallbackFunction::create([safe,d](int r){if(safe&&r==1){safe->librarySearch=d->getTextEditorContents("query").substring(0,160);safe->libraryScroll=0;safe->repaint();}}),true);break;}
    case folderBookmarks:{PopupMenu menu;menu.addItem(1,"Bookmark current folder",libraryFolder.isDirectory()&&bookmarks.size()<32);menu.addItem(2,"Remove current bookmark",bookmarks.contains(libraryFolder.getFullPathName()));menu.addSeparator();for(int i=0;i<bookmarks.size();++i)menu.addItem(i+10,bookmarks[i]);auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe](int r){if(!safe)return;if(r==1)safe->bookmarks.addIfNotAlreadyThere(safe->libraryFolder.getFullPathName());else if(r==2)safe->bookmarks.removeString(safe->libraryFolder.getFullPathName());else if(r>=10&&r-10<safe->bookmarks.size())safe->scanFolder(File(safe->bookmarks[r-10]));safe->saveLibrary();});break;}
    case browserEntry:if(data>=0&&data<(int)libraryEntries.size()){auto file=libraryEntries[(size_t)data];if(file.isDirectory())scanFolder(file);else previewBrowserSample({{},file.getFileName(),file});}break;
    case waveformOpen:if(data==1&&project.tracks[(size_t)selectedTrack].samplePath.isNotEmpty()){auto& t=project.tracks[(size_t)selectedTrack];auto base=currentFile==File()?root:currentFile.getParentDirectory();openWaveform(File::isAbsolutePath(t.samplePath)?File(t.samplePath):base.getChildFile(t.samplePath),t.sampleStart,t.sampleEnd);}else chooseFile(waveformOpen);break;
    case wavePreview:if(waveSample&&audioReady())engine->preview(waveSample,waveStart,waveEnd);break;
    case waveStop:engine->preview(nullptr);break;
    case waveAll:if(waveSample){waveStart=0;waveEnd=waveSample->audio.getNumSamples()/waveSample->sampleRate;++waveRevision;}break;
    case waveZoom:if(waveSample){waveViewStart=data==1?0:waveStart;waveSpan=data==1?waveSample->audio.getNumSamples()/waveSample->sampleRate:jmax(1/waveSample->sampleRate,waveEnd-waveStart);}break;
    case waveSampler:if(waveSample){engine->cancelPreview();if(project.tracks[(size_t)selectedTrack].isBus){status="Select a source channel in the Channel rack first.";break;}checkpoint();auto& t=project.tracks[(size_t)selectedTrack];auto previous=File::isAbsolutePath(t.samplePath)?File(t.samplePath):(currentFile==File()?root:currentFile.getParentDirectory()).getChildFile(t.samplePath);if(t.instrument!=Instrument::sample||previous!=waveFile){t.sampleRoot=60;t.sampleTune=0;}t.samplePath=waveFile.getFullPathName();t.sampleStart=waveStart;t.sampleEnd=waveEnd;t.instrument=Instrument::sample;changed();status="Selection assigned to "+t.name+". Root "+samplerNoteName(t.sampleRoot)+"; Tune aligns pitched samples. Undo restores the assignment.";}break;
    case wavePlaylist:if(waveSample){if(project.tracks[(size_t)selectedTrack].isBus||project.clips.size()>=maxClips||editCursorBeat+(waveEnd-waveStart)*project.bpm/60>4096){status="Select a source track and leave space inside the 4096-beat timeline for this full selection.";break;}checkpoint();Clip c;c.track=selectedTrack;c.beat=jlimit(0.,4096.,snapped(editCursorBeat));c.audioPath=waveFile.getFullPathName();c.sourceOffset=waveStart;c.length=(waveEnd-waveStart)*project.bpm/60;project.clips.push_back(c);selectedClip=(int)project.clips.size()-1;project.songMode=true;changed();showPane(playlistPane);status="Selected audio range placed at the Playlist cursor.";}break;
    case waveLoop:if(!project.tracks[(size_t)selectedTrack].isBus){checkpoint();auto& t=project.tracks[(size_t)selectedTrack];t.sampleLoop=!t.sampleLoop;changed();status="Sampler loops while a note is held; release finishes the current pass.";}break;
    case waveReverse:case waveNormalize:case waveFadeIn:case waveFadeOut:editWave(action);break;
    case waveUndo:applyWaveHistory(false);break;
    default:break;
    }return true;
}
}
