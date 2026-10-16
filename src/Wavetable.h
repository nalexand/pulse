// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
namespace pulse {
constexpr int wavetableCount=8,wavetableFrames=4,wavetableSize=1024;
class WavetableBank;
// Called before playback, never by the voice render function.
void prepareWavetables();
const WavetableBank* preparedWavetables() noexcept;
const char* wavetableName(int);
float readWavetable(const WavetableBank*,int table,double position,double phase,double phaseStep) noexcept;
}
