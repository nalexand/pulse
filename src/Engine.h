// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include "AutomationParameters.h"
#include "AutomationWrite.h"
#include "Analyzer.h"
#include "Metronome.h"
#include "AudioHistory.h"
#include "AudioStreaming.h"
#include <atomic>
#include <map>

namespace pulse {
struct Sample : std::enable_shared_from_this<Sample> {
    juce::AudioBuffer<float> audio;
    double sampleRate = 48000;
    std::shared_ptr<StreamedAudio> stream; // Playlist-only; sampler load() always decodes.
    int64_t frameCount()const{return stream?stream->frames:audio.getNumSamples();}
    float waveformPeak(int64_t start,int count)const;
    mutable bool cacheResident=false; // Control-thread-only residency accounting.
};
using SamplePtr = std::shared_ptr<const Sample>;
// Control-thread cache. Callback snapshots and UI consumers retain explicit leases.
// Only cache-only entries may be erased; no decoded buffer is freed by the callback.
class Assets {
public:
    explicit Assets(uint64_t limit = maxDecodedAudioBytes);
    ~Assets();
    SamplePtr load(const juce::File&, juce::String& error);
    SamplePtr loadPlaylist(const juce::File&,double offset,int identity,juce::String& error,bool offline=false);
    AudioStreaming::Statistics streamingStatistics()const{return streaming.statistics();}
    uint64_t availableBytes() const;
    struct Statistics { uint64_t limit=0,resident=0,pinned=0,reclaimable=0,evictions=0; int files=0; };
    Statistics statistics() const;
    uint64_t releaseUnused();
private:
    struct Entry { SamplePtr sample; uint64_t bytes=0,used=0; };
    bool makeRoom(uint64_t bytes);
    AudioStreaming streaming;
    std::map<juce::String,std::weak_ptr<const Sample>> playlistSamples;
    juce::AudioFormatManager formats;
    std::map<juce::String, Entry> samples;
    uint64_t cacheLimit=0,cachedBytes=0,clock=0,evictions=0;
};
constexpr int maxEvents = 32768;
struct Event { double beat = 0, length = .25; int track = 0, pitch = 60; float velocity = .8f, probability = 1; int conditionEvery=1,conditionStep=1,conditionPass=-1; };
struct TrackSound {
    Instrument instrument = Instrument::kick;
    float volume = .7f, pan = 0, cutoff = 16000, drive = 0, send = 0, attack = .005f, release = .18f;
    bool muted = false;
    const Sample* sample = nullptr;
    Track::Mixer mixer;
    bool inputMuted=false,isBus=false;
    double sampleStart=0,sampleEnd=0;
    bool sampleLoop=false;
    int sampleRoot=60;double sampleTune=0;
    MicroTuning tuning;
    SynthPatch synth;
    int chokeGroup=0;
    AuxSends auxSends;uint8_t routingEdges=33;bool hasPreSend=false;
};
struct AudioRegion {
    const Sample* sample = nullptr;
    int track = 0;
    double beat = 0, length = 0, offset = 0, fadeIn = 0, fadeOut = 0;
    float gain = 1;
    FadeCurve fadeInCurve=FadeCurve::linear,fadeOutCurve=FadeCurve::linear;
    int sourceIndex=-1;
};
struct AutomationRegion { int track = 0, target = 0, firstPoint = 0, pointCount = 0; double beat = 0, length = 0; bool trim=false; int slot=0; };
struct Playback {
    // Producer-owned leases; the audio thread only reads raw sample views below.
    std::array<SamplePtr,maxTracks+maxClips> sampleOwners;
    bool songMode=false;uint64_t revisionTag=0,resetSerial=0;
    std::array<Event, maxEvents> events;
    std::array<TrackSound,maxTracks> tracks;
    std::array<AudioRegion,maxClips> audio;
    std::array<AutomationRegion,maxAutomationClips> automation;
    std::array<AutomationPoint,maxTotalAutomationPoints> points;
    int automationCount = 0, pointCount = 0;
    std::array<float,maxAutomationClips> automationInitial{};
    int automationSlotCount=0;
    bool hasTrimAutomation=false;
    int eventCount = 0, trackCount = 0, audioCount = 0;
    std::array<int,maxTracks> routingOrder{};
    double bpm = 118, length = 8,loopStart=0,loopEnd=-1;
    float master = .8f, feedback = .3f, delayReturn = .5f;
    bool masterSoftClip=false;
    Track::Mixer masterMixer;
    std::array<uint8_t,maxTracks+1> effectBypassMask{};
    uint8_t masterProcessingMask=0;
    bool loop = true, metronome = false;
};
juce::Result compilePlayback(const Project&, Assets&, const juce::File& base, Playback&, juce::String& warnings,bool offline=false);
float automationValue(const AutomationPoint* points,int count,double localBeat);

// Single UI producer / single audio consumer triple buffer. The producer never
// touches the consumer's front slot. Exchanges transfer ownership of slots.
class Exchange {
public:
    Playback& beginWrite() { return slots[(size_t)back]; }
    void publish(bool reset=false) {if(reset)++resetSerial;slots[(size_t)back].resetSerial=resetSerial;back = middle.exchange(back | dirty, std::memory_order_acq_rel) & 3; slots[(size_t)back].sampleOwners.fill(nullptr); }
    // Control-thread only, serialized with publish(). An undirty middle slot was
    // returned by the consumer: it cannot become front until the next publish.
    void releaseRetiredSamples(){auto index=middle.load(std::memory_order_acquire);if((index&dirty)==0)slots[(size_t)(index&3)].sampleOwners.fill(nullptr);}
    const Playback& read(bool& changed) {
        changed = false;
        if ((middle.load(std::memory_order_acquire) & dirty) != 0) {
            front = middle.exchange(front,std::memory_order_acq_rel) & 3; changed = true;
        }
        return slots[(size_t)front];
    }
private:
    static constexpr int dirty = 8;
    std::array<Playback,3> slots;
    std::atomic<int> middle {1};
    int back = 2, front = 0;
    uint64_t resetSerial=0; // Producer-owned; carried by every later snapshot until read.
};

class Engine {
public:
    int rawOutputTrack=-1; // Set only before preparing a private offline renderer.
    uint32_t chokeTriggerMask=0; // Private stem rendering: retain inaudible peers' hit timing.
    struct TakePlan { double startBeat=0,gateBeat=0,endBeat=-1; int countInBeats=0; };
    bool beginTake(const TakePlan&);
    std::atomic<int> countInRemaining{0};
    std::atomic<bool> takeGateOpen{false},takeFinished{false};
    bool captureReady()const{return takeGateOpen.load()&&takeReadGeneration.load()==takeGeneration.load();}
    bool captureBlockValid()const{return takeReadGeneration.load()==takeGeneration.load();}
    // Audio-thread-only output from the last render call, relative to its block.
    int captureOffset=0,captureCount=0;
    HistorySpan historySpan; // Last callback span, read only by the calling producer.
    Engine();
    void prepare(double rate);
    void render(float* left, float* right, int frames);
    struct MonitorInput { const float* left=nullptr; const float* right=nullptr; int track=-1; float gain=0; };
    void render(float* left,float* right,int frames,const MonitorInput&);
    // Set only by an offline renderer before its first callback.
    bool preserveAutomationInTails=false;
    Exchange exchange;
    AutomationWrite automationWrite;
    AnalyzerCapture analyzer;
    std::atomic<bool> playing {false}, resetRequested {false};
    std::atomic<bool> auditionPanicRequested{false};
    std::atomic<double> position {0};
    std::atomic<double> seekRequested {-1};
    std::array<std::atomic<float>,maxTracks> meters;
    std::array<std::atomic<float>,maxTracks> gainReduction;
    std::array<std::array<std::atomic<float>,automationTargetCount>,maxTracks+1> automationReadout;
    std::atomic<float> masterAutomation{-1};
    std::atomic<float> masterGainReduction{0};
    std::atomic<float> masterMeter {0}, cpuPercent {0};
    std::atomic<uint64_t> processedFrames {0};
    std::atomic<uint64_t> completedLoops{0};
    std::atomic<uint64_t> chasedNotes{0};
    void setMetronomeSettings(const MetronomeSettings& s){metronomeConfig.store(packMetronome(s),std::memory_order_release);}
    std::atomic<float> observedPeak {0};
    bool audition(int track, int pitch, bool on, float velocity = .8f,float seconds=60);
    void stop();
    void start();
    // Borrowed stack samples must outlive audition; managed samples are retained.
    bool preview(const Sample*,double startSeconds=0,double endSeconds=0,float gain=.7f,double sourceBpm=0,bool repeat=false,bool align=false);
    bool preview(const SamplePtr& sample,double start=0,double end=0,float gain=.7f,double bpm=0,bool repeat=false,bool align=false){return preview(sample.get(),start,end,gain,bpm,repeat,align);}
    // Call only from the same control thread as preview(). Never from render().
    void reclaimPreviewSamples();
    void cancelPreview(){previewGeneration.fetch_add(1,std::memory_order_acq_rel);}
    std::atomic<bool> previewPlaying{false};
    std::atomic<double> previewPosition{0};
private:
    struct TakeCommand { TakePlan plan; uint64_t generation=0; };
    std::array<TakeCommand,8> takeCommands;
    std::atomic<unsigned> takeWrite{0},takeRead{0};
    std::atomic<uint64_t> takeGeneration{0},takeReadGeneration{0};
    TakePlan takePlan;
    bool takeRunning=false;
    uint64_t takeCountFrames=0,takeCountElapsed=0,takeTimelineFrames=0,takeGateStart=0,takeGateEnd=UINT64_MAX;
    double takeFramesPerBeat=24000;
    int takeClickIndex=0;
    MetronomeClick click;
    std::atomic<uint32_t> metronomeConfig{1424}; // Classic, 100%, accented.
    float monitorGain=0;
    int monitorTrack=-1;
    struct PreviewCommand { const Sample* sample=nullptr; double start=0,end=0; uint64_t generation=0;float gain=.7f;double sourceBpm=0;bool repeat=false,align=false; SamplePtr owner; };
    std::atomic<uint64_t> previewGeneration{0};
    uint64_t previewSourceGeneration=0;
    std::array<PreviewCommand,32> previewCommands;
    std::atomic<unsigned> previewWrite{0},previewRead{0};
    const Sample* previewSource=nullptr;
    SamplePtr previewSourceOwner;
    // Control-thread guards prevent the final shared release on the audio thread.
    std::vector<SamplePtr> previewOwners;
    unsigned previewReclaimed=0;
    double previewFrame=0,previewEnd=0,previewStart=0,previewBpm=0,previewWait=0;float previewGain=.7f;bool previewRepeat=false;
    struct Voice {
        bool active = false, released = false;
        int track = 0, pitch = 60;
        int scheduledEvent = -1;
        double sourceBeat = 0, tuningCents=0;
        double phase = 0, phase2 = 0, age = 0, gate = 0, releasedAt = 0, samplePosition = 0;
        float velocity = 1, lastNoise = 0, right = 0;
        uint32_t random = 1234;
        SynthVoice synth;
        int chokeLeft=0,chokeTotal=0;
    };
    struct Input { int track = 0, pitch = 60; bool on = true; float velocity = .8f,seconds=60; };
    std::array<Input,128> input;
    std::atomic<unsigned> inputWrite {0}, inputRead {0};
    std::array<Voice,128> voices;
    std::array<float,maxTracks> filterL {}, filterR {}, filterCoeff {}, panL {}, panR {}, smoothGain {};
    std::array<std::array<float,auxSendCount>,maxTracks> auxGain{};
    std::array<float,maxTracks> auxMuteGain{};
    std::array<float,maxTracks> lastCutoff {},lastPan {};
    float automatedDelayReturn=.5f,automatedFeedback=.3f,automatedSoftClip=0;
    bool masterProcessingReady=false;
    struct ChannelFX {
        std::array<std::array<float,6>,3> eq;
        std::array<std::array<float,2>,3> zL{},zR{};
        juce::Reverb reverb;
        StereoChorus chorus;
        StereoUtility stereo;
        PeakDynamics dynamics;
        StereoFlanger flanger;
        StereoPhaser phaser;
        FiniteEcho delay;
        GuitarDistortion distortion;
        Track::Mixer lastSettings;
        std::array<float,builtinEffectCount> bypassGain{};
        float bypassStep=1;
        float envelope=0,attack=0,release=0,reduction=0;
    };
    std::array<ChannelFX,maxTracks+1> effects; // Final slot is the independent Master chain.
    std::array<float,maxTracks> detectorLast{};
    void configureEffects();
    void configureEffect(int,const Track::Mixer&,bool force=false);
    uint64_t automationControlClock=0;
    void processEffects(int track,const Track::Mixer&,float& left,float& right);
    std::vector<float> delayL, delayR;
    size_t delayPos = 0;
    double sampleRate = 48000, beat = 0;
    int nextEvent = 0, loopCycle = 0;
    const Playback* state = nullptr;
    bool preparedChange = true;uint64_t resetSerialSeen=0;
    bool chasePending = false;
    std::atomic<bool> resumeRequested{false};
    Voice& trigger(const Event&, double gateSamples,int eventIndex=-1);
    void chaseNotes(double at);
    void clear();
    float voiceSample(Voice&, const TrackSound&);
    void chokeVoices(const Event&);
};
juce::Result renderWav(const Project&, Assets&, const juce::File& base, const juce::File& target,
                       int bits = 24, int onlyTrack = -1, std::atomic<bool>* cancel = nullptr,
                       std::atomic<float>* progress = nullptr);
}

