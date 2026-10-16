// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Project.h"
#include <deque>
#include <map>
#include <cmath>

namespace pulse {
struct TypingSettings {
    int velocity=102,octave=4;
    bool enabled=true;
    int transpose=0;
    bool capture=true;
    int captureSeconds=60;
};
juce::Result loadTypingSettings(const juce::File&,TypingSettings&);
juce::Result saveTypingSettings(const juce::File&,const TypingSettings&);

// Message-thread, memory-only performance history. Times are monotonic milliseconds,
// independent of transport seeks, loops and wall-clock changes. No hardware MIDI.
class TypingCapture {
public:
    static constexpr double windowMs=60000;
    static constexpr size_t capacity=512;
    void clear();
    bool setWindow(double milliseconds){if(!std::isfinite(milliseconds)||milliseconds<1000||milliseconds>windowMs)return false;historyMs=milliseconds;return true;}
    void noteOn(int pitch,float velocity,double now);
    void noteOff(int pitch,double now);
    void allOff(double now);
    void expire(double now){if(std::isfinite(now))prune(now);}
    juce::Result phrase(double now,double bpm,std::vector<Note>& output,double& length)const;
    size_t size()const{return complete.size()+active.size();}
private:
    struct Played {int pitch=60;float velocity=.8f;double start=0,end=0;};
    std::deque<Played> complete;
    std::map<int,Played> active;
    double historyMs=windowMs;
    void finish(Played,double now);
    void prune(double now);
};
}
