// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "Synth.h"
#include "Chorus.h"
#include "Modulation.h"
#include "Stereo.h"
#include <array>
#include <vector>
#include <functional>

namespace pulse {
constexpr int projectSchema=14;
constexpr int patternColourCount=13; // Automatic plus twelve fixed palette colours.
juce::String patternColourName(int);
uint32_t patternColourARGB(int); // Automatic returns zero; caller supplies legacy colour.
constexpr int maxTracks = 16;
constexpr int maxPatterns = 32;
constexpr int maxNotesPerLane = 512;
constexpr int maxClips = 512;
constexpr int maxAutomationClips = 64, maxAutomationPoints = 256, maxTotalAutomationPoints = 4096;
constexpr int maxSongMarkers=128;
struct SongMarker {
    juce::String id,name;
    double beat=0,endBeat=-1;
    int colour=0;
    bool isSection()const{return endBeat>=0;}
};
enum class AutomationTarget { volume, pan, cutoff, drive, send, master };
juce::String automationName(AutomationTarget);
struct AutomationPoint { double beat = 0; float value = .5f; bool hold = false; };
enum class Instrument { kick, snare, hat, clap, openHat, bass, pad, lead, sample, analog, fm, wavetable };
constexpr int instrumentCount=12;
bool isSynth(Instrument);
juce::String instrumentName(Instrument);
struct Note {
    double beat = 0, length = 0.25;
    int pitch = 60;
    float velocity = 0.8f, probability = 1.0f;
    int ratchets = 1;
    bool operator==(const Note&) const = default;
};
constexpr int builtinEffectCount=6;
using EffectOrder=std::array<int,builtinEffectCount>;
juce::String effectName(int,bool compact=false);
juce::Result validateEffectOrder(const EffectOrder&);
juce::var effectOrderToJson(const EffectOrder&);
juce::Result effectOrderFromJson(const juce::var&,EffectOrder&);
struct Track {
    struct Mixer {
        int output=-1,sidechain=-1;
        bool eq=false,compressor=false,reverb=false;
        EffectOrder order{0,1,2,3,4,5};
        ChorusSettings chorus;
        StereoSettings stereo;
        ModulationSettings flanger=defaultModulation(ModulationKind::flanger),phaser=defaultModulation(ModulationKind::phaser);
        float low=0,mid=0,high=0,threshold=-18,ratio=4,compAttack=10,compRelease=120,makeup=0;
        float reverbMix=.15f,room=.5f,damping=.5f;
    } mixer;
    bool isBus=false;
    juce::String name;
    Instrument instrument = Instrument::kick;
    float volume = 0.7f, pan = 0, cutoff = 16000, drive = 0, send = 0;
    float attack = 0.005f, release = 0.18f;
    bool mute = false, solo = false;
    juce::String samplePath;
    double sampleStart=0,sampleEnd=0; // Seconds; zero end means the full source.
    bool sampleLoop=false;
    SynthPatch synth;
    juce::String soundName;
};
struct Pattern {
    juce::String name;
    double length = 8;
    int colour=0;
    std::vector<std::vector<Note>> lanes;
    // Editor-only identity survives project copies/history, never serialized.
    juce::String editorId=juce::Uuid().toString();
};
struct Clip {
    int track = 0, pattern = 0; double beat = 0, length = 8;
    juce::String audioPath;
    double sourceOffset = 0; // Seconds, independent of project tempo.
    float gain = 1; double fadeIn = 0, fadeOut = 0; // Fades in beats.
    int automationTarget = -1;
    int automationTrack = -1; // Link is independent of the Playlist lane.
    std::vector<AutomationPoint> points;
    bool isAudio() const { return audioPath.isNotEmpty(); }
    bool isAutomation() const { return automationTarget >= 0; }
};
struct Project {
    struct Recording { int countInBars=0,preRollBars=1; bool punch=false; double punchStart=0,punchEnd=8; } recording;
    juce::String title = "Untitled session";
    double bpm = 118;
    float swing = 0, master = 0.8f, delayFeedback = 0.3f;
    bool songMode = false, loop = true, metronome = false;
    int activePattern = 0;
    std::vector<Track> tracks;
    std::vector<Pattern> patterns;
    std::vector<Clip> clips;
    std::vector<SongMarker> markers;
    juce::String loopSection;
    juce::String notes;
    double songLength() const;
};
Project demoProject();
Project emptyProject();
void ensurePatternEditorIds(Project&);
int patternIndexForEditorId(const Project&,const juce::String&);
juce::var projectToJson(const Project&);
juce::Result projectFromJson(const juce::var&, Project&);
juce::Result saveProject(const Project&, const juce::File&);
juce::Result loadProject(const juce::File&, Project&);
Project absoluteAssets(const Project&, const juce::File& base);
juce::Result collectAssets(const Project&, const juce::File& base, const juce::File& target, Project& collected);
juce::Result writeMidi(const Project&, const juce::File&);
juce::Result importMidi(const juce::File&, Project&);
bool isDrum(Instrument);
juce::Result validateRouting(const Project&,std::array<int,maxTracks>* order=nullptr);
const SongMarker* findSongMarker(const Project&,const juce::String& id);
juce::Result validateSongMarkers(const Project&);
juce::Result storeSongMarker(Project&,const SongMarker&);
juce::Result removeSongMarker(Project&,const juce::String& id);
const SongMarker* adjacentSongMarker(const Project&,double beat,bool next);
}

