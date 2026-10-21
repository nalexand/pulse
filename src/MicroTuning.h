// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
#include <array>
namespace pulse {
// Offsets from MIDI 12-TET at the reference A4. Stored in the project, never a file dependency.
struct MicroTuning {
    bool enabled=false;
    juce::String name="12-tone equal temperament";
    double referenceHz=440;
    std::array<double,128> cents{};
    bool operator==(const MicroTuning&)const=default;
};
juce::Result validateMicroTuning(const MicroTuning&);
double microTuningCents(const MicroTuning&,int pitch);
double microTuningFrequency(const MicroTuning&,int pitch);
juce::String microTuningLabel(const MicroTuning&,int pitch);
juce::var microTuningToJson(const MicroTuning&);
juce::Result microTuningFromJson(const juce::var&,MicroTuning&);
juce::Result equalDivisionTuning(int divisions,int root,double referenceHz,MicroTuning&);
juce::Result scalaTuning(const juce::String&,int root,double referenceHz,MicroTuning&);
juce::Result loadMicroTuning(const juce::File&,int root,double referenceHz,MicroTuning&);
juce::Result saveMicroTuning(const MicroTuning&,const juce::File&);
}
