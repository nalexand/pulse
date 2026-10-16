// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Engine.h"
#include "Recorder.h"
#include "ProjectAssets.h"
#include "Templates.h"
#include "RecordingTools.h"
#include "InputAudio.h"
#include "Render.h"
#include "NoteTools.h"
#include "Rhythm.h"
#include "Arpeggiator.h"
#include "NotePresets.h"
#include "PatternTools.h"
#include "ArrangementTools.h"
#include "WaveTools.h"
#include "SynthPresets.h"
#include "SoundLibrary.h"
#include "EffectPresets.h"
#include "WorkspaceLayouts.h"
#include "ControlMailbox.h"
#include "TypingPerformance.h"
#include <optional>
#include "CommandPalette.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>
#include <deque>
#include <set>

namespace pulse {
class Studio final : public juce::AudioAppComponent, private juce::Timer,
                     public juce::FileDragAndDropTarget {
public:
    explicit Studio(const juce::File& workspace, bool enableAudio = true,bool resumeRecovery = false);
    ~Studio() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void prepareToPlay(int,double) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override { activeOutputs.store(0);activeInputs.store(0);inputMonitor.store(false);inputPeaks.reset();if(recorder.isActive())recordDeviceStopped.store(true); }
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool) override;
    void focusLost(FocusChangeType) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    void openProject(const juce::File&);
    std::function<void()> fitMainWindow;
    std::function<void()> interfaceScaleChanged;
    bool setInterfaceScale(int);
    int requestedInterfaceScale()const{return interfaceScalePercent;}
    void setEffectiveInterfaceScale(float scale){effectiveInterfaceScale=scale;}
    juce::Result setLocalControlEnabled(bool);
    bool isDirty() const { return dirty; }
    void save();
    bool snapshot(const juce::File&);
    bool runEditorChecks(const juce::File&,bool typingOnly=false,bool arrangementOnly=false,bool appearanceOnly=false,bool selectionOnly=false,bool sectionsOnly=false,bool resamplingOnly=false);
    void startAudioSmoke();
    void startPlayback(){if(!engine->playing.load())perform(play);}
    void startTransportChecks(const juce::File&);
private:
    enum Action { play=1,stop,mode,loop,metro,record,newProject,open,saveAction,exportAudio,settings,help,
                  composeTab,arrangeTab,mixTab,patternSelect,patternAdd,patternDuplicate,patternClear,undoAction,redoAction,
                  trackSelect,mute,solo,instrumentSelect,importSample,importMidiAction,exportMidiAction,
                  arrangeFill,generate,quantize,transposeUp,transposeDown,scaleToggle,rename,notesAction,
                  volumeDial,panDial,cutoffDial,driveDial,sendDial,attackDial,releaseDial,masterDial,bpmDial,swingDial,
                  noteVelocity,noteLength,noteProbability,noteRatchet,mixFader,clipHit,noteHit,pianoGrid,rackGrid,arrangeGrid,
                  exportStems,recover,saveVersion,menuOpen,patternMenu,patternPrevious,patternNext,patternRename,patternSteps,
                  pianoOpen,channelOpen,browserToggle,browserFolder,typingToggle,toolSelect,snapMenu,
                  paneBody,paneMove,paneResize,paneClose,paneMaximize,layoutReset,pianoKey,velocityDrag,
                  addChannel,cloneChannel,deleteChannel,selectArea,seekRuler,browserResize,
                  copySelection,cutSelection,pasteSelection,duplicateSelection,paintArea,eraseArea,rackPaint,
                  importAudioClip,recordMode,audioClipSettings,splitClip,
                  automationOpen,automationCreate,automationGrid,automationPoint,automationHold,automationReset,
                  effectsOpen,effectsTab,effectToggle,outputRoute,sidechainRoute,addBus,routeDestination,
                  eqLow,eqMid,eqHigh,compThreshold,compRatio,compAttack,compRelease,compMakeup,reverbMix,reverbRoom,reverbDamping,
                  localBrowser,folderChoose,folderUp,folderSearch,folderBookmarks,browserEntry,
                  waveformOpen,wavePreview,waveStop,waveGrid,waveAll,waveZoom,waveSampler,wavePlaylist,waveLoop,
                  waveReverse,waveNormalize,waveFadeIn,waveFadeOut,waveUndo,
                  assetsOpen,assetsRefresh,assetSelect,assetLocate,assetFind,assetApplyMatches,assetChooseMatch,assetPreview,assetEdit,assetCancelScan,
                  templatesOpen,templatesRefresh,templateSelect,templateFilter,templateFind,templateSave,templateStart,
                  recordingOpen,tapTempoAction,tapReset,countInSelect,preRollSelect,punchToggle,punchRange,punchInHere,punchOutHere,
                  inputModeSelect,monitorToggle,monitorLevelChange,inputAudioSettings,
                  renderSource,renderFormat,renderRate,renderBits,renderChannels,renderFull,renderRange,renderPunch,renderTail,renderNormalize,renderTarget,renderDither,renderIsolated,renderChoose,renderCancel,
                  markersOpen,markerAdd,sectionAdd,markerSelect,markerEdit,markerDelete,markerJump,markerPrevious,markerNext,markerLoop,markerClearLoop,markerExport,markerColour,markerHit,markerRail,
                  humanize,quickQuantize,noteToolMode,noteToolCapture,noteToolGrid,noteToolValue,noteToolWindow,noteToolChords,noteToolSeed,noteToolReroll,noteToolPreview,noteToolReset,noteToolApply,noteToolCancel,commandSearch,paletteWindow,paletteClip,paletteTemplate,euclideanOpen,rhythmAdjust,rhythmNumber,rhythmRange,rhythmWrite,arpeggiatorOpen,arpDirection,arpRate,arpAdjust,arpNumber,arpRestart,notePresetsOpen,patternMakeUnique,patternSplitChannels,patternMergeLayers,patternClipMenu,patternMoveEarlier,patternMoveLater,patternDeleteUnused,waveEditMenu,waveCopy,waveCut,wavePaste,waveReplace,waveDuplicate,waveTrim,waveDelete,waveSilence,waveRedo,waveSliceMenu,waveSlice,waveDetect,waveClearMarks,waveSliceMarks,synthOpen,synthPresets,synthSave,synthLoad,synthPreview,synthTab,synthWave,synthValue,synthChannel,synthLibrarySave,synthLibraryImport,synthLibraryManage,chorusMix,chorusRate,chorusDepth,chorusDelay,chorusFeedback,chorusWidth,flangerMix,flangerRate,flangerDepth,flangerCentre,flangerFeedback,flangerWidth,phaserMix,phaserRate,phaserDepth,phaserCentre,phaserFeedback,phaserWidth,effectsChainMenu,effectsChainCopy,effectsChainPaste,effectsChainSave,effectsChainLoad,effectsChainReset,effectMoveEarlier,effectMoveLater,effectsFactoryRecall,analyzerOpen,analyzerMaster,analyzerChannel,analyzerFreeze,analyzerReset,analyzerBack,stereoOpen,stereoEnable,stereoMono,stereoSwap,stereoInvertLeft,stereoInvertRight,stereoReset,stereoWidth,stereoTrim,workspaceMenu,workspaceSave,workspaceFactory,workspaceRecall,interfaceScaleMenu,interfaceScale,controlToggle,typingMenu,typingSustainToggle,typingVelocitySet,typingVelocityEdit,typingOctaveSet,typingRecall,typingCaptureClear,typingTransposeSet,typingCaptureToggle,typingCaptureWindow,patternMergeSequence,patternDeleteUsed,patternColourMenu,patternColourSet,metronomeSettingsOpen,patternExtract,patternMergeOptions,sectionClipsOpen,waveResample,waveCancelConversion,lastAction };
    enum MenuId { menuView=50000,menuMaximize=50100,menuClose=50200,menuInstrument=50300,menuSaveAs=50400,menuPattern=50500,menuMergePattern=50600,menuSequencePattern=50700,menuPatternColour=50800 };
    static_assert(lastAction<menuView);
    enum Pane { playlistPane=0,rackPane,pianoPane,mixerPane,channelPane,automationPane,effectsPane,wavePane,assetsPane,templatesPane,recordingPane,exportPane,markersPane,noteToolsPane,synthPane,paneCount };
    static_assert(paneCount==workspacePaneCount);
    struct PaneState { juce::Rectangle<float> bounds,restoreBounds; bool visible=false,maximized=false; };
    struct Hit { juce::Rectangle<float> rect; int action=0,data=0,extra=0,pane=-1; };
    ControlMailbox controlMailbox;
    bool controlBackupDone=false;
    juce::File controlBackupFile;
    juce::Result backupForControl(bool force=false);
    juce::Result handleControlCommand(const juce::String&,const juce::var&,juce::var&);
    juce::var controlState()const;
    void runControlChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    Project project;
    Assets assets;
    std::unique_ptr<Engine> engine;
    Recorder recorder;
    std::atomic<int> activeInputs{0};
    std::atomic<int> activeOutputs{2};
    std::atomic<int> inputMode{0},inputMonitorTrack{5};
    std::atomic<bool> inputMonitor{false};
    std::atomic<float> monitorLevel{.5f};
    InputPeaks inputPeaks;
    std::array<float,2> inputLevels{};
    std::array<int,2> inputClipTicks{};
    void restoreInputSettings();
    void saveInputSettings();
    std::vector<float> monoRight=std::vector<float>(512);
    juce::AudioBuffer<float> captureInput{2,512};
    std::atomic<bool> captureScheduled{false};
    std::atomic<bool> captureClosing{false};
    std::atomic<int> captureCallbacks{0};
    bool takeScheduled=false,takeRestoreLoop=false,takeRestoreMode=false,takeOldLoop=true,takeOldMode=false;
    double takeGateEnd=-1;
    TapTempo tapTempo;
    double lastTapTime=-1;
    uint64_t tapRevision=0;
    bool tapHasCheckpoint=false;
    void paintRecording(juce::Graphics&,juce::Rectangle<float>);
    bool performRecordingAction(int,int);
    void applyTap(double);
    Engine::TakePlan recordingPlan(bool keyboard) const;
    void startKeyboardRecording();
    void restoreTakeTransport();
    juce::File transportReport;
    int transportTicks=0;
    juce::var transportResults;
    uint64_t transportFrames=0,lastPlaybackFrames=0;
    int stalledPlaybackTicks=0;
    void runTransportTick();
    bool audioReady();
    double firstSoundBeat=-1;
    std::atomic<bool> recordDeviceStopped{false};
    juce::File takeFile;
    bool audioRecordMode=false;
    double takeBeat=0,takeTempo=118;
    int takeTrack=0;
    std::array<const Sample*,maxClips> clipSamples{};
    juce::File root,currentFile;
    std::vector<Hit> hits;
    std::unique_ptr<CommandPalette> commandPalette;
    uint64_t paletteRevision=0;
    int paletteContextPane=rackPane;
    std::vector<PaletteEntry> paletteEntries()const;
    void showCommandPalette();
    void closeCommandPalette();
    void positionCommandPalette();
    void executePalette(const juce::String&);
    std::deque<Project> undoStack,redoStack;
    std::unique_ptr<juce::FileChooser> chooser;
    std::thread exportThread;
    std::atomic<bool> exporting{false},exportDone{false},cancelExport{false};
    std::atomic<float> exportProgress{0};
    juce::String exportMessage;
    RenderSettings renderSettings;
    bool renderIsolatedTracks=false;
    void showExport(bool isolated);
    void paintExport(juce::Graphics&,juce::Rectangle<float>);
    bool performExportAction(int,int);
    void restoreExportSettings();
    void saveExportSettings();
    bool dirty=false,recording=false,scaleGuide=true,audioEnabled=false,recoveryRestored=false;
    NoteToolOptions noteToolOptions;
    bool noteToolBound=false,noteToolAudition=true,noteToolValid=false;
    RhythmOptions rhythmOptions;
    RhythmStats rhythmStats;
    ArpOptions arpOptions;
    ArpStats arpStats;
    std::array<juce::String,4> selectedNotePreset;
    NotePreset captureNotePreset(const juce::String&,const juce::String&)const;
    bool applyNotePreset(const NotePreset&);
    bool storeNotePreset(const NotePreset&,bool replace=false);
    bool deleteNotePreset(const juce::String&);
    void nameNotePreset(NotePreset,bool rename=false);
    void showNotePresets();
    void runNotePresetChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    bool performPatternToolAction(int,int);
    bool commitPatternToolProject(Project,int,const PatternEditStats&);
    void confirmPatternDelete(bool removeClips=false);
    bool deletePatternNow(int,bool removeClips=false);
    bool acceptPatternDelete(const juce::String&,uint64_t,bool);
    void runPatternBankChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void showPatternClipMenu(int);
    bool selectedPatternClip()const;
    std::set<int> patternNoteSelection()const;
    void showPatternExtraction();
    bool acceptPatternExtraction(const juce::String&,int,const std::set<int>&,uint64_t,bool,bool);
    void showPatternMergeOptions();
    bool acceptPatternMerge(const juce::String&,const juce::String&,uint64_t,PatternMergePolicy);
    void runPatternSelectionChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void runPatternToolChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void runSequenceChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void paintArpeggiator(juce::Graphics&,juce::Rectangle<float>);
    bool performArpAction(int,int);
    void setArpNumber(int,int);
    uint32_t& noteToolSeedValue(NoteToolMode);
    void paintRhythm(juce::Graphics&,juce::Rectangle<float>);
    bool performRhythmAction(int,int);
    void setRhythmNumber(int,int);
    int rhythmNumberValue(int)const;
    uint64_t noteToolRevision=0;
    int noteToolPattern=-1,noteToolTrack=-1;
    double noteToolLength=8;
    std::set<int> noteToolSelection,noteToolCapturedSelection;
    std::vector<Note> noteToolSource,noteToolCandidate;
    juce::String noteToolTarget,noteToolMessage="Open Quantize or Humanize to capture the current note selection.";
    bool noteToolCurrent()const;
    bool pendingNoteTool()const;
    const std::vector<Note>& visibleNotes(int pattern,int track)const;
    void openNoteTools(NoteToolMode);
    void refreshNoteToolPreview();
    void cancelNoteTool(bool republish=true);
    void setNoteToolValue(int,float);
    float noteToolValueFor(int)const;
    void paintNoteTools(juce::Graphics&,juce::Rectangle<float>);
    void noteToolSlider(juce::Graphics&,const juce::String&,juce::Rectangle<float>,int,const juce::String&);
    bool performNoteToolAction(int,int);
    int page=0,selectedTrack=5,selectedNote=-1,trackScroll=0,octave=4;
    int dragAction=0,dragData=0,dragExtra=0;
    float dragValue=0;
    juce::Point<float> dragOrigin;
    Note dragNote;
    Clip dragClip;
    int pianoLow=36;
    int pianoRows=24;
    int activePane=rackPane,editorTool=0,browserGroup=0;
    bool browserVisible=true,typingEnabled=true,layoutReady=false;
    float browserWidth=216;
    double snap=.25,lastNoteLength=.5,pianoStart=0,pianoSpan=8,playlistStart=0,playlistSpan=40;
    float playlistLaneHeight=51;
    int selectedClip=-1;
    std::array<PaneState,paneCount> panes;
    std::vector<int> paneOrder;
    int interfaceScalePercent=100;
    float effectiveInterfaceScale=1;
    juce::String selectedWorkspace;
    juce::File workspaceLibraryFile()const{return root.getChildFile("data/workspaces.json");}
    WorkspaceLayout captureWorkspace()const;
    bool applyWorkspace(const WorkspaceLayout&,const juce::String& name);
    bool applyFactoryWorkspace(int);
    bool recallWorkspace(const juce::String&,const juce::String& expected);
    bool storeWorkspace(WorkspacePreset,bool,const juce::String& expected,const juce::String& source);
    void nameWorkspace(WorkspacePreset,bool,const juce::String& expected,const juce::String& source);
    void confirmWorkspaceDelete(const WorkspacePreset&,const juce::String& expected);
    void showWorkspaceMenu();
    bool performWorkspaceAction(int,int);
    juce::String workspaceSourceToken()const;
    void showInterfaceScaleMenu();
    void runWorkspaceChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    std::array<bool,4> browserExpanded{true,true,true,true};
    std::array<const Sample*,maxTracks> previewSamples{};
    juce::Rectangle<float> dragPaneBounds,selectionBox;
    std::set<int> selectedNotes;
    std::vector<std::pair<int,Note>> dragNotes;
    std::vector<Note> noteClipboard;
    Clip clipClipboard;
    juce::String clipboardPatternId;
    bool clipboardHasClip=false;
    double editCursorBeat=0;
    juce::String hoverHint;
    juce::Rectangle<float> rackRect,pianoRect,velocityRect,arrangeRect;
    juce::Rectangle<float> automationRect;
    juce::Rectangle<float> markerRailRect,markerListRect;
    juce::String selectedMarker;
    int markerScroll=0;
    SongMarker dragSongMarker;
    bool dragMarkerCheckpoint=false;
    void paintMarkers(juce::Graphics&,juce::Rectangle<float>);
    void paintMarkerRail(juce::Graphics&);
    bool performMarkerAction(int,int);
    void editSongMarker(bool section,const juce::String& id={},double at=-1);
    bool applySongMarker(const SongMarker&,bool undo=true);
    void jumpToMarker(const juce::String&);
    void showMarkerMenu(double,const juce::String& id={});
    void showSectionClips();
    bool acceptSectionClips(const juce::String&,uint64_t,double,bool,bool);
    void runSectionClipChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    int selectedPoint=-1;
    int selectedEffect=0;
    double automationCursor=0;
    std::vector<AutomationPoint> automationClipboard;
    juce::String status=L"Ready. Space to play. Z–M to play notes. Everything stays on your computer.";
    struct TypingHeld {int track=0,pitch=60;};
    std::map<int,TypingHeld> heldKeys;
    std::vector<TypingHeld> sustainedKeys;
    int typingVelocity=102;
    int typingTranspose=0,typingCaptureSeconds=60;
    bool typingCaptureEnabled=true;
    bool typingSustain=false;
    TypingCapture typingCapture;
    juce::String typingCaptureContext;
    void restoreTypingSettings();
    void saveTypingPreferences();
    void showTypingMenu();
    bool performTypingAction(int,int);
    bool typingKeyOn(int key,int pitch);
    void typingKeyOff(int key);
    void releaseTypingNote(TypingHeld);
    void setTypingSustain(bool);
    juce::String typingContext()const;
    bool recallTypingPhrase(double now);
    void runTypingChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    std::map<int,std::pair<int,double>> recordedKeys;
    int mousePitch=-1,mousePitchTrack=0;
    int autosaveCounter=0;
    int runtimeCounter=0,audioSmokeFrames=0;
    uint64_t revision=0,autosavedRevision=0;
    juce::int64 openedAt=juce::Time::currentTimeMillis();
    juce::Colour colorFor(int) const;
    juce::Colour patternColor(int) const;
    MetronomeSettings metronomeSettings;
    uint64_t metronomeRevision=0;
    bool performAppearanceAction(int,int);
    void restoreMetronomeSettings();
    bool applyMetronomeSettings(const MetronomeSettings&,uint64_t);
    void showMetronomeSettings();
    bool applyPatternColour(const juce::String&,uint64_t,int);
    void showPatternColours();
    void runAppearanceChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void timerCallback() override;
    void button(juce::Graphics&,const juce::String&,juce::Rectangle<float>,int,int=0,bool=false,bool=false);
    void label(juce::Graphics&,const juce::String&,juce::Rectangle<float>,float,juce::Colour,bool=false,int=juce::Justification::centredLeft);
    void panel(juce::Graphics&,juce::Rectangle<float>,juce::Colour,float=10);
    void dial(juce::Graphics&,const juce::String&,juce::Rectangle<float>,int,float,int=0);
    void paintHeader(juce::Graphics&);
    void paintBrowser(juce::Graphics&);
    void paintRack(juce::Graphics&,juce::Rectangle<float>);
    void paintPiano(juce::Graphics&,juce::Rectangle<float>);
    void paintArrangement(juce::Graphics&,juce::Rectangle<float>);
    void paintMixer(juce::Graphics&,juce::Rectangle<float>);
    void paintInspector(juce::Graphics&);
    bool performSoundLibraryAction(int,int);
    juce::File soundLibraryFile()const{return root.getChildFile("data/synth-library.json");}
    void showSoundLibrary(bool manage=false);
    void nameLibrarySound(SynthPreset,const juce::String& id={},bool fromCurrent=false);
    void confirmLibraryUpdate(const juce::String&);
    void confirmLibraryDelete(const juce::String&);
    bool recallLibrarySound(const juce::String&);
    bool toggleLibraryFavorite(const juce::String&);
    void chooseLibrarySoundFile(bool importing,const juce::String& id={});
    void runSoundLibraryChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    bool performSynthAction(int,int);
    bool applySynthPreset(const SynthPreset&,int track);
    void showSynthPresets();
    void chooseSynthFile(bool save);
    void paintSynth(juce::Graphics&,juce::Rectangle<float>);
    void editSynthValue(int);
    void showWavetableMenu();
    bool applyWavetableFamily(int,int,uint64_t);
    void runWavetableChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    bool changeSynthValue(int,double,bool checkpoint=true);
    void runSynthChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    int synthPage=0;
    void paintChannel(juce::Graphics&,juce::Rectangle<float>);
    void paintAutomation(juce::Graphics&,juce::Rectangle<float>);
    std::optional<EffectChainPreset> effectClipboard;
    bool performEffectChainAction(int,int);
    bool applyEffectChain(const EffectChainPreset&,int);
    bool moveEffect(int,int);
    void showEffectChainMenu(int effect=-1);
    bool invokeEffectChainMenu(int,int,int,uint64_t);
    void chooseEffectChainFile(bool);
    void runEffectChainChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void editModulationValue(ModulationKind,int);
    void showModulationParameterMenu(int,int);
    void runModulationChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void editChorusValue(int);
    void showChorusParameterMenu(int,int);
    void runChorusChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    bool analyzerView=false,analyzerFollowTrack=false,analyzerFrozen=false,analyzerWasActive=false;
    int analyzerLastSource=-2;
    uint32_t analyzerToken=0;
    uint64_t analyzerDropBaseline=0;
    double analyzerLastFresh=0;
    AnalyzerModel analyzerModel;
    bool performAnalyzerAction(int,int);
    void updateAnalyzer();
    void restoreAnalyzerSettings();
    void saveAnalyzerSettings();
    void paintAnalyzer(juce::Graphics&,juce::Rectangle<float>);
    void runAnalyzerChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    bool performStereoAction(int,int);
    void paintStereo(juce::Graphics&,juce::Rectangle<float>);
    void showStereoParameterMenu(int,int);
    void editStereoValue(int);
    void runStereoChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void paintEffects(juce::Graphics&,juce::Rectangle<float>);
    void paintWaveform(juce::Graphics&,juce::Rectangle<float>);
    void paintLocalBrowser(juce::Graphics&,juce::Rectangle<float>);
    bool performSampleAction(int,int);
    void openWaveform(const juce::File&,double start=0,double end=0);
    void selectWave(double);
    double waveTime(float) const;
    void editWave(int);
    bool performWaveAction(int,int);
    bool saveWaveEdit(const juce::AudioBuffer<float>&,int first,int end,double rate=0);
    void showWaveResample();
    bool beginWaveResample(double targetRate,bool paste=false,bool replace=false);
    void finishWaveResample();
    void runWaveResampleChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void waveClipboardAction(int);
    void applyWaveHistory(bool redo);
    void showWaveMenu(bool slicing=false);
    bool sliceWave(double step);
    bool commitWaveSlices(Project);
    bool sliceWaveMarks();
    void detectWaveTransients();
    bool applyWaveDetection(const TransientOptions&);
    void toggleWaveMarker(double seconds);
    void runWaveEditingChecks(const juce::File&,const std::function<void(bool,const juce::String&)>&);
    void scanFolder(const juce::File&);
    void finishFolderScan();
    void saveLibrary();
    void restoreLibrary();
    bool browserLocal=false;
    juce::File libraryFolder,waveFile;
    juce::String librarySearch,scanError;
    juce::StringArray bookmarks;
    std::vector<juce::File> libraryEntries,scanEntries;
    std::thread scanThread;
    std::atomic<bool> scanDone{false},scanCancel{false};
    bool scanning=false;
    int libraryScroll=0;
    juce::Rectangle<float> libraryRect,waveRect;
    const Sample* waveSample=nullptr;
    double waveStart=0,waveEnd=0,waveViewStart=0,waveSpan=1,waveAnchor=0;
    struct WaveHistory { juce::File file; double start=0,end=0,viewStart=0,span=1; };
    std::vector<WaveHistory> waveHistory,waveRedoHistory;
    juce::AudioBuffer<float> waveClipboard;
    double waveClipboardRate=0;
    uint64_t waveRevision=0;
    std::thread waveEditThread;
    std::atomic<bool> waveEditCancel{false},waveEditDone{false};
    bool waveEditBusy=false;
    uint64_t waveEditSourceRevision=0;
    juce::AudioBuffer<float> waveEditOutput;
    juce::String waveEditError;
    double waveEditRate=0;
    int waveEditFirst=0,waveEditEnd=0;
    std::vector<int> waveMarkers;
    TransientOptions transientOptions;
    std::vector<ProjectAsset> assetRows,assetWorkerRows;
    std::thread assetThread;
    std::atomic<bool> assetDone{false},assetCancel{false};
    bool assetBusy=false,assetNeedsRefresh=true,assetRevealMissing=false;
    uint64_t assetGeneration=0,assetTaskGeneration=0;
    juce::int64 assetLastCheck=0;
    int selectedAsset=-1,assetScroll=0;
    juce::String assetWorkerMessage,assetMessage;
    juce::Rectangle<float> assetListRect;
    void paintProjectAssets(juce::Graphics&,juce::Rectangle<float>);
    void startAssetAudit(const juce::File& searchFolder={});
    void finishAssetAudit();
    bool performAssetAction(int,int);
    bool applyAssetReplacements(const std::vector<AssetReplacement>&);
    std::vector<ProjectTemplate> templateRows=builtInProjectTemplates(),templateWorkerRows;
    std::thread templateThread;
    std::atomic<bool> templateDone{false},templateCancel{false};
    bool templateBusy=false,templatesNeedRefresh=true,templateSaved=false;
    int templateTask=0,selectedTemplate=0,templateScroll=0,templateGroup=0;
    juce::String templateQuery,templateMessage="Built-in starting sessions",templateWorkerMessage,templateSavedId;
    juce::Rectangle<float> templateListRect;
    juce::File lastTemplateBackup;
    void paintTemplates(juce::Graphics&,juce::Rectangle<float>);
    void refreshTemplates();
    void finishTemplateTask();
    void saveTemplateDialog();
    void saveTemplateCopy(Project,const juce::File& base,const juce::String&,const juce::String&,TemplateContent);
    bool startFromTemplate(const ProjectTemplate&);
    bool templateMatches(const ProjectTemplate&) const;
    bool performTemplateAction(int,int);
    static bool isParameter(int action){return(action>=volumeDial&&action<=mixFader)||(action>=eqLow&&action<=reverbDamping)||(action>=chorusMix&&action<=chorusWidth)||(action>=flangerMix&&action<=phaserWidth)||(action>=stereoWidth&&action<=stereoTrim);}
    void chooseRouting(bool sidechain);
    bool routeTrack(int source,int destination);
    void createAutomation(int action,int track);
    int automationTargetFor(int action,int track) const;
    float liveParameter(int action,int track,float fallback) const;
    Clip* automationClip();
    bool automationMouseDown(const Hit&,const juce::MouseEvent&);
    void automationMouseDrag(const juce::MouseEvent&);
    void deleteAutomationPoint();
    void editAutomationClipboard(int action);
    void paintPane(juce::Graphics&,int);
    void rotary(juce::Graphics&,juce::Rectangle<float>,int,float,int=0,juce::Colour=juce::Colour(0xffb8c997));
    void windowFrame(juce::Graphics&,juce::Rectangle<float>,const juce::String&,int);
    void showPane(int,bool toggle=false);
    void resetLayout();
    void saveLayout();
    void restoreLayout();
    juce::Rectangle<float> workspaceBounds() const;
    juce::Rectangle<float> boundedPane(juce::Rectangle<float>,int) const;
    juce::Rectangle<float> noteBounds(const Note&) const;
    double snapped(double,bool bypass=false) const;
    juce::String hintFor(const Hit&) const;
    juce::PopupMenu::Options menuOptions(int action=0,int data=-1);
    void showMenu(int);
    void dispatchMenuResult(int);
    void showPatternMenu();
    void renamePattern();
    void setPatternSteps();
    void showChannelMenu(int);
    void modifyChannel(int,int);
    void deleteSelection();
    void editClipboard(int);
    void perform(int,int=0);
    void checkpoint();
    void changed(bool compile=true);
    void publish();
    void applyHistory(bool redo);
    void chooseFile(int);
    void startExport(const juce::File&,bool stems=false);
    void startExport(const juce::File&,const RenderSettings&,bool stems=false);
    void loadSample(const juce::File&,int);
    void addAudioClip(const juce::File&,int,double);
    void toggleAudioRecording();
    void finishAudioRecording();
    void editAudioClip();
    void splitSelectedClip();
    void editName(bool projectName,bool projectNotes=false);
    void commitSave(const juce::File&);
    void rememberProject(const juce::File&);
    bool restoreLastProject();
    void writeRuntimeReport(const juce::File&);
    void showHelp();
    void releaseKeys();
    void recordOn(int pitch);
    void recordOff(int pitch);
    float parameterValue(int,int) const;
    void setParameter(int,int,float);
    std::vector<Note>& lane();
    Note* note();
    int findNote(double,int) const;
};
}
