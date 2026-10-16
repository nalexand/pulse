// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>

namespace pulse {
using namespace juce;
void Studio::mouseDown(const MouseEvent& e){
    if(recording){for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->rect.contains(e.position)){if(it->action==record&&e.mods.isRightButtonDown())perform(recordingOpen);else if(it->action==record||it->action==stop||it->action==play||it->action==monitorToggle||it->action==monitorLevelChange||it->action==recordingOpen||(it->action>=analyzerOpen&&it->action<=analyzerBack))perform(it->action,it->data);else status="Finish the current take with REC or Stop before editing.";return;}return;}
    grabKeyboardFocus();dragAction=0;for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->rect.contains(e.position)){
        Hit hit=*it;dragOrigin=e.position;dragData=hit.data;dragExtra=hit.extra;if(hit.pane>=0){activePane=hit.pane;paneOrder.erase(std::remove(paneOrder.begin(),paneOrder.end(),hit.pane),paneOrder.end());paneOrder.push_back(hit.pane);}
        if(hit.action==markerHit){if(hit.data<0||hit.data>=(int)project.markers.size())return;dragSongMarker=project.markers[(size_t)hit.data];selectedMarker=dragSongMarker.id;if(e.mods.isRightButtonDown()){showMarkerMenu(dragSongMarker.beat,selectedMarker);return;}dragAction=markerHit;dragMarkerCheckpoint=false;repaint();return;}
        if(hit.action==markerRail||(hit.action==seekRuler&&hit.data==playlistPane&&e.mods.isRightButtonDown())){double beat=jlimit(0.,4096.,snapped(playlistStart+(e.position.x-arrangeRect.getX())/arrangeRect.getWidth()*playlistSpan,e.mods.isAltDown()));editCursorBeat=beat;if(e.mods.isRightButtonDown())showMarkerMenu(beat);else engine->seekRequested.store(beat);return;}
        if(hit.action==synthValue){if(pendingNoteTool()||!isSynth(project.tracks[(size_t)selectedTrack].instrument))return;if(e.mods.isRightButtonDown()){editSynthValue(hit.data);return;}dragAction=synthValue;dragData=hit.data;dragExtra=selectedTrack;dragValue=(float)synthParameterNormal(hit.data,synthParameterValue(project.tracks[(size_t)selectedTrack],hit.data));checkpoint();if(e.mods.isAltDown()){Track defaults;changeSynthValue(hit.data,synthParameterValue(defaults,hit.data),false);dragAction=0;}return;}
        if(hit.action==noteToolValue){dragAction=noteToolValue;setNoteToolValue(hit.data,(e.position.x-hit.rect.getX())/hit.rect.getWidth());return;}
        if(noteToolCurrent()&&(hit.action==noteHit||hit.action==pianoGrid||hit.action==velocityDrag||hit.action==rackGrid)){cancelNoteTool();status="Note preview cancelled. Edit the original notes, then reopen the tool.";return;}
        if(hit.action==quantize&&e.mods.isRightButtonDown()){PopupMenu menu;menu.addItem(quantize,"Quantize...");menu.addItem(humanize,"Humanize...");menu.addItem(euclideanOpen,"Euclidean rhythm...");menu.addItem(arpeggiatorOpen,"Arpeggiator...");menu.addItem(quickQuantize,"Quick quantize");auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe](int action){if(safe&&action)safe->perform(action);});return;}
        if(hit.action==paneBody){repaint();return;}
        if(hit.action==paneMove||hit.action==paneResize){if(panes[(size_t)hit.data].maximized)return;dragAction=hit.action;dragPaneBounds=panes[(size_t)hit.data].bounds;return;}
        if(hit.action==browserResize){dragAction=browserResize;dragValue=browserWidth;return;}
        if(hit.action==effectsOpen&&e.mods.isRightButtonDown()){showEffectChainMenu(hit.data);return;}
        if(hit.action==record&&e.mods.isRightButtonDown()){perform(recordingOpen);return;}
        if(hit.action==metro&&e.mods.isRightButtonDown()){perform(metronomeSettingsOpen);return;}
        if(hit.action==typingToggle&&e.mods.isRightButtonDown()){perform(typingMenu);return;}
        if(hit.action==channelOpen&&e.mods.isRightButtonDown()){showChannelMenu(hit.data);return;}
        if(hit.action==mute&&e.mods.isRightButtonDown()){perform(solo,hit.data);return;}
        if(hit.action==patternMenu&&e.mods.isRightButtonDown()){renamePattern();return;}
        if(hit.action==waveGrid){if(e.mods.isRightButtonDown()){perform(waveEditMenu);return;}if(e.mods.isAltDown()){toggleWaveMarker(waveTime(e.position.x));return;}dragAction=waveGrid;waveAnchor=waveTime(e.position.x);selectWave(waveAnchor);return;}
        if(automationMouseDown(hit,e))return;
        if(hit.action==stereoWidth||hit.action==stereoTrim){
            if(e.mods.isRightButtonDown()){showStereoParameterMenu(hit.action,hit.data);return;}
            if(e.mods.isAltDown()||e.mods.isMiddleButtonDown()){checkpoint();setParameter(hit.action,hit.data,hit.action==stereoWidth?.5f:.8f);return;}
        }
        if(hit.action>=flangerMix&&hit.action<=phaserWidth){
            if(e.mods.isRightButtonDown()){showModulationParameterMenu(hit.action,hit.data);return;}
            if(e.mods.isAltDown()||e.mods.isMiddleButtonDown()){auto kind=hit.action>=phaserMix?ModulationKind::phaser:ModulationKind::flanger;int i=hit.action-(kind==ModulationKind::phaser?phaserMix:flangerMix);checkpoint();setParameter(hit.action,hit.data,(float)modulationNormal(kind,i,modulationSpecs(kind)[(size_t)i].initial));return;}
        }
        if(hit.action>=chorusMix&&hit.action<=chorusWidth){
            if(e.mods.isRightButtonDown()){showChorusParameterMenu(hit.action,hit.data);return;}
            if(e.mods.isAltDown()||e.mods.isMiddleButtonDown()){checkpoint();int i=hit.action-chorusMix;setParameter(hit.action,hit.data,(float)chorusNormal(i,chorusParameterSpecs()[(size_t)i].initial));return;}
        }
        if(isParameter(hit.action)){
            auto defaultValue=[&]()->float{switch(hit.action){case panDial:return .5f;case bpmDial:return(118-30)/270.f;case masterDial:return.8f;case volumeDial:case mixFader:return hit.data<0?.8f:(project.tracks[(size_t)hit.data].isBus?1.f:.7f)/1.5f;case cutoffDial:return(float)(std::log(16000./60)/std::log(20000./60));case attackDial:return(float)(std::log(5.)/std::log(2000.));case releaseDial:return(float)(std::log(9.)/std::log(150.));case noteVelocity:return.8f;case noteProbability:return 1.f;case eqLow:case eqMid:case eqHigh:case reverbRoom:case reverbDamping:return .5f;case compThreshold:return .7f;case compRatio:return 3/19.f;case compAttack:return 9.9f/99.9f;case compRelease:return 115/995.f;case compMakeup:return 1/3.f;case reverbMix:return .15f;default:return 0.f;}};
            if(e.mods.isAltDown()||e.mods.isMiddleButtonDown()){checkpoint();setParameter(hit.action,hit.data,defaultValue());return;}
            if(e.mods.isRightButtonDown()){
                PopupMenu m;m.addItem(1,"Reset");m.addItem(2,"Type in value...");if(automationTargetFor(hit.action,hit.data)>=0)m.addItem(3,"Create automation clip");if(hit.action==bpmDial)m.addItem(4,"Tap tempo / recording setup...");auto safe=Component::SafePointer<Studio>(this);float defaultVal=defaultValue();
                m.showMenuAsync(menuOptions(),[safe,hit,defaultVal](int result){if(!safe||result==0)return;if(result==4){safe->perform(recordingOpen);return;}if(result==3){safe->createAutomation(hit.action,hit.data);return;}if(result==1){safe->checkpoint();safe->setParameter(hit.action,hit.data,defaultVal);return;}
                    bool tempo=hit.action==bpmDial;auto* d=new AlertWindow(tempo?"Tempo":"Parameter value",tempo?"BPM (30-300)":"Normalized value (0-100%)",MessageBoxIconType::NoIcon,safe.getComponent());
                    d->addTextEditor("value",String(tempo?safe->project.bpm:safe->parameterValue(hit.action,hit.data)*100,tempo?3:2));d->addButton("Accept",1,KeyPress(KeyPress::returnKey));d->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
                    d->enterModalState(true,ModalCallbackFunction::create([safe,hit,d,tempo](int r){if(!safe||r!=1)return;auto content=d->getTextEditorContents("value").trim();if(content.isEmpty()||!content.containsOnly("0123456789.-"))return;double v=content.getDoubleValue();if(!std::isfinite(v))return;safe->checkpoint();safe->setParameter(hit.action,hit.data,(float)(tempo?(v-30)/270:v/100));}),true);
                });return;
            }
            checkpoint();dragAction=hit.action;dragValue=parameterValue(hit.action,hit.data);return;
        }
        if(hit.action==seekRuler){auto area=hit.data==pianoPane?pianoRect:arrangeRect;double start=hit.data==pianoPane?pianoStart:playlistStart,span=hit.data==pianoPane?pianoSpan:playlistSpan;engine->seekRequested.store(jmax(0.,start+(e.position.x-area.getX())/area.getWidth()*span));repaint();return;}
        if(hit.action==pianoKey){engine->audition(selectedTrack,hit.data,true);mousePitch=hit.data;mousePitchTrack=selectedTrack;return;}
        if(hit.action==rackGrid){checkpoint();if(selectedTrack!=hit.data){releaseKeys();pianoLow=-1;}selectedTrack=hit.data;selectedNote=-1;selectedNotes.clear();auto& ns=lane();double beat=hit.extra*.25;
            auto found=std::find_if(ns.begin(),ns.end(),[beat](const Note& n){return std::abs(n.beat-beat)<.0001;});bool willEnable=found==ns.end()&&!e.mods.isRightButtonDown();
            if(found!=ns.end()){if(e.mods.isShiftDown())found->ratchets=found->ratchets%4+1;else ns.erase(found);}else if(!e.mods.isRightButtonDown()&&ns.size()<maxNotesPerLane){int pitch=isDrum(project.tracks[(size_t)selectedTrack].instrument)?(selectedTrack==0?36:selectedTrack==1?38:42):60;ns.push_back({beat,.25,pitch,.8f,1.f,e.mods.isShiftDown()?3:1});engine->audition(selectedTrack,pitch,true);mousePitch=pitch;mousePitchTrack=selectedTrack;}
            dragAction=e.mods.isShiftDown()?0:rackPaint;dragExtra=willEnable?1:0;changed();return;
        }
        if(hit.action==noteHit){
            if(e.mods.isRightButtonDown()||editorTool==2){checkpoint();lane().erase(lane().begin()+hit.data);selectedNote=-1;selectedNotes.clear();dragAction=eraseArea;dragData=pianoPane;changed();return;}
            if(e.mods.isCtrlDown()){if(selectedNotes.count(hit.data))selectedNotes.erase(hit.data);else selectedNotes.insert(hit.data);selectedNote=selectedNotes.empty()?-1:hit.data;repaint();return;}
            if(!selectedNotes.count(hit.data)){selectedNotes.clear();selectedNotes.insert(hit.data);}selectedNote=hit.data;checkpoint();
            if(e.mods.isShiftDown()){if(lane().size()+selectedNotes.size()>maxNotesPerLane)return;std::set<int> copies;int main=selectedNote;for(int i:selectedNotes){auto n=lane()[(size_t)i];int index=(int)lane().size();lane().push_back(n);copies.insert(index);if(i==main)selectedNote=index;}selectedNotes=std::move(copies);}
            dragNote=*note();dragNotes.clear();for(int i:selectedNotes)dragNotes.push_back({i,lane()[(size_t)i]});dragAction=noteHit;dragExtra=e.position.x>hit.rect.getRight()-jmin(7.f,hit.rect.getWidth()*.3f)?1:0;
            engine->audition(selectedTrack,dragNote.pitch,true);mousePitch=dragNote.pitch;mousePitchTrack=selectedTrack;repaint();return;
        }
        if(hit.action==pianoGrid){
            if(editorTool==3||e.mods.isCtrlDown()){dragAction=selectArea;selectionBox={e.position.x,e.position.y,0,0};if(!e.mods.isShiftDown())selectedNotes.clear();selectedNote=-1;repaint();return;}
            if(e.mods.isRightButtonDown()||editorTool==2){checkpoint();dragAction=eraseArea;dragData=pianoPane;return;}if(lane().size()>=maxNotesPerLane){status="This build supports 512 notes per lane.";return;}
            checkpoint();double length=project.patterns[(size_t)project.activePattern].length;double raw=pianoStart+(e.position.x-pianoRect.getX())/pianoRect.getWidth()*pianoSpan;
            double beat=e.mods.isAltDown()||snap==0?raw:std::floor(raw/snap)*snap;beat=jlimit(0.,length-.001,beat);
            int pitch=jlimit(0,127,pianoLow+pianoRows-1-(int)((e.position.y-pianoRect.getY())/pianoRect.getHeight()*pianoRows));
            lane().push_back({beat,isDrum(project.tracks[(size_t)selectedTrack].instrument)?.25:lastNoteLength,pitch,.8f});selectedNote=(int)lane().size()-1;selectedNotes={selectedNote};dragNote=*note();dragNotes={{selectedNote,dragNote}};dragAction=editorTool==1?paintArea:noteHit;dragData=pianoPane;dragExtra=1;
            engine->audition(selectedTrack,pitch,true);mousePitch=pitch;mousePitchTrack=selectedTrack;changed();return;
        }
        if(hit.action==velocityDrag){int index=hit.data;if(index<0){float distance=11;for(int i=0;i<(int)lane().size();++i){float x=noteBounds(lane()[(size_t)i]).getX();float d=std::abs(e.position.x-x);if(d<distance){distance=d;index=i;}}}if(index<0)return;checkpoint();selectedNote=index;selectedNotes={index};dragAction=velocityDrag;dragData=index;lane()[(size_t)index].velocity=jlimit(.01f,1.f,(velocityRect.getBottom()-e.position.y)/velocityRect.getHeight());changed();return;}
        if(hit.action==clipHit&&e.mods.isAltDown()&&e.mods.isRightButtonDown()){showPatternClipMenu(hit.data);return;}
        if(hit.action==clipHit){
            selectedClip=hit.data;selectedTrack=project.clips[(size_t)selectedClip].track;
            if(e.mods.isRightButtonDown()||editorTool==2){checkpoint();project.clips.erase(project.clips.begin()+hit.data);selectedClip=-1;dragAction=eraseArea;dragData=playlistPane;changed();return;}
            if(editorTool==3||e.mods.isCtrlDown()){repaint();return;}
            if(e.mods.isShiftDown()){if(project.clips.size()>=maxClips)return;auto& original=project.clips[(size_t)hit.data];if(original.isAutomation()){int clips=0,points=(int)original.points.size();for(const auto& c:project.clips)if(c.isAutomation()){++clips;points+=(int)c.points.size();}if(clips>=maxAutomationClips||points>maxTotalAutomationPoints){status="Automation clip/point limit reached.";return;}}}
            checkpoint();
            if(e.mods.isShiftDown()){auto c=project.clips[(size_t)hit.data];project.clips.push_back(c);dragData=(int)project.clips.size()-1;selectedClip=dragData;}
            dragAction=clipHit;dragClip=project.clips[(size_t)dragData];dragExtra=e.position.x>=hit.rect.getRight()-6?1:e.position.x<=hit.rect.getX()+6&&dragClip.isAudio()?2:0;return;
        }
        if(hit.action==arrangeGrid){if(e.mods.isRightButtonDown()||editorTool==2){checkpoint();selectedClip=-1;dragAction=eraseArea;dragData=playlistPane;return;}if(editorTool==3||project.clips.size()>=maxClips){selectedClip=-1;repaint();return;}checkpoint();
            int t=jlimit(0,(int)project.tracks.size()-1,(int)((e.position.y-arrangeRect.getY())/playlistLaneHeight));
            double beat=snapped(playlistStart+(e.position.x-arrangeRect.getX())/arrangeRect.getWidth()*playlistSpan,e.mods.isAltDown());beat=jmax(0.,beat);
            project.clips.push_back({t,project.activePattern,beat,project.patterns[(size_t)project.activePattern].length});selectedClip=(int)project.clips.size()-1;selectedTrack=t;if(editorTool==1){dragAction=paintArea;dragData=playlistPane;}changed();return;
        }
        perform(hit.action,hit.data);return;
    }
}
void Studio::mouseDrag(const MouseEvent& e){
    if(recording)return;
    if(dragAction==noteToolValue){for(const auto& hit:hits)if(hit.action==noteToolValue&&hit.data==dragData){setNoteToolValue(dragData,(e.position.x-hit.rect.getX())/hit.rect.getWidth());break;}return;}
    if(dragAction==markerHit){auto* existing=findSongMarker(project,dragSongMarker.id);if(!existing)return;auto marker=dragSongMarker;double delta=snapped((e.position.x-dragOrigin.x)/arrangeRect.getWidth()*playlistSpan,e.mods.isAltDown());if(dragExtra==1&&marker.isSection())marker.endBeat=jlimit(marker.beat+.25,4096.,dragSongMarker.endBeat+delta);else{delta=jlimit(-dragSongMarker.beat,4096.-(dragSongMarker.isSection()?dragSongMarker.endBeat:dragSongMarker.beat),delta);marker.beat+=delta;if(marker.isSection())marker.endBeat+=delta;}if(marker.beat==existing->beat&&marker.endBeat==existing->endBeat)return;if(applySongMarker(marker,!dragMarkerCheckpoint))dragMarkerCheckpoint=true;return;}
    if(dragAction==waveGrid){selectWave(waveTime(e.position.x));return;}
    if(dragAction==automationPoint){automationMouseDrag(e);return;}
    if(dragAction==rackPaint){for(auto h=hits.rbegin();h!=hits.rend();++h)if(h->rect.contains(e.position)){if(h->action!=rackGrid)return;auto& ns=project.patterns[(size_t)project.activePattern].lanes[(size_t)h->data];double beat=h->extra*.25;auto n=std::find_if(ns.begin(),ns.end(),[beat](const Note& v){return std::abs(v.beat-beat)<.00001;});if(dragExtra&&n==ns.end()&&ns.size()<maxNotesPerLane){int pitch=project.tracks[(size_t)h->data].instrument==Instrument::sample?60:h->data==0?36:h->data==1?38:42;ns.push_back({beat,.25,pitch,.8f});changed();}else if(!dragExtra&&n!=ns.end()){ns.erase(n);changed();}return;}return;}
    if(dragAction==paintArea||dragAction==eraseArea){bool erasing=dragAction==eraseArea;
        if(dragData==pianoPane&&pianoRect.contains(e.position)){double raw=pianoStart+(e.position.x-pianoRect.getX())/pianoRect.getWidth()*pianoSpan;double beat=snap>0&&!e.mods.isAltDown()?std::floor(raw/snap)*snap:raw;int pitch=jlimit(0,127,pianoLow+pianoRows-1-(int)((e.position.y-pianoRect.getY())/pianoRect.getHeight()*pianoRows));int found=findNote(raw,pitch);
            if(erasing&&found>=0){lane().erase(lane().begin()+found);selectedNote=-1;selectedNotes.clear();changed();}else if(!erasing&&found<0&&lane().size()<maxNotesPerLane&&beat<project.patterns[(size_t)project.activePattern].length){lane().push_back({beat,lastNoteLength,pitch,.8f});selectedNote=(int)lane().size()-1;selectedNotes.insert(selectedNote);changed();}return;}
        if(dragData==playlistPane&&arrangeRect.contains(e.position)){double raw=playlistStart+(e.position.x-arrangeRect.getX())/arrangeRect.getWidth()*playlistSpan;int track=jlimit(0,(int)project.tracks.size()-1,(int)((e.position.y-arrangeRect.getY())/playlistLaneHeight));auto found=std::find_if(project.clips.begin(),project.clips.end(),[raw,track](const Clip& c){return c.track==track&&raw>=c.beat&&raw<c.beat+c.length;});
            if(erasing&&found!=project.clips.end()){project.clips.erase(found);selectedClip=-1;changed();}else if(!erasing&&found==project.clips.end()&&project.clips.size()<maxClips){double length=project.patterns[(size_t)project.activePattern].length;double beat=std::floor(raw/length)*length;project.clips.push_back({track,project.activePattern,beat,length});selectedClip=(int)project.clips.size()-1;changed();}return;}return;
    }
    if(dragAction==paneMove||dragAction==paneResize){auto next=dragPaneBounds;auto delta=e.position-dragOrigin;
        if(dragAction==paneMove){next=next.translated(delta.x,delta.y);if(!e.mods.isAltDown()){auto ws=workspaceBounds();if(std::abs(next.getX()-ws.getX())<8)next.setX(ws.getX());if(std::abs(next.getY()-ws.getY())<8)next.setY(ws.getY());for(int p=0;p<paneCount;++p)if(p!=dragData&&panes[(size_t)p].visible){auto other=panes[(size_t)p].bounds;if(std::abs(next.getX()-other.getRight())<8)next.setX(other.getRight());if(std::abs(next.getY()-other.getBottom())<8)next.setY(other.getBottom());}}}
        else next.setSize(next.getWidth()+delta.x,next.getHeight()+delta.y);panes[(size_t)dragData].bounds=boundedPane(next,dragData);repaint();return;
    }
    if(dragAction==browserResize){browserWidth=jlimit(160.f,340.f,dragValue+e.position.x-dragOrigin.x);for(int p=0;p<paneCount;++p)panes[(size_t)p].bounds=boundedPane(panes[(size_t)p].bounds,p);repaint();return;}
    if(dragAction==synthValue){if(selectedTrack!=dragExtra)return;float factor=e.mods.isCtrlDown()?.1f:1.f;double normal=jlimit(0.,1.,(double)dragValue+(e.position.x-dragOrigin.x)/300.*factor-(e.position.y-dragOrigin.y)/180.*factor);changeSynthValue(dragData,synthParameterFromNormal(dragData,normal),false);return;}
    if(isParameter(dragAction)){float factor=e.mods.isCtrlDown()?.1f:1.f;float v=dragValue+(e.position.x-dragOrigin.x)/300.f*factor-(e.position.y-dragOrigin.y)/180.f*factor;
        if(dragAction==mixFader)for(const auto& hit:hits)if(hit.action==mixFader&&hit.data==dragData){v=(hit.rect.getBottom()-8-e.position.y)/(hit.rect.getHeight()-16);break;}setParameter(dragAction,dragData,v);return;}
    if(dragAction==selectArea){selectionBox=Rectangle<float>::leftTopRightBottom(jmin(e.position.x,dragOrigin.x),jmin(e.position.y,dragOrigin.y),jmax(e.position.x,dragOrigin.x),jmax(e.position.y,dragOrigin.y)).getIntersection(pianoRect);
        if(!e.mods.isShiftDown())selectedNotes.clear();for(int i=0;i<(int)lane().size();++i)if(noteBounds(lane()[(size_t)i]).intersects(selectionBox))selectedNotes.insert(i);selectedNote=selectedNotes.empty()?-1:*selectedNotes.begin();repaint();return;}
    if(dragAction==velocityDrag&&dragData>=0&&dragData<(int)lane().size()){lane()[(size_t)dragData].velocity=jlimit(.01f,1.f,(velocityRect.getBottom()-e.position.y)/velocityRect.getHeight());changed();return;}
    if(dragAction==noteHit&&note()){
        double length=project.patterns[(size_t)project.activePattern].length,db=(e.position.x-dragOrigin.x)/pianoRect.getWidth()*pianoSpan;
        if(dragExtra==1){note()->length=jlimit(.0625,64.,snapped(dragNote.length+db,e.mods.isAltDown()));lastNoteLength=note()->length;}
        else{db=snapped(db,e.mods.isAltDown());int dp=-(int)std::round((e.position.y-dragOrigin.y)/pianoRect.getHeight()*pianoRows);double low=0,high=length;int minPitch=127,maxPitch=0;for(const auto&[i,n]:dragNotes){low=jmin(low,n.beat);high=jmin(high,length-.0625-n.beat);minPitch=jmin(minPitch,n.pitch);maxPitch=jmax(maxPitch,n.pitch);}double minBeat=length;for(const auto&[i,n]:dragNotes)minBeat=jmin(minBeat,n.beat);db=jlimit(-minBeat,high,db);dp=jlimit(-minPitch,127-maxPitch,dp);
            for(const auto&[i,n]:dragNotes)if(i<(int)lane().size()){lane()[(size_t)i].beat=n.beat+db;lane()[(size_t)i].pitch=n.pitch+dp;}}
        changed();return;
    }
    if(dragAction==clipHit&&dragData<(int)project.clips.size()){
        auto& c=project.clips[(size_t)dragData];double delta=snapped((e.position.x-dragOrigin.x)/arrangeRect.getWidth()*playlistSpan,e.mods.isAltDown());
        auto* sample=clipSamples[(size_t)dragData];
        if(dragExtra==1){double available=600;if(c.isAudio()&&sample)available=jmax(.001,(sample->audio.getNumSamples()/sample->sampleRate-c.sourceOffset)*project.bpm/60);c.length=jlimit(.001,available,dragClip.length+delta);}
        else if(dragExtra==2){delta=jlimit(jmax(-dragClip.beat,-dragClip.sourceOffset*project.bpm/60),dragClip.length-.001,delta);c.beat=dragClip.beat+delta;c.length=dragClip.length-delta;c.sourceOffset=dragClip.sourceOffset+delta*60/project.bpm;}
        else{c.beat=jlimit(0.,4096.,dragClip.beat+delta);c.track=jlimit(0,(int)project.tracks.size()-1,dragClip.track+(int)std::round((e.position.y-dragOrigin.y)/playlistLaneHeight));}
        c.fadeIn=jmin(c.fadeIn,c.length);c.fadeOut=jmin(c.fadeOut,c.length);changed();
    }
}
void Studio::mouseUp(const MouseEvent&){if(dragAction==markerHit&&!dragMarkerCheckpoint&&dragExtra==0)jumpToMarker(selectedMarker);if(mousePitch>=0){engine->audition(mousePitchTrack,mousePitch,false);mousePitch=-1;}if(dragAction==paneMove||dragAction==paneResize||dragAction==browserResize)saveLayout();dragAction=0;selectionBox={};repaint();}
void Studio::mouseDoubleClick(const MouseEvent& e){if(recording)return;for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->rect.contains(e.position)){
    auto h=*it;if(noteToolCurrent()&&h.action==noteHit){cancelNoteTool();return;}if(h.action==markerHit||h.action==markerSelect){if(h.data>=0&&h.data<(int)project.markers.size()){auto marker=project.markers[(size_t)h.data];selectedMarker=marker.id;editSongMarker(marker.isSection(),marker.id);}}else if(h.action==browserEntry){performSampleAction(browserEntry,h.data);if(h.data>=0&&h.data<(int)libraryEntries.size()&&libraryEntries[(size_t)h.data].existsAsFile())openWaveform(libraryEntries[(size_t)h.data]);}else if(h.action==paneMove)perform(paneMaximize,h.data);else if(h.action==clipHit){selectedClip=h.data;auto c=project.clips[(size_t)h.data];if(c.isAutomation()){selectedPoint=-1;showPane(automationPane);}else if(c.isAudio())editAudioClip();else{perform(patternSelect,c.pattern);perform(pianoOpen,c.track);}}
    else if(h.action==trackSelect){selectedTrack=h.data;editName(false);}else if(h.action==noteHit){selectedNote=h.data;selectedNotes={h.data};showPane(channelPane);}else if(h.action==patternMenu)renamePattern();else if(h.action==pianoOpen)perform(pianoOpen,h.data);return;}}
void Studio::mouseWheelMove(const MouseEvent& e,const MouseWheelDetails& wheel){
    if(recording)return;
    for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->rect.contains(e.position)){auto h=*it;
        if(noteToolCurrent()&&((h.action==noteHit&&e.mods.isAltDown()&&!e.mods.isCtrlDown())||h.action==rackGrid)){cancelNoteTool();status="Preview cancelled. Adjust the original notes, then reopen the tool.";return;}
        if(h.action==synthValue){double normal=synthParameterNormal(h.data,synthParameterValue(project.tracks[(size_t)selectedTrack],h.data));changeSynthValue(h.data,synthParameterFromNormal(h.data,normal+(wheel.deltaY>0?1:-1)*(e.mods.isCtrlDown()?.003:.03)));return;}
        if(h.action==noteToolValue){setNoteToolValue(h.data,noteToolValueFor(h.data)+(wheel.deltaY>0?1.f:-1.f)*(e.mods.isCtrlDown()?.005f:.025f));return;}
        if(h.pane==markersPane){markerScroll=jlimit(0,jmax(0,(int)project.markers.size()-1),markerScroll+(wheel.deltaY<0?2:-2));repaint();return;}
        if(h.pane==templatesPane){int count=0;for(auto& info:templateRows)if(templateMatches(info))++count;templateScroll=jlimit(0,jmax(0,count-1),templateScroll+(wheel.deltaY<0?2:-2));repaint();return;}
        if(h.pane==assetsPane){assetScroll=jlimit(0,jmax(0,(int)assetRows.size()-1),assetScroll+(wheel.deltaY<0?3:-3));repaint();return;}
        if(h.action==browserEntry||libraryRect.contains(e.position)){int count=0;for(auto& file:libraryEntries)if(librarySearch.isEmpty()||file.getFileName().containsIgnoreCase(librarySearch))++count;libraryScroll=jlimit(0,jmax(0,count-1),libraryScroll+(wheel.deltaY<0?3:-3));repaint();return;}
        if(h.pane==wavePane&&waveSample){double duration=waveSample->audio.getNumSamples()/waveSample->sampleRate;if(e.mods.isShiftDown())waveViewStart=jlimit(0.,jmax(0.,duration-waveSpan),waveViewStart+(wheel.deltaY<0?1:-1)*waveSpan*.15);else{double anchor=waveTime(e.position.x),fraction=jlimit(0.,1.,(double)(e.position.x-waveRect.getX())/waveRect.getWidth());waveSpan=jlimit(jmin(.001,duration),duration,waveSpan*(wheel.deltaY>0?.8:1.25));waveViewStart=jlimit(0.,jmax(0.,duration-waveSpan),anchor-fraction*waveSpan);}repaint();return;}
        if(isParameter(h.action)){checkpoint();float amount=wheel.deltaY>0?1.f:-1.f;float step=h.action==bpmDial?1/270.f:h.action==noteRatchet?1/7.f:.03f;if(e.mods.isCtrlDown())step*=.1f;setParameter(h.action,h.data,parameterValue(h.action,h.data)+amount*step);return;}
        if(h.action==patternMenu||h.action==patternSelect){perform(patternSelect,jlimit(0,(int)project.patterns.size()-1,project.activePattern+(wheel.deltaY<0?1:-1)));return;}
        if(h.pane==pianoPane&&e.mods.isCtrlDown()){double length=project.patterns[(size_t)project.activePattern].length;pianoSpan=jlimit(1.,length,pianoSpan*(wheel.deltaY>0?.8:1.25));pianoStart=jlimit(0.,jmax(0.,length-pianoSpan),pianoStart);repaint();return;}
        if(h.action==noteHit&&e.mods.isAltDown()){checkpoint();selectedNote=h.data;note()->velocity=jlimit(.01f,1.f,note()->velocity+(wheel.deltaY>0?.05f:-.05f));changed();return;}
        if(h.pane==pianoPane){if(e.mods.isShiftDown())pianoStart=jlimit(0.,jmax(0.,project.patterns[(size_t)project.activePattern].length-pianoSpan),pianoStart+(wheel.deltaY<0?1:-1));else pianoLow=jlimit(0,128-pianoRows,pianoLow+(wheel.deltaY>0?3:-3));repaint();return;}
        if(h.action==rackGrid){checkpoint();selectedTrack=h.data;double beat=h.extra*.25;for(auto& n:lane())if(std::abs(n.beat-beat)<.00001)n.velocity=jlimit(.01f,1.f,n.velocity+(wheel.deltaY>0?.05f:-.05f));changed();return;}
        if(h.pane==rackPane){trackScroll=jlimit(0,jmax(0,(int)project.tracks.size()-8),trackScroll+(wheel.deltaY<0?1:-1));repaint();return;}
        if(h.pane==playlistPane){if(e.mods.isCtrlDown())playlistSpan=jlimit(8.,512.,playlistSpan*(wheel.deltaY>0?.8:1.25));else playlistStart=jmax(0.,playlistStart+(wheel.deltaY<0?4:-4));repaint();return;}return;
    }
}
bool Studio::keyPressed(const KeyPress& k){
    auto mods=k.getModifiers();auto c=CharacterFunctions::toLowerCase(k.getTextCharacter());int key=k.getKeyCode();
    if(mods.isCommandDown()&&mods.isShiftDown()&&c=='k'){perform(typingSustainToggle);return true;}
    if(recording){if(key==KeyPress::spaceKey||key==KeyPress::escapeKey){perform(stop);return true;}if(audioRecordMode||mods.isCommandDown()||mods.isAltDown())return true;String keys="zsxdcvgbhnjm";int offset=keys.indexOfChar(c);if(offset<0){keys="q2w3er5t6y7u";offset=keys.indexOfChar(c);if(offset>=0)offset+=12;}if(offset>=0&&!heldKeys.count(key)){int pitch=(octave+1)*12+offset;typingKeyOn(key,pitch);}return true;}
    if(mods.isAltDown()&&c=='t'){perform(mods.isCommandDown()?sectionAdd:markerAdd);return true;}
    if(mods.isAltDown()&&(key==KeyPress::leftKey||key==KeyPress::rightKey)){perform(key==KeyPress::rightKey?markerNext:markerPrevious);return true;}
    if(mods.isAltDown()&&c=='s'){perform(splitClip);return true;}
    if(key==KeyPress::F1Key){showHelp();return true;}if(key==KeyPress::F5Key){showPane(playlistPane,true);return true;}if(key==KeyPress::F6Key){showPane(rackPane,true);return true;}if(key==KeyPress::F7Key){showPane(pianoPane,true);return true;}
    if(key==KeyPress::F9Key){showPane(mixerPane,true);return true;}if(key==KeyPress::F8Key&&mods.isAltDown()){perform(browserToggle);return true;}if(key==KeyPress::F10Key){perform(settings);return true;}if(key==KeyPress::F11Key){perform(notesAction);return true;}
    if(key==KeyPress::F12Key){cancelNoteTool();releaseKeys();for(auto& p:panes)p.visible=false;saveLayout();repaint();return true;}if(key==KeyPress::F4Key){perform(patternAdd);return true;}if(key==KeyPress::F2Key){if(activePane==mixerPane)editName(false);else renamePattern();return true;}
    if(mods.isCommandDown()){
        if(mods.isShiftDown()&&(key==KeyPress::leftKey||key==KeyPress::rightKey)){perform(typingTransposeSet,jlimit(-12,12,typingTranspose+(key==KeyPress::rightKey?1:-1)));return true;}
        if(mods.isShiftDown()&&(key==KeyPress::upKey||key==KeyPress::downKey)){perform(typingVelocitySet,jlimit(1,127,typingVelocity+(key==KeyPress::upKey?8:-8)));return true;}
        if(key==KeyPress::spaceKey){perform(play);return true;}if(key==KeyPress::upKey){perform(transposeUp);return true;}if(key==KeyPress::downKey){perform(transposeDown);return true;}
        if(c=='s'){if(mods.isShiftDown())chooseFile(saveAction);else save();return true;}if(c=='o'){chooseFile(open);return true;}if(c=='n'){perform(saveVersion);return true;}
        if(c=='z'&&activePane==wavePane){perform(mods.isShiftDown()?waveRedo:waveUndo);return true;}if(c=='y'&&activePane==wavePane){perform(waveRedo);return true;}
        if(c=='z'){if(mods.isAltDown())applyHistory(false);else if(mods.isShiftDown())applyHistory(true);else applyHistory(!redoStack.empty());return true;}if(c=='y'){applyHistory(true);return true;}
        if(c=='c'){perform(copySelection);return true;}if(c=='x'){perform(cutSelection);return true;}if(c=='v'){perform(activePane==wavePane&&mods.isShiftDown()?waveReplace:pasteSelection);return true;}if(c=='b'){perform(duplicateSelection);return true;}
        if(c=='p'){perform(commandSearch);return true;}
        if(c=='q'){perform(quickQuantize);return true;}
        if(c=='r'){perform(exportAudio);return true;}if(c=='m'){if(mods.isShiftDown())chooseFile(exportMidiAction);else perform(metro);return true;}if(c=='t'){perform(typingToggle);return true;}
        if(c=='h'){if(mods.isShiftDown()){perform(layoutReset);}else{releaseKeys();engine->resetRequested.store(true);}return true;}
        if(c=='a'&&activePane==wavePane){perform(waveAll);return true;}
        if(c=='a'&&activePane==pianoPane){selectedNotes.clear();for(int i=0;i<(int)lane().size();++i)selectedNotes.insert(i);selectedNote=selectedNotes.empty()?-1:*selectedNotes.begin();repaint();return true;}
        if(c=='d'&&activePane==pianoPane){selectedNotes.clear();selectedNote=-1;repaint();return true;}return false;
    }
    if(mods.isAltDown()&&c=='q'){perform(quantize);return true;}
    if(mods.isAltDown()&&c=='r'){perform(humanize);return true;}
    if(mods.isAltDown()&&c=='e'){perform(euclideanOpen);return true;}
    if(mods.isAltDown()&&c=='a'){perform(arpeggiatorOpen);return true;}
    if(key==KeyPress::spaceKey){if(engine->playing.load())perform(stop);else perform(play);return true;}
    if(key==KeyPress::homeKey){engine->seekRequested.store(0);return true;}
    if(key==KeyPress::escapeKey){if(exporting.load()){cancelExport.store(true);return true;}perform(paneClose,activePane);return true;}
    if(key==KeyPress::deleteKey){deleteSelection();return true;}if(key==KeyPress::backspaceKey){snap=snap>0?0:.25;repaint();return true;}
    if(key==KeyPress::tabKey){auto start=std::find(paneOrder.begin(),paneOrder.end(),activePane);int index=start==paneOrder.end()?0:(int)(start-paneOrder.begin());for(int n=1;n<=paneCount;++n){int p=paneOrder[(size_t)((index+n)%paneCount)];if(panes[(size_t)p].visible){showPane(p);break;}}return true;}
    if(key==KeyPress::returnKey&&activePane==playlistPane){perform(paneMaximize,playlistPane);return true;}
    if(key==KeyPress::upKey||key==KeyPress::downKey){perform(trackSelect,jlimit(0,(int)project.tracks.size()-1,selectedTrack+(key==KeyPress::downKey?1:-1)));repaint();return true;}
    if(!typingEnabled){if(c=='p'||c=='b'||c=='d'||c=='e'){perform(toolSelect,c=='p'?0:c=='b'?1:c=='d'?2:3);return true;}if(c=='l'){perform(mode);return true;}if(c=='r'){perform(record);return true;}}
    if(c=='['){perform(typingOctaveSet,jmax(1,octave-1));return true;}if(c==']'){perform(typingOctaveSet,jmin(7,octave+1));return true;}
    if(typingEnabled){String keys="zsxdcvgbhnjm";int offset=keys.indexOfChar(c);if(offset<0){keys="q2w3er5t6y7u";offset=keys.indexOfChar(c);if(offset>=0)offset+=12;}
        if(offset>=0){if(!heldKeys.count(key)){int pitch=(octave+1)*12+offset;typingKeyOn(key,pitch);}return true;}}
    return false;
}
bool Studio::keyStateChanged(bool){std::vector<int> released;for(const auto& [key,note]:heldKeys)if(!KeyPress::isKeyCurrentlyDown(key))released.push_back(key);for(int key:released)typingKeyOff(key);return true;}
void Studio::releaseKeys(){for(const auto& [key,note]:heldKeys)releaseTypingNote(note);heldKeys.clear();for(const auto& note:sustainedKeys)releaseTypingNote(note);sustainedKeys.clear();if(mousePitch>=0){engine->audition(mousePitchTrack,mousePitch,false);mousePitch=-1;}}
void Studio::focusLost(FocusChangeType){releaseKeys();}
}
