// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
namespace pulse {
enum class ScoreClef { automatic,treble,bass };
struct ScoreMark {
    int tick=0,duration=1,pitch=-1,voice=0,accidental=-1;
    bool chord=false,tieIn=false,tieOut=false;
    bool rest()const{return pitch<0;}
};
struct PatternScore {
    juce::String title,pattern,channel;
    double bpm=120,length=8,maxTimingError=0;
    int bars=2,voices=1,notes=0,rounded=0,omitted=0,clipped=0;
    bool bass=false,performance=false;
    std::vector<ScoreMark> marks;
};
// Read-only one-channel pattern snapshot: 4/4, four ticks per quarter note.
// C0 (MIDI 12) to G9 (127), at most 512 authored notes / 16 independent voices.
// Coincident notes with identical gates form chords; other overlaps use voices.
// Source notes are never quantized in place. Failure retains output.
juce::Result buildPatternScore(const Project&,int pattern,int track,ScoreClef,PatternScore&);
juce::Result validatePatternScore(const PatternScore&);
juce::String scoreSummary(const PatternScore&);
juce::String scoreMusicXml(const PatternScore&);
juce::String scoreSvg(const PatternScore&);
// Atomic/read-back-verified local file export; no external assets/URLs/DTD.
juce::Result savePatternScore(const juce::File&,const PatternScore&,bool svg);
int scoreDiatonic(int pitch);
const char* scoreNoteType(int ticks);
bool scoreDotted(int ticks);
}
