// SPDX-License-Identifier: AGPL-3.0-only
#include "Version.h"
#include "Studio.h"
#include <cmath>

namespace pulse {
using namespace juce;
namespace {
const Colour surface(0xff434c51);
String pitchName(int pitch) { static const char* n[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}; return String(n[pitch%12])+String(pitch/12-1); }
bool blackKey(int p) { int k=p%12; return k==1||k==3||k==6||k==8||k==10; }
}
Studio::Studio(const File& workspace,bool enableAudio,bool resumeRecovery):project(demoProject()),engine(std::make_unique<Engine>()),root(workspace),audioEnabled(enableAudio) {
    setOpaque(true); setWantsKeyboardFocus(true); setSize(1440,940);
    root.getChildFile("data").createDirectory();
    resetLayout();if(enableAudio){auto retentionLoaded=loadRecoveryRetention(root.getChildFile("data/recovery-retention.json"),retentionPolicy);if(retentionLoaded.failed())status=retentionLoaded.getErrorMessage();auto scaleResult=loadInterfaceScale(root.getChildFile("data/interface.json"),interfaceScalePercent);if(scaleResult.failed())status=scaleResult.getErrorMessage();restoreLayout();restoreLibrary();restoreInputSettings();restoreAudioHistoryOptions();restoreExportSettings();restoreAnalyzerSettings();restoreMixAnalysis();restoreReferenceReport();restoreRestorationOptions();restoreTypingSettings();restoreShortcutSettings();restoreMetronomeSettings();auto harmonyResult=loadHarmonyGuide(root.getChildFile("data/harmony.json"),harmonyGuide);if(harmonyResult.failed())status=harmonyResult.getErrorMessage();}
    getLookAndFeel().setColour(PopupMenu::backgroundColourId,Colour(0xff3d484e));
    getLookAndFeel().setColour(PopupMenu::highlightedBackgroundColourId,Colour(0xff65764f));
    getLookAndFeel().setColour(PopupMenu::textColourId,Colour(0xffd9ddde));
    publish();
    if (enableAudio) {
        auto deviceState=XmlDocument::parse(root.getChildFile("data/audio-device.xml"));
        setAudioChannels(0,2,deviceState.get()); startTimerHz(30);
        restoreStartupRecovery(resumeRecovery);
        assetRevealMissing=true;
        auto controlSettings=JSON::parse(root.getChildFile("data/mcp-settings.json").loadFileAsString());if(controlSettings["enabled"].isBool()&&(bool)controlSettings["enabled"]){auto started=controlMailbox.start(root);if(started.failed())status=started.getErrorMessage();}
    }
}
Studio::~Studio() {
    audioHistory.pause();if(historyTask){historyTask->cancel.store(true);historyTask.reset();}discardAudioHistory();
    if(transcriptionTask){transcriptionTask->cancel.store(true);transcriptionTask.reset();}stopTranscriptionAudition(false);
    if(referenceTask){referenceReveal=false;referenceTask->cancel.store(true);referenceTask->waitForThreadToExit(-1);referenceReport=std::move(referenceTask->report);referenceTask.reset();}discardReference();
    if(restorationTask){restorationTask->cancel.store(true);restorationTask.reset();}discardRestoration();
    mixTask.reset();closeEditHistory();
    stopTimer();retentionCancel.store(true);if(retentionThread.joinable())retentionThread.join();releaseAutosaveVerification();synchronizeAutosave();
    samplePitchCancel.store(true);if(samplePitchThread.joinable())samplePitchThread.join();
    versionCancel.store(true);if(versionThread.joinable())versionThread.join();
    archiveCancel.store(true);if(archiveThread.joinable())archiveThread.join();
    waveEditCancel.store(true);if(waveEditThread.joinable())waveEditThread.join();
    controlMailbox.stop();
    if(audioEnabled)if(auto state=deviceManager.createStateXml())state->writeTo(root.getChildFile("data/audio-device.xml"));
    templateCancel.store(true);if(templateThread.joinable())templateThread.join();
    assetCancel.store(true);if(assetThread.joinable())assetThread.join();
    scanCancel.store(true);if(scanThread.joinable())scanThread.join();
    stopTimer(); shutdownAudio(); finishAudioRecording(); cancelExport.store(true); if (exportThread.joinable()) exportThread.join();
    saveLayout();
    if (dirty && audioEnabled) saveProject(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),root.getChildFile("data/recovery-v0.29.pulse"));
}
void Studio::prepareToPlay(int block,double rate) { audioHistory.invalidate();historyRate.store(rate); inputMonitor.store(false);inputPeaks.reset();monoRight.assign((size_t)jmax(1,block),0);captureInput.setSize(2,jmax(1,block));engine->prepare(rate);if(auto* device=deviceManager.getCurrentAudioDevice()){activeInputs.store(device->getActiveInputChannels().countNumberOfSetBits());activeOutputs.store(device->getActiveOutputChannels().countNumberOfSetBits());} }
void Studio::getNextAudioBlock(const AudioSourceChannelInfo& b) {
    if (b.buffer->getNumChannels()<1||activeOutputs.load()==0) { b.clearActiveBufferRegion(); return; }
    captureCallbacks.fetch_add(1);bool captureAllowed=!captureClosing.load()&&recorder.isActive();
    for(int at=0;at<b.numSamples;){int count=jmin(b.numSamples-at,(int)monoRight.size());bool wasPlaying=engine->playing.load();
        for(int channel=0;channel<2;++channel){if(activeInputs.load()>channel&&b.buffer->getNumChannels()>channel)captureInput.copyFrom(channel,0,b.buffer->getReadPointer(channel,b.startSample+at),count);else captureInput.clear(channel,0,count);}
        inputPeaks.measure(captureInput,activeInputs.load(),count);auto signal=selectInput(captureInput,activeInputs.load(),(InputMode)inputMode.load());
        bool historyWasActive=audioHistory.active();
        Engine::MonitorInput monitor{signal.left,signal.right,inputMonitor.load()?inputMonitorTrack.load():-1,monitorLevel.load()};
        auto* left=b.buffer->getWritePointer(0,b.startSample+at);bool mono=activeOutputs.load()==1||b.buffer->getNumChannels()==1;auto* right=mono?monoRight.data():b.buffer->getWritePointer(1,b.startSample+at);engine->render(left,right,count,monitor);
        if(historyWasActive)audioHistory.capture(captureInput,activeInputs.load(),engine->historySpan);
        if(captureAllowed&&recorder.isActive()){int offset=captureScheduled.load()?engine->captureOffset:0,frames=captureScheduled.load()?(engine->captureBlockValid()?engine->captureCount:0):(wasPlaying?count:0);if(frames>0&&signal.channels>0)recorder.capture(signal.left+offset,signal.channels>1?signal.right+offset:nullptr,frames);}
        if(mono)for(int i=0;i<count;++i)left[i]=(left[i]+right[i])*.5f;at+=count;
    }
    captureCallbacks.fetch_sub(1);
}
void Studio::resized() { positionCommandPalette();positionEditHistory();if(layoutReady){for(int p=0;p<paneCount;++p)panes[(size_t)p].bounds=boundedPane(panes[(size_t)p].bounds,p);}repaint(); }
std::vector<Note>& Studio::lane(){return project.patterns[(size_t)project.activePattern].lanes[(size_t)selectedTrack];}
Note* Studio::note(){return selectedNote>=0&&selectedNote<(int)lane().size()?&lane()[(size_t)selectedNote]:nullptr;}
void Studio::checkpoint(){undoStack.push_back(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()));if(undoStack.size()>100)undoStack.pop_front();redoStack.clear();++historyGeneration;}
void Studio::changed(bool compile){bool restore=noteToolBound&&noteToolAudition;if(noteToolBound){noteToolBound=false;noteToolMessage="Project edited; the previous note preview was cancelled.";}dirty=true;++revision;if(compile||restore)publish();else{++assetGeneration;assetNeedsRefresh=true;}repaint();}
void Studio::publish(){auto inventory=projectAssetInventory(project,currentFile==File()?root:currentFile.getParentDirectory());if(inventory!=assetFingerprintInventory){assetFingerprintInventory=inventory;assetLearnPending=true;}ensurePatternEditorIds(project);ensurePlaylistEditorIds(project);reconcilePlaylistSelection();if(noteToolBound&&!noteToolCurrent())noteToolBound=false;std::unique_ptr<Project> auditionProject;if(noteToolCurrent()&&noteToolValid&&noteToolAudition){auditionProject=std::make_unique<Project>(project);auditionProject->patterns[(size_t)noteToolPattern].lanes[(size_t)noteToolTrack]=noteToolCandidate;}++assetGeneration;assetNeedsRefresh=true;String warnings;auto& next=engine->exchange.beginWrite();auto result=compilePlayback(auditionProject?*auditionProject:project,assets,currentFile==File()?root:currentFile.getParentDirectory(),next,warnings);if(result.failed()&&auditionProject){noteToolValid=false;noteToolCandidate=noteToolSource;noteToolMessage="Preview could not compile: "+result.getErrorMessage();status=noteToolMessage;warnings.clear();result=compilePlayback(project,assets,currentFile==File()?root:currentFile.getParentDirectory(),next,warnings);}if(result.wasOk()){next.revisionTag=revision;firstSoundBeat=-1;auto audible=[&](int track){const auto& t=next.tracks[(size_t)track];return !t.muted&&!t.inputMuted&&t.volume>0&&(t.instrument!=Instrument::sample||t.sample);};for(int i=0;i<next.eventCount;++i){const auto& event=next.events[(size_t)i];double at=event.beat;if(event.conditionPass>=0){if(!noteConditionMatches(event.conditionEvery,event.conditionStep,(uint64_t)event.conditionPass))continue;}else if(event.conditionStep>1){if(!next.loop)continue;at+=(event.conditionStep-1)*next.length;}if(audible(event.track)&&event.velocity>0&&event.probability>0&&(firstSoundBeat<0||at<firstSoundBeat))firstSoundBeat=at;}for(int i=0;i<next.audioCount;++i){const auto& region=next.audio[(size_t)i];const auto& t=next.tracks[(size_t)region.track];if(!t.muted&&!t.inputMuted&&t.volume>0&&region.gain>0&&(firstSoundBeat<0||region.beat<firstSoundBeat))firstSoundBeat=region.beat;}
    if(recorder.isActive())next.length=jmax(next.length,takeBeat+recorder.limitSeconds()*takeTempo/60+1);clipSamples.fill(nullptr);if(project.songMode){for(size_t i=0;i<project.clips.size();++i)if(project.clips[i].isAudio()){String error;const auto& path=project.clips[i].audioPath;clipSamples[i]=assets.load(File::isAbsolutePath(path)?File(path):(currentFile==File()?root:currentFile.getParentDirectory()).getChildFile(path),error);}}for(int i=0;i<maxTracks;++i)previewSamples[(size_t)i]=i<next.trackCount?next.tracks[(size_t)i].sample:nullptr;engine->exchange.publish();}else{engine->stop();status=result.getErrorMessage();}if(warnings.isNotEmpty())status=warnings.trim();}
void Studio::applyHistory(bool redo){if((redo?redoStack:undoStack).empty())return;if(restoreHistoryState((int)undoStack.size()+(redo?1:-1),revision,historyGeneration))status=redo?"Edit restored.":"Edit undone.";}
void Studio::timerCallback(){
    controlMailbox.poll([this](const String& command,const var& args,var& result){return handleControlCommand(command,args,result);});
    inputMonitorTrack.store(selectedTrack);
    updateAnalyzer();
    for(int ch=0;ch<2;++ch){float peak=inputPeaks.consume(ch);inputLevels[(size_t)ch]=jmax(peak,inputLevels[(size_t)ch]*.86f);if(peak>=.999f)inputClipTicks[(size_t)ch]=60;else if(inputClipTicks[(size_t)ch]>0)--inputClipTicks[(size_t)ch];}
    finishRecoveryCleanup();finishFolderScan();finishAssetAudit();finishTemplateTask();finishWaveResample();finishProjectArchive();finishVersionTask();finishSamplerPitch();finishMixAnalysis();finishRestoration();finishReference();finishTranscription();finishAudioHistory();
    if(audioEnabled&&templatesNeedRefresh&&!templateBusy&&panes[templatesPane].visible)refreshTemplates();
    if(audioEnabled&&assetNeedsRefresh&&!assetBusy&&(panes[assetsPane].visible||assetRevealMissing||assetLearnPending)&&Time::currentTimeMillis()-assetLastCheck>700)startAssetAudit();
    if(transportTicks>0){runTransportTick();if(transportTicks==0)return;}
    if(audioEnabled){auto request=root.getChildFile("data/diagnose-v0.29.request");if(request.existsAsFile()){request.deleteFile();writeRuntimeReport(root.getChildFile("data/runtime.json"));saveProject(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),root.getChildFile("data/diagnostic-session.pulse"));snapshot(root.getChildFile("data/diagnostic-ui.png"));}}
    if(audioEnabled&&engine->playing.load()){auto frames=engine->processedFrames.load();stalledPlaybackTicks=frames==lastPlaybackFrames?stalledPlaybackTicks+1:0;lastPlaybackFrames=frames;if(stalledPlaybackTicks>=30){engine->stop();status="Playback stopped: no audio callbacks. Open F10 and select an available output device.";}}
    else stalledPlaybackTicks=0;
    // App-owned update handoff: finish a take, commit recovery, then close.
    // A timestamp prevents an old request from closing a newly launched app.
    if(audioEnabled){auto request=root.getChildFile("data/close-for-update-v0.29.request");
        if(request.existsAsFile()){
            auto requestedAt=request.loadFileAsString().trim().getLargeIntValue();request.deleteFile();
            if(requestedAt>=openedAt&&requestedAt<=Time::currentTimeMillis()){
                if(saveRecoveryForClose()){JUCEApplication::getInstance()->quit();return;}
                status="Update postponed: "+status;
            }
        }
    }
    if(recording&&(engine->takeFinished.load()||!engine->playing.load()||(recorder.isActive()&&(recorder.needsStop()||recordDeviceStopped.exchange(false)))))finishAudioRecording();
    if(exportDone.exchange(false)){if(exportThread.joinable())exportThread.join();exporting.store(false);status=exportMessage;}
    tickAutosave();
    if(++runtimeCounter>=30){runtimeCounter=0;typingCapture.expire(Time::getMillisecondCounterHiRes());writeRuntimeReport(root.getChildFile("data/runtime.json"));}
    if(audioSmokeFrames>0&&--audioSmokeFrames==0){engine->stop();writeRuntimeReport(root.getChildFile("data/audio-smoke.json"));JUCEApplication::getInstance()->quit();}
    repaint();
}
bool Studio::snapshot(const File& target){auto image=createComponentSnapshot(getLocalBounds(),true,1.f);TemporaryFile temporary(target);auto stream=temporary.getFile().createOutputStream();if(!stream||!PNGImageFormat().writeImageToStream(image,*stream))return false;stream.reset();return temporary.overwriteTargetFileWithTemporary();}

void Studio::perform(int action,int data){
    if(action==browserPreviewStop){engine->cancelPreview();status="Browser audition stopped.";repaint();return;}
    if(action==audioTakeLimit){setAudioTakeLimit(data);return;}
    if(action==samplerToPlaylist){samplerAudioToPlaylist();return;}
    if(action==audioHistoryOpen){showAudioHistory();return;}
    if(action==audioHistoryOff){stopAudioHistory();return;}
    if(action==notationOpen){showNotation();return;}
    if(action==spectralOpen){if(restorationTask){status="Finish or cancel the current audio edit first.";return;}if(restorationOptions.kind!=RestorationKind::spectral){discardRestoration();restorationOptions.kind=RestorationKind::spectral;}showRestorationSettings();return;}
    if(action==restorationOpen){showRestorationSettings();return;}
    if(action==restorationCancel){discardRestoration();return;}
    if(action==transcriptionOpen){showTranscriptionSettings();return;}
    if(action==transcriptionCancel){discardTranscription();return;}
    if(action==referenceOpen){showReferenceComparison();return;}
    if(action==referenceCancel){discardReference();return;}
    if(action==mixAnalysisOpen){showMixAnalysis();return;}
    if(action==mixAnalysisCancel){if(mixTask){mixTask->cancel.store(true);mixShowResult=false;}return;}
    if(action==projectCommandsOpen){showProjectCommands();return;}
    if(action==microTuningOpen){showMicroTuning();return;}
    if(action==dynamicsModeMenu){showDynamicsModes();return;}
    if(action==noteConditionOpen){showNoteConditions();return;}
    if(action==editHistoryOpen){showEditHistory();return;}
    if(action==audioFadeCurvesOpen){editAudioFadeCurves();return;}
    if(action==recoveryRetentionOpen){showRecoveryRetention();return;}
    if(action==recoveryCleanupReview){startRecoveryCleanup(false);return;}
    if(action==recoveryCleanupCancel){cancelRecoveryCleanup();return;}
    if(action==recover){restoreRecoveryFile(root.getChildFile("data/recovery-v0.29.pulse"),true);return;}
    if(action==openPreviousSave){if(currentFile==File()){status="Open a saved project first, or use File > Open to choose a .pulse.bak copy.";return;}openProject(projectBackupFile(currentFile));return;}
    if(action==harmonyPresetsOpen){if(recording||recorder.isActive()){status="Finish the take before using harmony presets.";return;}if(!harmonyToolActive()||!noteToolCurrent())openNoteTools(harmonyToolActive()?noteToolOptions.mode:NoteToolMode::buildChord);if(noteToolCurrent())showHarmonyPresets();return;}
    if(action==playlistAudioTools){showPlaylistAudioTools();return;}
    if(action==audioCrossfadeOpen){showAudioCrossfades();return;}
    if(action==audioFadesClear){auto selection=playlistSelection();setPlaylistSelection(selection,selectedClip);applyAudioCrossfades(selectedPlaylistIds,revision,true);return;}
    if(action==playlistLoopSelection){PlaylistGroupStats range;auto valid=inspectPlaylistGroup(project,playlistSelection(),range);if(valid.failed())status=valid.getErrorMessage();else applyPlaylistLoop(jmin(4095.75,range.start),jmin(4096.,jmax(range.start+.25,range.end)),revision);return;}
    if(action==playlistLoopClear){Project next;auto valid=clearPlaylistLoopRange(project,next);if(valid.failed())status=valid.getErrorMessage();else if(project.loopSection.isNotEmpty()||project.loop||findSongMarker(project,playlistLoopMarkerId)) {auto indices=playlistSelection();int track=selectedTrack,note=selectedNote,point=selectedPoint,low=pianoLow;auto notes=selectedNotes;if(commitPlaylistGroup(std::move(next),std::vector<int>(indices.begin(),indices.end()),selectedClip,"Playlist loop range cleared; looping is off.")){selectedTrack=track;selectedNote=note;selectedPoint=point;pianoLow=low;selectedNotes=std::move(notes);repaint();}}return;}
    if(action==playlistGroupOpen){showPlaylistGroupDialog();return;}
    if(action==playlistAll||action==playlistNone){std::set<int> indices;if(action==playlistAll)for(int i=0;i<(int)project.clips.size();++i)indices.insert(i);setPlaylistSelection(indices);status=String((int)indices.size())+" Playlist clips selected.";return;}

    if(performSamplerTuningAction(action)){repaint();return;}
    if(action==shortcutsOpen){showShortcutSettings();return;}
    if(performAppearanceAction(action,data)){repaint();return;}
    if(performAutomationToolAction(action)){repaint();return;}
    if(performTypingAction(action,data)){repaint();return;}
    if(action==controlToggle){auto result=setLocalControlEnabled(!controlMailbox.enabled());if(result.failed())status=result.getErrorMessage();return;}
    if(performVersionAction(action)){repaint();return;}
    if(performWorkspaceAction(action,data)){repaint();return;}
    if(performAnalyzerAction(action,data)){repaint();return;}
    if(performStereoAction(action,data)){repaint();return;}
    if(performEffectChainAction(action,data)){repaint();return;}
    if(performSoundLibraryAction(action,data)){repaint();return;}
    if(performSynthAction(action,data)){repaint();return;}
    if(action==commandSearch){showCommandPalette();return;}
    if(performPatternToolAction(action,data)){repaint();return;}
    if(performHarmonyAction(action,data)){repaint();return;}
    if(performNoteToolAction(action,data)){repaint();return;}
    if(performMarkerAction(action,data)){repaint();return;}
    if(performExportAction(action,data)){repaint();return;}
    if(performRecordingAction(action,data)){repaint();return;}
    if(performTemplateAction(action,data)){repaint();return;}
    if(performAssetAction(action,data)){repaint();return;}
    if(performWaveAction(action,data)){repaint();return;}
    if(performSampleAction(action,data)){repaint();return;}
    if(recording&&action!=record&&action!=stop&&action!=play&&action!=help){status="Finish the current take with REC or Stop before editing.";repaint();return;}
    if(action==saveVersion&&pendingNoteTool()){status="Apply or cancel the note preview before saving a version.";showPane(noteToolsPane);return;}
    switch(action){
    case play:if(recording)finishAudioRecording();else if(engine->playing.load()){engine->playing.store(false);status="Playback paused.";}else if(audioReady()){engine->start();lastPlaybackFrames=engine->processedFrames.load();stalledPlaybackTicks=0;status=project.songMode?"Playing Song.":"Playing pattern: "+project.patterns[(size_t)project.activePattern].name;if(firstSoundBeat>engine->position.load()+.25)status="Playing; first unmuted part starts in "+String((firstSoundBeat-engine->position.load())*60/project.bpm,1)+" seconds.";}break;
    case stop:engine->preview(nullptr);finishAudioRecording();engine->stop();releaseKeys();recording=false;break;
    case mode:checkpoint();project.songMode=!project.songMode;engine->resetRequested.store(true);changed();break;
    case loop:checkpoint();project.loop=!project.loop;changed();break;
    case metro:checkpoint();project.metronome=!project.metronome;changed();break;
    case record:if(project.tracks[(size_t)selectedTrack].isBus){status="Record on a source track. Bus resampling is not implemented yet.";break;}if(recording)finishAudioRecording();else{cancelNoteTool();if(audioRecordMode)toggleAudioRecording();else startKeyboardRecording();}break;
    case masterEffectsOpen:masterInsertView=false;analyzerView=false;updateAnalyzer();masterEffectsView=true;showPane(effectsPane);break;
    case masterInsertsOpen:masterEffectsView=false;masterInsertView=true;analyzerView=false;updateAnalyzer();selectedEffect=jlimit(0,5,data);showPane(effectsPane);break;
    case masterStereoOpen:masterEffectsView=false;masterInsertView=true;analyzerView=false;updateAnalyzer();selectedEffect=6;showPane(effectsPane);break;
    case masterSoftClipToggle:checkpoint();project.masterSoftClip=!project.masterSoftClip;changed();break;
    case masterDry:checkpoint();project.masterSoftClip=false;project.delayReturn=0;{auto& m=project.masterMixer;m.eq=m.compressor=m.reverb=false;m.chorus.enabled=m.flanger.enabled=m.phaser.enabled=m.stereo.enabled=false;}changed();status="Master is dry: inserts, stereo utility and saturation bypassed; shared delay return muted. Channel and bus effects are unchanged.";break;
    case effectsOpen:masterInsertView=false;masterEffectsView=false;if(analyzerView){analyzerView=false;saveAnalyzerSettings();updateAnalyzer();}selectedEffect=jlimit(0,5,data);showPane(effectsPane);break;
    case effectsTab:masterEffectsView=false;if(analyzerView){analyzerView=false;saveAnalyzerSettings();updateAnalyzer();}selectedEffect=jlimit(0,5,data);break;
    case effectToggle:{if(data<0||data>5)break;checkpoint();auto& fx=effectMixer(effectTarget());auto& enabled=data==0?fx.eq:data==1?fx.compressor:data==2?fx.reverb:data==3?fx.chorus.enabled:data==4?fx.flanger.enabled:fx.phaser.enabled;enabled=!enabled;changed();break;}
    case routeDestination:if(editingMasterEffects()){status="Master output routing is fixed. Select a source or bus first.";break;}routeTrack(selectedTrack,data);break;
    case outputRoute:chooseRouting(false);break;case sidechainRoute:chooseRouting(true);break;case addBus:modifyChannel(addBus,0);break;
    case automationCreate:createAutomation(volumeDial,selectedTrack);break;
    case automationOpen:showPane(automationPane,true);break;
    case automationHold:if(auto* c=automationClip())if(selectedPoint>=0&&selectedPoint<(int)c->points.size()){checkpoint();c->points[(size_t)selectedPoint].hold=!c->points[(size_t)selectedPoint].hold;changed();}break;
    case automationReset:if(auto* c=automationClip()){checkpoint();float value=c->points.front().value;c->points={{0,value,false},{c->length,value,false}};selectedPoint=-1;changed();}break;
    case composeTab:page=0;showPane(rackPane,true);break;case arrangeTab:page=1;showPane(playlistPane,true);break;case mixTab:page=2;showPane(mixerPane,true);break;
    case pianoOpen:if(data>=0&&data<(int)project.tracks.size()&&project.tracks[(size_t)data].isBus){status="A mixer bus processes routed audio. Select a source channel for Piano roll.";break;}if(data>=0&&data<(int)project.tracks.size()&&data!=selectedTrack){releaseKeys();selectedTrack=data;selectedNote=-1;selectedNotes.clear();pianoLow=-1;}showPane(pianoPane);break;
    case channelOpen:if(data>=0&&data<(int)project.tracks.size()&&data!=selectedTrack){releaseKeys();selectedTrack=data;selectedNote=-1;selectedNotes.clear();pianoLow=-1;}showPane(project.tracks[(size_t)selectedTrack].isBus?effectsPane:isSynth(project.tracks[(size_t)selectedTrack].instrument)?synthPane:channelPane);break;
    case menuOpen:showMenu(data);break;case patternMenu:showPatternMenu();break;case patternRename:renamePattern();break;case patternSteps:setPatternSteps();break;
    case patternPrevious:perform(patternSelect,jmax(0,project.activePattern-1));break;
    case patternNext:perform(patternSelect,jmin((int)project.patterns.size()-1,project.activePattern+1));break;
    case typingToggle:releaseKeys();typingEnabled=!typingEnabled;saveTypingPreferences();status=typingEnabled?"Typing keyboard to piano enabled.":"Typing keyboard off. P/B/D/E editor tools and L/R transport shortcuts enabled.";break;
    case browserToggle:browserVisible=!browserVisible;for(int p=0;p<paneCount;++p)panes[(size_t)p].bounds=boundedPane(panes[(size_t)p].bounds,p);saveLayout();break;
    case browserFolder:if(data>=0&&data<(int)browserExpanded.size())browserExpanded[(size_t)data]=!browserExpanded[(size_t)data];break;
    case projectArchiveExport:chooseProjectArchive(false);break;
    case projectArchiveImport:chooseProjectArchive(true);break;
    case projectArchiveCancel:if(archiveBusy){archiveCancel.store(true);status="Cancelling project archive...";}else status="No project archive is running.";break;
    case browserFilterOpen:showBrowserFilter();break;
    case browserFilterClear:setBrowserFilter({});break;
    case browserAsset:if(data>=0&&data<(int)browserAssets.size())applyBrowserAsset(browserAssets[(size_t)data],selectedTrack);break;
    case toolSelect:editorTool=jlimit(0,3,data);updateEditorPointer(lastEditorPointer,lastEditorModifiers);status=String(editorTool==3?"Select":editorTool==2?"Delete":editorTool==1?"Paint":"Draw")+" tool selected.";break;
    case snapMenu:{PopupMenu m;const char* names[]={"None","Bar","Beat","1/2 beat","1/4 beat","1/6 beat","1/8 beat"};const double values[]={0,4,1,.5,.25,1./6,.125};for(int i=0;i<7;++i)m.addItem(i+1,names[i],true,std::abs(snap-values[i])<.0001);auto safe=Component::SafePointer<Studio>(this);m.showMenuAsync(menuOptions(),[safe](int r){if(safe&&r>0&&r<=7){const double v[]={0,4,1,.5,.25,1./6,.125};safe->snap=v[r-1];safe->repaint();}});break;}
    case paneClose:if(data==noteToolsPane)cancelNoteTool();panes[(size_t)data].visible=false;releaseKeys();for(auto i=paneOrder.rbegin();i!=paneOrder.rend();++i)if(panes[(size_t)*i].visible){activePane=*i;break;}saveLayout();break;
    case paneMaximize:{auto& p=panes[(size_t)data];if(!p.maximized){p.restoreBounds=p.bounds;p.maximized=true;}else{p.maximized=false;p.bounds=boundedPane(p.restoreBounds,data);}showPane(data);break;}
    case layoutReset:if(fitMainWindow)fitMainWindow();selectedWorkspace.clear();resetLayout();saveLayout();break;
    case addChannel:case cloneChannel:case deleteChannel:modifyChannel(action,data);break;
    case notePasteCursor:pasteCopiedNotes(NotePasteMode::atCursor);break;
    case notePasteOriginal:pasteCopiedNotes(NotePasteMode::originalPositions);break;
    case notePasteStart:pasteCopiedNotes(NotePasteMode::atStart);break;
    case noteCopyToPattern:showCopyNotesToPattern();break;
    case copySelection:case cutSelection:case pasteSelection:case duplicateSelection:editClipboard(action);break;
    case undoAction:if(activePane==wavePane)applyWaveHistory(false);else applyHistory(false);break;case redoAction:if(activePane==wavePane)applyWaveHistory(true);else applyHistory(true);break;
    case newProject:perform(templatesOpen);break;
    case open:chooseFile(open);break;case saveAction:save();break;case exportAudio:showExport(false);break;
    case importMidiAction:chooseFile(importMidiAction);break;case exportMidiAction:chooseFile(exportMidiAction);break;
    case importSample:chooseFile(importSample);break;
    case importAudioClip:chooseFile(importAudioClip);break;
    case recordMode:if(recording){status="Stop recording before changing its mode.";break;}audioRecordMode=!audioRecordMode;status=audioRecordMode?"REC captures the input configured in F10 into the Playlist.":"REC captures typing-keyboard notes into the active pattern.";break;
    case audioClipSettings:editAudioClip();break;case splitClip:splitSelectedClip();break;
    case exportStems:showExport(true);break;
    case settings:{DialogWindow::LaunchOptions options;options.dialogTitle=L"Audio output — Pulse Studio";options.dialogBackgroundColour=surface;
        options.content.setOwned(new AudioDeviceSelectorComponent(deviceManager,0,2,2,2,false,false,true,false));options.content->setSize(530,420);
        options.componentToCentreAround=this;options.useNativeTitleBar=true;options.resizable=false;options.launchAsync();break;}
    case help:showHelp();break;
    case patternSelect:if(data>=0&&data<(int)project.patterns.size()){bool switched=data!=project.activePattern;releaseKeys();project.activePattern=data;pianoNoteCursor=0;selectedNote=-1;selectedNotes.clear();pianoStart=pianoNoteCursor=0;pianoSpan=jmin(8.,project.patterns[(size_t)data].length);pianoLow=-1;changed();if(switched&&!project.songMode)engine->seekRequested.store(0);}break;
    case patternAdd:if(project.patterns.size()<maxPatterns){checkpoint();Pattern p;p.name="Pattern "+String((int)project.patterns.size()+1);p.lanes.resize(project.tracks.size());project.patterns.push_back(p);project.activePattern=(int)project.patterns.size()-1;pianoNoteCursor=0;selectedNote=-1;selectedNotes.clear();pianoStart=pianoNoteCursor=0;pianoSpan=8;pianoLow=-1;changed();}else status="This build supports up to 32 patterns.";break;
    case patternDuplicate:if(project.patterns.size()<maxPatterns){checkpoint();auto p=project.patterns[(size_t)project.activePattern];p.name+=" copy";p.editorId=Uuid().toString();project.patterns.push_back(std::move(p));project.activePattern=(int)project.patterns.size()-1;pianoStart=pianoNoteCursor=0;pianoSpan=jmin(8.,project.patterns.back().length);selectedNote=-1;selectedNotes.clear();changed();if(!project.songMode)engine->seekRequested.store(0);}break;
    case patternClear:checkpoint();lane().clear();selectedNote=-1;selectedNotes.clear();changed();break;
    case trackSelect:masterEffectsView=masterInsertView=false;releaseKeys();selectedTrack=data;selectedNote=-1;selectedNotes.clear();pianoLow=-1;break;
    case mute:checkpoint();project.tracks[(size_t)data].mute=!project.tracks[(size_t)data].mute;changed();break;
    case solo:checkpoint();project.tracks[(size_t)data].solo=!project.tracks[(size_t)data].solo;changed();break;
    case instrumentSelect:if(data==8){chooseFile(importSample);break;}if(project.tracks[(size_t)selectedTrack].isBus){status="Select an instrument channel to replace its sound.";break;}checkpoint();project.tracks[(size_t)selectedTrack].instrument=(Instrument)data;project.tracks[(size_t)selectedTrack].soundName.clear();changed();if(isSynth((Instrument)data))showPane(synthPane);status="Channel instrument changed. Existing notes and pattern length are preserved.";if(data==8)chooseFile(importSample);break;
    case arrangeFill:{if(project.clips.size()+project.tracks.size()>maxClips){status="Arrangement clip limit reached.";break;}checkpoint();double start=project.clips.empty()?0:project.songLength();for(int t=0;t<(int)project.tracks.size();++t)if(!project.patterns[(size_t)project.activePattern].lanes[(size_t)t].empty())project.clips.push_back({t,project.activePattern,start,project.patterns[(size_t)project.activePattern].length});changed();break;}
    case generate:{auto i=project.tracks[(size_t)selectedTrack].instrument;if(isDrum(i)){openNoteTools(NoteToolMode::euclidean);break;}checkpoint();lane().clear();
        if(i==Instrument::pad){const int roots[]={57,53,60,55};for(int c=0;c<4;++c)for(int offset:{0,3,7})lane().push_back({c*2.,1.75,roots[c]+offset,.7f});}
        else{const int scale[]={0,2,3,5,7,8,10};Random random((int64)revision+42);for(int n=0;n<16;++n)lane().push_back({n*.5,.35,(i==Instrument::bass?45:69)+scale[random.nextInt(7)],.6f+random.nextFloat()*.25f});}
        selectedNote=-1;selectedNotes.clear();pianoLow=-1;changed();status="Generated an editable local musical variation. Undo restores the previous notes.";break;}
    case transposeUp:case transposeDown:checkpoint();if(!selectedNotes.empty()){for(int i:selectedNotes)if(i<(int)lane().size())lane()[(size_t)i].pitch=jlimit(0,127,lane()[(size_t)i].pitch+(action==transposeUp?12:-12));}else if(auto* n=note())n->pitch=jlimit(0,127,n->pitch+(action==transposeUp?12:-12));else for(auto& n:lane())n.pitch=jlimit(0,127,n.pitch+(action==transposeUp?12:-12));changed();break;
    case scaleToggle:{auto next=activeHarmonyGuide();next.enabled=!next.enabled;setHarmonyGuide(next,harmonyGuideRevision,harmonyGuideToken(root.getChildFile("data/harmony.json")));break;}
    case rename:editName(data==1);break;case notesAction:editName(false,true);break;

    default:break;
    } repaint();grabKeyboardFocus();
}
void Studio::chooseFile(int action){
    if(pendingNoteTool()&&(action==saveAction||action==exportAudio||action==exportStems||action==exportMidiAction)){status="Apply or cancel the note preview before saving or exporting.";showPane(noteToolsPane);return;}
    if(exporting.load()&&(action==exportAudio||action==exportStems)){status="A render is already running. Press Escape to cancel.";return;}
    String title,filter;int flags=FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles;
    File start=root.getChildFile("data");
    if(action==open){title="Open local project";filter="*.pulse;*.bak;*.json;*.zip";}
    else if(action==saveAction){title="Save local project";filter="*.pulse";flags=FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting;start=start.getChildFile(project.title+".pulse");}
    else if(action==folderChoose){title="Choose local sample folder";filter="*";flags=FileBrowserComponent::openMode|FileBrowserComponent::canSelectDirectories;}
    else if(action==waveformOpen){title="Open audio in waveform editor";filter="*.wav;*.aif;*.aiff;*.flac;*.ogg";}
    else if(action==importAudioClip){title="Import audio into Playlist";filter="*.wav;*.aif;*.aiff;*.flac;*.ogg";}
    else if(action==importSample){title="Load audio into selected sampler";filter="*.wav;*.aif;*.aiff;*.flac;*.ogg";}
    else if(action==importMidiAction){title="Import a MIDI composition";filter="*.mid;*.midi";}
    else if(action==exportMidiAction){title="Export standard MIDI file";filter="*.mid";flags=FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting;start=start.getChildFile(project.title+".mid");}
    else if(action==exportStems){title="Choose folder for isolated track exports";filter="*";flags=FileBrowserComponent::openMode|FileBrowserComponent::canSelectDirectories;}
    else{bool flac=renderSettings.format==RenderFormat::flac;title=flac?"Export FLAC":"Export WAV";filter=flac?"*.flac":"*.wav";flags=FileBrowserComponent::saveMode|FileBrowserComponent::canSelectFiles|FileBrowserComponent::warnAboutOverwriting;start=start.getChildFile(project.title+(flac?".flac":".wav"));}
    chooser=std::make_unique<FileChooser>(title,start,filter,true);auto safe=Component::SafePointer<Studio>(this);int track=selectedTrack;
    chooser->launchAsync(flags,[safe,action,track,options=renderSettings](const FileChooser& fc){if(!safe)return;auto file=fc.getResult();if(file==File())return;
        if(action==folderChoose){safe->browserLocal=true;safe->browserVisible=true;safe->scanFolder(file);}
        else if(action==waveformOpen)safe->openWaveform(file);
        else if(action==open)safe->openProject(file);
        else if(action==saveAction)safe->commitSave(file.withFileExtension("pulse"));
        else if(action==importSample)safe->loadSample(file,track);
        else if(action==importAudioClip)safe->addAudioClip(file,track,safe->editCursorBeat);
        else if(action==importMidiAction){Project p;auto result=importMidi(file,p);if(result.wasOk()){safe->synchronizeAutosave();safe->checkpoint();safe->engine->stop();safe->project=std::move(p);safe->currentFile=File();safe->selectedTrack=0;safe->selectedNote=-1;safe->changed();safe->status="Imported PPQ MIDI notes. Controllers and tempo maps are not imported by this build.";}else safe->status=result.getErrorMessage();}
        else if(action==exportMidiAction){auto result=writeMidi(safe->project,file.withFileExtension("mid"));safe->status=result.wasOk()?(std::any_of(safe->project.tracks.begin(),safe->project.tracks.end(),[](const Track& t){return t.tuning.enabled;})?"MIDI exported with ordinary note numbers; microtonal tuning is not encoded. Render audio to preserve pitch.":"MIDI exported locally."):result.getErrorMessage();}
        else safe->startExport(action==exportStems?file:file.withFileExtension(options.format==RenderFormat::flac?"flac":"wav"),options,action==exportStems);
        safe->repaint();
    });
}
void Studio::save(){
    if(pendingNoteTool()){status="Apply or cancel the note preview before saving.";showPane(noteToolsPane);return;}
    if(currentFile==File()||(currentFile.getFileName()=="recovery.pulse"||currentFile.getFileName()=="recovery-v0.3.pulse"||currentFile.getFileName()=="recovery-v0.4.pulse"||currentFile.getFileName()=="recovery-v0.5.pulse"||currentFile.getFileName()=="recovery-v0.6.pulse"||currentFile.getFileName()=="recovery-v0.7.pulse"||currentFile.getFileName()=="recovery-v0.8.pulse"||currentFile.getFileName()=="recovery-v0.9.pulse"||currentFile.getFileName()=="recovery-v0.10.pulse"||currentFile.getFileName()=="recovery-v0.11.pulse"||currentFile.getFileName()=="recovery-v0.12.pulse"||currentFile.getFileName()=="recovery-v0.13.pulse"||currentFile.getFileName()=="recovery-v0.14.pulse"||currentFile.getFileName()=="recovery-v0.15.pulse"||currentFile.getFileName()=="recovery-v0.16.pulse"||currentFile.getFileName()=="recovery-v0.17.pulse"||currentFile.getFileName()=="recovery-v0.18.pulse"||currentFile.getFileName()=="recovery-v0.19.pulse"||currentFile.getFileName()=="recovery-v0.20.pulse"||currentFile.getFileName()=="recovery-v0.21.pulse"||currentFile.getFileName()=="recovery-v0.22.pulse"||currentFile.getFileName()=="recovery-v0.29.pulse")){chooseFile(saveAction);return;}
    commitSave(currentFile);
}
void Studio::rememberProject(const File& file){
    if(!file.existsAsFile()||!file.hasFileExtension("pulse")||(file.getParentDirectory()==root.getChildFile("data")&&file.getFileName().startsWith("recovery")))return;
    auto* object=new DynamicObject();object->setProperty("schema",1);object->setProperty("path",file.getFullPathName());TemporaryFile temporary(root.getChildFile("data/last-project.json"));if(temporary.getFile().replaceWithText(JSON::toString(var(object),false)))temporary.overwriteTargetFileWithTemporary();
}
bool Studio::restoreLastProject(){
    auto preferences=root.getChildFile("data/last-project.json");if(!preferences.existsAsFile()||preferences.getSize()>16384)return false;auto value=JSON::parse(preferences.loadFileAsString());auto path=value["path"].toString();if(!value.isObject()||(int)value["schema"]!=1||!File::isAbsolutePath(path)||!File(path).hasFileExtension("pulse"))return false;
    Project restored;auto file=File(path);auto result=loadProject(file,restored);if(result.failed()){status="Last saved project could not be opened. Use File > Open to locate it.";return false;}
    synchronizeAutosave();cancelNoteTool(false);project=std::move(restored);currentFile=file;selectedTrack=jmin(5,(int)project.tracks.size()-1);selectedNote=-1;selectedNotes.clear();selectedClip=-1;trackScroll=0;pianoLow=-1;dirty=false;recoveryRestored=false;publish();status="Opened last saved project: "+file.getFileName();return true;
}
void Studio::commitSave(const File& target){if(retentionBusy&&target.getParentDirectory()==root.getChildFile("data/History")){status="Wait for recovery cleanup before saving into History.";return;}if(pendingNoteTool()){status="Apply or cancel the note preview before saving.";showPane(noteToolsPane);return;}synchronizeAutosave();Project collected;auto result=collectAssets(project,currentFile==File()?root:currentFile.getParentDirectory(),target,collected);if(result.wasOk())result=saveProject(collected,target);if(result.wasOk()){project=std::move(collected);currentFile=target;dirty=false;rememberProject(target);status="Saved with collected local assets: "+target.getFileName();if(target!=root.getChildFile("data/recovery-v0.29.pulse")){retireRecovery();if(audioEnabled&&retentionPolicy.automatic&&!root.getChildFile("data/recovery-v0.29.pulse").exists())startRecoveryCleanup(true);}publish();}else{status=result.getErrorMessage();assetRevealMissing=true;assetNeedsRefresh=true;}repaint();}
void Studio::openProject(const File& file){
    if(retentionBusy&&file.getParentDirectory()==root.getChildFile("data/History")){status="Wait for recovery cleanup before opening a History copy.";return;}
    if(file.hasFileExtension("zip")){startProjectArchive(file,true,revision);return;}
    if(recording||recorder.isActive()){status="Stop the take before opening another project.";return;}
    synchronizeAutosave();Project p;auto result=loadProject(file,p);if(result.failed()){status=result.getErrorMessage();repaint();return;}
    if(dirty){auto backup=root.getChildFile("data/History").getChildFile("Before_open_"+Uuid().toString()+".pulse");result=saveProject(absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory()),backup);if(result.failed()){status="Current unsaved session could not be backed up; project was not opened: "+result.getErrorMessage();return;}}
    const int previousTrack=selectedTrack;const bool previous=file.getFileName().endsWithIgnoreCase(".pulse.bak");if(previous)p=absoluteAssets(p,file.getParentDirectory());
    cancelNoteTool(false);checkpoint();releaseKeys();engine->preview(nullptr);engine->stop();++revision;project=std::move(p);currentFile=file;rememberProject(file);selectedTrack=jmin(5,(int)project.tracks.size()-1);selectedNote=-1;trackScroll=0;dirty=false;
    selectedNotes.clear();selectedClip=-1;clipboardHasClip=false;pianoStart=pianoNoteCursor=0;pianoSpan=jmin(8.,project.patterns[(size_t)project.activePattern].length);pianoLow=-1;if(previous){currentFile=File();dirty=true;recoveryRestored=true;selectedTrack=jlimit(0,(int)project.tracks.size()-1,previousTrack);}publish();status=previous?"Opened previous saved copy. Save as a project to restore it; original files are retained.":"Opened locally: "+file.getFileName();if(!previous)retireRecovery();assetRevealMissing=audioEnabled;repaint();
}
void Studio::loadSample(const File& file,int track){
    if(project.tracks[(size_t)track].isBus){status="Load audio into a source channel, then route it to the bus.";return;}
    String error;const auto* sample=assets.load(file,error);if(!sample){status=error;return;}
    engine->cancelPreview();checkpoint();project.tracks[(size_t)track].sampleRoot=60;project.tracks[(size_t)track].sampleTune=0;project.tracks[(size_t)track].sampleStart=0;project.tracks[(size_t)track].sampleEnd=0;project.tracks[(size_t)track].sampleLoop=false;project.tracks[(size_t)track].instrument=Instrument::sample;project.tracks[(size_t)track].samplePath=file.getFullPathName();selectedTrack=track;changed();status="Loaded sample: "+file.getFileName()+". Sampler root note is C4.";
}
void Studio::startExport(const File& target,bool stems){startExport(target,renderSettings,stems);}
void Studio::startExport(const File& target,const RenderSettings& options,bool stems){
    if(pendingNoteTool()){status="Apply or cancel the note preview before exporting.";showPane(noteToolsPane);return;}
    if(exporting.load())return;RenderPlan plan;auto validation=planRender(project,options,plan);if(validation.failed()){status=validation.getErrorMessage();return;}if(exportThread.joinable())exportThread.join();
    auto p=absoluteAssets(project,currentFile==File()?root:currentFile.getParentDirectory());File base=root;exporting.store(true);exportDone.store(false);cancelExport.store(false);exportProgress.store(0);
    status=stems?"Rendering isolated tracks from a snapshot; shared processing can prevent summing to the master.":"Rendering a local audio snapshot. You can keep editing; Escape cancels.";
    exportThread=std::thread([this,p,base,target,stems,options]{Assets localAssets;Result result=Result::ok();
        if(stems){for(int t=0;t<(int)p.tracks.size();++t){if(cancelExport.load()){result=Result::fail("Export cancelled. Completed isolated files are kept.");break;}auto file=target.getChildFile(String(t+1).paddedLeft('0',2)+"_"+p.tracks[(size_t)t].name.replaceCharacters("/\\:*?\"<>|","_________")+(options.format==RenderFormat::flac?".flac":".wav"));result=renderAudio(p,localAssets,base,file,options,t,&cancelExport);if(result.failed())break;exportProgress.store((float)(t+1)/p.tracks.size());}}
        else result=renderAudio(p,localAssets,base,target,options,-1,&cancelExport,&exportProgress);
        exportMessage=result.wasOk()?"Export complete: "+target.getFullPathName():result.getErrorMessage();exportDone.store(true,std::memory_order_release);
    });
}
void Studio::editName(bool projectName,bool editNotes){
    auto* dialog=new AlertWindow(editNotes?"Project notes":projectName?"Name session":"Name channel","Stored locally in your project.",MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("value",editNotes?project.notes:projectName?project.title:project.tracks[(size_t)selectedTrack].name,"",false);
    if(editNotes)dialog->getTextEditor("value")->setMultiLine(true);
    dialog->addButton("Apply",1,KeyPress(KeyPress::returnKey));dialog->addButton("Cancel",0,KeyPress(KeyPress::escapeKey));
    auto safe=Component::SafePointer<Studio>(this);int track=selectedTrack;
    dialog->enterModalState(true,ModalCallbackFunction::create([safe,dialog,projectName,editNotes,track](int result){if(safe&&result==1){auto name=dialog->getTextEditorContents("value").trim();if(name.isNotEmpty()||editNotes){safe->checkpoint();if(editNotes)safe->project.notes=name;else if(projectName)safe->project.title=name.substring(0,100);else safe->project.tracks[(size_t)track].name=name.substring(0,50);safe->changed(false);}}}),true);
}
void Studio::showHelp(){
    AlertWindow::showMessageBoxAsync(MessageBoxIconType::InfoIcon,L"Pulse Studio — quick start",
        "Options > Keyboard shortcuts remaps commands locally. Menu hints and command search show current bindings. Escape and Ctrl+Shift+H stay fixed. The keys below are factory defaults.\n\nCtrl+P: find commands, channels, patterns, clips and help.\nAlt+Q: quantize with preview. Alt+R: humanize. Ctrl+Q: quick quantize.\nF5 Playlist / F6 Channel rack / F7 Piano roll / F9 Mixer\nAlt+F8 Browser / F10 Audio settings / F11 Project notes\nDrag window titles to move. Double-click a title to maximize.\nResize at the bottom-right corner. Esc closes the active window.\nCtrl+Shift+H fits the app to the screen and restores the default window arrangement.\n\nSpace: play/stop. Ctrl+Space: play/pause. Home: rewind.\nCtrl+T: typing keyboard on/off. Z through / and Q through ] play both full piano rows; pressed and sustained notes highlight in Piano roll. Right-click KEYS for velocity, octave, transpose and retrospective capture. Alt+[ / Alt+] changes octave; Ctrl+Shift+Up/Down changes velocity; Ctrl+Shift+Left/Right transposes. Ctrl+Shift+K toggles sustain. Recall last typing phrase creates a new PAT pattern from memory with one Undo. REC captures typing-keyboard notes. The NOTES/AUDIO button switches to input recording; choose the input in F10. REC or Stop finishes a 24-bit WAV take into the Playlist. Options > Tempo and recording has input meters, input selection and monitoring. Monitoring follows the selected source channel and starts off; use headphones. WAV takes stay dry.\n\nMixer: click an effect slot to open EQ, compressor, reverb, chorus, flanger or phaser; its green switch enables it. Audio analyzer opens live spectrum, peak/RMS and correlation. Stereo opens width, mono, swap, input polarity and output trim, after the six effects. Right-click a slot to move it; Chain opens factory presets, copy/paste and local chain files. Use + Bus to add a bus. Select a source and click the destination arrow to route its output. Compressor Detector chooses a sidechain source.\n\nAutomation: right-click mixer volume, pan, filter, saturation, delay send, or master volume and choose Create automation clip. Click the curve to add points; drag to edit; right-click deletes interior points. STEP holds the selected point value until the next point. Ctrl+C/X/V/B copies/cuts/pastes/duplicates points. Song mode plays automation.\n\nPlaylist: import audio with Audio... or drop a file. Drag edges to trim. Double-click audio for gain, source offset, and fades; Alt+S splits at cursor.\n\nADD Pulse Analog, Pulse FM, Pulse Wavetable or Pulse Additive creates an editable generator. Sounds offers 64 original factory patches plus your local library. Wavetable adds family selection, position, envelope and LFO; the LFO rate is on Envelopes & LFO. Additive has sixteen harmonic levels, plus Tone & filter and Envelopes & LFO pages. Right-click a knob for a typed value; drag/wheel edits use Undo.\n\nBrowser: expand Pulse Analog / Pulse FM / Pulse Wavetable / Pulse Additive for all factory patches, or Favorite / Personal sounds. Click a sound or Project sample to replace the selected channel sound; drag onto another rack/Mixer channel to assign there. Wheel scrolls the tree.\n\nChannel rack: left-click enables steps, right-click disables. Click a channel name for settings; right-click for Piano roll and channel menu. Add instruments with ADD or +.\n\nHarmony presets: open Fit notes to scale or Chord builder, then Presets. Factory choices and up to 128 personal presets support save/update/rename/delete and .pulseharmony files. Recall previews settings and a temporary key; Apply commits notes once, Cancel restores originals and the normal guide. Chord insertion beat stays with the target. Scale settings > Apply guide explicitly saves a default.\n\nPiano roll: draw/move/resize notes; right-click erases. Ctrl-drag selects, Shift-drag clones. Ctrl+wheel zooms; wheel scrolls pitches; Shift+wheel scrolls time. Alt bypasses snap; Backspace toggles snap. Drag velocity stems to edit. Ctrl+C/X/V copies/cuts/pastes at the editor cursor; Ctrl+B duplicates to the right.\n\nPlaylist: Ctrl+A selects all blocks; Ctrl+D deselects. Ctrl-click toggles blocks; Ctrl-drag empty grid lassos (Shift adds). Drag a selection to move; Shift-drag copies. Group (N) opens a typed destination-bar dialog. Click the Playlist ruler before Ctrl+V to choose a group paste destination. Escape cancels a drag; each accepted group edit is one Undo.\n\nAudio transitions: overlap two audio clips on one Playlist lane, select both, then Edit > Crossfade selected audio clips. Apply fits linear fades to the overlap without moving clips or changing files. Clear selected audio fades removes fades with one Undo. Right-click Audio... for these tools.\n\nLoop a phrase: right-drag or Shift-left-drag across Playlist bar numbers. Space plays the full Song accompaniment in that range while you edit Piano-roll notes. The Loop button uses selected blocks; right-click it to clear the range. LOOP toggles repetition. Automatic patterns have distinct colours by order; PATTERNS > Current pattern colour pins a manual swatch.\n\nCtrl+S saves. Ctrl+O opens. Ctrl+R exports WAV. Ctrl+Shift+M exports MIDI. Ctrl+Z toggles the last undo/redo; Ctrl+Alt+Z undoes step by step.\n\nAll work stays local. FLP import, third-party plugins, and local AI are not implemented yet.","Got it",this);
}
float Studio::parameterValue(int a,int t)const{
    if((a==stereoWidth||a==stereoTrim)&&t>=-1&&t<(int)project.tracks.size()){const auto& s=effectMixer(t).stereo;return a==stereoWidth?s.width/2:(s.trim+24)/30;}
    if(a>=flangerMix&&a<=phaserWidth&&t>=-1&&t<(int)project.tracks.size()){auto kind=a>=phaserMix?ModulationKind::phaser:ModulationKind::flanger;int i=a-(kind==ModulationKind::phaser?phaserMix:flangerMix);const auto& m=effectMixer(t);return(float)modulationNormal(kind,i,modulationValue(kind==ModulationKind::phaser?m.phaser:m.flanger,i));}
    if(a>=chorusMix&&a<=chorusWidth&&t>=-1&&t<(int)project.tracks.size())return(float)chorusNormal(a-chorusMix,chorusValue(effectMixer(t).chorus,a-chorusMix));
    if(a>=eqLow&&a<=reverbDamping){if(t< -1||t>=(int)project.tracks.size())return 0;const auto& fx=effectMixer(t);switch(a){case eqLow:return(fx.low+18)/36;case eqMid:return(fx.mid+18)/36;case eqHigh:return(fx.high+18)/36;case compThreshold:return(fx.threshold+60)/60;case compRatio:return(fx.ratio-1)/19;case compAttack:return(fx.compAttack-.1f)/99.9f;case compRelease:return(fx.compRelease-5)/995;case compMakeup:return(fx.makeup+12)/36;case reverbMix:return fx.reverbMix;case reverbRoom:return fx.room;default:return fx.damping;}}
    if(a>=dynamicsRange&&a<=dynamicsKnee){if(t< -1||t>=(int)project.tracks.size())return 0;const auto& d=effectMixer(t).dynamics;return a==dynamicsRange?d.range/96:a==dynamicsHysteresis?d.hysteresis/24:a==dynamicsHold?d.hold/1000:d.knee/24;}
    if(a==masterDelayReturn)return project.delayReturn;if(a==masterDelayFeedback)return project.delayFeedback/.8f;
    if(a==bpmDial)return(float)((project.bpm-30)/270);if(a==swingDial)return project.swing/.6f;if(a==masterDial||(a==mixFader&&t==-1))return project.master;
    if(a>=noteVelocity&&a<=noteRatchet){const auto& ns=project.patterns[(size_t)project.activePattern].lanes[(size_t)selectedTrack];if(selectedNote<0||selectedNote>=(int)ns.size())return 0;const auto& n=ns[(size_t)selectedNote];if(a==noteVelocity)return n.velocity;if(a==noteProbability)return n.probability;if(a==noteRatchet)return(n.ratchets-1)/7.f;return(float)(n.length/4);}
    const auto& tr=project.tracks[(size_t)jlimit(0,(int)project.tracks.size()-1,t)];
    switch(a){case volumeDial:case mixFader:return tr.volume/1.5f;case panDial:return(tr.pan+1)/2;case cutoffDial:return(float)(std::log(tr.cutoff/60)/std::log(20000./60));case driveDial:return tr.drive;case sendDial:return tr.send/.8f;case attackDial:return(float)(std::log(tr.attack/.001f)/std::log(2000.));case releaseDial:return(float)(std::log(tr.release/.02f)/std::log(150.));default:return 0;}
}
void Studio::setParameter(int a,int t,float v){if(!std::isfinite(v))return;v=jlimit(0.f,1.f,v);
    if(a==stereoWidth||a==stereoTrim){if(recording||recorder.isActive()||t< -1||t>=(int)project.tracks.size())return;auto& s=effectMixer(t).stereo;if(a==stereoWidth)s.width=v*2;else s.trim=v*30-24;changed();return;}
    if(a>=flangerMix&&a<=phaserWidth){if(recording||recorder.isActive()||t< -1||t>=(int)project.tracks.size())return;auto kind=a>=phaserMix?ModulationKind::phaser:ModulationKind::flanger;int i=a-(kind==ModulationKind::phaser?phaserMix:flangerMix);auto& m=effectMixer(t);auto result=setModulationValue(kind==ModulationKind::phaser?m.phaser:m.flanger,kind,i,modulationFromNormal(kind,i,v));if(result.wasOk())changed();else status=result.getErrorMessage();return;}
    if(a>=chorusMix&&a<=chorusWidth){if(recording||recorder.isActive()||t< -1||t>=(int)project.tracks.size())return;auto& p=effectMixer(t).chorus;auto result=setChorusValue(p,a-chorusMix,chorusFromNormal(a-chorusMix,v));if(result.wasOk())changed();else status=result.getErrorMessage();return;}
    if(a>=eqLow&&a<=reverbDamping){if(recording||recorder.isActive()||t< -1||t>=(int)project.tracks.size())return;auto& fx=effectMixer(t);switch(a){case eqLow:fx.low=v*36-18;break;case eqMid:fx.mid=v*36-18;break;case eqHigh:fx.high=v*36-18;break;case compThreshold:fx.threshold=v*60-60;break;case compRatio:fx.ratio=1+v*19;break;case compAttack:fx.compAttack=.1f+v*99.9f;break;case compRelease:fx.compRelease=5+v*995;break;case compMakeup:fx.makeup=v*36-12;break;case reverbMix:fx.reverbMix=v;break;case reverbRoom:fx.room=v;break;case reverbDamping:fx.damping=v;break;}changed();return;}
    if(a>=dynamicsRange&&a<=dynamicsKnee){if(recording||recorder.isActive()||t< -1||t>=(int)project.tracks.size())return;auto& d=effectMixer(t).dynamics;if(a==dynamicsRange)d.range=v*96;else if(a==dynamicsHysteresis)d.hysteresis=v*24;else if(a==dynamicsHold)d.hold=v*1000;else d.knee=v*24;changed();return;}
    if(a==masterDelayReturn){project.delayReturn=v;changed();return;}if(a==masterDelayFeedback){project.delayFeedback=v*.8f;changed();return;}
    if(a==bpmDial)project.bpm=std::round((30+v*270)*10)/10.;else if(a==swingDial)project.swing=v*.6f;
    else if(a==masterDial||(a==mixFader&&t==-1))project.master=v;
    else if(a>=noteVelocity&&a<=noteRatchet){if(auto* n=note()){if(a==noteVelocity)n->velocity=v;else if(a==noteProbability)n->probability=v;else if(a==noteRatchet)n->ratchets=1+(int)std::round(v*7);else n->length=jmax(.0625,4.*v);}}
    else{auto& tr=project.tracks[(size_t)jlimit(0,(int)project.tracks.size()-1,t)];switch(a){case volumeDial:case mixFader:tr.volume=v*1.5f;break;case panDial:tr.pan=v*2-1;break;case cutoffDial:tr.cutoff=60*(float)std::pow(20000./60,v);break;case driveDial:tr.drive=v;break;case sendDial:tr.send=v*.8f;break;case attackDial:tr.attack=.001f*(float)std::pow(2000.,v);break;case releaseDial:tr.release=.02f*(float)std::pow(150.,v);break;default:break;}}
    changed();
}
int Studio::findNote(double beat,int pitch)const{const auto& ns=project.patterns[(size_t)project.activePattern].lanes[(size_t)selectedTrack];for(int i=(int)ns.size()-1;i>=0;--i)if(ns[(size_t)i].pitch==pitch&&beat>=ns[(size_t)i].beat&&beat<ns[(size_t)i].beat+ns[(size_t)i].length)return i;return-1;}
void Studio::recordOn(int pitch){
    if(!recording||audioRecordMode||(takeScheduled&&!engine->captureReady())||project.tracks[(size_t)selectedTrack].isBus||lane().size()>=maxNotesPerLane)return;
    double global=engine->position.load();if(takeGateEnd>=0&&global>=takeGateEnd-.000001)return;double length=project.patterns[(size_t)project.activePattern].length,local=std::fmod(global,length);double start=std::floor(local*4)/4;
    if(project.recording.punch)start=jmax(start,project.recording.punchStart);double available=takeGateEnd>=0?takeGateEnd-start:length-start;if(available<.01)return;
    lane().push_back({start,jmin(.25,available),pitch,typingVelocity/127.f});recordedKeys[pitch]={(int)lane().size()-1,global};changed();
}
void Studio::recordOff(int pitch){
    auto it=recordedKeys.find(pitch);if(it==recordedKeys.end())return;int index=it->second.first;if(index<(int)lane().size()){
        double global=engine->position.load(),length=project.patterns[(size_t)project.activePattern].length;if(takeGateEnd>=0)global=jmin(global,takeGateEnd);double duration=global-it->second.second;if(duration<0)duration+=length;
        double maximum=takeGateEnd>=0?takeGateEnd-lane()[(size_t)index].beat:64.;lane()[(size_t)index].length=jlimit(.01,jmax(.01,maximum),jmax(.0625,duration));changed();
    }recordedKeys.erase(it);
}
bool Studio::isInterestedInFileDrag(const StringArray& files){for(const auto& f:files)if(File(f).isDirectory()||File(f).hasFileExtension("wav;aif;aiff;flac;ogg;pulse;flp;mid;midi;zip"))return true;return false;}
void Studio::filesDropped(const StringArray& files,int x,int y){if(files.isEmpty())return;if(recording||recorder.isActive()){status="Stop the take before importing files.";return;}File f(files[0]);if(f.isDirectory()){browserLocal=true;browserVisible=true;scanFolder(f);return;}if(f.hasFileExtension("zip")){startProjectArchive(f,true,revision);return;}if(activePane==wavePane&&panes[wavePane].visible){openWaveform(f);return;}if(f.hasFileExtension("pulse;flp")){openProject(f);return;}if(f.hasFileExtension("mid;midi")){Project p;auto r=importMidi(f,p);if(r.wasOk()){checkpoint();releaseKeys();engine->stop();project=std::move(p);selectedTrack=0;selectedNote=-1;selectedNotes.clear();selectedClip=-1;pianoLow=-1;pianoStart=pianoNoteCursor=0;pianoSpan=8;clipboardHasClip=false;currentFile=File();changed();}else status=r.getErrorMessage();return;}int track=selectedTrack;for(auto h=hits.rbegin();h!=hits.rend();++h)if(h->rect.contains((float)x,(float)y)){if(h->action==trackSelect||h->action==rackGrid||h->action==channelOpen)track=h->data;break;}if(activePane==playlistPane&&arrangeRect.contains((float)x,(float)y)){track=jlimit(0,(int)project.tracks.size()-1,(int)((y-arrangeRect.getY())/playlistLaneHeight));double beat=jmax(0.,snapped(playlistStart+(x-arrangeRect.getX())/arrangeRect.getWidth()*playlistSpan));addAudioClip(f,track,beat);}else loadSample(f,track);}
void Studio::writeRuntimeReport(const File& file){
    auto* data=new DynamicObject();var report(data);data->setProperty("application","Pulse Studio");data->setProperty("version",String(appVersion));data->setProperty("interfaceScaleRequested",interfaceScalePercent);data->setProperty("interfaceScaleFitted",effectiveInterfaceScale);
    data->setProperty("updatedAt",Time::getCurrentTime().toISO8601(true));data->setProperty("playing",engine->playing.load());data->setProperty("positionBeats",engine->position.load());data->setProperty("activeOutputs",activeOutputs.load());data->setProperty("keyboardFocus",hasKeyboardFocus(true));data->setProperty("modalDialog",Component::getCurrentlyModalComponent()!=nullptr);data->setProperty("status",status);
    data->setProperty("framesProcessed",(int64)engine->processedFrames.load());data->setProperty("peakOutput",engine->observedPeak.load());
    data->setProperty("lastDspPercent",engine->cpuPercent.load());data->setProperty("backend",deviceManager.getCurrentAudioDeviceType());
    data->setProperty("xruns",deviceManager.getXRunCount());data->setProperty("localOnly",true);
    data->setProperty("recoveryRestored",recoveryRestored);data->setProperty("autosaveBusy",recoveryWriter.active());data->setProperty("autosavedRevision",(int64)autosavedRevision);data->setProperty("autosaveError",autosaveError);data->setProperty("recoveryCleanupBusy",retentionBusy);data->setProperty("trackCount",(int)project.tracks.size());data->setProperty("patternCount",(int)project.patterns.size());
    int assetIssues=0;for(auto& row:assetRows)assetIssues+=row.issue!=AssetIssue::ready;data->setProperty("audioAssetCount",(int)assetRows.size());data->setProperty("audioAssetIssues",assetIssues);data->setProperty("assetWindowVisible",panes[assetsPane].visible);data->setProperty("assetScanBusy",assetBusy);data->setProperty("recording",recording);data->setProperty("countInBeatsLeft",engine->countInRemaining.load());data->setProperty("captureGateOpen",engine->captureReady());data->setProperty("recordedFrames",(int64)recorder.frames());
    data->setProperty("activeInputs",activeInputs.load());data->setProperty("inputMode",inputMode.load());data->setProperty("inputMonitoring",inputMonitor.load());data->setProperty("monitorLevel",monitorLevel.load());data->setProperty("monitorTrack",inputMonitorTrack.load());Array<var> inputMeters;for(float level:inputLevels)inputMeters.add(level);data->setProperty("inputPeaks",inputMeters);data->setProperty("exporting",exporting.load());data->setProperty("exportProgress",exportProgress.load());data->setProperty("exportWindowVisible",panes[exportPane].visible);data->setProperty("songMarkerCount",(int)project.markers.size());data->setProperty("loopSection",project.loopSection);data->setProperty("completedLoops",(int64)engine->completedLoops.load());
    if(auto* device=deviceManager.getCurrentAudioDevice()){data->setProperty("device",device->getName());data->setProperty("sampleRate",device->getCurrentSampleRate());data->setProperty("bufferSize",device->getCurrentBufferSizeSamples());data->setProperty("outputLatencySamples",device->getOutputLatencyInSamples());data->setProperty("inputLatencySamples",device->getInputLatencyInSamples());}
    file.replaceWithText(JSON::toString(report,false));
}
void Studio::startAudioSmoke(){audioSmokeFrames=90;engine->start();status="Running a three-second native audio smoke test.";}
}
