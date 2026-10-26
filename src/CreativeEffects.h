// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <memory>
namespace pulse {
struct EchoSettings {
    bool enabled=false;
    int repeats=4;
    float interval=250,fade=.4f,mix=.3f;
    bool operator==(const EchoSettings&)const=default;
};
struct DistortionSettings {
    bool enabled=false;
    int mode=0;
    float drive=18,lowCut=80,tone=6500,presence=0,asymmetry=0,cabinet=.5f,mix=1,output=-6;
    bool operator==(const DistortionSettings&)const=default;
};
struct CreativeSpec {const char* key;const char* name;double low,high,initial;const char* unit;bool logarithmic=false,integer=false;};
const std::array<CreativeSpec,4>& echoSpecs();
const std::array<CreativeSpec,8>& distortionSpecs();
double creativeNormal(const CreativeSpec&,double);
double creativeValue(const CreativeSpec&,double);
double echoValue(const EchoSettings&,int);
double distortionValue(const DistortionSettings&,int);
juce::Result setEchoValue(EchoSettings&,int,double);
juce::Result setDistortionValue(DistortionSettings&,int,double);
juce::Result validateEcho(const EchoSettings&);
juce::Result validateDistortion(const DistortionSettings&);
juce::var echoToJson(const EchoSettings&);
juce::var distortionToJson(const DistortionSettings&);
juce::Result echoFromJson(const juce::var&,EchoSettings&);
juce::Result distortionFromJson(const juce::var&,DistortionSettings&);
class FiniteEcho {
public:
    void prepare(double);
    void reset() noexcept;
    void setSettings(const EchoSettings&,bool preserveHistory=false);
    void process(float&,float&) noexcept;
private:
    // Uninitialized storage is never read until written; bypass/reset do not touch
    // the history. Allocation is restricted to prepare(), outside the callback.
    std::array<std::unique_ptr<float[]>,2> history;
    size_t capacity=0,write=0,filled=0;
    double interval=1,targetInterval=1;bool smoothTime=false;
    double rate=48000,smoothing=.001;
    EchoSettings settings;
    float mix=0,decay=.6f;
};
class GuitarDistortion {
public:
    void prepare(double);
    void reset() noexcept;
    void setSettings(const DistortionSettings&);
    void process(float&,float&) noexcept;
private:
    DistortionSettings settings;
    std::array<double,8> smooth{};
    struct State {double previous=0,low=0,tone=0,air=0,cab1=0,cab2=0,cabLow=0,dc=0,anti1=0,anti2=0;};
    std::array<State,2> state;
    double rate=48000,smoothing=.001;
};
}
