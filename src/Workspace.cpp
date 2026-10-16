// SPDX-License-Identifier: AGPL-3.0-only
#include "Studio.h"
#include <cmath>

namespace pulse {
using namespace juce;
Rectangle<float> Studio::workspaceBounds()const{float left=browserVisible?browserWidth+4:4;return{left,96,(float)getWidth()-left-4,(float)getHeight()-118};}
Rectangle<float> Studio::boundedPane(Rectangle<float> r,int pane)const{
    auto ws=workspaceBounds();const float minW[]={720,620,580,640,430,620,620,660,720,760,650,680,760,660,760},minH[]={260,210,330,326,360,320,420,370,390,430,650,620,430,650,560};
    float w=jmin(ws.getWidth(),jmax(minW[pane],r.getWidth())),h=jmin(ws.getHeight(),jmax(minH[pane],r.getHeight()));
    return{jlimit(ws.getX(),jmax(ws.getX(),ws.getRight()-w),r.getX()),jlimit(ws.getY(),jmax(ws.getY(),ws.getBottom()-h),r.getY()),w,h};
}
void Studio::resetLayout(){
    if(noteToolBound)cancelNoteTool();
    auto ws=workspaceBounds();
    panes[playlistPane]={{ws.getX()+2,ws.getY()+3,ws.getWidth()-4,jmax(320.f,ws.getHeight()-336)}, {},true,false};
    panes[mixerPane]={{ws.getX()+2,ws.getBottom()-328,ws.getWidth()-4,326},{},true,false};
    panes[rackPane]={{ws.getX()+25,ws.getY()+42,jmin(836.f,ws.getWidth()-60),303},{},true,false};
    panes[pianoPane]={{ws.getX()+35,ws.getY()+58,ws.getWidth()-70,jmin(570.f,ws.getHeight()-95)},{},false,false};
    panes[channelPane]={{ws.getCentreX()-230,ws.getY()+115,460,410},{},false,false};
    panes[automationPane]={{ws.getX()+80,ws.getY()+80,jmin(820.f,ws.getWidth()-100),390},{},false,false};
    panes[effectsPane]={{ws.getX()+110,ws.getY()+72,jmin(800.f,ws.getWidth()-130),460},{},false,false};
    panes[wavePane]={{ws.getX()+60,ws.getY()+55,jmin(960.f,ws.getWidth()-85),480},{},false,false};
    panes[assetsPane]={{ws.getX()+50,ws.getY()+62,jmin(1000.f,ws.getWidth()-75),480},{},false,false};
    panes[templatesPane]={{ws.getX()+60,ws.getY()+54,jmin(1000.f,ws.getWidth()-80),510},{},false,false};
    panes[recordingPane]={{ws.getX()+95,ws.getY()+68,720,650},{},false,false};
    panes[exportPane]={{ws.getX()+80,ws.getY()+55,760,650},{},false,false};
    panes[markersPane]={{ws.getX()+60,ws.getY()+60,820,510},{},false,false};
    panes[noteToolsPane]={{ws.getRight()-740,ws.getY()+70,720,650},{},false,false};
    for(int p=0;p<paneCount;++p)panes[(size_t)p].bounds=boundedPane(panes[(size_t)p].bounds,p);
    panes[synthPane]={{ws.getX()+45,ws.getY()+35,jmin(920.f,ws.getWidth()-60),590},{},false,false};
    paneOrder={playlistPane,mixerPane,rackPane,pianoPane,channelPane,automationPane,effectsPane,wavePane,assetsPane,templatesPane,recordingPane,exportPane,markersPane,noteToolsPane,synthPane};activePane=rackPane;layoutReady=true;pianoLow=-1;repaint();
}
void Studio::showPane(int pane,bool toggle){
    if(!layoutReady)resetLayout();auto& p=panes[(size_t)pane];
    if(toggle&&p.visible&&activePane==pane){if(pane==noteToolsPane)cancelNoteTool();p.visible=false;releaseKeys();for(auto it=paneOrder.rbegin();it!=paneOrder.rend();++it)if(panes[(size_t)*it].visible){activePane=*it;break;}}
    else{p.visible=true;activePane=pane;paneOrder.erase(std::remove(paneOrder.begin(),paneOrder.end(),pane),paneOrder.end());paneOrder.push_back(pane);}
    saveLayout();repaint();
}
void Studio::saveLayout(){
    if(!audioEnabled||!layoutReady)return;
    TemporaryFile temp(root.getChildFile("data/layout-v0.29.json"));if(temp.getFile().replaceWithText(JSON::toString(workspaceToJson(captureWorkspace()),false,12)))temp.overwriteTargetFileWithTemporary();
}
void Studio::restoreLayout(){
    auto file=root.getChildFile("data/layout-v0.29.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.28.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.27.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.26.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.25.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.24.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.23.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.22.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.21.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.20.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.19.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.18.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.17.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.16.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.15.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.14.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.13.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.12.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.11.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.10.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.9.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.8.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.7.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.6.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.5.json");if(!file.existsAsFile())file=root.getChildFile("data/layout-v0.4.json");if(!file.existsAsFile())file=root.getChildFile("data/layout.json");if(!file.existsAsFile()||file.getSize()>16384)return;WorkspaceLayout layout;auto parsed=workspaceFromJson(JSON::parse(file.loadFileAsString()),layout);if(parsed.wasOk())applyWorkspace(layout,"restored layout");
}
double Studio::snapped(double value,bool bypass)const{return bypass||snap<=0?value:std::round(value/snap)*snap;}
void Studio::mouseMove(const MouseEvent& e){
    hoverHint.clear();setMouseCursor(MouseCursor::NormalCursor);for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->rect.contains(e.position)){hoverHint=hintFor(*it);
        if(it->pane==automationPane&&automationRect.contains(e.position))if(auto* c=automationClip())automationCursor=snapped((e.position.x-automationRect.getX())/automationRect.getWidth()*c->length);
        if(it->pane==pianoPane&&pianoRect.contains(e.position))editCursorBeat=snapped(pianoStart+(e.position.x-pianoRect.getX())/pianoRect.getWidth()*pianoSpan);
        if(it->pane==playlistPane&&arrangeRect.contains(e.position))editCursorBeat=snapped(playlistStart+(e.position.x-arrangeRect.getX())/arrangeRect.getWidth()*playlistSpan);
        if(it->action==paneResize)setMouseCursor(MouseCursor::BottomRightCornerResizeCursor);else if(it->action==paneMove)setMouseCursor(MouseCursor::DraggingHandCursor);else if(it->action==browserResize)setMouseCursor(MouseCursor::LeftRightResizeCursor);
        else if(it->action==pianoGrid||it->action==arrangeGrid)setMouseCursor(MouseCursor::CrosshairCursor);else if(isParameter(it->action))setMouseCursor(MouseCursor::UpDownResizeCursor);else if(it->action!=paneBody)setMouseCursor(MouseCursor::PointingHandCursor);break;}
        repaint({0,0,305,46});repaint({0,getHeight()-22,getWidth(),22});
}
void Studio::mouseExit(const MouseEvent&){hoverHint.clear();repaint();}
String Studio::hintFor(const Hit& h)const{
    switch(h.action){
    case arpeggiatorOpen:return"Arpeggiate selected chords into editable notes with live preview (Alt+A)";
    case euclideanOpen:return"Euclidean rhythms: pulses, rotation, range and live preview (Alt+E)";
    case commandSearch:return"Search commands, channels, patterns, clips and help (Ctrl+P)";
    case recordingOpen:return"Tap tempo, count-in, pre-roll and punch recording";
    case markersOpen:return"Open Song markers and section navigation";
    case markerHit:return"Drag a flag to move; drag a section end to resize; right-click for marker actions";
    case markerRail:return"Right-click to add a marker or section here";
    case markerLoop:return"Loop the selected Song section without moving its clips";
    case markerExport:return"Open export settings using the selected section range";
    case renderChoose:return"Export a snapshot to a local file or folder; editing can continue";
    case renderCancel:return"Cancel the current render; existing destination files are preserved";
    case renderDither:return"Apply deterministic TPDF dither during final integer quantization";
    case renderPunch:return"Use the recording punch markers as this export's beat range";
    case monitorToggle:return"Monitor selected input through the selected source channel; use headphones";
    case monitorLevelChange:return"Change monitor level by 5%; dry WAV recording level is unchanged";
    case inputModeSelect:return"Record and monitor stereo inputs or one mono input; changing selection turns monitoring off";
    case inputAudioSettings:return"Select and enable local audio device input channels";
    case templatesOpen:return"Browse local project templates";case templateStart:return"Start a new session; the current song is backed up and kept in undo";case templateSave:return"Save a reusable local copy, collecting its samples";
    case assetsOpen:return"Project audio files: audit, find and relink missing samples";case assetsRefresh:return"Refresh audio metadata and references";case assetLocate:return"Locate a replacement for every reference to this source";case assetFind:return"Search a local folder and subfolders; review matches before applying";case assetApplyMatches:return"Apply unique matches together; ambiguous names remain unchanged";case assetChooseMatch:return"Choose one matching path explicitly";case assetCancelScan:return"Cancel folder search; keep existing project references";
    case play:return"Play / stop (Space); pause (Ctrl+Space)";case stop:return"Stop and rewind (Home)";case record:return"Record typing-keyboard notes (R with typing keyboard off)";
    case mode:return"Pattern / song mode (L with typing keyboard off)";case bpmDial:return"Tempo: drag up/down, wheel, or right-click to type a value";
    case composeTab:return"Channel rack (F6)";case arrangeTab:return"Playlist (F5)";case pianoOpen:return"Piano roll (F7)";case mixTab:return"Mixer (F9)";
    case browserToggle:return"Show / hide Browser (Alt+F8)";case typingToggle:return"Typing keyboard (Ctrl+T). Right-click for velocity, sustain and retrospective capture.";
    case paneMove:return"Drag window; double-click to maximize / restore";case paneResize:return"Resize window";case paneMaximize:return"Maximize / restore window";case paneClose:return"Close window (Esc)";
    case patternMoveEarlier:case patternMoveLater:return"Reorder the current pattern while preserving Playlist and clipboard references";case patternDeleteUnused:return"Delete a pattern with no Playlist references, with confirmation and undo";case patternMakeUnique:return"Clone the selected clip's pattern and relink only that clip";case patternSplitChannels:return"Separate channels into patterns and preserve Song playback";case patternMergeLayers:return"Create one layered pattern; originals and clips stay intact";case notePresetsOpen:return"Save, recall and exchange local settings for the active note tool";case channelOpen:return"Open channel settings; right-click for Piano roll and channel menu";case rackGrid:return"Left-click: enable step; right-click: disable step; Shift-click: ratchets";
    case mute:return"Channel enabled / muted; right-click: solo";case pianoGrid:return"Draw notes; right-click erases; Ctrl-drag selects; Alt bypasses snap";case noteHit:return"Move note; drag its right edge to resize; Shift-drag clones; right-click erases";
    case arrangeGrid:return"Place current pattern; right-click erases; Ctrl-drag selects";case clipHit:return"Alt-right-click: clip menu. Move clip; drag edges to trim; Shift-drag clones; double-click opens settings";case recordMode:return"Switch REC between keyboard notes and audio input";case importAudioClip:return"Import audio into Playlist";
    case velocityDrag:return"Drag to edit note velocity";case snapMenu:return"Snap to grid; Alt bypasses snap; Backspace toggles snap";case seekRuler:return"Click ruler to position playback";
    case volumeDial:case mixFader:return"Volume: drag / wheel; Ctrl for fine adjustment; Alt-click resets";case panDial:return"Pan: drag / wheel; Alt-click resets to center";
    case cutoffDial:return"Tone filter cutoff";case driveDial:return"Saturation amount";case sendDial:return"Ping-pong delay send";case attackDial:return"Amplitude attack";case releaseDial:return"Amplitude release";
    case patternMenu:return"Select pattern; right-click to rename (F2)";case patternSelect:return"Select pattern";case metronomeSettingsOpen:return"Choose local metronome sound, level and bar accent";case patternColourMenu:return"Colour the current pattern and every Playlist instance";case sectionClipsOpen:return"Copy or move complete clips in the selected section with explicit destination overlap handling";case patternExtract:return"Copy or move explicitly selected Piano roll notes into a new pattern";case patternMergeOptions:return"Layer two patterns with explicit duplicate/onset conflict handling";case patternAdd:return"New empty pattern (F4)";case patternDuplicate:return"Clone current pattern";
    case instrumentSelect:return"Replace selected channel's instrument with this factory sound";case addChannel:return"Add an instrument channel";case browserFolder:return"Expand / collapse folder";
    case effectsOpen:return"Open insert effects";case effectToggle:return"Enable or bypass insert processing";case routeDestination:return"Route selected insert exclusively to this destination";case outputRoute:return"Choose output destination; feedback routes are disabled";case sidechainRoute:return"Compressor detector source (one sample delay)";case addBus:return"Add a mixer bus";case quantize:return"Quantize with strength and preview (Alt+Q); right-click for other note tools";case humanize:return"Humanize note timing, length and velocity (Alt+R)";case quickQuantize:return"Quick quantize starts and lengths to quarter-beats (Ctrl+Q)";case generate:return"Generate editable rhythm, chords, or melody locally";case automationPoint:return"Drag automation point; right-click deletes interior points";case automationGrid:return"Click to add automation points; Alt bypasses snap";case automationHold:return"Switch selected point segment between linear and step";case settings:return"Audio settings (F10)";
    case saveAction:return"Save project with collected assets (Ctrl+S)";case exportAudio:return"Export audio settings (Ctrl+R)";case toolSelect:return"Choose editor tool; P/B/D/E shortcuts with typing keyboard off";
    default:return"Pulse Studio";
    }
}
PopupMenu::Options Studio::menuOptions(int action,int data){
    auto options=PopupMenu::Options().withTargetComponent(this).withDeletionCheck(*this).withMousePosition();
    if(action>0)for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->action==action&&(data<0||it->data==data))return options.withTargetScreenArea(localAreaToGlobal(it->rect.toNearestInt()));
    return options;
}
void Studio::dispatchMenuResult(int result){
    if(result==0)return;
    if(result>=menuView&&result<menuView+paneCount)showPane(result-menuView,true);
    else if(result>=menuMaximize&&result<menuMaximize+paneCount)perform(paneMaximize,result-menuMaximize);
    else if(result>=menuClose&&result<menuClose+paneCount)perform(paneClose,result-menuClose);
    else if(result>=menuInstrument&&result<menuInstrument+instrumentCount)modifyChannel(addChannel,result-menuInstrument);
    else if(result==menuSaveAs)chooseFile(saveAction);
    else if(result>=menuPattern&&result<menuPattern+(int)project.patterns.size())perform(patternSelect,result-menuPattern);
    else if(result>=menuMergePattern&&result<menuMergePattern+(int)project.patterns.size())perform(patternMergeLayers,result-menuMergePattern);
    else if(result>=menuSequencePattern&&result<menuSequencePattern+(int)project.patterns.size())perform(patternMergeSequence,result-menuSequencePattern);
    else if(result>=menuPatternColour&&result<menuPatternColour+patternColourCount)perform(patternColourSet,result-menuPatternColour);
    else if(result>0&&result<lastAction)perform(result);
}
void Studio::showMenu(int kind){
    PopupMenu menu;
    if(kind==0){menu.addItem(newProject,"New from template...");menu.addItem(templateSave,"Save current as template...");menu.addItem(open,"Open...   Ctrl+O");menu.addItem(saveAction,"Save   Ctrl+S");menu.addItem(menuSaveAs,"Save as...   Ctrl+Shift+S");menu.addItem(saveVersion,"Save new version   Ctrl+N");menu.addSeparator();PopupMenu exportMenu;exportMenu.addItem(exportAudio,"Audio file (WAV / FLAC)...   Ctrl+R");exportMenu.addItem(exportMidiAction,"MIDI file...   Ctrl+Shift+M");exportMenu.addItem(exportStems,"Isolated tracks...");menu.addSubMenu("Export",exportMenu);menu.addSeparator();menu.addItem(assetsOpen,"Project audio files / missing samples...");menu.addItem(waveformOpen,"Open audio in waveform editor...");menu.addItem(folderChoose,"Browse local sample folder...");menu.addItem(importSample,"Import audio sample...");menu.addItem(importAudioClip,"Import audio clip into Playlist...");menu.addItem(importMidiAction,"Import MIDI...");}
    else if(kind==1){menu.addItem(undoAction,"Undo last edit   Ctrl+Alt+Z",activePane==wavePane?!waveHistory.empty():!undoStack.empty());menu.addItem(redoAction,"Redo   Ctrl+Y",activePane==wavePane?!waveRedoHistory.empty():!redoStack.empty());menu.addSeparator();menu.addItem(copySelection,"Copy   Ctrl+C");menu.addItem(cutSelection,"Cut   Ctrl+X");menu.addItem(pasteSelection,"Paste at cursor   Ctrl+V");menu.addItem(duplicateSelection,"Duplicate to the right   Ctrl+B");menu.addSeparator();menu.addItem(quantize,"Quantize...   Alt+Q");menu.addItem(humanize,"Humanize...   Alt+R");menu.addItem(euclideanOpen,"Euclidean rhythm...   Alt+E");menu.addItem(arpeggiatorOpen,"Arpeggiator...   Alt+A");menu.addItem(quickQuantize,"Quick quantize   Ctrl+Q");menu.addItem(patternClear,"Clear selected channel notes");}
    else if(kind==2){menu.addItem(addBus,"Mixer bus");menu.addSeparator();for(int i=0;i<instrumentCount;++i)menu.addItem(menuInstrument+i,instrumentName((Instrument)i));}
    else if(kind==3){showPatternMenu();return;}
    else if(kind==4){const char* names[]={"Playlist   F5","Channel rack   F6","Piano roll   F7","Mixer   F9","Channel settings","Automation editor","Mixer effects","Waveform editor","Project audio files","Project templates","Tempo and recording","Export audio","Song markers and sections","Piano roll note tools","Synthesizer"};for(int p=0;p<paneCount;++p)menu.addItem(menuView+p,names[p],true,panes[(size_t)p].visible);menu.addItem(browserToggle,"Browser   Alt+F8",true,browserVisible);menu.addSeparator();menu.addItem(workspaceMenu,"Workspaces...");menu.addItem(workspaceSave,"Save workspace as...");menu.addItem(interfaceScaleMenu,"Interface scaling...");menu.addItem(layoutReset,"Arrange windows   Ctrl+Shift+H");}
    else if(kind==5){menu.addItem(controlToggle,"Local MCP control",true,controlMailbox.enabled());menu.addSeparator();menu.addItem(recordingOpen,"Tempo and recording...");menu.addItem(settings,"Audio settings...   F10");menu.addItem(typingToggle,"Typing keyboard to piano   Ctrl+T",true,typingEnabled);menu.addItem(typingMenu,"Typing velocity, octave and capture...");menu.addItem(typingRecall,"Recall last typing phrase (new pattern)");menu.addItem(metro,"Metronome   Ctrl+M",true,project.metronome);menu.addItem(metronomeSettingsOpen,"Metronome sound and level...");menu.addItem(loop,"Loop playback",true,project.loop);menu.addItem(recordMode,"Record audio input into Playlist",true,audioRecordMode);}
    else if(kind==6){menu.addItem(commandSearch,"Find commands...   Ctrl+P");menu.addSeparator();menu.addItem(automationCreate,"Create volume automation clip");menu.addItem(generate,"Generate notes...");menu.addItem(quantize,"Quantize...   Alt+Q");menu.addItem(humanize,"Humanize...   Alt+R");menu.addItem(euclideanOpen,"Euclidean rhythm...   Alt+E");menu.addItem(arpeggiatorOpen,"Arpeggiator...   Alt+A");menu.addItem(quickQuantize,"Quick quantize   Ctrl+Q");menu.addItem(transposeUp,"Transpose up one octave   Ctrl+Up");menu.addItem(transposeDown,"Transpose down one octave   Ctrl+Down");}
    else if(kind==7){showHelp();return;}
    else{int p=kind-10;if(p<0||p>=paneCount)return;menu.addItem(menuMaximize+p,"Maximize / restore");menu.addItem(menuClose+p,"Close window");menu.addItem(layoutReset,"Arrange windows");if(p==rackPane){menu.addSeparator();menu.addItem(addChannel,"Add channel...");menu.addItem(patternDuplicate,"Clone pattern");}if(p==playlistPane){menu.addSeparator();menu.addItem(markersOpen,"Song markers and sections...");menu.addItem(importAudioClip,"Import audio clip...");menu.addItem(audioClipSettings,"Audio clip settings...");menu.addItem(splitClip,"Split audio at cursor   Alt+S");menu.addItem(patternMakeUnique,"Make selected pattern clip unique",selectedPatternClip()&&project.patterns.size()<maxPatterns);}if(p==pianoPane){menu.addSeparator();menu.addItem(quantize,"Quantize...");menu.addItem(humanize,"Humanize...");menu.addItem(euclideanOpen,"Euclidean rhythm...");menu.addItem(arpeggiatorOpen,"Arpeggiator...");menu.addItem(generate,"Generate notes");menu.addItem(patternExtract,"Pattern from selected notes...",!patternNoteSelection().empty()&&project.patterns.size()<maxPatterns&&!pendingNoteTool());}}
    auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(kind>=0&&kind<8?menuOptions(menuOpen,kind):menuOptions(),[safe](int result){if(safe)safe->dispatchMenuResult(result);});
}
void Studio::showPatternMenu(){
    PopupMenu menu;std::array<int,maxPatterns> usage{};for(const auto& clip:project.clips)if(!clip.isAudio()&&!clip.isAutomation()&&clip.pattern>=0&&clip.pattern<maxPatterns)++usage[(size_t)clip.pattern];
    for(int i=0;i<(int)project.patterns.size();++i)menu.addColouredItem(menuPattern+i,String(i+1)+". "+project.patterns[(size_t)i].name+"  ["+String(usage[(size_t)i])+" clips]",project.patterns[(size_t)i].colour?patternColor(i):Colour(0xffd9ddde),true,i==project.activePattern);
    menu.addSeparator();menu.addItem(patternAdd,"Find next empty / add pattern   F4");menu.addItem(patternRename,"Rename pattern...   F2");menu.addItem(patternSteps,"Set pattern length (steps)...");PopupMenu colours;for(int c=0;c<patternColourCount;++c)colours.addColouredItem(menuPatternColour+c,patternColourName(c),c?Colour(patternColourARGB(c)):Colour(0xffd9ddde),!recording&&!pendingNoteTool(),c==project.patterns[(size_t)project.activePattern].colour);menu.addSubMenu("Current pattern colour",colours);menu.addItem(patternDuplicate,"Clone pattern");menu.addSeparator();
    menu.addItem(patternMakeUnique,"Make selected Playlist clip unique",selectedPatternClip()&&project.patterns.size()<maxPatterns&&!pendingNoteTool());int lanes=0;for(const auto& lane:project.patterns[(size_t)project.activePattern].lanes)lanes+=!lane.empty();menu.addItem(patternSplitChannels,"Split current pattern by channel",lanes>=2&&project.patterns.size()+lanes<=maxPatterns&&!pendingNoteTool());
    menu.addItem(patternExtract,"Pattern from selected notes...",!patternNoteSelection().empty()&&!project.tracks[(size_t)selectedTrack].isBus&&project.patterns.size()<maxPatterns&&!pendingNoteTool());menu.addItem(patternMergeOptions,"Merge patterns with conflict options...",project.patterns.size()>1&&project.patterns.size()<maxPatterns&&!pendingNoteTool());
    PopupMenu merge;for(int i=0;i<(int)project.patterns.size();++i)if(i!=project.activePattern)merge.addItem(menuMergePattern+i,project.patterns[(size_t)i].name,project.patterns.size()<maxPatterns&&!pendingNoteTool());menu.addSubMenu("Merge current pattern with (layer once)",merge,project.patterns.size()>1);
    PopupMenu sequence;for(int i=0;i<(int)project.patterns.size();++i)if(i!=project.activePattern)sequence.addItem(menuSequencePattern+i,project.patterns[(size_t)i].name,project.patterns.size()<maxPatterns&&project.patterns[(size_t)project.activePattern].length+project.patterns[(size_t)i].length<=64&&!pendingNoteTool());menu.addSubMenu("Append pattern after current (new pattern)",sequence,project.patterns.size()>1);
    menu.addSeparator();bool editable=!pendingNoteTool()&&!recording;menu.addItem(patternMoveEarlier,"Move current pattern earlier",editable&&project.activePattern>0);menu.addItem(patternMoveLater,"Move current pattern later",editable&&project.activePattern+1<(int)project.patterns.size());menu.addItem(patternDeleteUnused,"Delete current unused pattern...",editable&&project.patterns.size()>1&&patternUsage(project,project.activePattern)==0);menu.addItem(patternDeleteUsed,"Delete current pattern and its Playlist clips...",editable&&project.patterns.size()>1&&patternUsage(project,project.activePattern)>0);
    auto safe=Component::SafePointer<Studio>(this);auto menuRevision=revision;menu.showMenuAsync(menuOptions(patternMenu),[safe,menuRevision](int result){if(!safe||!result)return;if(safe->recording||safe->revision!=menuRevision){safe->status="The project changed. Reopen the pattern menu.";return;}safe->dispatchMenuResult(result);});
}
void Studio::renamePattern(){
    int p=project.activePattern;auto* dialog=new AlertWindow("Pattern name","",MessageBoxIconType::NoIcon,this);dialog->addTextEditor("value",project.patterns[(size_t)p].name);dialog->addButton("Accept",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,p](int result){if(!safe||result!=1||p>=(int)safe->project.patterns.size())return;auto name=dialog->getTextEditorContents("value").trim();if(name.isNotEmpty()){safe->checkpoint();safe->project.patterns[(size_t)p].name=name.substring(0,80);safe->changed(false);}}),true);
}
void Studio::setPatternSteps(){
    if(recording||recorder.isActive()||pendingNoteTool()){status="Finish the take and resolve the note preview before changing pattern length.";return;}
    int p=project.activePattern;if(p<0||p>=(int)project.patterns.size())return;
    int current=jlimit(16,jmax(16,(int)std::llround(project.patterns[(size_t)p].length*4)),64);
    auto* dialog=new AlertWindow("Pattern length","Steps per pattern (16-64 sixteenths; 16 = 4 beats, 32 = 8 beats, 64 = 16 beats).",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("value",String(current));dialog->addButton("Accept",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));auto safe=Component::SafePointer<Studio>(this);
    auto sourceRevision=revision;auto patternId=project.patterns[(size_t)p].editorId;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,p,sourceRevision,patternId](int result){if(!safe||result!=1||p>=(int)safe->project.patterns.size())return;if(safe->recording||safe->recorder.isActive()||safe->revision!=sourceRevision||safe->project.patterns[(size_t)p].editorId!=patternId){safe->status="Pattern changed. Reopen Set pattern length.";return;}auto text=dialog->getTextEditorContents("value").trim();if(text.isEmpty()||text.length()>3||!text.containsOnly("0123456789"))return;int steps=jlimit(16,64,text.getIntValue());steps=jmax(16,steps/4*4);double length=steps/4.;auto& pat=safe->project.patterns[(size_t)p];if(std::abs(pat.length-length)<1e-9)return;safe->checkpoint();for(auto& lane:pat.lanes)std::erase_if(lane,[&](const Note& n){return n.beat>=length-1e-9;});pat.length=length;safe->changed();}),true);
}
void Studio::showChannelMenu(int track){
    releaseKeys();selectedTrack=track;PopupMenu menu;menu.addItem(pianoOpen,"Piano roll   F7",!project.tracks[(size_t)track].isBus);menu.addItem(channelOpen,"Channel settings");menu.addItem(rename,"Rename...");menu.addSeparator();menu.addItem(cloneChannel,"Clone channel",project.tracks.size()<maxTracks);menu.addItem(deleteChannel,"Delete channel",project.tracks.size()>1);menu.addSeparator();menu.addItem(mute,"Mute / unmute",true,project.tracks[(size_t)track].mute);menu.addItem(solo,"Solo",true,project.tracks[(size_t)track].solo);
    auto safe=Component::SafePointer<Studio>(this);menu.showMenuAsync(menuOptions(),[safe,track](int result){if(!safe||result==0)return;if(result==rename)safe->editName(false);else if(result==cloneChannel||result==deleteChannel)safe->modifyChannel(result,track);else safe->perform(result,track);});
}
void Studio::modifyChannel(int action,int data){
    if(action==addChannel&&data<0){showMenu(2);return;}
    if(action!=deleteChannel&&project.tracks.size()>=maxTracks){status="This build supports 16 channels.";return;}
    if(action==deleteChannel&&project.tracks.size()<=1)return;releaseKeys();checkpoint();engine->stop();
    if(action==deleteChannel){if(clipboardHasClip&&clipClipboard.isAutomation()){if(clipClipboard.automationTrack==data)clipboardHasClip=false;else if(clipClipboard.automationTrack>data)--clipClipboard.automationTrack;}project.tracks.erase(project.tracks.begin()+data);for(auto& p:project.patterns)p.lanes.erase(p.lanes.begin()+data);project.clips.erase(std::remove_if(project.clips.begin(),project.clips.end(),[data](const Clip& c){return c.track==data||(c.isAutomation()&&c.automationTrack==data);}),project.clips.end());for(auto& t:project.tracks){auto& m=t.mixer;if(m.output==data)m.output=-1;else if(m.output>data)--m.output;if(m.sidechain==data)m.sidechain=-1;else if(m.sidechain>data)--m.sidechain;}for(auto& c:project.clips){if(c.track>data)--c.track;if(c.isAutomation()&&c.automationTrack>data)--c.automationTrack;}selectedTrack=jmin(data,(int)project.tracks.size()-1);}
    else{Track t;if(action==addBus){t.isBus=true;t.name="Bus "+String((int)project.tracks.size()+1);t.volume=1;t.cutoff=20000;}else if(action==cloneChannel){t=project.tracks[(size_t)data];t.name+=" copy";}else{t.instrument=(Instrument)jlimit(0,instrumentCount-1,data);t.name=instrumentName(t.instrument);t.volume=.6f;}project.tracks.push_back(t);for(auto& p:project.patterns){if(action==cloneChannel){auto notes=p.lanes[(size_t)data];p.lanes.push_back(std::move(notes));}else p.lanes.emplace_back();}selectedTrack=(int)project.tracks.size()-1;}
    selectedNote=-1;selectedNotes.clear();selectedClip=-1;pianoLow=-1;trackScroll=jmax(0,selectedTrack-7);changed();showPane(action==addBus?mixerPane:rackPane);if(action==addChannel&&isSynth(project.tracks[(size_t)selectedTrack].instrument))showPane(synthPane);if(action==addChannel&&data==(int)Instrument::sample)chooseFile(importSample);
}
void Studio::deleteSelection(){
    if(activePane==noteToolsPane){status="Use Apply or Cancel in the note tools window.";return;}
    if(activePane==markersPane){perform(markerDelete);return;}
    if(activePane==exportPane){status="Use export controls; musical content is preserved.";return;}
    if(activePane==recordingPane){status="Use the tempo and recording controls in this window.";return;}
    if(activePane==templatesPane){status="Choose a starting template, then click New from template.";return;}
    if(activePane==assetsPane){status="Relink files in Project audio files; channels and clips are preserved.";return;}
    if(activePane==wavePane){perform(waveDelete);return;}
    if(activePane==automationPane){deleteAutomationPoint();return;}
    if(activePane==playlistPane&&selectedClip>=0&&selectedClip<(int)project.clips.size()){checkpoint();project.clips.erase(project.clips.begin()+selectedClip);selectedClip=-1;changed();return;}
    if(selectedNotes.empty()&&note())selectedNotes.insert(selectedNote);if(selectedNotes.empty())return;checkpoint();for(auto i=selectedNotes.rbegin();i!=selectedNotes.rend();++i)if(*i<(int)lane().size())lane().erase(lane().begin()+*i);selectedNotes.clear();selectedNote=-1;changed();
}
void Studio::editClipboard(int action){
    if(activePane==noteToolsPane){status="Use Apply or Cancel in the note tools window.";return;}
    if(activePane==markersPane){status="Use marker controls; musical clips are preserved.";return;}
    if(activePane==exportPane){status="Use export controls; musical content is preserved.";return;}
    if(activePane==recordingPane){status="Use the tempo and recording controls in this window.";return;}
    if(activePane==templatesPane){status="Choose a starting template, then click New from template.";return;}
    if(activePane==assetsPane){status="Relink files in Project audio files; channels and clips are preserved.";return;}
    if(activePane==wavePane){perform(action==copySelection?waveCopy:action==cutSelection?waveCut:action==duplicateSelection?waveDuplicate:wavePaste);return;}
    if(activePane==automationPane){editAutomationClipboard(action);return;}
    bool copy=action==copySelection,cut=action==cutSelection,duplicate=action==duplicateSelection;
    if(activePane==playlistPane){
        Clip value;if(copy||cut||duplicate){if(selectedClip<0||selectedClip>=(int)project.clips.size())return;value=project.clips[(size_t)selectedClip];if(!duplicate){clipClipboard=value;clipboardHasClip=true;clipboardPatternId=!value.isAudio()&&!value.isAutomation()?project.patterns[(size_t)value.pattern].editorId:String();}if(copy){status="Playlist clip copied.";return;}if(cut){deleteSelection();return;}value.beat+=value.length;}
        else{if(!clipboardHasClip)return;value=clipClipboard;if(!value.isAudio()&&!value.isAutomation()){int pattern=patternIndexForEditorId(project,clipboardPatternId);if(pattern<0){status="The copied pattern is not in this project. Undo its deletion or copy another clip.";return;}value.pattern=pattern;}value.beat=jmax(0.,snapped(editCursorBeat));value.track=selectedTrack;}
        if(project.clips.size()>=maxClips||(!value.isAudio()&&!value.isAutomation()&&(value.pattern<0||value.pattern>=(int)project.patterns.size()))||value.track>=(int)project.tracks.size()||value.beat>4096){status="Cannot paste clip outside this project's limits.";return;}
        if(value.isAutomation()){int clips=0,points=(int)value.points.size();for(const auto& c:project.clips)if(c.isAutomation()){++clips;points+=(int)c.points.size();}if(clips>=maxAutomationClips||points>maxTotalAutomationPoints){status="Automation clip/point limit reached.";return;}}
        checkpoint();project.clips.push_back(value);selectedClip=(int)project.clips.size()-1;changed();return;
    }
    std::vector<Note> values;
    if(copy||cut||duplicate){for(int i=0;i<(int)lane().size();++i)if(activePane==rackPane||selectedNotes.empty()||selectedNotes.count(i))values.push_back(lane()[(size_t)i]);
        if(values.empty())return;if(!duplicate){noteClipboard=values;clipboardHasClip=false;}if(copy){status="Notes copied. Paste at the editor cursor or duplicate with Ctrl+B.";return;}
        if(cut){if(activePane==rackPane||selectedNotes.empty()){checkpoint();lane().clear();selectedNotes.clear();selectedNote=-1;changed();}else deleteSelection();return;}}
    else values=noteClipboard;
    if(values.empty())return;if(lane().size()+values.size()>maxNotesPerLane){status="Paste exceeds 512 notes per lane.";return;}
    double first=64,end=0;for(const auto& n:values){first=jmin(first,n.beat);end=jmax(end,n.beat+n.length);}double unit=snap>0?snap:.25;
    double at=duplicate?std::ceil(end/unit)*unit:jmax(0.,snapped(editCursorBeat));double last=at+end-first;
    if(last>64){status="Paste exceeds the 64-beat pattern limit.";return;}
    checkpoint();selectedNotes.clear();for(auto n:values){n.beat+=at-first;int index=(int)lane().size();lane().push_back(n);selectedNotes.insert(index);}
    selectedNote=*selectedNotes.begin();auto& pat=project.patterns[(size_t)project.activePattern];if(last>pat.length)pat.length=jmin(64.,std::ceil(last/4)*4);
    if(at>pianoStart+pianoSpan)pianoStart=jmax(0.,at-pianoSpan*.1);pianoStart=jmin(pianoStart,jmax(0.,pat.length-pianoSpan));changed();
}
}
