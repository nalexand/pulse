// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>
#include <cstdlib>
namespace pulse {
using namespace juce;
bool Studio::applySongMarker(const SongMarker& marker,bool undo){
    if(recording)return false;Project next;next.markers=project.markers;next.loopSection=project.loopSection;auto result=storeSongMarker(next,marker);if(result.failed()){status=result.getErrorMessage();return false;}
    if(undo)checkpoint();project.markers=std::move(next.markers);project.loopSection=next.loopSection;selectedMarker=marker.id;changed();return true;
}
void Studio::editSongMarker(bool section,const String& id,double at){
    if(recording)return;SongMarker marker;if(id.isNotEmpty()){auto* found=findSongMarker(project,id);if(!found)return;marker=*found;section=marker.isSection();}
    else{marker.id=Uuid().toString();marker.name=section?"Section":"Marker";marker.beat=jlimit(0.,section?4095.75:4096.,snapped(at>=0?at:engine->position.load()));marker.endBeat=section?jmin(4096.,marker.beat+4):-1;marker.colour=(int)project.markers.size()%8;}
    auto* dialog=new AlertWindow(id.isEmpty()?"Add song marker":"Edit song marker","Markers use beats in the Song. Bars are 1-based; a section end is excluded. No clips are moved.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("name",marker.name,"Name");dialog->addComboBox("type",{"Marker","Section range"},"Type");dialog->getComboBoxComponent("type")->setSelectedItemIndex(section?1:0);
    dialog->addTextEditor("start",String(marker.beat/4+1,5),"Start bar");dialog->addTextEditor("end",String((section?marker.endBeat:jmin(4096.,marker.beat+4))/4+1,5),"End bar (sections only)");
    dialog->addComboBox("colour",{"Green","Silver","Olive","Purple","Gold","Teal","Sage","Blue"},"Colour");dialog->getComboBoxComponent("colour")->setSelectedItemIndex(marker.colour);
    dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);auto expected=revision;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,marker,expected](int r)mutable{
        if(!safe||r!=1||safe->recording)return;if(safe->revision!=expected){safe->status="Project changed; reopen the marker editor before applying.";return;}
        auto parse=[](const String& text,double& value){auto trimmed=text.trim();const char* first=trimmed.toRawUTF8();char* end=nullptr;value=std::strtod(first,&end);return end!=first&&*end==0&&std::isfinite(value);};
        double start=0,end=0;bool section=dialog->getComboBoxComponent("type")->getSelectedItemIndex()==1;
        if(!parse(dialog->getTextEditorContents("start"),start)||(section&&!parse(dialog->getTextEditorContents("end"),end))){safe->status="Enter numeric marker bars.";return;}
        marker.name=dialog->getTextEditorContents("name").trim();marker.beat=(start-1)*4;marker.endBeat=section?(end-1)*4:-1;marker.colour=dialog->getComboBoxComponent("colour")->getSelectedItemIndex();
        if(safe->applySongMarker(marker))safe->status="Song marker saved. Drag flags to move; drag a section's end handle to resize.";
    }),true);
}
void Studio::jumpToMarker(const String& id){
    auto* found=findSongMarker(project,id);if(!found)return;auto marker=*found;auto* section=findSongMarker(project,project.loopSection);bool outside=project.loop&&section&&(marker.beat<section->beat||marker.beat>=section->endBeat);
    if(!project.songMode||outside){checkpoint();project.songMode=true;if(outside)project.loop=false;changed();}
    selectedMarker=id;editCursorBeat=marker.beat;engine->seekRequested.store(marker.beat);if(marker.beat<playlistStart||marker.beat>playlistStart+playlistSpan-1)playlistStart=jmax(0.,marker.beat-4);if(!panes[playlistPane].visible)showPane(playlistPane);
    status="Jump to "+marker.name+(outside?". Loop switched off because the marker is outside the section.":".");repaint();
}
void Studio::showMarkerMenu(double beat,const String& id){
    PopupMenu menu;menu.addItem(1,"Add marker here...");menu.addItem(2,"Add section here...");if(id.isNotEmpty()){menu.addSeparator();menu.addItem(3,"Jump to marker");menu.addItem(4,"Edit marker...");auto* marker=findSongMarker(project,id);if(marker&&marker->isSection()){menu.addItem(5,"Loop section");menu.addItem(6,"Export section...");menu.addItem(9,"Copy or move section clips...");}menu.addItem(7,"Delete marker");}menu.addSeparator();menu.addItem(8,"Markers and sections...");auto safe=Component::SafePointer<Studio>(this);
    menu.showMenuAsync(menuOptions(),[safe,beat,id](int r){if(!safe||safe->recording)return;if(r==1||r==2)safe->editSongMarker(r==2,{},beat);else if(r==8)safe->perform(markersOpen);else if(r==9){safe->selectedMarker=id;safe->perform(sectionClipsOpen);}else if(r>=3&&r<=7){safe->selectedMarker=id;if(!findSongMarker(safe->project,id))return;safe->perform(r==3?markerJump:r==4?markerEdit:r==5?markerLoop:r==6?markerExport:markerDelete);}});
}
void Studio::paintMarkerRail(Graphics& g){
    g.saveState();g.reduceClipRegion(markerRailRect.toNearestInt());g.setColour(Colour(0xff263136));g.fillRect(markerRailRect);hits.push_back({markerRailRect,markerRail});float width=arrangeRect.getWidth()/(float)playlistSpan;std::vector<Hit> ends;
    for(size_t i=0;i<project.markers.size();++i){const auto& m=project.markers[i];float start=arrangeRect.getX()+(float)(m.beat-playlistStart)*width,end=arrangeRect.getX()+(float)(m.endBeat-playlistStart)*width;auto colour=colorFor(m.colour);
        if(m.isSection()){Rectangle<float> range{start,markerRailRect.getBottom()-6,jmax(1.f,end-start),6};range=range.getIntersection(markerRailRect);g.setColour(colour.withAlpha(project.loopSection==m.id&&project.loop?.9f:.35f));g.fillRect(range);if(!range.isEmpty())hits.push_back({range,markerHit,(int)i});Rectangle<float> handle{end-5,markerRailRect.getBottom()-10,10,10};handle=handle.getIntersection(markerRailRect);if(!handle.isEmpty())ends.push_back({handle,markerHit,(int)i,1});}
        float available=140;if(i+1<project.markers.size())available=jmin(available,(float)(project.markers[i+1].beat-m.beat)*width-2);Rectangle<float> flag{start,markerRailRect.getY()+1,jmax(18.f,available),15};flag=flag.getIntersection(markerRailRect);if(flag.isEmpty())continue;
        g.setColour(colour.withAlpha(selectedMarker==m.id?.95f:.62f));g.fillRect(flag);label(g,m.name,flag.reduced(4,0),9,Colour(0xff172322),true);hits.push_back({flag,markerHit,(int)i});
    }for(const auto& hit:ends){g.setColour(colorFor(project.markers[(size_t)hit.data].colour));g.fillRect(hit.rect.withWidth(jmin(3.f,hit.rect.getWidth())));hits.push_back(hit);}g.restoreState();
}
void Studio::paintMarkers(Graphics& g,Rectangle<float> r){
    Colour text(0xffd9ddde),muted(0xffa5b0b2);g.setColour(Colour(0xff394349));g.fillRect(r);float x=r.getX()+12,y=r.getY()+10;
    button(g,"Marker at playhead",{x,y,140,26},markerAdd);button(g,"Section...",{x+147,y,104,26},sectionAdd);button(g,"Edit...",{x+258,y,65,26},markerEdit);button(g,"Delete",{x+330,y,65,26},markerDelete);button(g,"< Previous",{x+402,y,95,26},markerPrevious);button(g,"Next >",{x+504,y,82,26},markerNext);
    label(g,"NAME / TYPE",{x+13,y+41,280,22},10,muted,true);label(g,"START BAR",{r.getRight()-269,y+41,100,22},10,muted,true);label(g,"END BAR",{r.getRight()-151,y+41,90,22},10,muted,true);
    markerListRect={x,y+67,r.getWidth()-24,jmax(70.f,r.getHeight()-205)};g.setColour(Colour(0xff29343a));g.fillRect(markerListRect);markerScroll=jlimit(0,jmax(0,(int)project.markers.size()-1),markerScroll);
    if(project.markers.empty())label(g,"Add Intro, Verse, Chorus or other cues. Right-click the Playlist ruler to add at the cursor.",markerListRect.reduced(15),12,muted);
    int visible=(int)(markerListRect.getHeight()/32);for(int row=0;row<visible&&row+markerScroll<(int)project.markers.size();++row){int index=row+markerScroll;const auto& m=project.markers[(size_t)index];Rectangle<float> line{markerListRect.getX(),markerListRect.getY()+row*32,markerListRect.getWidth(),30};g.setColour(selectedMarker==m.id?Colour(0xff53624f):Colour(0xff344148));g.fillRect(line);g.setColour(colorFor(m.colour));g.fillRect(line.withWidth(5));label(g,m.name+(m.isSection()?"  [section]":""),line.reduced(12,0).withWidth(jmax(100.f,line.getWidth()-310)),12,text,true);label(g,String(m.beat/4+1,3),{r.getRight()-269,line.getY(),95,30},11,text);label(g,m.isSection()?String(m.endBeat/4+1,3):"-",{r.getRight()-151,line.getY(),85,30},11,text);if(project.loopSection==m.id)label(g,"LOOP",{r.getRight()-61,line.getY(),49,30},9,project.loop?colorFor(m.colour):muted,true);hits.push_back({line,markerSelect,index});}
    float bottom=r.getBottom()-113;button(g,"Jump",{x,bottom,88,27},markerJump);button(g,"Loop section",{x+96,bottom,128,27},markerLoop);button(g,"Whole-song loop",{x+232,bottom,142,27},markerClearLoop);button(g,"Export section...",{x+382,bottom,148,27},markerExport);
    button(g,"Copy / move clips...",{r.getRight()-180,bottom+35,168,27},sectionClipsOpen);
    if(auto* selected=findSongMarker(project,selectedMarker))label(g,"Selected: "+selected->name+". Marker edits leave clips in place.",{x,bottom+37,r.getWidth()-205,23},12,text);else label(g,"Choose a marker or section from the list.",{x,bottom+37,r.getWidth()-205,23},12,muted);
    label(g,"Alt+T adds a marker; Ctrl+Alt+T adds a section. Alt+Left / Right jumps to the previous / next position.",{x,r.getBottom()-33,r.getWidth()-24,22},10,muted);
}
bool Studio::performMarkerAction(int action,int data){
    if((action<markersOpen||action>markerRail)&&action!=sectionClipsOpen)return false;if(recording){status="Finish the take before editing or navigating song markers.";return true;}
    if(action==sectionClipsOpen){showSectionClips();return true;}
    if(action==markersOpen){showPane(markersPane);return true;}if(action==markerAdd||action==sectionAdd){editSongMarker(action==sectionAdd);return true;}
    if(action==markerSelect){if(data>=0&&data<(int)project.markers.size())selectedMarker=project.markers[(size_t)data].id;return true;}
    if(action==markerPrevious||action==markerNext){double position=engine->seekRequested.load();if(position<0)position=engine->position.load();if(auto* m=adjacentSongMarker(project,position,action==markerNext))jumpToMarker(m->id);else status="No more song markers in this direction.";return true;}
    if(action==markerClearLoop){checkpoint();project.loopSection.clear();project.loop=true;project.songMode=true;changed();status="Looping the whole Song.";return true;}
    auto* found=findSongMarker(project,selectedMarker);if(!found){status="Select a song marker or section first.";return true;}auto marker=*found;
    if(action==markerJump){jumpToMarker(marker.id);return true;}if(action==markerEdit){editSongMarker(marker.isSection(),marker.id);return true;}
    if(action==markerDelete){checkpoint();removeSongMarker(project,marker.id);selectedMarker.clear();changed();status="Marker deleted; clips are preserved.";return true;}
    if(action==markerColour){marker.colour=jlimit(0,7,data);applySongMarker(marker);return true;}
    if(action==markerLoop){if(!marker.isSection()){status="Choose a section range to loop.";return true;}checkpoint();project.loopSection=marker.id;project.loop=true;project.songMode=true;changed();engine->seekRequested.store(marker.beat);status="Looping section: "+marker.name+". LOOP toggles it; Whole-song loop clears the range.";return true;}
    if(action==markerExport){if(!marker.isSection()){status="Choose a section range to export.";return true;}if(exporting.load()){status="Finish or cancel the current export first.";return true;}renderSettings.source=RenderSource::song;renderSettings.startBeat=marker.beat;renderSettings.endBeat=marker.endBeat;showExport(false);status="Export range set to section: "+marker.name;return true;}
    return true;
}
}
