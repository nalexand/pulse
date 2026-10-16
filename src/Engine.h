// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include "Analyzer.h"
#include "Metronome.h"
#include <atomic>
#include <map>

namespace pulse {
struct Sample {
    juce::AudioBuffer<float> audio;
    double sampleRate = 48000;
};
class Assets {
public:
    Assets();
    const Sample* load(const juce::File&, juce::String& error);
private:
    juce::AudioFormatManager formats;
    std::map<juce::String, std::unique_ptr<Sample>> samples;
    uint64_t cachedBytes = 0;
};
constexpr int maxEvents = 32768;
struct Event { double beat = 0, length = .25; int track = 0, pitch = 60; float velocity = .8f, probability = 1; };
struct TrackSound {
    Instrument instrument = Instrument::kick;
    float volume = .7f, pan = 0, cutoff = 16000, drive = 0, send = 0, attack = .005f, release = .18f;
    bool muted = false;
    const Sample* sample = nullptr;
    Track::Mixer mixer;
    bool inputMuted=false,isBus=false;
    double sampleStart=0,sampleEnd=0;
    bool sampleLoop=false;
    SynthPatch synth;
};
struct AudioRegion {
    const Sample* sample = nullptr;
    int track = 0;
    double beat = 0, length = 0, offset = 0, fadeIn = 0, fadeOut = 0;
    float gain = 1;
};
struct AutomationRegion { int track = 0, target = 0, firstPoint = 0, pointCount = 0; double beat = 0, length = 0; };
struct Playback {
    std::array<Event, maxEvents> events;
    std::array<TrackSound,maxTracks> tracks;
    std::array<AudioRegion,maxClips> audio;
    std::array<AutomationRegion,maxAutomationClips> automation;
    std::array<AutomationPoint,maxTotalAutomationPoints> points;
    int automationCount = 0, pointCount = 0;
    int eventCount = 0, trackCount = 0, audioCount = 0;
    std::array<int,maxTracks> routingOrder{};
    double bpm = 118, length = 8,loopStart=0,loopEnd=-1;
    float master = .8f, feedback = .3f;
    bool loop = true, metronome = false;
};
juce::Result compilePlayback(const Project&, Assets&, const juce::File& base, Playback&, juce::String& warnings);
float automationValue(const AutomationPoint* points,int count,double localBeat);

// Single UI producer / single audio consumer triple buffer. The producer never
// touches the consumer's front slot. Exchanges transfer ownership of slots.
class Exchange {
public:
    Playback& beginWrite() { return slots[(size_t)back]; }
    void publish() { back = middle.exchange(back | dirty, std::memory_order_acq_rel) & 3; }
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
};

class Engine {
public:
    struct TakePlan { double startBeat=0,gateBeat=0,endBeat=-1; int countInBeats=0; };
    bool beginTake(const TakePlan&);
    std::atomic<int> countInRemaining{0};
    std::atomic<bool> takeGateOpen{false},takeFinished{false};
    bool captureReady()const{return takeGateOpen.load()&&takeReadGeneration.load()==takeGeneration.load();}
    bool captureBlockValid()const{return takeReadGeneration.load()==takeGeneration.load();}
    // Audio-thread-only output from the last render call, relative to its block.
    int captureOffset=0,captureCount=0;
    Engine();
    void prepare(double rate);
    void render(float* left, float* right, int frames);
    struct MonitorInput { const float* left=nullptr; const float* right=nullptr; int track=-1; float gain=0; };
    void render(float* left,float* right,int frames,const MonitorInput&);
    // Set only by an offline renderer before its first callback.
    bool preserveAutomationInTails=false;
    Exchange exchange;
    AnalyzerCapture analyzer;
    std::atomic<bool> playing {false}, resetRequested {false};
    std::atomic<bool> auditionPanicRequested{false};
    std::atomic<double> position {0};
    std::atomic<double> seekRequested {-1};
    std::array<std::atomic<float>,maxTracks> meters;
    std::array<std::atomic<float>,maxTracks> gainReduction;
    std::array<std::array<std::atomic<float>,5>,maxTracks> automationReadout;
    std::atomic<float> masterAutomation{-1};
    std::atomic<float> masterMeter {0}, cpuPercent {0};
    std::atomic<uint64_t> processedFrames {0};
    std::atomic<uint64_t> completedLoops{0};
    std::atomic<uint64_t> chasedNotes{0};
    void setMetronomeSettings(const MetronomeSettings& s){metronomeConfig.store(packMetronome(s),std::memory_order_release);}
    std::atomic<float> observedPeak {0};
    bool audition(int track, int pitch, bool on, float velocity = .8f,float seconds=60);
    void stop();
    void start();
    bool preview(const Sample*,double startSeconds=0,double endSeconds=0);
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
    struct PreviewCommand { const Sample* sample=nullptr; double start=0,end=0; };
    std::array<PreviewCommand,32> previewCommands;
    std::atomic<unsigned> previewWrite{0},previewRead{0};
    const Sample* previewSource=nullptr;
    double previewFrame=0,previewEnd=0;
    struct Voice {
        bool active = false, released = false;
        int track = 0, pitch = 60;
        int scheduledEvent = -1;
        double sourceBeat = 0;
        double phase = 0, phase2 = 0, age = 0, gate = 0, releasedAt = 0, samplePosition = 0;
        float velocity = 1, lastNoise = 0, right = 0;
        uint32_t random = 1234;
        SynthVoice synth;
    };
    struct Input { int track = 0, pitch = 60; bool on = true; float velocity = .8f,seconds=60; };
    std::array<Input,128> input;
    std::atomic<unsigned> inputWrite {0}, inputRead {0};
    std::array<Voice,128> voices;
    std::array<float,maxTracks> filterL {}, filterR {}, filterCoeff {}, panL {}, panR {}, smoothGain {};
    std::array<float,maxTracks> lastCutoff {},lastPan {};
    struct ChannelFX {
        std::array<std::array<float,6>,3> eq;
        std::array<std::array<float,2>,3> zL{},zR{};
        juce::Reverb reverb;
        StereoChorus chorus;
        StereoUtility stereo;
        StereoFlanger flanger;
        StereoPhaser phaser;
        float envelope=0,attack=0,release=0,reduction=0;
    };
    std::array<ChannelFX,maxTracks> effects;
    std::array<float,maxTracks> detectorLast{};
    void configureEffects();
    void processEffects(int track,const TrackSound&,float& left,float& right);
    std::vector<float> delayL, delayR;
    size_t delayPos = 0;
    double sampleRate = 48000, beat = 0;
    int nextEvent = 0, loopCycle = 0;
    const Playback* state = nullptr;
    bool preparedChange = true;
    bool chasePending = false;
    std::atomic<bool> resumeRequested{false};
    Voice& trigger(const Event&, double gateSamples,int eventIndex=-1);
    void chaseNotes(double at);
    void clear();
    float voiceSample(Voice&, const TrackSound&);
};
juce::Result renderWav(const Project&, Assets&, const juce::File& base, const juce::File& target,
                       int bits = 24, int onlyTrack = -1, std::atomic<bool>* cancel = nullptr,
                       std::atomic<float>* progress = nullptr);
}

