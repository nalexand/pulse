// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
namespace pulse {
// Local input preferences. Off ignores the retained manual correction.
struct InputAlignmentSettings {int mode=0;double manualMs=0;bool operator==(const InputAlignmentSettings&)const=default;};
juce::Result validateInputAlignment(const InputAlignmentSettings&);
juce::var inputAlignmentToJson(const InputAlignmentSettings&);
juce::Result inputAlignmentFromJson(const juce::var&,InputAlignmentSettings&);
juce::String inputAlignmentName(int);
juce::Result resolveInputAlignment(const InputAlignmentSettings&,double rate,int inputSamples,int outputSamples,int64_t& frames);
struct AlignedInputWindow {double startBeat=0,gateBeat=0,endBeat=0,clipBeat=0;uint64_t frames=0;bool limited=false;};
// Shift the capture clock, not the PCM or intended musical placement. Inputs
// must already include the user-selected duration/punch and storage budgets.
juce::Result alignInputWindow(double start,double gate,double end,double bpm,double rate,int64_t offset,AlignedInputWindow&);
}
