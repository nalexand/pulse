// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Chorus.h"
namespace pulse {
enum class ModulationKind { flanger,phaser };
struct ModulationSettings {
    bool enabled=false;
    float mix=.5f,rate=.25f,depth=.6f,centre=2,feedback=.5f,width=1;
    bool operator==(const ModulationSettings&)const=default;
};
ModulationSettings defaultModulation(ModulationKind);
const char* modulationName(ModulationKind);
const std::array<ChorusParameterSpec,6>& modulationSpecs(ModulationKind);
double modulationValue(const ModulationSettings&,int);
double modulationNormal(ModulationKind,int,double);
double modulationFromNormal(ModulationKind,int,double);
juce::Result setModulationValue(ModulationSettings&,ModulationKind,int,double);
juce::Result validateModulation(const ModulationSettings&,ModulationKind);
juce::var modulationToJson(const ModulationSettings&);
juce::Result modulationFromJson(const juce::var&,ModulationKind,ModulationSettings&);
class StereoFlanger {
public:
    void prepare(double sampleRate);
    void reset();
    void setSettings(const ModulationSettings&);
    void process(float&,float&)noexcept;
private:
    std::array<std::vector<float>,2> history;
    ModulationSettings target;
    std::array<double,6> smooth{};
    double rate=48000,coefficient=.001,fadeCoefficient=.002,phase=0,wetGain=0;
    size_t write=0,filled=0;
    bool running=false;
    float read(int,double)const noexcept;
};
// Six first-order all-pass sections per channel, with a bounded resonance tap.
class StereoPhaser {
public:
    void prepare(double sampleRate);
    void reset();
    void setSettings(const ModulationSettings&);
    void process(float&,float&)noexcept;
private:
    ModulationSettings target=defaultModulation(ModulationKind::phaser);
    std::array<double,6> smooth{};
    std::array<std::array<double,6>,2> memory{};
    std::array<double,2> last{};
    double rate=48000,coefficient=.001,fadeCoefficient=.002,phase=0,wetGain=0;
    bool running=false;
};
}
